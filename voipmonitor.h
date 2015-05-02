#include <sys/types.h>
#include <string>
#include "config.h"
#include "common.h"
#include "heap_safe.h"

#ifndef VOIPMONITOR_H
#define VOIPMONITOR_H

#define RTPSENSOR_VERSION "11.1.2"
#define NAT

#define FORMAT_WAV	1
#define FORMAT_OGG	2
#define REGISTER_CLEAN_PERIOD 60	// clean register table for expired items every 60 seconds

#define TYPE_SIP 1
#define TYPE_RTP 2
#define TYPE_RTCP 3
#define TYPE_SKINNY 4

#define STORE_PROC_ID_CDR_1 11
#define STORE_PROC_ID_MESSAGE_1 21
#define STORE_PROC_ID_CLEANSPOOL 41
#define STORE_PROC_ID_REGISTER_1 51
#define STORE_PROC_ID_SAVE_PACKET_SQL 61
#define STORE_PROC_ID_HTTP_1 71
#define STORE_PROC_ID_WEBRTC_1 81
#define STORE_PROC_ID_CACHE_NUMBERS_LOCATIONS 91
#define STORE_PROC_ID_FRAUD_ALERT_INFO 92
#define STORE_PROC_ID_IPACC_1 101
#define STORE_PROC_ID_IPACC_AGR_INTERVAL 111
#define STORE_PROC_ID_IPACC_AGR_HOUR 112
#define STORE_PROC_ID_IPACC_AGR_DAY 113
#define STORE_PROC_ID_IPACC_AGR2_HOUR_1 121

#define GRAPH_DELIMITER 4294967295
#define GRAPH_VERSION 4294967294 
#define GRAPH_MARK 4294967293 

#define SNIFFER_INLINE_FUNCTIONS true
#define TCPREPLAY_WORKARROUND false

#define QUEUE_NONBLOCK2 1

#define SYNC_PCAP_BLOCK_STORE true
#define SYNC_CALL_RTP true

#define RTP_PROF false
#define TAR_PROF false

#define MAX_PROCESS_RTP_PACKET_THREADS 6

#define TAR_MODULO_SECONDS 60

#if defined(HAVE__ATOMIC_FETCH_ADD)
#  define ATOMIC_FETCH_AND_ADD(a, b) __atomic_fetch_add ((a), (b), __ATOMIC_SEQ_CST)
#  define ATOMIC_FETCH_AND_SUB(a, b) __atomic_fetch_sub ((a), (b), __ATOMIC_SEQ_CST)
#  define ATOMIC_TEST_AND_SET(a, b)  __atomic_test_and_set((a), (b))
#  define ATOMIC_CLEAR(a)            __atomic_clear((a), __ATOMIC_SEQ_CST)
#elif defined(HAVE__SYNC_FETCH_AND_ADD)
#  define ATOMIC_FETCH_AND_ADD(a, b) __sync_fetch_and_add((a), (b))
#  define ATOMIC_FETCH_AND_SUB(a, b) __sync_fetch_and_sub((a), (b))
#  define ATOMIC_TEST_AND_SET(a, b)  __sync_lock_test_and_set((a), (b))
#  define ATOMIC_CLEAR(a)            __sync_lock_release((a))
#else
#  define ATOMIC_FETCH_AND_ADD(a, b) (a) += (b)
#  define ATOMIC_FETCH_AND_SUB(a, b) (a) -= (b)
#  define ATOMIC_TEST_AND_SET(a, b)  (a), (a)=(b)
#  define ATOMIC_CLEAR(a)            (a) = NULL
#  error "atomic operations not available"
#endif

/* choose what method wil be used to synchronize threads. NONBLOCK is the fastest. Do not enable both at once */
// this is now defined in Makefile 
//#define QUEUE_NONBLOCK 
//#define QUEUE_MUTEX 

/* if you want to see all new calls in syslog enable DEBUG_INVITE */
//#define DEBUG_INVITE

using namespace std;

void reload_config();
void reload_capture_rules();
void set_context_config();
void convert_filesindex();

void terminate_packetbuffer();

/* For compatibility with Linux definitions... */

#if ( defined( __FreeBSD__ ) || defined ( __NetBSD__ ) )
# ifndef FREEBSD
#  define FREEBSD
# endif
#endif

#ifdef FREEBSD
# include <sys/endian.h>
# define __BYTE_ORDER _BYTE_ORDER
# define __BIG_ENDIAN _BIG_ENDIAN
# define __LITTLE_ENDIAN _LITTLE_ENDIAN
#else
# include <endian.h>
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
# ifndef __BIG_ENDIAN_BITFIELD
#  define __BIG_ENDIAN_BITFIELD
# endif
#else
# ifndef __LITTLE_ENDIAN_BITFIELD
#  define __LITTLE_ENDIAN_BITFIELD
# endif
#endif
#if defined(__BIG_ENDIAN_BITFIELD) && defined(__LITTLE_ENDIAN_BITFIELD)
# error Cannot define both __BIG_ENDIAN_BITFIELD and __LITTLE_ENDIAN_BITFIELD
#endif


#ifndef ulong 
#define ulong unsigned long 
#endif

struct tcphdr2
  {
    u_int16_t source;
    u_int16_t dest;
    u_int32_t seq;
    u_int32_t ack_seq;
#  if __BYTE_ORDER == __LITTLE_ENDIAN
    u_int16_t res1:4;
    u_int16_t doff:4;
    u_int16_t fin:1;
    u_int16_t syn:1;
    u_int16_t rst:1;
    u_int16_t psh:1;
    u_int16_t ack:1;
    u_int16_t urg:1;
    u_int16_t res2:2;
#  elif __BYTE_ORDER == __BIG_ENDIAN
    u_int16_t doff:4;
    u_int16_t res1:4;
    u_int16_t res2:2;
    u_int16_t urg:1;
    u_int16_t ack:1;
    u_int16_t psh:1;
    u_int16_t rst:1;
    u_int16_t syn:1;
    u_int16_t fin:1;
#  else
#   error "Adjust your <bits/endian.h> defines"
#  endif
    u_int16_t window;
    u_int16_t check;
    u_int16_t urg_ptr;
};

#ifndef GLOBAL_DECLARATION
extern 
#endif
sVerbose sverb;

void vm_terminate();

#endif
