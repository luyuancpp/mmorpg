#include "base64.h"
#include <string>
#include <boost/beast/core/detail/base64.hpp>

std::string Base64Encode(const std::string& input) {
	std::string output;
	std::size_t encodedLen = boost::beast::detail::base64::encoded_size(input.size());
	output.resize(encodedLen);
	std::size_t writtenLen = boost::beast::detail::base64::encode(output.data(), input.data(), input.size());
	output.resize(writtenLen);
	return output;
}