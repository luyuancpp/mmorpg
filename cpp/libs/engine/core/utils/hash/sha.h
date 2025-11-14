#pragma once
// 示例（直接放到 centre_service_handler.cpp 或 util 文件）
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <sstream>
#include <iomanip>
#include <string>

static std::string Sha256Hex(const std::string& s) {
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SHA256(reinterpret_cast<const unsigned char*>(s.data()), s.size(), hash);
	std::ostringstream oss;
	oss << std::hex << std::setfill('0');
	for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
		oss << std::setw(2) << static_cast<int>(hash[i]);
	}
	return oss.str();
}

static std::string HmacSha256Hex(const std::string& key, const std::string& msg) {
	unsigned char* result = HMAC(EVP_sha256(),
		reinterpret_cast<const unsigned char*>(key.data()), static_cast<int>(key.size()),
		reinterpret_cast<const unsigned char*>(msg.data()), static_cast<int>(msg.size()),
		nullptr, nullptr);
	if (!result) return {};
	std::ostringstream oss;
	oss << std::hex << std::setfill('0');
	for (int i = 0; i < 32; ++i) { // SHA256 length = 32
		oss << std::setw(2) << static_cast<int>(result[i]);
	}
	return oss.str();
}