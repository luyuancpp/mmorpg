#include "base64.h"
#include <string>
#include <boost/beast/core/detail/base64.hpp>

using namespace boost::beast::detail::base64;

std::string Base64Encode(const std::string& input) {
	std::string output;
	std::size_t encodedLen = encoded_size(input.size());
	output.resize(encodedLen);
	std::size_t writtenLen = encode(output.data(), input.data(), input.size());
	output.resize(writtenLen);
	return output;
}

std::vector<uint8_t> Base64Decode(const std::string& input) {
	// 计算解码后的大小
	std::size_t output_size = decoded_size(input.size());

	// 分配输出缓冲区
	std::vector<uint8_t> output(output_size);

	// 执行解码
	auto result = decode(output.data(), input.data(), input.size());

	// 修正大小（根据实际写入的字节）
	output.resize(result.first);
	return output;
}