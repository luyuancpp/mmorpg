#include "process_info.h"
//////////////////////WIN32/////////////////////////////////
#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "muduo/base/ProcessInfo.h"
#endif // __linux__

#ifdef  WIN32

#include <Windows.h>
#include <process.h>

namespace muduo
{
	namespace ProcessInfo
	{

		std::string hostname()
		{
			// HOST_NAME_MAX 64
			// _POSIX_HOST_NAME_MAX 255
			char buf[256];
			if (::gethostname(buf, sizeof buf) == 0)
			{
				buf[sizeof(buf) - 1] = '\0';
				return buf;
			}
			else
			{
				return "unknownhost";
			}
		}

	}

}//namespace muduo


#endif//WIN32

std::string localip()
{
	auto host = muduo::ProcessInfo::hostname();
	struct hostent* host_entry = gethostbyname(host.c_str());
	return inet_ntoa(*((struct in_addr*)host_entry->h_addr));
}