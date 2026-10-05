#pragma once

#include <windows.h>

#include <cmark-gfm.h>
#include <cmark-gfm-extension_api.h>
#include <cmark-gfm-core-extensions.h>

#include <cstring>
#include <climits>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace UrlUtilDetail {
	inline bool IsWebLink(const cmark_node* node) {
		if (cmark_node_get_type(const_cast<cmark_node*>(node)) != CMARK_NODE_LINK) return false;
		const char* url = cmark_node_get_url(const_cast<cmark_node*>(node));
		return url && (std::strncmp(url, "http://", 7) == 0 ||
			std::strncmp(url, "https://", 8) == 0 ||
			std::strncmp(url, "ftp://", 6) == 0);
	}

	inline bool HasWebLink(cmark_node* node, std::string_view exactText = {}) {
		for (; node; node = cmark_node_next(node)) {
			if (IsWebLink(node)) {
				if (exactText.empty()) return true;
				cmark_node* child = cmark_node_first_child(node);
				const char* literal = child ? cmark_node_get_literal(child) : nullptr;
				if (literal && std::string_view(literal) == exactText && !cmark_node_next(child)) return true;
			}
			if (HasWebLink(cmark_node_first_child(node), exactText)) return true;
		}
		return false;
	}

	inline std::string ToUtf8(std::wstring_view text) {
		if (text.empty()) return {};
		if (text.size() > static_cast<size_t>(INT_MAX)) return {};
		const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
			text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
		if (length <= 0) return {};
		std::string result(length, '\0');
		if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
			static_cast<int>(text.size()), result.data(), length, nullptr, nullptr)) return {};
		return result;
	}

	inline bool Check(std::string_view text, bool exact) {
		if (text.empty()) return false;
		static std::once_flag registered;
		std::call_once(registered, [] { cmark_gfm_core_extensions_ensure_registered(); });
		using Parser = std::unique_ptr<cmark_parser, decltype(&cmark_parser_free)>;
		Parser parser(cmark_parser_new(CMARK_OPT_DEFAULT), &cmark_parser_free);
		if (!parser) return false;
		auto* autolink = cmark_find_syntax_extension("autolink");
		if (!autolink || !cmark_parser_attach_syntax_extension(parser.get(), autolink)) return false;
		cmark_parser_feed(parser.get(), text.data(), text.size());
		using Document = std::unique_ptr<cmark_node, decltype(&cmark_node_free)>;
		Document document(cmark_parser_finish(parser.get()), &cmark_node_free);
		return document && HasWebLink(cmark_node_first_child(document.get()), exact ? text : std::string_view{});
	}
}

// Follows cmark-gfm autolink rules. A full URL must be the entire input.
inline bool isValidUrl(std::string_view url) {
	return UrlUtilDetail::Check(url, true);
}

inline bool isValidUrl(std::wstring_view url) {
	return isValidUrl(UrlUtilDetail::ToUtf8(url));
}

inline bool isContainsUrl(std::string_view text) {
	return UrlUtilDetail::Check(text, false);
}

inline bool isContainsUrl(std::wstring_view text) {
	return isContainsUrl(UrlUtilDetail::ToUtf8(text));
}

inline std::string getContainsUrl(std::wstring_view text) {
	return UrlUtilDetail::ToUtf8(text);
}
