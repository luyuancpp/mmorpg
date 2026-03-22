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
    __u8	tcpi_ca_state;     //congestion state
    __u8	tcpi_retransmits;  //pending retransmit count; reset to zero after completion
    __u8	tcpi_probes;		//segments sent by persist/keepalive timer, not yet acked
    __u8	tcpi_backoff;		//exponential backoff exponent for persist timer; incremented on transmit timeout
    __u8	tcpi_options;		//TCP header options present: window scale, timestamp, MSS, etc.
    __u8	tcpi_snd_wscale : 4, tcpi_rcv_wscale : 4; //window scale factor values
    __u8	tcpi_delivery_rate_app_limited : 1;  //rate-limited flag

    __u32	tcpi_rto;		//retransmission timeout; proportional to RTT
    __u32	tcpi_ato;		//estimated delayed ACK timeout in microseconds;
                            //adjusted based on inter-packet arrival interval and
                            //delayed-ACK timer conditions
    __u32	tcpi_snd_mss;	// local MSS
    __u32	tcpi_rcv_mss;	// peer MSS

    __u32	tcpi_unacked;	//unacknowledged segments
    __u32	tcpi_sacked;    //dual meaning: listen backlog (server), or SACK count
    __u32	tcpi_lost;		//locally detected lost packets; reset after retransmission
    __u32	tcpi_retrans;   /* retransmitted but unacknowledged segments */
    __u32	tcpi_fackets;

    /* Times. */
    __u32	tcpi_last_data_sent;	//ms since last data packet sent
    __u32	tcpi_last_ack_sent;     /* unused */
    __u32	tcpi_last_data_recv;	//ms since last data packet received
    __u32	tcpi_last_ack_recv;     //ms since last ACK received

    /* Metrics. */
    __u32	tcpi_pmtu;			/* last updated path MTU */
    __u32	tcpi_rcv_ssthresh;   //current receive window size
    __u32	tcpi_rtt;			//smoothed round trip time, microseconds	
    __u32	tcpi_rttvar;		//mean RTT deviation; higher value = more jitter
    __u32	tcpi_snd_ssthresh;  //slow-start threshold
    __u32	tcpi_snd_cwnd;		//congestion window size
    __u32	tcpi_advmss;		//local MSS upper limit
    __u32	tcpi_reordering;	/* max reorderable segments without loss */

    __u32	tcpi_rcv_rtt;		// receiver-measured RTT in microseconds; computed from incoming data timing,
                                // not from sender. Typically 0 on data senders that receive no data.
    __u32	tcpi_rcv_space;		/* current receive buffer size */

    __u32	tcpi_total_retrans;  //cumulative total retransmitted packets

    __u64	tcpi_pacing_rate;		//send pacing rate
    __u64	tcpi_max_pacing_rate;	//max pacing rate; unlimited by default, set via SO_MAX_PACING_RATE
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



	
