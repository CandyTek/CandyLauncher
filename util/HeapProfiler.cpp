// In-process heap profiler for memory attribution. Compiled only into profiling builds
// (cmake -DCL_HEAP_PROFILE=ON, see the end of the root CMakeLists.txt); never part of a normal build.
//
// Every module that links this file (host exe, plugin DLLs, cpp-pinyin.dll) gets its own replacement
// operator new / delete. The replacements still call the shared CRT malloc / free, so pointers may
// cross module boundaries as before; a side table (open addressing, VirtualAlloc'd) records the live
// allocations of this module with the call stack that made them. A pointer freed by another module
// simply stays in the table of the module that allocated it (over-count), it is never corrupted.
//
// Each module exports CandyHeapProfileDump(dir): writes "<dir>\<module>.txt" with the live bytes per
// call stack (symbolized through dbghelp, PDBs next to the binaries) and an aggregation by the first
// frame outside std:: / the allocator. The exe additionally starts a thread that waits on the named
// event "CandyLauncherHeapDump", then calls the export of every loaded module into
// "<exe dir>\heap_profile\<n>\" together with a process wide summary (_process.txt: GetProcessMemoryInfo,
// VirtualQuery region totals, all NT heaps with a block size histogram) and sets "CandyLauncherHeapDumpDone".
// Trigger from PowerShell: [Threading.EventWaitHandle]::OpenExisting("CandyLauncherHeapDump").Set()
#include <windows.h>
#include <psapi.h>
#include <dbghelp.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace {

constexpr uint32_t kMaxFrames = 24;

struct StackRec {
	uint64_t hash;
	uint32_t depth;
	uint32_t used;
	void* frames[kMaxFrames];
	int64_t liveBytes;
	int64_t liveCount;
	int64_t totalCount;
	int64_t totalBytes;
};

struct PtrRec {
	void* ptr;
	uint32_t size;
	uint32_t stack;
};

struct State {
	SRWLOCK lock;
	StackRec* stacks;
	PtrRec* ptrs;
	size_t stackCap;
	size_t ptrCap;
	size_t stackCount;
	size_t ptrCount;
	bool ptrOverflow;
	bool stackOverflow;
	bool disabled;
	int64_t liveBytes;
	int64_t liveCount;
	int64_t totalAllocs;
	int64_t totalBytes;
};

// Zero initialized static storage: usable before any dynamic initializer of this module runs.
State g_state;
thread_local bool t_bypass = false;

bool IsExeModule() {
	return GetModuleHandleW(nullptr) == reinterpret_cast<HMODULE>(&__ImageBase);
}

inline uint64_t HashPtr(const void* p) {
	uint64_t x = reinterpret_cast<uint64_t>(p) >> 4;
	x *= 0x9E3779B97F4A7C15ULL;
	x ^= x >> 29;
	return x;
}

bool EnsureTables() {
	if (g_state.stacks) return true;
	if (g_state.disabled) return false;
	// The exe allocates far more than any plugin; keep the plugin tables small so the commit charge of the
	// profiler itself stays low (the dump reports the table sizes so they can be subtracted).
	const bool exe = IsExeModule();
	g_state.stackCap = exe ? (size_t{1} << 17) : (size_t{1} << 14);
	g_state.ptrCap = exe ? (size_t{1} << 22) : (size_t{1} << 19);
	g_state.stacks = static_cast<StackRec*>(VirtualAlloc(nullptr, g_state.stackCap * sizeof(StackRec),
														MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	g_state.ptrs = static_cast<PtrRec*>(VirtualAlloc(nullptr, g_state.ptrCap * sizeof(PtrRec),
													MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
	if (!g_state.stacks || !g_state.ptrs) {
		g_state.disabled = true;
		g_state.stacks = nullptr;
		g_state.ptrs = nullptr;
		return false;
	}
	return true;
}

void Record(void* p, const size_t n) {
	if (!p || t_bypass) return;
	t_bypass = true;
	void* frames[kMaxFrames];
	const USHORT depth = RtlCaptureStackBackTrace(2, kMaxFrames, frames, nullptr);
	uint64_t h = 1469598103934665603ULL;
	for (USHORT i = 0; i < depth; ++i) {
		h ^= reinterpret_cast<uint64_t>(frames[i]);
		h *= 1099511628211ULL;
	}
	AcquireSRWLockExclusive(&g_state.lock);
	if (EnsureTables()) {
		g_state.liveBytes += static_cast<int64_t>(n);
		g_state.liveCount += 1;
		g_state.totalAllocs += 1;
		g_state.totalBytes += static_cast<int64_t>(n);

		const size_t smask = g_state.stackCap - 1;
		size_t si = h & smask;
		uint32_t sidx = UINT32_MAX;
		for (size_t probe = 0; probe < g_state.stackCap; ++probe, si = (si + 1) & smask) {
			StackRec& s = g_state.stacks[si];
			if (!s.used) {
				if (g_state.stackCount < g_state.stackCap * 3 / 4) {
					s.used = 1;
					s.hash = h;
					s.depth = depth;
					std::memcpy(s.frames, frames, sizeof(void*) * depth);
					++g_state.stackCount;
					sidx = static_cast<uint32_t>(si);
				} else {
					g_state.stackOverflow = true;
				}
				break;
			}
			if (s.hash == h && s.depth == depth && std::memcmp(s.frames, frames, sizeof(void*) * depth) == 0) {
				sidx = static_cast<uint32_t>(si);
				break;
			}
		}
		if (sidx != UINT32_MAX) {
			StackRec& s = g_state.stacks[sidx];
			s.liveBytes += static_cast<int64_t>(n);
			s.liveCount += 1;
			s.totalCount += 1;
			s.totalBytes += static_cast<int64_t>(n);
			if (g_state.ptrCount < g_state.ptrCap * 3 / 4) {
				const size_t pmask = g_state.ptrCap - 1;
				size_t j = HashPtr(p) & pmask;
				while (g_state.ptrs[j].ptr) j = (j + 1) & pmask;
				g_state.ptrs[j].ptr = p;
				g_state.ptrs[j].size = static_cast<uint32_t>((std::min)(n, size_t{UINT32_MAX}));
				g_state.ptrs[j].stack = sidx;
				++g_state.ptrCount;
			} else {
				g_state.ptrOverflow = true;
			}
		}
	}
	ReleaseSRWLockExclusive(&g_state.lock);
	t_bypass = false;
}

void Forget(void* p) {
	if (!p || t_bypass || !g_state.ptrs) return;
	t_bypass = true;
	AcquireSRWLockExclusive(&g_state.lock);
	const size_t pmask = g_state.ptrCap - 1;
	size_t j = HashPtr(p) & pmask;
	for (size_t probe = 0; probe < g_state.ptrCap; ++probe, j = (j + 1) & pmask) {
		PtrRec& rec = g_state.ptrs[j];
		if (!rec.ptr) break;
		if (rec.ptr != p) continue;
		StackRec& s = g_state.stacks[rec.stack];
		s.liveBytes -= rec.size;
		s.liveCount -= 1;
		g_state.liveBytes -= rec.size;
		g_state.liveCount -= 1;
		// Linear probing delete with backward shift (no tombstones).
		size_t i = j;
		size_t k = j;
		for (;;) {
			k = (k + 1) & pmask;
			if (!g_state.ptrs[k].ptr) break;
			const size_t home = HashPtr(g_state.ptrs[k].ptr) & pmask;
			const bool movable = (i <= k) ? (home <= i || home > k) : (home <= i && home > k);
			if (movable) {
				g_state.ptrs[i] = g_state.ptrs[k];
				i = k;
			}
		}
		g_state.ptrs[i].ptr = nullptr;
		--g_state.ptrCount;
		break;
	}
	ReleaseSRWLockExclusive(&g_state.lock);
	t_bypass = false;
}

// ---- dump ----

struct BypassScope {
	bool prev;
	BypassScope() : prev(t_bypass) { t_bypass = true; }
	~BypassScope() { t_bypass = prev; }
};

bool g_symReady = false;

void EnsureSymbols() {
	if (g_symReady) return;
	SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
	// Returns FALSE when another module already initialized dbghelp for this process; that is fine.
	SymInitializeW(GetCurrentProcess(), nullptr, TRUE);
	SymRefreshModuleList(GetCurrentProcess());
	g_symReady = true;
}

std::string Narrow(const wchar_t* s) {
	if (!s) return {};
	const int len = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
	std::string out(len > 0 ? len - 1 : 0, '\0');
	if (len > 1) WideCharToMultiByte(CP_UTF8, 0, s, -1, out.data(), len, nullptr, nullptr);
	return out;
}

struct FrameText {
	std::string module;
	std::string function;
	std::string location;
};

FrameText Symbolize(void* addr) {
	FrameText ft;
	HMODULE mod = nullptr;
	if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
						static_cast<LPCWSTR>(addr), &mod) && mod) {
		wchar_t path[MAX_PATH];
		if (GetModuleFileNameW(mod, path, MAX_PATH)) {
			const wchar_t* base = wcsrchr(path, L'\\');
			ft.module = Narrow(base ? base + 1 : path);
		}
	}
	alignas(SYMBOL_INFOW) char buffer[sizeof(SYMBOL_INFOW) + 512 * sizeof(wchar_t)];
	auto* sym = reinterpret_cast<SYMBOL_INFOW*>(buffer);
	sym->SizeOfStruct = sizeof(SYMBOL_INFOW);
	sym->MaxNameLen = 512;
	DWORD64 disp = 0;
	if (SymFromAddrW(GetCurrentProcess(), reinterpret_cast<DWORD64>(addr), &disp, sym)) {
		char tmp[640];
		std::snprintf(tmp, sizeof(tmp), "%s+0x%llx", Narrow(sym->Name).c_str(), static_cast<unsigned long long>(disp));
		ft.function = tmp;
	} else {
		char tmp[64];
		std::snprintf(tmp, sizeof(tmp), "0x%llx", reinterpret_cast<unsigned long long>(addr));
		ft.function = tmp;
	}
	IMAGEHLP_LINEW64 line{};
	line.SizeOfStruct = sizeof(line);
	DWORD ldisp = 0;
	if (SymGetLineFromAddrW64(GetCurrentProcess(), reinterpret_cast<DWORD64>(addr), &ldisp, &line)) {
		const wchar_t* file = wcsrchr(line.FileName, L'\\');
		char tmp[640];
		std::snprintf(tmp, sizeof(tmp), "%s:%lu", Narrow(file ? file + 1 : line.FileName).c_str(), line.LineNumber);
		ft.location = tmp;
	}
	return ft;
}

bool IsAllocatorFrame(const FrameText& f) {
	const std::string& fn = f.function;
	return fn.rfind("std::", 0) == 0 || fn.rfind("operator new", 0) == 0 || fn.find("Record") == 0 ||
		fn.rfind("`anonymous namespace'::", 0) == 0 || fn.find("HeapProfiler") != std::string::npos ||
		fn.rfind("nlohmann::", 0) == 0 || fn.rfind("rapidfuzz::", 0) == 0;
}

void WriteModuleDump(const wchar_t* dir) {
	BypassScope bypass;
	wchar_t modulePath[MAX_PATH];
	GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), modulePath, MAX_PATH);
	const wchar_t* moduleName = wcsrchr(modulePath, L'\\');
	moduleName = moduleName ? moduleName + 1 : modulePath;

	std::vector<StackRec> stacks;
	State snapshot{};
	{
		AcquireSRWLockExclusive(&g_state.lock);
		snapshot = g_state;
		if (g_state.stacks) {
			stacks.reserve(g_state.stackCount);
			for (size_t i = 0; i < g_state.stackCap; ++i) {
				if (g_state.stacks[i].used && g_state.stacks[i].liveBytes > 0) stacks.push_back(g_state.stacks[i]);
			}
		}
		ReleaseSRWLockExclusive(&g_state.lock);
	}
	std::sort(stacks.begin(), stacks.end(), [](const StackRec& a, const StackRec& b) { return a.liveBytes > b.liveBytes; });

	std::wstring outPath = std::wstring(dir) + L"\\" + moduleName + L".txt";
	FILE* f = _wfopen(outPath.c_str(), L"wb");
	if (!f) return;
	std::fprintf(f, "module %s\n", Narrow(moduleName).c_str());
	std::fprintf(f, "live_bytes %lld live_count %lld total_allocs %lld total_bytes %lld distinct_stacks %zu\n",
				static_cast<long long>(snapshot.liveBytes), static_cast<long long>(snapshot.liveCount),
				static_cast<long long>(snapshot.totalAllocs), static_cast<long long>(snapshot.totalBytes), stacks.size());
	std::fprintf(f, "profiler_tables_bytes %zu ptr_overflow %d stack_overflow %d\n",
				snapshot.stackCap * sizeof(StackRec) + snapshot.ptrCap * sizeof(PtrRec), snapshot.ptrOverflow ? 1 : 0,
				snapshot.stackOverflow ? 1 : 0);

	EnsureSymbols();
	// Symbol cache: the same return addresses repeat across stacks.
	struct Cached {
		void* addr;
		FrameText text;
	};
	std::vector<Cached> cache;
	auto lookup = [&](void* addr) -> const FrameText& {
		for (const auto& c : cache) {
			if (c.addr == addr) return c.text;
		}
		cache.push_back({addr, Symbolize(addr)});
		return cache.back().text;
	};

	// Aggregation by the first frame that is not part of the allocator / standard library.
	struct Agg {
		std::string key;
		int64_t bytes = 0;
		int64_t count = 0;
	};
	std::vector<Agg> aggs;
	const size_t detailLimit = (std::min)(stacks.size(), size_t{600});
	std::fprintf(f, "\n==== top stacks by live bytes (%zu of %zu) ====\n", detailLimit, stacks.size());
	for (size_t i = 0; i < stacks.size(); ++i) {
		const StackRec& s = stacks[i];
		std::string owner;
		for (uint32_t d = 0; d < s.depth; ++d) {
			const FrameText& ft = lookup(s.frames[d]);
			if (!IsAllocatorFrame(ft)) {
				owner = ft.function;
				const size_t plus = owner.find('+');
				if (plus != std::string::npos) owner.resize(plus);
				if (!ft.location.empty()) owner += " [" + ft.location + "]";
				break;
			}
		}
		if (owner.empty()) owner = "<unknown>";
		bool found = false;
		for (auto& a : aggs) {
			if (a.key == owner) {
				a.bytes += s.liveBytes;
				a.count += s.liveCount;
				found = true;
				break;
			}
		}
		if (!found) aggs.push_back({owner, s.liveBytes, s.liveCount});
		if (i >= detailLimit) continue;
		std::fprintf(f, "\n#%zu live_bytes %lld live_count %lld total_count %lld\n", i, static_cast<long long>(s.liveBytes),
					static_cast<long long>(s.liveCount), static_cast<long long>(s.totalCount));
		for (uint32_t d = 0; d < s.depth; ++d) {
			const FrameText& ft = lookup(s.frames[d]);
			std::fprintf(f, "    %s!%s %s\n", ft.module.c_str(), ft.function.c_str(), ft.location.c_str());
		}
	}
	std::sort(aggs.begin(), aggs.end(), [](const Agg& a, const Agg& b) { return a.bytes > b.bytes; });
	std::fprintf(f, "\n==== live bytes by owning function (first frame outside std:: / allocator) ====\n");
	for (const auto& a : aggs) {
		std::fprintf(f, "%12lld %9lld  %s\n", static_cast<long long>(a.bytes), static_cast<long long>(a.count), a.key.c_str());
	}
	std::fclose(f);
}

// ---- process wide summary (exe only) ----

void WriteProcessSummary(const wchar_t* dir) {
	BypassScope bypass;
	const std::wstring outPath = std::wstring(dir) + L"\\_process.txt";
	FILE* f = _wfopen(outPath.c_str(), L"wb");
	if (!f) return;

	PROCESS_MEMORY_COUNTERS_EX pmc{};
	pmc.cb = sizeof(pmc);
	GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc));
	std::fprintf(f, "private_usage %zu working_set %zu peak_ws %zu gdi_objects %lu user_objects %lu\n", pmc.PrivateUsage,
				pmc.WorkingSetSize, pmc.PeakWorkingSetSize, GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS),
				GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS));

	// Region scan: committed bytes by type / protection. Mapped regions without a backing file are pagefile
	// backed sections (DIB sections, shared memory); they count towards the private commit charge.
	size_t privateRw = 0, privateOther = 0, mappedFile = 0, mappedPagefile = 0, imageRw = 0, imageOther = 0, reserved = 0,
		guard = 0;
	struct BigRegion {
		const void* base;
		size_t size;
		DWORD type;
		DWORD protect;
		std::string file;
	};
	std::vector<BigRegion> big;
	MEMORY_BASIC_INFORMATION mbi{};
	for (const char* addr = nullptr; VirtualQuery(addr, &mbi, sizeof(mbi)) == sizeof(mbi);) {
		if (mbi.State == MEM_COMMIT) {
			const bool rw = (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
			if (mbi.Protect & PAGE_GUARD) guard += mbi.RegionSize;
			std::string file;
			if (mbi.Type == MEM_MAPPED) {
				wchar_t name[MAX_PATH];
				if (GetMappedFileNameW(GetCurrentProcess(), mbi.BaseAddress, name, MAX_PATH)) {
					const wchar_t* base = wcsrchr(name, L'\\');
					file = Narrow(base ? base + 1 : name);
					mappedFile += mbi.RegionSize;
				} else {
					file = "<pagefile>";
					mappedPagefile += mbi.RegionSize;
				}
			}
			if (mbi.Type == MEM_PRIVATE) (rw ? privateRw : privateOther) += mbi.RegionSize;
			else if (mbi.Type == MEM_IMAGE) (rw ? imageRw : imageOther) += mbi.RegionSize;
			if (mbi.Type != MEM_IMAGE && mbi.RegionSize >= 192 * 1024) {
				big.push_back({mbi.BaseAddress, mbi.RegionSize, mbi.Type, mbi.Protect, file});
			}
		} else if (mbi.State == MEM_RESERVE) {
			reserved += mbi.RegionSize;
		}
		const char* next = static_cast<const char*>(mbi.BaseAddress) + mbi.RegionSize;
		if (next <= addr) break;
		addr = next;
	}
	std::fprintf(f, "regions: private_rw %zu private_other %zu mapped_file %zu mapped_pagefile %zu image_rw %zu image_other %zu reserved %zu guard %zu\n",
				privateRw, privateOther, mappedFile, mappedPagefile, imageRw, imageOther, reserved, guard);
	std::sort(big.begin(), big.end(), [](const BigRegion& a, const BigRegion& b) { return a.size > b.size; });
	std::fprintf(f, "committed regions >= 192 KB (non image): %zu\n", big.size());
	for (const auto& r : big) {
		std::fprintf(f, "  %p %10zu %s prot 0x%lx %s\n", r.base, r.size, r.type == MEM_PRIVATE ? "private" : "mapped ", r.protect,
					r.file.c_str());
	}

	// Threads with their start routine (NtQueryInformationThread ThreadQuerySetWin32StartAddress) and committed stack.
	EnsureSymbols();
	typedef LONG(NTAPI * NtQueryInformationThreadFn)(HANDLE, int, PVOID, ULONG, PULONG);
	const auto ntQuery = reinterpret_cast<NtQueryInformationThreadFn>(
		GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationThread"));
	const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (snap != INVALID_HANDLE_VALUE && ntQuery) {
		THREADENTRY32 te{};
		te.dwSize = sizeof(te);
		size_t threadCount = 0;
		std::string lines;
		for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
			if (te.th32OwnerProcessID != GetCurrentProcessId()) continue;
			++threadCount;
			const HANDLE h = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
			void* start = nullptr;
			if (h) ntQuery(h, 9, &start, sizeof(start), nullptr);
			const FrameText ft = Symbolize(start);
			char line[1024];
			std::snprintf(line, sizeof(line), "  tid %6lu %s!%s %s\n", te.th32ThreadID, ft.module.c_str(), ft.function.c_str(),
						ft.location.c_str());
			lines += line;
			if (h) CloseHandle(h);
		}
		CloseHandle(snap);
		std::fprintf(f, "threads %zu\n%s", threadCount, lines.c_str());
	}

	// NT heaps: busy / committed bytes per heap and a block size histogram over all heaps.
	HANDLE heaps[256];
	const DWORD heapCount = GetProcessHeaps(256, heaps);
	static const size_t kBuckets[] = {16, 32, 48, 64, 96, 128, 192, 256, 512, 1024, 4096, 16384, 65536, SIZE_MAX};
	constexpr size_t kBucketCount = sizeof(kBuckets) / sizeof(kBuckets[0]);
	size_t histCount[kBucketCount] = {};
	size_t histBytes[kBucketCount] = {};
	size_t totalBusy = 0, totalCommitted = 0, totalBlocks = 0;
	for (DWORD i = 0; i < heapCount && i < 256; ++i) {
		size_t busy = 0, committed = 0, blocks = 0, freeBytes = 0;
		if (!HeapLock(heaps[i])) continue;
		PROCESS_HEAP_ENTRY entry{};
		while (HeapWalk(heaps[i], &entry)) {
			if (entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) {
				busy += entry.cbData;
				++blocks;
				size_t b = 0;
				while (entry.cbData > kBuckets[b]) ++b;
				histCount[b] += 1;
				histBytes[b] += entry.cbData;
			} else if (entry.wFlags & PROCESS_HEAP_REGION) {
				committed += entry.Region.dwCommittedSize;
			} else if (entry.wFlags == 0 || (entry.wFlags & PROCESS_HEAP_UNCOMMITTED_RANGE) == 0) {
				freeBytes += entry.cbData;
			}
		}
		HeapUnlock(heaps[i]);
		std::fprintf(f, "heap %2lu %p busy %10zu blocks %8zu free_in_segments %10zu region_committed %10zu%s\n", i, heaps[i], busy,
					blocks, freeBytes, committed, heaps[i] == GetProcessHeap() ? "  (process heap / CRT)" : "");
		totalBusy += busy;
		totalCommitted += committed;
		totalBlocks += blocks;
	}
	std::fprintf(f, "heaps_total busy %zu blocks %zu region_committed %zu\n", totalBusy, totalBlocks, totalCommitted);
	std::fprintf(f, "busy block size histogram (<= size: count bytes)\n");
	for (size_t b = 0; b < kBucketCount; ++b) {
		if (kBuckets[b] == SIZE_MAX) std::fprintf(f, "  >65536: %zu %zu\n", histCount[b], histBytes[b]);
		else std::fprintf(f, "  <=%zu: %zu %zu\n", kBuckets[b], histCount[b], histBytes[b]);
	}

	// Loaded modules (for the record).
	HMODULE mods[1024];
	DWORD needed = 0;
	if (EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed)) {
		const DWORD n = (std::min)(static_cast<DWORD>(needed / sizeof(HMODULE)), static_cast<DWORD>(1024));
		std::fprintf(f, "modules %lu\n", n);
		for (DWORD i = 0; i < n; ++i) {
			wchar_t path[MAX_PATH];
			MODULEINFO mi{};
			if (GetModuleFileNameW(mods[i], path, MAX_PATH) && GetModuleInformation(GetCurrentProcess(), mods[i], &mi, sizeof(mi))) {
				std::fprintf(f, "  %8lu %s\n", mi.SizeOfImage, Narrow(path).c_str());
			}
		}
	}
	std::fclose(f);
}

typedef void(__cdecl* DumpFn)(const wchar_t*);

DWORD WINAPI DumpThread(LPVOID) {
	HANDLE trigger = CreateEventW(nullptr, FALSE, FALSE, L"CandyLauncherHeapDump");
	HANDLE done = CreateEventW(nullptr, FALSE, FALSE, L"CandyLauncherHeapDumpDone");
	if (!trigger) return 0;
	wchar_t exePath[MAX_PATH];
	GetModuleFileNameW(nullptr, exePath, MAX_PATH);
	std::wstring base(exePath);
	base.resize(base.rfind(L'\\'));
	base += L"\\heap_profile";
	int serial = 0;
	for (;;) {
		WaitForSingleObject(trigger, INFINITE);
		BypassScope bypass;
		CreateDirectoryW(base.c_str(), nullptr);
		std::wstring dir = base + L"\\" + std::to_wstring(++serial);
		CreateDirectoryW(dir.c_str(), nullptr);
		WriteProcessSummary(dir.c_str());
		HMODULE mods[1024];
		DWORD needed = 0;
		if (EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed)) {
			const DWORD n = (std::min)(static_cast<DWORD>(needed / sizeof(HMODULE)), static_cast<DWORD>(1024));
			for (DWORD i = 0; i < n; ++i) {
				if (const auto fn = reinterpret_cast<DumpFn>(GetProcAddress(mods[i], "CandyHeapProfileDump"))) fn(dir.c_str());
			}
		}
		if (done) SetEvent(done);
	}
}

struct ExeStarter {
	ExeStarter() {
		if (IsExeModule()) {
			if (HANDLE h = CreateThread(nullptr, 256 * 1024, DumpThread, nullptr, 0, nullptr)) CloseHandle(h);
		}
	}
};
ExeStarter g_exeStarter;

} // namespace

extern "C" __declspec(dllexport) void __cdecl CandyHeapProfileDump(const wchar_t* dir) {
	WriteModuleDump(dir);
}

// ---- replaceable allocation functions ----

void* operator new(const size_t n) {
	void* p = std::malloc(n ? n : 1);
	if (!p) throw std::bad_alloc();
	Record(p, n);
	return p;
}

void* operator new[](const size_t n) {
	void* p = std::malloc(n ? n : 1);
	if (!p) throw std::bad_alloc();
	Record(p, n);
	return p;
}

void* operator new(const size_t n, const std::nothrow_t&) noexcept {
	void* p = std::malloc(n ? n : 1);
	if (p) Record(p, n);
	return p;
}

void* operator new[](const size_t n, const std::nothrow_t&) noexcept {
	void* p = std::malloc(n ? n : 1);
	if (p) Record(p, n);
	return p;
}

void operator delete(void* p) noexcept {
	Forget(p);
	std::free(p);
}

void operator delete[](void* p) noexcept {
	Forget(p);
	std::free(p);
}

void operator delete(void* p, size_t) noexcept {
	Forget(p);
	std::free(p);
}

void operator delete[](void* p, size_t) noexcept {
	Forget(p);
	std::free(p);
}

void operator delete(void* p, const std::nothrow_t&) noexcept {
	Forget(p);
	std::free(p);
}

void operator delete[](void* p, const std::nothrow_t&) noexcept {
	Forget(p);
	std::free(p);
}

void* operator new(const size_t n, const std::align_val_t al) {
	void* p = _aligned_malloc(n ? n : 1, static_cast<size_t>(al));
	if (!p) throw std::bad_alloc();
	Record(p, n);
	return p;
}

void* operator new[](const size_t n, const std::align_val_t al) {
	void* p = _aligned_malloc(n ? n : 1, static_cast<size_t>(al));
	if (!p) throw std::bad_alloc();
	Record(p, n);
	return p;
}

void* operator new(const size_t n, const std::align_val_t al, const std::nothrow_t&) noexcept {
	void* p = _aligned_malloc(n ? n : 1, static_cast<size_t>(al));
	if (p) Record(p, n);
	return p;
}

void* operator new[](const size_t n, const std::align_val_t al, const std::nothrow_t&) noexcept {
	void* p = _aligned_malloc(n ? n : 1, static_cast<size_t>(al));
	if (p) Record(p, n);
	return p;
}

void operator delete(void* p, std::align_val_t) noexcept {
	Forget(p);
	_aligned_free(p);
}

void operator delete[](void* p, std::align_val_t) noexcept {
	Forget(p);
	_aligned_free(p);
}

void operator delete(void* p, size_t, std::align_val_t) noexcept {
	Forget(p);
	_aligned_free(p);
}

void operator delete[](void* p, size_t, std::align_val_t) noexcept {
	Forget(p);
	_aligned_free(p);
}

void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept {
	Forget(p);
	_aligned_free(p);
}

void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept {
	Forget(p);
	_aligned_free(p);
}
