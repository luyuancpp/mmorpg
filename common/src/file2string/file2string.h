#ifndef COMMON_SRC_FILE2STRING_FILE2STRING_H_
#define COMMON_SRC_FILE2STRING_FILE2STRING_H_

#include <fstream>
#include <string>

namespace common
{
    //http://www.cplusplus.com/reference/istream/istream/read/
    static std::string File2String(const std::string& filename)
    {
        std::ifstream is(filename, std::ifstream::binary);
        std::string jssbuffer;
        if (is) {
            // get length of file:
            is.seekg(0, is.end);
            int length = (int)is.tellg();
            length = length + 1;
            is.seekg(0, is.beg);
            char* buffer = new char[length];
            memset(buffer, 0, length);
            // read data as a block:
            is.read(buffer, length);
            is.close();
            // ...buffer contains the entire file...
            jssbuffer = buffer;
            //jssbuffer.erase(remove_if(jssbuffer.begin(), jssbuffer.end(), iscntrl), jssbuffer.end());
            //jssbuffer.erase(remove_if(jssbuffer.begin(), jssbuffer.end(), isspace), jssbuffer.end());
            delete[] buffer;
        }
        return jssbuffer;
    }

}//namespace common
#endif//COMMON_SRC_FILE2STRING_FILE2STRING_H_