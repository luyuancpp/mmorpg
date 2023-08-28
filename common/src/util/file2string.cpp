#include "file2string.h"

#include <fstream>

#include "muduo/base/Logging.h"

namespace common
{
//http://www.cplusplus.com/reference/istream/istream/read/
std::string File2String(const std::string& filename)
{
    std::ifstream is(filename, std::ifstream::binary);
    std::string json_buffer;
    if (is)
    {
        // get length of file:
        is.seekg(0, is.end);
        int length = (int)is.tellg();
        length = length + 1;
        json_buffer.resize(length);
        is.seekg(0, is.beg);
        // read data as a block:
        is.read(json_buffer.data(), length);
        is.close();
        // ...buffer contains the entire file...
        //json_buffer.erase(remove_if(json_buffer.begin(), json_buffer.end(), iscntrl), json_buffer.end());
        //json_buffer.erase(remove_if(json_buffer.begin(), json_buffer.end(), isspace), json_buffer.end());
    }
    else
    {
        LOG_FATAL << " No such file or directory " << filename;
    }
    return json_buffer;
}

}//namespace common
