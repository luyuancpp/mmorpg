
#include <muduo/base/CrossPlatformAdapterFunction.h>

#include <algorithm>
#include <vector>
#include <chrono>
#include <thread>

#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>

#include <muduo/base/Types.h>

//////////////////////WIN32/////////////////////////////////
#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "ProcessInfo.h"
#endif // __linux__

#ifdef  WIN32

void setbuffer(FILE *stream, char *buf, size_t size)
{
	(void*)stream;
	(void*)buf;
	(void)size;
}

char * strerror_r(int errnum, char *buf, size_t buflen)
{
	strerror_s(buf, buflen, errnum);
	return buf;
}

size_t fwrite_unlocked(const void *ptr, size_t size, size_t n,
	FILE *stream)
{
	return ::fwrite(ptr, size, n, stream);
}

struct tm *gmtime_r(const time_t *timep, struct tm *result)
{
	gmtime_s(result, timep);
	return result;
}

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	(void)tz;
	std::micro microratio;
	uint64_t c = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	tv->tv_sec = (long)(c / microratio.den);
	tv->tv_usec = (long)(c % microratio.den);
	return 0;
}                     

int gethostbyname_r(const char *name,
	struct hostent *ret, char *buf, size_t buflen,
	struct hostent **result, int *h_errnop)
{
	return 0;
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset)
{
	//return ::_read(fd, buf, count);
	(void)fd;
	(void*)buf;
	(void)count;
	(void)offset;
	return 0;
}

extern inline int __libc_use_alloca(size_t size)
{
	return size <= __MAX_ALLOCA_CUTOFF;
}

ssize_t winreadsock(int fd, void *buf, size_t count)
{
	WSAOVERLAPPED recvOverlapped = {};
	WSABUF dataBuf = {};
	DWORD recvBytes = 0;
	DWORD flags = 0;
	dataBuf.len = (ULONG)count;// notice bug
	dataBuf.buf = (char *)buf;

	int ret =  WSARecv(fd, &dataBuf, 1, &recvBytes, &flags, &recvOverlapped, NULL);
	if (ret < 0)
	{
		printf("WSARecv failed with error: %d\n", WSAGetLastError());
		_set_errno(WSAGetLastError());
		return -1;
	}
	return recvBytes;
}

ssize_t winwritesock(int fd, const void *buf, size_t count)
{
	WSAOVERLAPPED sendOverlapped = {};
	SecureZeroMemory((PVOID)& sendOverlapped, sizeof(WSAOVERLAPPED));

	
	DWORD sendBytes = 0;

	WSABUF dataBuf = {};
	dataBuf.len = (ULONG)count;//notice bug
	dataBuf.buf = (char *)buf;
	int ret = WSASend(fd, &dataBuf, 1, &sendBytes, 0, &sendOverlapped, NULL);
	if (ret < 0)
	{
		printf("WSASend failed with error: %d\n", WSAGetLastError());
		_set_errno(WSAGetLastError());
		return -1;
	}
	return sendBytes;
}

ssize_t winclosesock(int fd)
{
	return closesocket(fd);
}

//glibc
ssize_t readv(int fd, const struct iovec *vector, int count)
{

	/* Find the total number of bytes to be read.  */
	ssize_t bytes = 0;
	for (int i = 0; i < count; ++i)
	{
		/* Check for ssize_t overflow.  */
		if (SSIZE_MAX - bytes < vector[i].iov_len)
		{
			SetLastError(EINVAL);
			return -1;
		}
		bytes += vector[i].iov_len;
	}

	/* Allocate a temporary buffer to hold the data.  We should normally
	use alloca since it's faster and does not require synchronization
	with other threads.  But we cannot if the amount of memory
	required is too large.  */
	char *buffer = NULL;
	char *malloced_buffer  = NULL;
	char stack_buffer[__MAX_ALLOCA_CUTOFF] = {};
	std::vector<char> mb((std::size_t)bytes);
	if (__libc_use_alloca((std::size_t)bytes))
		buffer = stack_buffer;
	else
	{
		malloced_buffer = buffer = mb.data();
	}

	/* Read the data.  */
	ssize_t bytes_read = ::winreadsock(fd, buffer, (std::size_t)bytes);
	if (bytes_read < 0)
		return -1;

	/* Copy the data from BUFFER into the memory specified by VECTOR.  */
	bytes = bytes_read;
	for (int i = 0; i < count; ++i)
	{
		ssize_t copy = std::min(vector[i].iov_len, bytes);

		(void)memcpy((void *)vector[i].iov_base, (void *)buffer, (std::size_t)copy);

		buffer += copy;
		bytes -= copy;
		if (bytes == 0)
			break;
	}
	
	return bytes_read;
}

void bzero(void *s, size_t n)
{
	memset(s, '\0', n);
}

char *basename(const char* full_path) {
	static char ret[1024];
	char path_copy[1024];
	char *ptr;

	if (full_path == NULL || strcmp(full_path, "") == 0) {
		strcpy_s(ret, ".");
	}
	else {
		strcpy_s(path_copy, full_path);
		char* p{ nullptr };
		ptr = strtok_s(path_copy, "/", &p);
		if (ptr == NULL)
			strcpy_s(ret, "/");

		while (ptr != NULL) {
			strcpy_s(ret, ptr);
			ptr = strtok_s(NULL, "/", &p);
		}
	}

	ptr = ret;
	return ptr;
}

const int64_t kNanoSecondsPerSecond = (int64_t)1e9;
int nanosleep(const struct timespec *req, struct timespec *rem)
{
	std::this_thread::sleep_for(std::chrono::nanoseconds(req->tv_sec * kNanoSecondsPerSecond + req->tv_nsec));
	(void)rem;
	return 0;
}

int setrlimit(int resource, const struct rlimit *rlim)
{
	(void)resource;
	(void)rlim;

	return 0;
}

class WindowsSocketInitFunction
{
public:
	WindowsSocketInitFunction()
	{
		WSADATA wsaData = { 0 };
		int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0) {
			//...
		}
		
	}
};


WindowsSocketInitFunction initSocketObj;




int timerfd_settime(int fd, int flags,
	const struct itimerspec *new_value,
	struct itimerspec *old_value)
{
	(void)fd;
	(void)flags;
	(void)new_value;
	(void)old_value;
	return 0;
}

unsigned int sleep(unsigned int seconds)
{
	std::this_thread::sleep_for(std::chrono::seconds(seconds));

	return 0;
}
int poll(struct pollfd *fds, size_t nfds, int timeout)
{
	return WSAPoll(fds, (ULONG)nfds, timeout);
}

uid_t geteuid()
{
    return 0;
}


int getsockopt(int sockfd, int level, int optname,
    int *optval, socklen_t *optlen)
{
    return ::getsockopt(sockfd, level, optname, (char*)(optval), optlen);
}

void setsockopt(int sockfd, int level, int optname,
    int *optval, socklen_t optlen)
{
    ::setsockopt(sockfd, level, optname, (const char*)(optval), optlen);
}

int getsockopt(int sockfd, int level, int optname,
    tcp_info *optval, socklen_t *optlen)
{
    return ::getsockopt(sockfd, level, optname, (char*)(optval), optlen);
}

int accept4(int sockfd, struct sockaddr *addr,
    socklen_t *addrlen, int flags)
{
    int connfd = (int)::accept(sockfd, addr, addrlen);
    u_long  mode = 0;
    ioctlsocket(connfd, FIONBIO, &mode);
    return connfd;
}

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


namespace muduo
{
    namespace ProcessInfo
    {
        std::string localip()
        {
            auto host = hostname();
            struct hostent* host_entry = gethostbyname(host.c_str());
            return inet_ntoa(*((struct in_addr*)host_entry->h_addr));
        }

    }

}//namespace muduo
