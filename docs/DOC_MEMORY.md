# Memory footprint: how it is measured and what was trimmed

Measured on 2026-10-15, x64 Release (`cmake-build-release-ninja-vs`), all plugins enabled, fuzzy match on,
~780 Folder items + 2000 browser history rows. "Private" is `PROCESS_MEMORY_COUNTERS_EX::PrivateUsage`
(the commit charge Task Manager shows as "Memory"), not the working set.

| state | before | after |
| --- | --- | --- |
| after start-up (15 s) | 50.2 MB | 36.4 MB |
| after 3 x 68 queries | 56.7 MB | 41.4 MB |
| idle 2 s later | 56.8 MB | 37.5 MB |
| median query round trip | 1.31 ms | 1.25 ms |

Search results are unchanged (result counts per query compared against the packaged 1.0.0 build with
`compare_result_counts.ps1`, see below); index times are unchanged.

## Where the bytes were

Attribution comes from an in-process heap profiler, not from guessing: `util/HeapProfiler.cpp` is compiled into the
host, every plugin DLL and `cpp-pinyin.dll` when CMake is configured with `-DCL_HEAP_PROFILE=ON` (see the end of
`CMakeLists.txt`). It replaces `operator new` / `delete` per module and records every live allocation with its call
stack; setting the named event `CandyLauncherHeapDump` writes `heap_profile\<n>\<module>.txt` (live bytes per stack,
symbolized through dbghelp from the PDBs next to the binaries, plus a per-function aggregation) and `_process.txt`
(memory counters, region totals, every NT heap with a block size histogram, threads with their start routines).
Normal builds never compile it.

Of the 50 MB before this pass, 30 MB was C++ heap (`operator new`), the rest GDI/GDI+ objects, thread stacks,
module data pages and per-DLL CRT state. The heap split by owner:

| owner | before | after | what changed |
| --- | --- | --- | --- |
| host: `CrashHandler::Install` | 5.2 MB | 0 | the crash report buffer was `std::wstring::reserve(2.5 M chars)` at start-up; now only address space is reserved (`VirtualAlloc MEM_RESERVE`) and committed when a crash happens (`ReportBuffer`) |
| host: per-item match fields | 1.4 MB copies | shared | `BaseAction` holds a `shared_ptr<const ProcessedMatchText>` into `g_matchTextCache` instead of copying `matchText / matchTitle / matchPinyin / matchInitials` per item; the cache now evicts entries unused for 10 minutes once it doubles |
| host: double pinyin scheme tables | 0.29 MB | 0.04 MB | the eight `unordered_map` code tables in `PinyinHelper.cpp` are built lazily, only the active scheme exists |
| host: skin | ~3 MB | ~0 | GDI+ `CachedBitmap`s and the window-size scaled bitmap were kept next to the premultiplied DIB layers that are actually painted; they are now created only when a layer failed, and the scaled bitmap / control background images are freed once the three layers exist |
| cpp-pinyin.dll dictionaries | 3.27 MB | 0.52 MB | `unordered_map<..., vector<u16string>>` (52 k allocations) replaced by sorted key arrays + one character pool (`src/ChineseG2p_p.h`: `CharDict`, `PhraseDict`, `CharMap`, `CharSet`); output verified identical over every dictionary entry, load 80 ms -> 20 ms |
| BrowserHistory plugin | 7.5 MB | ~2.5 MB | full URL kept as UTF-8 (`std::string`, half the size of `wstring`, converted only on open / copy); title, subtitle, match text, icon path and stable key are released after the host copied them (`ActionSet::Publish(..., releaseStrings = true)`) |
| CherryTree plugin | 1.3 MB | ~0.8 MB | same `releaseStrings` opt-in (its match text can contain the whole note) |
| NT heap free pages | ~5 MB committed but free | returned | `HeapSetInformation(HeapOptimizeResources)` runs 3 s after the plugin list was rebuilt (`TIMER_HEAP_TRIM`, `util/HeapTrim.hpp`); no effect on Windows 7 |
| Everything SDK result list | per query | freed | `Everything_Reset()` after the results were consumed |

Also fixed on the way: a title containing Chinese plus a non-BMP character (emoji) made `cpp-pinyin` throw
"Invalid UTF-16 surrogate pair" (`splitString` cut surrogate pairs apart); pairs are now kept together and a lone
surrogate becomes U+FFFD.

### What is left (and why it stayed)

- Host copies of title / subtitle (~1.9 MB for 4400 items) and the verbatim match texts plugins build themselves
  (~1 MB): the host must own its strings, plugins may unload.
- `g_matchTextCache` itself (~1.7 MB): `title` and `pinyin` are substrings of `text`; storing offsets instead of
  strings would save ~0.6 MB but needs `fuzzy::Fields` to take `wstring_view` (and the scorer tests with it).
- Plugin-side icon bitmaps of Folder items that also carry an `icon_key` (~0.8 MB duplicate of the host copy):
  releasing them is only safe while the host still holds a proxy with that key, which is not guaranteed for items
  that disappear and come back.
- Per-item `iconPath` copies in plugins that share one icon (BrowserHistory: 2000 x the browser path, ~0.3 MB):
  would need a shared string type in the SDK `ActionBase`.
- Windows thread-pool workers (`std::execution::par` scoring, `std::async` in plugins) grow the thread count from
  23 to ~40 after queries; their stacks are a few hundred KB in total and the pool retires them itself.
- A 70 MB pagefile-backed section visible in every GUI process on the development machine (notepad included) is
  Windhawk's injected engine, not CandyLauncher.

## Re-measuring

The driver scripts live outside the repo in Claude's memory folder for this project
(`measure_release_mem_and_query.ps1`, `run_heap_profile.ps1`, `compare_result_counts.ps1`, `build_relprof.cmd`):

1. `build_relprof.cmd` configures and builds `cmake-build-relprof-ninja-vs` with `-DCL_HEAP_PROFILE=ON` and Release
   flags plus `/Zi /DEBUG /OPT:REF /OPT:ICF` (same code layout as Release).
2. `run_heap_profile.ps1 -BuildDir <tree> [-NoQueries]` copies the all-plugins `user_settings.json` and
   `plugins\config_folder_plugin.json` in, starts the exe, dumps after settle (and after a query burst), prints
   which modules own the threads, and lists the dump files. Read the per-module `live_bytes` line and the
   "live bytes by owning function" table; the profiler's own tables (64 + 26 MB in the exe, 12 MB per DLL) make
   `PrivateUsage` of the profiling build meaningless, and the first dump loads PDBs (~300 MB on the CRT heap), so
   compare one dump per run.
3. `measure_release_mem_and_query.ps1 -BuildDir <tree> -Label x` gives the headline numbers (private / working set,
   threads, query timing) on the normal Release build.
4. `compare_result_counts.ps1 -BuildDir <tree>` reads `LVM_GETITEMCOUNT` after each of 40 queries; run it on the
   new build and on a scratch copy of a known-good build and diff the columns.
