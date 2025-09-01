#pragma once

#include <thread>

#include <muduo/base/Types.h>



#ifdef __linux__
#include <sys/types.h>
#include <sys/time.h>
#define auto_ptr unique_ptr
#endif // __linux__


//////////////////////WIN32/////////////////////////////////
#ifdef  WIN32
#pragma warning(disable:4068)
//head
#include <WS2tcpip.h>
#include <Windows.h>

#include <errno.h>
#include <io.h>
#include <process.h>
#include <string.h>
#include <stdio.h>  
#include <time.h>

#include <string>
#include <cstdint>
#include <cstdio>

//lib
#pragma comment(lib, "Ws2_32.lib")

#include <Windows.h>
#include <Dbghelp.h>
#pragma comment(lib, "Dbghelp.lib")

//function
#define S_ISREG(m) (((m) & 0170000) == (0100000))
#define S_ISDIR(m) (((m) & 0170000) == (0040000))

#define __builtin_expect(EXP, C)  (EXP)
#undef min
#undef max
#define timegm _mkgmtime

#define eventfd(initval, flags) sockets::createNonblockingOrDie(AF_INET)
#define timerfd_create(clockid, flags) muduo::net::sockets::createNonblockingOrDie(AF_INET)


#define SOL_TCP 0
#define TCP_INFO 0

//type 
typedef long long ssize_t;
#define pid_t int32_t
#undef off_t
#define off_t uint64_t
#define sa_family_t int32_t
#define rlim_t uint64_t
typedef uint64_t uid_t;
typedef uint32_t in_addr_t;



//keyword
#define __thread __declspec( thread )
#define O_CLOEXEC 0
#define RLIMIT_AS 0
#define SHUT_RD SD_RECEIVE
#define SHUT_WR SD_SEND
#define SHUT_RDWR SD_BOTH
#undef  SSIZE_MAX
#define SSIZE_MAX (LLONG_MAX >> 1)
#define __MAX_ALLOCA_CUTOFF	65536
#undef POLLRDHUP
#define POLLRDHUP 0
#define EFD_NONBLOCK FIONBIO
#define EFD_CLOEXEC 0
#define CLOCK_MONOTONIC 0
#define TFD_NONBLOCK FIONBIO
#define TFD_CLOEXEC 0
#undef POLLPRI
#define POLLPRI 0
#define  SOCK_NONBLOCK 0
#define  SOCK_CLOEXEC 0


//struct 
struct iovec {
	void  *iov_base;    /* Starting address */
	ssize_t iov_len;     /* Number of bytes to transfer */
};

struct rlimit {
	rlim_t rlim_cur;  /* Soft limit */
	rlim_t rlim_max;  /* Hard limit (ceiling for rlim_cur) */
};

struct itimerspec {
	struct timespec it_interval;  /* Interval for periodic timer */
	struct timespec it_value;     /* Initial expiration */
};



//class 




//function
void setbuffer(FILE *stream, char *buf, size_t size);

char * strerror_r(int errnum, char *buf, size_t buflen);



size_t fwrite_unlocked(const void *ptr, size_t size, size_t n, FILE *stream);

struct tm *gmtime_r(const time_t *timep, struct tm *result);

int gettimeofday(struct timeval *tv, struct timezone *tz);

int gethostbyname_r(const char *name,
	struct hostent *ret, char *buf, size_t buflen,
	struct hostent **result, int *h_errnop);

char* strptime(const char* s,
    const char* f,
    struct tm* tm);

/* Swap bytes in 16 bit value.  */
#define __bswap_constant_16(x) \
	((unsigned short int)((((x) >> 8) & 0xffu) | (((x) & 0xffu) << 8)))

/* Swap bytes in 32 bit value.  */
#define __bswap_constant_32(x) \
     ((((x) & 0xff000000u) >> 24) | (((x) & 0x00ff0000u) >>  8) |	      \
      (((x) & 0x0000ff00u) <<  8) | (((x) & 0x000000ffu) << 24))

static __inline unsigned int
__bswap_32(unsigned int __bsx)
{
	return __bswap_constant_32(__bsx);
}

/* Swap bytes in 64 bit value.  */
# define __bswap_constant_64(x) \
     ((((x) & 0xff00000000000000ull) >> 56)				      \
      | (((x) & 0x00ff000000000000ull) >> 40)				      \
      | (((x) & 0x0000ff0000000000ull) >> 24)				      \
      | (((x) & 0x000000ff00000000ull) >> 8)				      \
      | (((x) & 0x00000000ff000000ull) << 8)				      \
      | (((x) & 0x0000000000ff0000ull) << 24)				      \
      | (((x) & 0x000000000000ff00ull) << 40)				      \
      | (((x) & 0x00000000000000ffull) << 56))

static __inline uint64_t
__bswap_64(uint64_t __bsx)
{
	return __bswap_constant_64(__bsx);
}

static __inline unsigned short int
__bswap_16(unsigned short int __bsx)
{
	return __bswap_constant_16(__bsx);
}

# if __BYTE_ORDER == __LITTLE_ENDIAN
#  define htobe16(x) __bswap_16 (x)
#  define htole16(x) __uint16_identity (x)
#  define be16toh(x) __bswap_16 (x)
#  define le16toh(x) __uint16_identity (x)

#  define htobe32(x) __bswap_32 (x)
#  define htole32(x) __uint32_identity (x)
#  define be32toh(x) __bswap_32 (x)
#  define le32toh(x) __uint32_identity (x)

#  define htobe64(x) __bswap_64 (x)
#  define htole64(x) __uint64_identity (x)
#  define be64toh(x) __bswap_64 (x)
#  define le64toh(x) __uint64_identity (x)

# else
#  define htobe16(x) __uint16_identity (x)
#  define htole16(x) __bswap_16 (x)
#  define be16toh(x) __uint16_identity (x)
#  define le16toh(x) __bswap_16 (x)

#  define htobe32(x) __uint32_identity (x)
#  define htole32(x) __bswap_32 (x)
#  define be32toh(x) __uint32_identity (x)
#  define le32toh(x) __bswap_32 (x)

#  define htobe64(x) __uint64_identity (x)
#  define htole64(x) __bswap_64 (x)
#  define be64toh(x) __uint64_identity (x)
#  define le64toh(x) __bswap_64 (x)
# endif

ssize_t pread(int fd, void *buf, size_t count, off_t offset);

ssize_t winreadsock(int fd, void *buf, size_t count);

ssize_t winwritesock(int fd, const void *buf, size_t count);

ssize_t winclosesock(int fd);

ssize_t readv(int fd, const struct iovec *iov, int iovcnt);

void bzero(void *s, size_t n);

char *basename(const char* full_path);

int nanosleep(const struct timespec *req, struct timespec *rem);

int setrlimit(int resource, const struct rlimit *rlim);

int timerfd_settime(int fd, int flags,
	const struct itimerspec *new_value,
	struct itimerspec *old_value);

unsigned int sleep(unsigned int seconds);

int poll(struct pollfd *fds, size_t nfds, int timeout);

uid_t geteuid();

struct tcp_info {
    typedef uint32_t __u8;
    typedef uint32_t __u32;
    typedef uint64_t __u64;
    __u8	tcpi_state;		   //tcp state: TCP_SYN_SENT,TCP_SYN_RECV,TCP_FIN_WAIT1,TCP_CLOSE etc
    __u8	tcpi_ca_state;     //congestion state：
    __u8	tcpi_retransmits;  //重传数，表示当前待重传的包数，这个值在重传完毕后清零
    __u8	tcpi_probes;		///* 持续定时器或保活定时器发送且未确认的段数*/
    __u8	tcpi_backoff;		//用来计算持续定时器的下一个设计值的指数退避算法指数，在传送超时是会递增。
    __u8	tcpi_options;		//tcp头部选项是否包含：扩展因子、时间戳、MSS等内容
    __u8	tcpi_snd_wscale : 4, tcpi_rcv_wscale : 4; //扩展因子数值
    __u8	tcpi_delivery_rate_app_limited : 1;  //限速标志

    __u32	tcpi_rto;		//重传超时时间，这个和RTT有关系，RTT越大，rto越大
    __u32	tcpi_ato;		//用来延时确认的估值，单位为微秒. 
                            //在收到TCP报文时，会根据本次与上次接收的时间间隔来调整改制，在设置延迟确认定时器也会根据
                            //条件修改该值
    __u32	tcpi_snd_mss;	// 本端的MSS
    __u32	tcpi_rcv_mss;	// 对端的MSS

    __u32	tcpi_unacked;	//未确认的数据段数
    __u32	tcpi_sacked;    //2个含义：server端在listen阶段，可以接收连接的数量；收到的SACK报文数量
    __u32	tcpi_lost;		//本端在发送出去被丢失的报文数。重传完成后清零
    __u32	tcpi_retrans;   /* 重传且未确认的数据段数 */
    __u32	tcpi_fackets;

    /* Times. */
    __u32	tcpi_last_data_sent;	//当前时间-最近一个包的发送时间，单位是毫秒
    __u32	tcpi_last_ack_sent;     /* 未使用*/
    __u32	tcpi_last_data_recv;	//当前时间-最近接收数据包的时间，单位是毫秒
    __u32	tcpi_last_ack_recv;     //当前时间-最近接收ack的时间，单位是毫秒

    /* Metrics. */
    __u32	tcpi_pmtu;			/* 最后一次更新的路径MTU */
    __u32	tcpi_rcv_ssthresh;   //当前接收窗口的大小
    __u32	tcpi_rtt;			//smoothed round trip time,微妙	
    __u32	tcpi_rttvar;		//描述RTT的平均偏差，该值越大，说明RTT抖动越大
    __u32	tcpi_snd_ssthresh;  //拥塞控制慢开始阈值
    __u32	tcpi_snd_cwnd;		//拥塞控制窗口大小
    __u32	tcpi_advmss;		//本端的MSS上限
    __u32	tcpi_reordering;	/* 没有丢包时，可以重新排序的数据段数 */

    __u32	tcpi_rcv_rtt;		// 作为接收端，测出的RTT值，单位为微秒. 这个值不是对方计算并传送过来的rtt，而是作为接收端，在没发送数据的情况下
                                // 通过接收发送端发送的数据的情况计算得到的rtt值。在数据发送方，如果不接受数据，这个值一般情况下为0。
    __u32	tcpi_rcv_space;		/* 当前接收缓存的大小 */

    __u32	tcpi_total_retrans;  //统计总重传的包数，持续增长。

    __u64	tcpi_pacing_rate;		//发送速率
    __u64	tcpi_max_pacing_rate;	//最大发送速率，默认是unlimited，可以通过SO_MAX_PACING_RATE来设置
    __u64	tcpi_bytes_acked;    /* RFC4898 tcpEStatsAppHCThruOctetsAcked */
    __u64	tcpi_bytes_received; /* RFC4898 tcpEStatsAppHCThruOctetsReceived */
    __u32	tcpi_segs_out;	     /* RFC4898 tcpEStatsPerfSegsOut */
    __u32	tcpi_segs_in;	     /* RFC4898 tcpEStatsPerfSegsIn */

    __u32	tcpi_notsent_bytes;
    __u32	tcpi_min_rtt;
    __u32	tcpi_data_segs_in;	/* RFC4898 tcpEStatsDataSegsIn */
    __u32	tcpi_data_segs_out;	/* RFC4898 tcpEStatsDataSegsOut */

    __u64   tcpi_delivery_rate;

    __u64	tcpi_busy_time;      /* Time (usec) busy sending data */
    __u64	tcpi_rwnd_limited;   /* Time (usec) limited by receive window */
    __u64	tcpi_sndbuf_limited; /* Time (usec) limited by send buffer */
};

int getsockopt(int sockfd, int level, int optname,
    int *optval, socklen_t *optlen);
void setsockopt(int sockfd, int level, int optname,
    int *optval, socklen_t optlen);
int getsockopt(int sockfd, int level, int optname,
    tcp_info *optval, socklen_t *optlen);
int accept4(int sockfd, struct sockaddr *addr,
    socklen_t *addrlen, int flags);

namespace muduo
{
    namespace ProcessInfo
    {
        std::string hostname();
    }

}//namespace muduo

int32_t win_connect(int sockfd, const struct sockaddr* addr
);

#endif//WIN32

void win_clear();

namespace muduo
{
    namespace ProcessInfo
    {
        std::string localip();
    }

}//namespace muduo



	
