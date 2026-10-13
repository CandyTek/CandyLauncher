#pragma once
// 使用历史：记录每个条目被启动的次数（按半衰期衰减），搜索时给常用条目加一点分。
// 加分上限很小（kMaxBonus），只用于重排近似的结果，不会盖过明显更好的文本匹配。
//
// 读取路径是每次按键、每个候选条目，所以快照是不可变的 shared_ptr：Record 时复制一份再替换，
// 打分线程拿到快照后无锁查表。

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "util/json.hpp"
#include "util/LogUtil.hpp"

class UsageHistory {
public:
	struct Entry {
		double weight = 0; // 衰减后的累计次数
		int64_t lastUsed = 0; // unix 秒
	};

	using Map = std::unordered_map<uint64_t, Entry>;

	static constexpr double kHalfLifeDays = 7.0;
	static constexpr double kMaxBonus = 15.0;
	static constexpr size_t kMaxEntries = 3000;

	static int64_t Now() {
		return std::chrono::duration_cast<std::chrono::seconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();
	}

	static double Decayed(const Entry& e, const int64_t now) {
		const double days = std::max(0.0, static_cast<double>(now - e.lastUsed) / 86400.0);
		return e.weight * std::pow(0.5, days / kHalfLifeDays);
	}

	// 1 次 ≈ 6 分，3 次 ≈ 12 分，7 次以上封顶 15 分；一周不用减半
	static double BonusFor(const Map& map, const uint64_t key, const int64_t now) {
		if (key == 0 || map.empty()) return 0;
		const auto it = map.find(key);
		if (it == map.end()) return 0;
		const double w = Decayed(it->second, now);
		if (w < 0.05) return 0;
		return std::min(kMaxBonus, 6.0 * std::log2(1.0 + w));
	}

	void Load(const std::wstring& path) {
		auto map = std::make_shared<Map>();
		{
			std::ifstream in(path, std::ios::binary);
			if (in) {
				try {
					const nlohmann::json root = nlohmann::json::parse(in, nullptr, true, true);
					for (const auto& item : root.value("items", nlohmann::json::array())) {
						const std::string hex = item.value("k", "");
						if (hex.empty()) continue;
						Entry e;
						e.weight = item.value("w", 0.0);
						e.lastUsed = item.value("t", int64_t{0});
						(*map)[std::stoull(hex, nullptr, 16)] = e;
					}
				} catch (const std::exception& ex) {
					Loge(L"UsageHistory", L"Load failed: ", ex.what());
					map->clear();
				}
			}
		}
		std::lock_guard<std::mutex> lock(m_mutex);
		m_path = path;
		m_map = std::move(map);
	}

	// 记录一次启动并立即落盘（启动是低频操作，文件很小）
	void Record(const uint64_t key) {
		if (key == 0) return;
		const int64_t now = Now();
		std::shared_ptr<Map> next;
		std::wstring path;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			next = std::make_shared<Map>(*m_map);
			Entry& e = (*next)[key];
			e.weight = Decayed(e, now) + 1.0;
			e.lastUsed = now;
			if (next->size() > kMaxEntries) Prune(*next, now);
			m_map = next;
			path = m_path;
		}
		if (!path.empty()) Save(path, *next, now);
	}

	std::shared_ptr<const Map> Snapshot() const {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_map;
	}

private:
	mutable std::mutex m_mutex;
	std::shared_ptr<const Map> m_map = std::make_shared<Map>();
	std::wstring m_path;

	// 超过上限时丢掉衰减后最弱的四分之一
	static void Prune(Map& map, const int64_t now) {
		std::vector<std::pair<double, uint64_t>> order;
		order.reserve(map.size());
		for (const auto& [key, e] : map) order.emplace_back(Decayed(e, now), key);
		std::sort(order.begin(), order.end());
		const size_t drop = map.size() / 4;
		for (size_t i = 0; i < drop && i < order.size(); ++i) map.erase(order[i].second);
	}

	static void Save(const std::wstring& path, const Map& map, const int64_t now) {
		try {
			nlohmann::json items = nlohmann::json::array();
			for (const auto& [key, e] : map) {
				// 衰减到接近 0 的记录不再保存
				if (Decayed(e, now) < 0.05) continue;
				char hex[17];
				snprintf(hex, sizeof(hex), "%016llx", static_cast<unsigned long long>(key));
				items.push_back({{"k", hex}, {"w", e.weight}, {"t", e.lastUsed}});
			}
			nlohmann::json root;
			root["version"] = 1;
			root["items"] = std::move(items);
			std::ofstream out(path, std::ios::binary | std::ios::trunc);
			if (!out) {
				Loge(L"UsageHistory", L"Cannot open for write: ", path);
				return;
			}
			out << root.dump(1, '\t');
		} catch (const std::exception& ex) {
			Loge(L"UsageHistory", L"Save failed: ", ex.what());
		}
	}
};

inline UsageHistory g_usageHistory;
