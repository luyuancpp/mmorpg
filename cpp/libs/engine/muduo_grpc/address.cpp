#include "address.h"

namespace muduo_grpc
{

bool IsCanonicalIpv4(const std::string &ip)
{
    int parts = 0;
    size_t pos = 0;
    while (pos <= ip.size())
    {
        const size_t dot = ip.find('.', pos);
        const size_t end = dot == std::string::npos ? ip.size() : dot;
        const size_t len = end - pos;
        if (len == 0 || len > 3 || (len > 1 && ip[pos] == '0'))
        {
            return false;
        }
        // 逐字符要求是十进制数字:有符号的 from_chars 会接受 "-1"、"-0" 这类段。
        int value = 0;
        for (size_t i = pos; i < end; ++i)
        {
            const char c = ip[i];
            if (c < '0' || c > '9')
            {
                return false;
            }
            value = value * 10 + (c - '0');
        }
        if (value > 255)
        {
            return false;
        }
        ++parts;
        if (dot == std::string::npos)
        {
            break;
        }
        pos = dot + 1;
    }
    return parts == 4;
}

} // namespace muduo_grpc
