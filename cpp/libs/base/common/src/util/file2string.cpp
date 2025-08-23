#include "file2string.h"
#include <fstream>
#include "muduo/base/Logging.h"

// http://www.cplusplus.com/reference/istream/istream/read/
std::string File2String(const std::string& filename)
{
	std::ifstream ifstream(filename, std::ifstream::binary);
	std::string result;
	if (ifstream)
	{
		// Get length of file:
		ifstream.seekg(0, std::ifstream::end);
		const auto length = ifstream.tellg();
		result.resize(static_cast<std::string::size_type>(length));
		ifstream.seekg(0, std::ifstream::beg);
		// Read data as a block:
		ifstream.read(result.data(), length);
		ifstream.close();
		// Buffer contains the entire file...
		// jsonBuffer.erase(remove_if(jsonBuffer.begin(), jsonBuffer.end(), iscntrl), jsonBuffer.end());
		// jsonBuffer.erase(remove_if(jsonBuffer.begin(), jsonBuffer.end(), isspace), jsonBuffer.end());
	}
	else
	{
		LOG_FATAL << " No such file or directory " << filename;
	}
	return result;
}
