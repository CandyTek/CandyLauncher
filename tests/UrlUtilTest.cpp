#include "../util/UrlUtil.hpp"

#include <cassert>

int main() {
	assert(isValidUrl("https://github.com/github/cmark-gfm"));
	assert(isValidUrl("http://example.com/path?q=1"));
	assert(isValidUrl("www.example.com"));
	assert(isValidUrl(L"https://example.com/路径"));
	assert(!isValidUrl(""));
	assert(!isValidUrl("https://"));
	assert(!isValidUrl("Visit https://example.com"));
	assert(!isValidUrl("https://example.com, next"));
	assert(isContainsUrl("Visit https://example.com, next"));
	assert(isContainsUrl(L"访问 https://example.com/path。"));
	assert(!isContainsUrl("Nothing to link here"));
	assert(!isContainsUrl("name@example.com"));
}
