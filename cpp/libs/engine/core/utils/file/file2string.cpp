#include "file2string.h"
#include <fstream>
#include "muduo/base/Logging.h"

std::string File2String(const std::string& filename)
{
	std::ifstream ifstream(filename, std::ifstream::binary);
	std::string result;
	if (ifstream)
	{
		ifstream.seekg(0, std::ifstream::end);
		const auto length = ifstream.tellg();
		result.resize(static_cast<std::string::size_type>(length));
		ifstream.seekg(0, std::ifstream::beg);
		ifstream.read(result.data(), length);
		ifstream.close();
	}
	else
	{
		LOG_FATAL << " No such file or directory " << filename;
	}
	return result;
}
