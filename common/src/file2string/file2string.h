#ifndef COMMON_SRC_FILE2STRING_FILE2STRING_H_
#define COMMON_SRC_FILE2STRING_FILE2STRING_H_

#include <fstream>
#include <string>

namespace common
{
    //https://stackoverflow.com/questions/2602013/read-whole-ascii-file-into-c-stdstring
    std::string File2String(const std::string filename)
    {
        std::ifstream in(filename, std::ios::in | std::ios::binary);
        if (in)
        {
            std::string contents;
            in.seekg(0, std::ios::end);
            contents.resize(in.tellg());
            in.seekg(0, std::ios::beg);
            in.read(&contents[0], contents.size());
            in.close();
            return(contents);
        }
    }

}//namespace common
#endif//COMMON_SRC_FILE2STRING_FILE2STRING_H_