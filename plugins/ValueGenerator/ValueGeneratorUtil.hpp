#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <cctype>
#include <windows.h>
#include <rpc.h>
#include <wincrypt.h>
#include <shlwapi.h>
#include "../../util/md5.hpp"
#include "../../util/StringUtil.hpp"

#pragma comment(lib, "Rpcrt4.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Advapi32.lib")

// ================================
// 生成器数据结构
// ================================
struct GeneratorData {
	std::wstring keyword;		// 关键词(命令)
	std::wstring description;	// 描述
	std::wstring example;		// 示例
};

// ================================
// 计算请求接口
// ================================
class IComputeRequest {
public:
	virtual ~IComputeRequest() = default;

	// 执行计算
	virtual bool Compute() = 0;

	// 将结果转换为字符串
	virtual std::wstring ResultToString() = 0;

	// 获取描述信息
	virtual std::wstring GetDescription() const = 0;

	// 获取二进制结果
	virtual std::vector<uint8_t> GetResult() const = 0;

	// 是否成功
	virtual bool IsSuccessful() const = 0;

	// 获取错误消息
	virtual std::wstring GetErrorMessage() const = 0;
};

// ================================
// GUID 生成器
// ================================
class GUIDGenerator {
public:
	// 预定义的命名空间
	static std::map<std::wstring, GUID> PredefinedNamespaces;

	static void InitPredefinedNamespaces() {
		if (!PredefinedNamespaces.empty()) return;

		// DNS namespace
		PredefinedNamespaces[L"ns:dns"] = { 0x6ba7b810, 0x9dad, 0x11d1, {0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8} };
		// URL namespace
		PredefinedNamespaces[L"ns:url"] = { 0x6ba7b811, 0x9dad, 0x11d1, {0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8} };
		// OID namespace
		PredefinedNamespaces[L"ns:oid"] = { 0x6ba7b812, 0x9dad, 0x11d1, {0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8} };
		// X500 namespace
		PredefinedNamespaces[L"ns:x500"] = { 0x6ba7b814, 0x9dad, 0x11d1, {0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8} };
	}

	// UUID v1: 基于时间的 UUID
	static GUID V1() {
		GUID guid;
		UuidCreate(&guid);
		return guid;
	}

	// UUID v3: 基于 MD5 的命名空间 UUID
	static GUID V3(const GUID& namespaceGuid, const std::wstring& name) {
		return GenerateNameBasedUUID(namespaceGuid, name, 3);
	}

	// UUID v4: 随机 UUID
	static GUID V4() {
		GUID guid;
		UuidCreate(&guid);

		// 设置版本号为 4
		guid.Data3 = (guid.Data3 & 0x0FFF) | 0x4000;
		// 设置变体
		guid.Data4[0] = (guid.Data4[0] & 0x3F) | 0x80;

		return guid;
	}

	// UUID v5: 基于 SHA1 的命名空间 UUID
	static GUID V5(const GUID& namespaceGuid, const std::wstring& name) {
		return GenerateNameBasedUUID(namespaceGuid, name, 5);
	}

	// UUID v7: 基于时间排序的 UUID (使用时间戳 + 随机数)
	static GUID V7() {
		GUID guid;

		// 获取当前时间戳(毫秒)
		auto now = std::chrono::system_clock::now();
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

		// 使用随机数生成器
		std::random_device rd;
		std::mt19937_64 gen(rd());
		std::uniform_int_distribution<uint64_t> dis;

		// 前 48 位使用时间戳
		uint64_t timestamp = ms & 0xFFFFFFFFFFFF;
		guid.Data1 = static_cast<unsigned long>((timestamp >> 16) & 0xFFFFFFFF);
		guid.Data2 = static_cast<unsigned short>((timestamp) & 0xFFFF);

		// 版本号 7
		guid.Data3 = 0x7000 | (dis(gen) & 0x0FFF);

		// 变体和随机数
		uint64_t random = dis(gen);
		guid.Data4[0] = 0x80 | ((random >> 56) & 0x3F);
		for (int i = 1; i < 8; i++) {
			guid.Data4[i] = (random >> ((7 - i) * 8)) & 0xFF;
		}

		return guid;
	}

	// 将 GUID 转换为字符串
	static std::wstring GUIDToString(const GUID& guid) {
		wchar_t guidStr[39];
		swprintf_s(guidStr, 39,
			L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
			guid.Data1, guid.Data2, guid.Data3,
			guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
			guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

		std::wstring result = guidStr;
		// 转换为小写并移除花括号
		for (auto& c : result) {
			c = towlower(c);
		}
		return result.substr(1, result.length() - 2); // 移除 { }
	}

private:
	// 生成基于名称的 UUID (v3 使用 MD5, v5 使用 SHA1)
	static GUID GenerateNameBasedUUID(const GUID& namespaceGuid, const std::wstring& name, int version) {
		GUID result;

		// 将命名空间 GUID 转换为字节数组（按网络字节序）
		std::vector<uint8_t> data;
		data.resize(16 + name.length() * 2);

		// 复制命名空间 GUID (按网络字节序)
		memcpy(data.data(), &namespaceGuid, 16);

		// 复制名称(使用 UTF-8)
		std::string utf8Name = wide_to_utf8(name);
		memcpy(data.data() + 16, utf8Name.c_str(), utf8Name.length());
		data.resize(16 + utf8Name.length());

		if (version == 3) {
			// MD5
			std::string dataStr(data.begin(), data.end());
			std::string hash = websocketpp::md5::md5_hash_string(dataStr);

			// 将哈希的前 16 字节转换为 GUID
			memcpy(&result, hash.c_str(), 16);
		}
		else if (version == 5) {
			// SHA1 使用 Windows Crypto API
			HCRYPTPROV hProv = 0;
			HCRYPTHASH hHash = 0;

			if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
				if (CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
					if (CryptHashData(hHash, data.data(), static_cast<DWORD>(data.size()), 0)) {
						DWORD hashLen = 20;
						std::vector<BYTE> hashData(hashLen);
						if (CryptGetHashParam(hHash, HP_HASHVAL, hashData.data(), &hashLen, 0)) {
							// 将哈希的前 16 字节转换为 GUID
							memcpy(&result, hashData.data(), 16);
						}
					}
					CryptDestroyHash(hHash);
				}
				CryptReleaseContext(hProv, 0);
			}
		}

		// 设置版本号
		result.Data3 = (result.Data3 & 0x0FFF) | (version << 12);
		// 设置变体
		result.Data4[0] = (result.Data4[0] & 0x3F) | 0x80;

		return result;
	}
};

// 初始化静态成员
inline std::map<std::wstring, GUID> GUIDGenerator::PredefinedNamespaces;

// ================================
// 哈希请求
// ================================
enum class HashAlgorithm {
	MD5,
	SHA1,
	SHA256,
	SHA384,
	SHA512
};

class HashRequest : public IComputeRequest {
private:
	HashAlgorithm algorithm;
	std::vector<uint8_t> dataToHash;
	std::vector<uint8_t> result;
	bool isSuccessful = false;
	std::wstring errorMessage;

public:
	HashRequest(HashAlgorithm algo, const std::wstring& data)
		: algorithm(algo) {
		// 将 wstring 转换为 UTF-8 字节数组
		std::string utf8Data = wide_to_utf8(data);
		dataToHash.assign(utf8Data.begin(), utf8Data.end());
	}

	bool Compute() override {
		try {
			if (dataToHash.empty()) {
				errorMessage = L"数据为空";
				isSuccessful = false;
				return false;
			}

			// 使用 Windows Crypto API 计算哈希
			ALG_ID algId;
			switch (algorithm) {
				case HashAlgorithm::MD5:
					algId = CALG_MD5;
					break;
				case HashAlgorithm::SHA1:
					algId = CALG_SHA1;
					break;
				case HashAlgorithm::SHA256:
					algId = CALG_SHA_256;
					break;
				case HashAlgorithm::SHA384:
					algId = CALG_SHA_384;
					break;
				case HashAlgorithm::SHA512:
					algId = CALG_SHA_512;
					break;
				default:
					errorMessage = L"不支持的哈希算法";
					isSuccessful = false;
					return false;
			}

			HCRYPTPROV hProv = 0;
			HCRYPTHASH hHash = 0;

			if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
				if (CryptCreateHash(hProv, algId, 0, 0, &hHash)) {
					if (CryptHashData(hHash, dataToHash.data(), static_cast<DWORD>(dataToHash.size()), 0)) {
						DWORD hashLen = 0;
						DWORD hashLenSize = sizeof(DWORD);
						if (CryptGetHashParam(hHash, HP_HASHSIZE, (BYTE*)&hashLen, &hashLenSize, 0)) {
							std::vector<BYTE> hashData(hashLen);
							if (CryptGetHashParam(hHash, HP_HASHVAL, hashData.data(), &hashLen, 0)) {
								// 转换为十六进制字符串
								std::stringstream ss;
								ss << std::hex << std::setfill('0');
								for (BYTE b : hashData) {
									ss << std::setw(2) << static_cast<int>(b);
								}
								std::string hex_str = ss.str();
								result.assign(hex_str.begin(), hex_str.end());
							}
						}
					}
					CryptDestroyHash(hHash);
				}
				CryptReleaseContext(hProv, 0);
			}
			else {
				errorMessage = L"无法初始化加密上下文";
				isSuccessful = false;
				return false;
			}

			isSuccessful = true;
			return true;
		}
		catch (const std::exception& e) {
			errorMessage = utf8_to_wide(e.what());
			isSuccessful = false;
			return false;
		}
	}

	std::wstring ResultToString() override {
		if (!isSuccessful) {
			return errorMessage;
		}
		// 将结果转换为大写十六进制字符串
		std::string hexStr(result.begin(), result.end());
		std::wstring wHex = utf8_to_wide(hexStr);

		// 转换为大写
		for (auto& c : wHex) {
			c = towupper(c);
		}
		return wHex;
	}

	std::wstring GetDescription() const override {
		std::wstring algoName;
		switch (algorithm) {
			case HashAlgorithm::MD5: algoName = L"MD5"; break;
			case HashAlgorithm::SHA1: algoName = L"SHA1"; break;
			case HashAlgorithm::SHA256: algoName = L"SHA256"; break;
			case HashAlgorithm::SHA384: algoName = L"SHA384"; break;
			case HashAlgorithm::SHA512: algoName = L"SHA512"; break;
		}

		std::wstring data = utf8_to_wide(std::string(dataToHash.begin(), dataToHash.end()));
		return algoName + L"(" + data + L")";
	}

	std::vector<uint8_t> GetResult() const override {
		return result;
	}

	bool IsSuccessful() const override {
		return isSuccessful;
	}

	std::wstring GetErrorMessage() const override {
		return errorMessage;
	}
};

// ================================
// GUID 请求
// ================================
class GUIDRequest : public IComputeRequest {
private:
	int version;
	GUID namespaceGuid;
	std::wstring guidName;
	GUID guidResult;
	bool hasNamespace;
	bool isSuccessful = false;
	std::wstring errorMessage;
	std::vector<uint8_t> result;

public:
	GUIDRequest(int ver) : version(ver), hasNamespace(false) {
		if (version < 1 || version > 7 || version == 2 || version == 6) {
			errorMessage = L"不支持的 UUID 版本。支持的版本:1, 3, 4, 5, 7";
			isSuccessful = false;
		}
		GUIDGenerator::InitPredefinedNamespaces();
	}

	GUIDRequest(int ver, const std::wstring& namespaceStr, const std::wstring& name)
		: version(ver), guidName(name), hasNamespace(true) {
		if (version != 3 && version != 5) {
			errorMessage = L"UUID 版本 " + std::to_wstring(version) + L" 不需要命名空间参数";
			isSuccessful = false;
			return;
		}

		GUIDGenerator::InitPredefinedNamespaces();

		// 尝试从预定义命名空间中查找
		std::wstring lowerNamespace = namespaceStr;
		for (auto& c : lowerNamespace) {
			c = towlower(c);
		}

		auto it = GUIDGenerator::PredefinedNamespaces.find(lowerNamespace);
		if (it != GUIDGenerator::PredefinedNamespaces.end()) {
			namespaceGuid = it->second;
		}
		else {
			// 尝试解析为 GUID
			RPC_WSTR rpcStr = reinterpret_cast<RPC_WSTR>(const_cast<wchar_t*>(namespaceStr.c_str()));
			if (UuidFromStringW(rpcStr, &namespaceGuid) != RPC_S_OK) {
				errorMessage = L"无效的命名空间。需要有效的 GUID 或预定义命名空间(ns:dns, ns:url, ns:oid, ns:x500)";
				isSuccessful = false;
				return;
			}
		}
	}

	bool Compute() override {
		if (!errorMessage.empty()) {
			return false;
		}

		try {
			switch (version) {
				case 1:
					guidResult = GUIDGenerator::V1();
					break;
				case 3:
					if (!hasNamespace) {
						errorMessage = L"UUID v3 需要命名空间和名称参数";
						isSuccessful = false;
						return false;
					}
					guidResult = GUIDGenerator::V3(namespaceGuid, guidName);
					break;
				case 4:
					guidResult = GUIDGenerator::V4();
					break;
				case 5:
					if (!hasNamespace) {
						errorMessage = L"UUID v5 需要命名空间和名称参数";
						isSuccessful = false;
						return false;
					}
					guidResult = GUIDGenerator::V5(namespaceGuid, guidName);
					break;
				case 7:
					guidResult = GUIDGenerator::V7();
					break;
				default:
					errorMessage = L"不支持的 UUID 版本";
					isSuccessful = false;
					return false;
			}

			// 将 GUID 保存到结果中
			result.resize(16);
			memcpy(result.data(), &guidResult, 16);

			isSuccessful = true;
			return true;
		}
		catch (const std::exception& e) {
			errorMessage = utf8_to_wide(e.what());
			isSuccessful = false;
			return false;
		}
	}

	std::wstring ResultToString() override {
		if (!isSuccessful) {
			return errorMessage;
		}
		return GUIDGenerator::GUIDToString(guidResult);
	}

	std::wstring GetDescription() const override {
		switch (version) {
			case 1:
				return L"版本 1: 基于时间的 GUID";
			case 3:
				return L"版本 3 (MD5): 基于命名空间和名称的 GUID";
			case 4:
				return L"版本 4: 随机生成的 GUID";
			case 5:
				return L"版本 5 (SHA1): 基于命名空间和名称的 GUID";
			case 7:
				return L"版本 7: 基于时间排序的随机 GUID";
			default:
				return L"";
		}
	}

	std::vector<uint8_t> GetResult() const override {
		return result;
	}

	bool IsSuccessful() const override {
		return isSuccessful;
	}

	std::wstring GetErrorMessage() const override {
		return errorMessage;
	}
};

// ================================
// Base64 编码/解码请求
// ================================
class Base64Request : public IComputeRequest {
private:
	std::vector<uint8_t> dataToEncode;
	std::wstring result;
	bool isSuccessful = false;
	std::wstring errorMessage;

public:
	Base64Request(const std::wstring& data) {
		std::string utf8Data = wide_to_utf8(data);
		dataToEncode.assign(utf8Data.begin(), utf8Data.end());
	}

	bool Compute() override {
		try {
			if (dataToEncode.empty()) {
				result = L"";
				isSuccessful = true;
				return true;
			}

			DWORD encodedSize = 0;
			if (!CryptBinaryToStringW(
				dataToEncode.data(),
				static_cast<DWORD>(dataToEncode.size()),
				CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
				nullptr,
				&encodedSize)) {
				errorMessage = L"Base64 编码失败";
				isSuccessful = false;
				return false;
			}

			std::vector<wchar_t> buffer(encodedSize);
			if (!CryptBinaryToStringW(
				dataToEncode.data(),
				static_cast<DWORD>(dataToEncode.size()),
				CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
				buffer.data(),
				&encodedSize)) {
				errorMessage = L"Base64 编码失败";
				isSuccessful = false;
				return false;
			}

			result = std::wstring(buffer.data(), encodedSize - 1); // -1 to remove null terminator
			isSuccessful = true;
			return true;
		}
		catch (const std::exception& e) {
			errorMessage = utf8_to_wide(e.what());
			isSuccessful = false;
			return false;
		}
	}

	std::wstring ResultToString() override {
		return result;
	}

	std::wstring GetDescription() const override {
		return L"Base64 编码";
	}

	std::vector<uint8_t> GetResult() const override {
		std::string utf8Result = wide_to_utf8(result);
		return std::vector<uint8_t>(utf8Result.begin(), utf8Result.end());
	}

	bool IsSuccessful() const override {
		return isSuccessful;
	}

	std::wstring GetErrorMessage() const override {
		return errorMessage;
	}
};

class Base64DecodeRequest : public IComputeRequest {
private:
	std::wstring encodedData;
	std::wstring result;
	bool isSuccessful = false;
	std::wstring errorMessage;

public:
	Base64DecodeRequest(const std::wstring& data) : encodedData(data) {}

	bool Compute() override {
		try {
			if (encodedData.empty()) {
				result = L"";
				isSuccessful = true;
				return true;
			}

			DWORD decodedSize = 0;
			if (!CryptStringToBinaryW(
				encodedData.c_str(),
				static_cast<DWORD>(encodedData.length()),
				CRYPT_STRING_BASE64,
				nullptr,
				&decodedSize,
				nullptr,
				nullptr)) {
				errorMessage = L"Base64 解码失败:无效的 Base64 字符串";
				isSuccessful = false;
				return false;
			}

			std::vector<BYTE> buffer(decodedSize);
			if (!CryptStringToBinaryW(
				encodedData.c_str(),
				static_cast<DWORD>(encodedData.length()),
				CRYPT_STRING_BASE64,
				buffer.data(),
				&decodedSize,
				nullptr,
				nullptr)) {
				errorMessage = L"Base64 解码失败";
				isSuccessful = false;
				return false;
			}

			std::string utf8Result(buffer.begin(), buffer.end());
			result = utf8_to_wide(utf8Result);
			isSuccessful = true;
			return true;
		}
		catch (const std::exception& e) {
			errorMessage = utf8_to_wide(e.what());
			isSuccessful = false;
			return false;
		}
	}

	std::wstring ResultToString() override {
		return result;
	}

	std::wstring GetDescription() const override {
		return L"Base64 解码";
	}

	std::vector<uint8_t> GetResult() const override {
		std::string utf8Result = wide_to_utf8(result);
		return std::vector<uint8_t>(utf8Result.begin(), utf8Result.end());
	}

	bool IsSuccessful() const override {
		return isSuccessful;
	}

	std::wstring GetErrorMessage() const override {
		return errorMessage;
	}
};

// ================================
// URL 编码/解码请求
// ================================
class UrlEncodeRequest : public IComputeRequest {
private:
	std::wstring dataToEncode;
	std::wstring result;
	bool isSuccessful = false;
	std::wstring errorMessage;

public:
	UrlEncodeRequest(const std::wstring& data) : dataToEncode(data) {}

	bool Compute() override {
		try {
			std::wstringstream encoded;
			encoded.fill(L'0');
			encoded << std::hex << std::uppercase;

			std::string utf8Data = wide_to_utf8(dataToEncode);

			for (unsigned char c : utf8Data) {
				// 保留未编码的字符(字母、数字、-_.~)
				if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
					encoded << static_cast<wchar_t>(c);
				}
				else if (c == ' ') {
					encoded << L'+'; // 空格编码为 +
				}
				else {
					encoded << L'%' << std::setw(2) << static_cast<int>(c);
				}
			}

			result = encoded.str();
			isSuccessful = true;
			return true;
		}
		catch (const std::exception& e) {
			errorMessage = utf8_to_wide(e.what());
			isSuccessful = false;
			return false;
		}
	}

	std::wstring ResultToString() override {
		return result;
	}

	std::wstring GetDescription() const override {
		return L"URL 编码";
	}

	std::vector<uint8_t> GetResult() const override {
		std::string utf8Result = wide_to_utf8(result);
		return std::vector<uint8_t>(utf8Result.begin(), utf8Result.end());
	}

	bool IsSuccessful() const override {
		return isSuccessful;
	}

	std::wstring GetErrorMessage() const override {
		return errorMessage;
	}
};

class UrlDecodeRequest : public IComputeRequest {
private:
	std::wstring encodedData;
	std::wstring result;
	bool isSuccessful = false;
	std::wstring errorMessage;

	// 十六进制字符转数字
	int HexToInt(wchar_t c) {
		if (c >= L'0' && c <= L'9') return c - L'0';
		if (c >= L'A' && c <= L'F') return c - L'A' + 10;
		if (c >= L'a' && c <= L'f') return c - L'a' + 10;
		return -1;
	}

public:
	UrlDecodeRequest(const std::wstring& data) : encodedData(data) {}

	bool Compute() override {
		try {
			std::string decoded;

			for (size_t i = 0; i < encodedData.length(); ++i) {
				if (encodedData[i] == L'%') {
					if (i + 2 >= encodedData.length()) {
						errorMessage = L"URL 解码失败:无效的转义序列";
						isSuccessful = false;
						return false;
					}

					int high = HexToInt(encodedData[i + 1]);
					int low = HexToInt(encodedData[i + 2]);

					if (high == -1 || low == -1) {
						errorMessage = L"URL 解码失败:无效的十六进制字符";
						isSuccessful = false;
						return false;
					}

					decoded += static_cast<char>((high << 4) | low);
					i += 2;
				}
				else if (encodedData[i] == L'+') {
					decoded += ' ';
				}
				else {
					decoded += static_cast<char>(encodedData[i]);
				}
			}

			result = utf8_to_wide(decoded);
			isSuccessful = true;
			return true;
		}
		catch (const std::exception& e) {
			errorMessage = utf8_to_wide(e.what());
			isSuccessful = false;
			return false;
		}
	}

	std::wstring ResultToString() override {
		return result;
	}

	std::wstring GetDescription() const override {
		return L"URL 解码";
	}

	std::vector<uint8_t> GetResult() const override {
		std::string utf8Result = wide_to_utf8(result);
		return std::vector<uint8_t>(utf8Result.begin(), utf8Result.end());
	}

	bool IsSuccessful() const override {
		return isSuccessful;
	}

	std::wstring GetErrorMessage() const override {
		return errorMessage;
	}
};

// ================================
// Data Escape/Unescape 请求 (类似 Uri.EscapeDataString)
// ================================
class DataEscapeRequest : public IComputeRequest {
private:
	std::wstring dataToEscape;
	std::wstring result;
	bool isSuccessful = false;
	std::wstring errorMessage;

public:
	DataEscapeRequest(const std::wstring& data) : dataToEscape(data) {}

	bool Compute() override {
		try {
			std::wstring escaped;
			DWORD size = static_cast<DWORD>((dataToEscape.length() + 1) * 3);
			std::vector<wchar_t> buffer(size);

			DWORD bufSize = size;
			if (UrlEscapeW(dataToEscape.c_str(), buffer.data(), &bufSize, URL_ESCAPE_PERCENT | URL_ESCAPE_SEGMENT_ONLY)) {
				// 手动实现
				std::wstringstream ss;
				ss << std::hex << std::uppercase;

				std::string utf8Data = wide_to_utf8(dataToEscape);
				for (unsigned char c : utf8Data) {
					if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
						ss << static_cast<char>(c);
					}
					else {
						ss << L'%' << std::setw(2) << std::setfill(L'0') << static_cast<int>(c);
					}
				}
				result = ss.str();
			}
			else {
				result = buffer.data();
			}

			isSuccessful = true;
			return true;
		}
		catch (const std::exception& e) {
			errorMessage = utf8_to_wide(e.what());
			isSuccessful = false;
			return false;
		}
	}

	std::wstring ResultToString() override {
		return result;
	}

	std::wstring GetDescription() const override {
		return L"Data string escaped";
	}

	std::vector<uint8_t> GetResult() const override {
		std::string utf8Result = wide_to_utf8(result);
		return std::vector<uint8_t>(utf8Result.begin(), utf8Result.end());
	}

	bool IsSuccessful() const override {
		return isSuccessful;
	}

	std::wstring GetErrorMessage() const override {
		return errorMessage;
	}
};

class DataUnescapeRequest : public IComputeRequest {
private:
	std::wstring dataToUnescape;
	std::wstring result;
	bool isSuccessful = false;
	std::wstring errorMessage;

public:
	DataUnescapeRequest(const std::wstring& data) : dataToUnescape(data) {}

	bool Compute() override {
		try {
			DWORD size = static_cast<DWORD>(dataToUnescape.length() + 1);
			std::vector<wchar_t> buffer(size);

			wcscpy_s(buffer.data(), size, dataToUnescape.c_str());
			DWORD bufSize = size;

			if (UrlUnescapeW(buffer.data(), NULL, &bufSize, 0) == S_OK) {
				result = buffer.data();
			}
			else {
				errorMessage = L"Data unescape 失败";
				isSuccessful = false;
				return false;
			}

			isSuccessful = true;
			return true;
		}
		catch (const std::exception& e) {
			errorMessage = utf8_to_wide(e.what());
			isSuccessful = false;
			return false;
		}
	}

	std::wstring ResultToString() override {
		return result;
	}

	std::wstring GetDescription() const override {
		return L"Data string unescaped";
	}

	std::vector<uint8_t> GetResult() const override {
		std::string utf8Result = wide_to_utf8(result);
		return std::vector<uint8_t>(utf8Result.begin(), utf8Result.end());
	}

	bool IsSuccessful() const override {
		return isSuccessful;
	}

	std::wstring GetErrorMessage() const override {
		return errorMessage;
	}
};

// ================================
// Hex Escape/Unescape 请求 (类似 Uri.HexEscape)
// ================================
class HexEscapeRequest : public IComputeRequest {
private:
	std::wstring charToEscape;
	std::wstring result;
	bool isSuccessful = false;
	std::wstring errorMessage;

public:
	HexEscapeRequest(const std::wstring& data) : charToEscape(data) {
		if (data.length() != 1) {
			errorMessage = L"HexEscape 只能处理单个字符";
			isSuccessful = false;
		}
	}

	bool Compute() override {
		if (!errorMessage.empty()) {
			return false;
		}

		try {
			wchar_t c = charToEscape[0];
			std::wstringstream ss;
			ss << L'%' << std::hex << std::uppercase << std::setw(2) << std::setfill(L'0') << static_cast<int>(c);
			result = ss.str();

			isSuccessful = true;
			return true;
		}
		catch (const std::exception& e) {
			errorMessage = utf8_to_wide(e.what());
			isSuccessful = false;
			return false;
		}
	}

	std::wstring ResultToString() override {
		return result;
	}

	std::wstring GetDescription() const override {
		return L"Hex escaped char";
	}

	std::vector<uint8_t> GetResult() const override {
		std::string utf8Result = wide_to_utf8(result);
		return std::vector<uint8_t>(utf8Result.begin(), utf8Result.end());
	}

	bool IsSuccessful() const override {
		return isSuccessful;
	}

	std::wstring GetErrorMessage() const override {
		return errorMessage;
	}
};

class HexUnescapeRequest : public IComputeRequest {
private:
	std::wstring dataToUnescape;
	std::wstring result;
	bool isSuccessful = false;
	std::wstring errorMessage;

public:
	HexUnescapeRequest(const std::wstring& data) : dataToUnescape(data) {}

	bool Compute() override {
		try {
			if (dataToUnescape.length() < 3 || dataToUnescape[0] != L'%') {
				errorMessage = L"无效的十六进制转义序列";
				isSuccessful = false;
				return false;
			}

			int high = 0, low = 0;
			wchar_t c1 = dataToUnescape[1];
			wchar_t c2 = dataToUnescape[2];

			if (c1 >= L'0' && c1 <= L'9') high = c1 - L'0';
			else if (c1 >= L'A' && c1 <= L'F') high = c1 - L'A' + 10;
			else if (c1 >= L'a' && c1 <= L'f') high = c1 - L'a' + 10;
			else {
				errorMessage = L"无效的十六进制字符";
				isSuccessful = false;
				return false;
			}

			if (c2 >= L'0' && c2 <= L'9') low = c2 - L'0';
			else if (c2 >= L'A' && c2 <= L'F') low = c2 - L'A' + 10;
			else if (c2 >= L'a' && c2 <= L'f') low = c2 - L'a' + 10;
			else {
				errorMessage = L"无效的十六进制字符";
				isSuccessful = false;
				return false;
			}

			wchar_t unescapedChar = static_cast<wchar_t>((high << 4) | low);
			result = std::wstring(1, unescapedChar);

			isSuccessful = true;
			return true;
		}
		catch (const std::exception& e) {
			errorMessage = utf8_to_wide(e.what());
			isSuccessful = false;
			return false;
		}
	}

	std::wstring ResultToString() override {
		return result;
	}

	std::wstring GetDescription() const override {
		return L"Hex char unescaped";
	}

	std::vector<uint8_t> GetResult() const override {
		std::string utf8Result = wide_to_utf8(result);
		return std::vector<uint8_t>(utf8Result.begin(), utf8Result.end());
	}

	bool IsSuccessful() const override {
		return isSuccessful;
	}

	std::wstring GetErrorMessage() const override {
		return errorMessage;
	}
};
