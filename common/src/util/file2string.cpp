#include "file2string.h"

#include <fstream>

#include "muduo/base/Logging.h"

namespace common
{
//http://www.cplusplus.com/reference/istream/istream/read/
std::string File2String(const std::string& filename)
{
    std::ifstream if_stream(filename, std::ifstream::binary);
    std::string result;
    if (if_stream)
    {
        // get length of file:
        if_stream.seekg(0, std::ifstream::end);
        const auto length = if_stream.tellg();
        result.resize(static_cast<std::string::size_type>(length) + 1);
        if_stream.seekg(0, std::ifstream::beg);
        // read data as a block:
        if_stream.read(result.data(), length);
        if_stream.close();
        // ...buffer contains the entire file...
        //json_buffer.erase(remove_if(json_buffer.begin(), json_buffer.end(), iscntrl), json_buffer.end());
        //json_buffer.erase(remove_if(json_buffer.begin(), json_buffer.end(), isspace), json_buffer.end());
    }
    else
    {
        LOG_FATAL << " No such file or directory " << filename;
    }
    return result;
}

}//namespace common
