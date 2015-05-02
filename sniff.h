/* Martin Vit support@voipmonitor.org
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2.
*/

#ifndef SNIFF_H
#define SNIFF_H

#include <queue>
#include <map>

#include "rqueue.h"
#include "voipmonitor.h"
#include "calltable.h"
#include "pcap_queue_block.h"

#ifdef FREEBSD
#include <machine/endian.h>
#else
#include "asm/byteorder.h"
#endif

#define MAXPACKETLENQRING 1600
#define RTP_FIXED_HEADERLEN 12

#ifdef QUEUE_NONBLOCK
extern "C" {
#include "liblfds.6/inc/liblfds.h"
}
#endif

#define IP_DF           0x4000          /* Flag: "Don't Fragment"       */
#define IP_MF           0x2000          /* Flag: "More Fragments"       */
#define IP_OFFSET       0x1FFF          /* "Fragment Offset" part       */

struct iphdr2 {
#if __BYTE_ORDER == __LITTLE_ENDIAN
	unsigned int ihl:4;
	unsigned int version:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
	unsigned int version:4;
	unsigned int ihl:4;
#else
# error "Please fix <bits/endian.h>"
#endif 
	u_int8_t tos;
	u_int16_t tot_len;
	u_int16_t id;
	u_int16_t frag_off;
	u_int8_t ttl;
	u_int8_t protocol;
	u_int16_t check;
	u_int32_t saddr;
	u_int32_t daddr;
	/*The options start here. */
#ifdef PACKED
} __attribute__((packed));
#else
};
#endif

void *rtp_read_thread_func(void *arg);
void *pcap_read_thread_func(void *arg);

void readdump_libnids(pcap_t *handle);
void readdump_libpcap(pcap_t *handle);
void save_packet(Call *call, struct pcap_pkthdr *header, const u_char *packet, 
		 unsigned int saddr, int source, unsigned int daddr, int dest, 
		 int istcp, iphdr2 *header_ip, char *data, unsigned int datalen, unsigned int dataoffset, int type, 
		 int forceSip, int dlt, int sensor_id);


typedef std::map<in_addr_t, in_addr_t> nat_aliases_t; //!< 


/* this is copied from libpcap sll.h header file, which is not included in debian distribution */
#define SLL_ADDRLEN       8               /* length of address field */
struct sll_header {
	u_int16_t sll_pkttype;          /* packet type */
	u_int16_t sll_hatype;           /* link-layer address type */
	u_int16_t sll_halen;            /* link-layer address length */
	u_int8_t sll_addr[SLL_ADDRLEN]; /* link-layer address */
	u_int16_t sll_protocol;         /* protocol */
};

struct udphdr2 {
        uint16_t        source;
        uint16_t        dest;
        uint16_t        len;
        uint16_t        check;
};

typedef struct {
	Call *call;
#if defined(QUEUE_MUTEX) || defined(QUEUE_NONBLOCK)
	unsigned char *data;
#endif
#ifdef QUEUE_NONBLOCK2
	unsigned char data[MAXPACKETLENQRING];
#endif
	int datalen;
	int dataoffset;
	u_int32_t saddr;
	u_int32_t daddr;
	unsigned short sport;
	unsigned short dport;
	char iscaller;
	char is_rtcp;
	int dlt;
	int sensor_id;
	char save_packet;
	const u_char *packet;
	char istcp;
	struct pcap_pkthdr header;
	struct iphdr2 header_ip;
	volatile char free;
} rtp_packet;

typedef struct {
	Call *call;
	u_int32_t saddr;
	u_int32_t daddr;
	unsigned short sport;
	unsigned short dport;
	char iscaller;
	char is_rtcp;
	int dlt;
	int sensor_id;
	char save_packet;
	const u_char *packet;
	char istcp;
	u_char *data;
	int datalen;
	int dataoffset;
	struct pcap_pkthdr header;
	pcap_block_store *block_store;
	int block_store_index;
} rtp_packet_pcap_queue;

#ifdef QUEUE_NONBLOCK2
extern int opt_pcap_queue;
#endif

struct rtp_read_thread {
	rtp_read_thread()  {
		#ifdef QUEUE_NONBLOCK
			this->pqueue = NULL;
		#endif
		#ifdef QUEUE_NONBLOCK2
			this->vmbuffer = NULL;
			this->vmbuffermax = 0;
			this->readit = 0;
			this->writeit = 0;
			this->rtpp_queue = NULL;
			this->rtpp_queue_quick = NULL;
			this->rtpp_queue_quick_boost = NULL;
		#endif
	}
	pthread_t thread;	       // ID of worker storing CDR thread 
#ifdef QUEUE_MUTEX
	queue<rtp_packet*> pqueue;
	pthread_mutex_t qlock;
	sem_t semaphore;
#endif
#ifdef QUEUE_NONBLOCK
	struct queue_state *pqueue;
#endif
#ifdef QUEUE_NONBLOCK2
	rtp_packet *vmbuffer;
	int vmbuffermax;
	volatile int readit;
	volatile int writeit;
	rqueue<rtp_packet_pcap_queue> *rtpp_queue;
	rqueue_quick<rtp_packet_pcap_queue> *rtpp_queue_quick;
	rqueue_quick_boost<rtp_packet_pcap_queue> *rtpp_queue_quick_boost;
#endif
};

#if defined(QUEUE_MUTEX) || defined(QUEUE_NONBLOCK)
typedef struct {
	struct pcap_pkthdr header;
	u_char *packet;
	int offset;
} pcap_packet;
#endif


#if defined(QUEUE_NONBLOCK2)
typedef struct {
	struct pcap_pkthdr header;
	u_char packet[MAXPACKETLENQRING];
	u_char *packet2;
	int offset;
	volatile char free;
} pcap_packet;
#endif

#define MAXLIVEFILTERS 10
#define MAXLIVEFILTERSCHARS 32

typedef struct livesnifferfilter_s {
	struct state_s {
		bool all_saddr;
		bool all_daddr;
		bool all_bothaddr;
		bool all_addr;
		bool all_srcnum;
		bool all_dstnum;
		bool all_bothnum;
		bool all_num;
		bool all_siptypes;
		bool all_all;
	};
        unsigned int lv_saddr[MAXLIVEFILTERS];
        unsigned int lv_daddr[MAXLIVEFILTERS];
	unsigned int lv_bothaddr[MAXLIVEFILTERS];
        char lv_srcnum[MAXLIVEFILTERS][MAXLIVEFILTERSCHARS];
        char lv_dstnum[MAXLIVEFILTERS][MAXLIVEFILTERSCHARS];
	char lv_bothnum[MAXLIVEFILTERS][MAXLIVEFILTERSCHARS];
	unsigned char lv_siptypes[MAXLIVEFILTERS];
        int uid;
        time_t created_at;
	state_s state;
	void updateState();
} livesnifferfilter_t;

struct livesnifferfilter_use_siptypes_s {
	bool u_invite;
	bool u_register;
	bool u_options;
	bool u_subscribe;
	bool u_message;
};

struct gre_hdr {
#if defined(__LITTLE_ENDIAN_BITFIELD)
#ifdef FREEBSD
        u_int16_t rec:3,
#else
        __u16   rec:3,
#endif
                srr:1,
                seq:1,
                key:1,
                routing:1,
                csum:1,
                version:3,
                reserved:4,
                ack:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
#ifdef FREEBSD
        u_int16_t   csum:1,
#else
	__u16   csum:1,
#endif
                routing:1,
                key:1,
                seq:1,
                srr:1,
                rec:3,
                ack:1,
                reserved:4,
                version:3;
#else
#error "Adjust your <asm/byteorder.h> defines"
#endif
#ifdef FREEBSD
        u_int16_t  protocol;
#else
	__be16	protocol;
#endif
};


#define MAX_TCPSTREAMS 1024

class TcpReassemblySip {
public:
	struct tcp_stream2_s {
		u_int64_t packet_number;
		unsigned int saddr;
		int source; 
		unsigned int daddr;
		int dest;
		char *data;
		int datalen;
		int dataoffset;
		pcap_pkthdr header;
		u_char *packet;
		iphdr2 *header_ip;
		pcap_t *handle;
		int dlt; 
		int sensor_id;
		u_int hash;
		time_t ts;
		u_int32_t seq;
		u_int32_t next_seq;
		u_int32_t ack_seq;
		tcp_stream2_s *next;
		int lastpsh;
	};
public:
	TcpReassemblySip();
	void processPacket(
		u_int64_t packet_number,
		unsigned int saddr, int source, unsigned int daddr, int dest, char *data, int datalen, int dataoffset,
		pcap_t *handle, pcap_pkthdr header, const u_char *packet, struct iphdr2 *header_ip,
		int dlt, int sensor_id,
		bool issip);
	void clean(time_t ts = 0);
private:
	tcp_stream2_s *addPacket(
		tcp_stream2_s *stream, u_int hash,
		u_int64_t packet_number,
		unsigned int saddr, int source, unsigned int daddr, int dest, char *data, int datalen, int dataoffset,
		pcap_t *handle, pcap_pkthdr header, const u_char *packet, struct iphdr2 *header_ip,
		int dlt, int sensor_id);
	void complete(
		tcp_stream2_s *stream, u_int hash);
	tcp_stream2_s *getLastStreamItem(tcp_stream2_s *stream) {
		while(stream->next) {
			stream = stream->next;
		}
		return(stream);
	}
	bool isCompleteStream(tcp_stream2_s *stream) {
		bool streamOneItems = false;
		tcp_stream2_s *lastStreamItem = NULL;
		if(stream->next) {
			lastStreamItem = getLastStreamItem(stream);
		} else {
			lastStreamItem = stream;
			streamOneItems = true;
		}
		if(lastStreamItem->datalen >= 2 && 
		   lastStreamItem->data[lastStreamItem->datalen - 2] == 0x0d && lastStreamItem->data[lastStreamItem->datalen - 1] == 0x0a) {
			return(true);
		}
		if(streamOneItems) {
			char *endHeaderSepPos = (char*)memmem(stream->data, stream->datalen, "\r\n\r\n", 4);
			if(endHeaderSepPos) {
				*endHeaderSepPos = 0;
				char *contentLengthPos = strcasestr(stream->data, "Content-Length: ");
				bool okLength = false;
				if(contentLengthPos) {
					unsigned int contentLength = atol(contentLengthPos + 16);
					if((endHeaderSepPos - stream->data) + 4 + contentLength == stream->datalen) {
						okLength = true;
					}
				}
				*endHeaderSepPos = '\r';
				if(okLength) {
					return(true);
				}
			}
		}
		return(false);
	}
private:
	tcp_stream2_s *tcp_streams_hashed[MAX_TCPSTREAMS];
	list<tcp_stream2_s*> tcp_streams_list;
};


class PreProcessPacket {
public:
	struct packet_s {
		u_int64_t packet_number;
		unsigned int saddr;
		int source; 
		unsigned int daddr; 
		int dest;
		char *data; 
		int datalen; 
		int dataoffset;
		pcap_t *handle; 
		pcap_pkthdr header; 
		const u_char *packet; 
		bool packetDelete;
		int istcp; 
		struct iphdr2 *header_ip; 
		int forceSip;
		pcap_block_store *block_store; 
		int block_store_index; 
		int dlt; 
		int sensor_id;
		bool is_ssl;
	};
	struct packet_parse_s {
		packet_parse_s() {
			init();
		}
		void init() {
			sip_method = -1;
			lastSIPresponseNum = -1;
			call_cancel_lsr487 = false;
			call = NULL;
			call_created = NULL;
			detectUserAgent = false;
			_getCallID_reassembly = false;
			_getSipMethod = false;
			_getLastSipResponse = false;
			_findCall = false;
			_createCall = false;
		}
		packet_s packet;
		ParsePacket parse;
		u_int32_t sipDataLen;
		bool isSip;
		string callid;
		int sip_method;
		int lastSIPresponseNum;
		string lastSIPresponse;
		bool call_cancel_lsr487;
		Call *call;
		Call *call_created;
		bool detectUserAgent;
		bool _getCallID_reassembly;
		bool _getSipMethod;
		bool _getLastSipResponse;
		bool _findCall;
		bool _createCall;
		unsigned int hash[2];
		volatile int used;
	};
public:
	PreProcessPacket();
	~PreProcessPacket();
	void push(bool is_ssl, u_int64_t packet_number,
		  unsigned int saddr, int source, unsigned int daddr, int dest, 
		  char *data, int datalen, int dataoffset,
		  pcap_t *handle, pcap_pkthdr *header, const u_char *packet, bool packetDelete,
		  int istcp, struct iphdr2 *header_ip, int forceSip,
		  pcap_block_store *block_store, int block_store_index, int dlt, int sensor_id,
		  bool disableLock = false);
	void preparePstatData();
	double getCpuUsagePerc(bool preparePstatData);
	void terminate();
private:
	bool sipProcess(packet_parse_s *parse_packet);
	bool sipProcess_getCallID(packet_parse_s *parse_packet);
	bool sipProcess_reassembly(packet_parse_s *parse_packet);
	void sipProcess_getSipMethod(packet_parse_s *parse_packet);
	void sipProcess_getLastSipResponse(packet_parse_s *parse_packet);
	void sipProcess_findCall(packet_parse_s *parse_packet);
	void sipProcess_createCall(packet_parse_s *parse_packet);
	void *outThreadFunction();
	void lock_push() {
		while(ATOMIC_TEST_AND_SET(&this->_sync_push, 1)) {
			usleep(10);
		}
	}
	void unlock_push() {
		ATOMIC_CLEAR(&this->_sync_push);
	}
private:
	packet_parse_s **qring;
	unsigned int qringmax;
	volatile unsigned int readit;
	volatile unsigned int writeit;
	pthread_t out_thread_handle;
	pstat_data threadPstatData[2];
	int outThreadId;
	volatile int _sync_push;
	bool term_preProcess;
friend inline void *_PreProcessPacket_outThreadFunction(void *arg);
};


class ProcessRtpPacket {
public:
	struct packet_s {
		unsigned int saddr;
		int source; 
		unsigned int daddr; 
		int dest;
		char *data; 
		int datalen; 
		int dataoffset;
		pcap_t *handle;
		pcap_pkthdr header; 
		const u_char *packet; 
		int istcp;
		struct iphdr2 *header_ip; 
		pcap_block_store *block_store; 
		int block_store_index; 
		int dlt; 
		int sensor_id;
		unsigned int hash_s;
		unsigned int hash_d;
		volatile int used;
	};
	struct rtp_call_info {
		Call *call;
		bool iscaller;
		bool is_rtcp;
		bool is_fax;
		bool use_sync;
	};
public:
	ProcessRtpPacket(int indexThread);
	~ProcessRtpPacket();
	void push(unsigned int saddr, int source, unsigned int daddr, int dest, 
		  char *data, int datalen, int dataoffset,
		  pcap_t *handle, pcap_pkthdr *header, const u_char *packet, int istcp, struct iphdr2 *header_ip,
		  pcap_block_store *block_store, int block_store_index, int dlt, int sensor_id,
		  unsigned int hash_s, unsigned int hash_d);
	void preparePstatData();
	double getCpuUsagePerc(bool preparePstatData);
	void terminate();
private:
	void *outThreadFunction();
	void rtp(packet_s *_packet);
public:
	#if RTP_PROF
	volatile unsigned long long __prof__ProcessRtpPacket_outThreadFunction_begin;
	volatile unsigned long long __prof__ProcessRtpPacket_outThreadFunction;
	volatile unsigned long long __prof__ProcessRtpPacket_outThreadFunction__usleep;
	volatile unsigned long long __prof__ProcessRtpPacket_rtp;
	volatile unsigned long long __prof__ProcessRtpPacket_rtp__hashfind;
	volatile unsigned long long __prof__ProcessRtpPacket_rtp__fill_call_array;
	volatile unsigned long long __prof__process_packet__rtp;
	volatile unsigned long long __prof__add_to_rtp_thread_queue;
	#endif
	int indexThread;
	int outThreadId;
private:
	packet_s *qring;
	unsigned int qringmax;
	volatile unsigned int readit;
	volatile unsigned int writeit;
	pthread_t out_thread_handle;
	pstat_data threadPstatData[2];
	bool term_processRtp;
friend inline void *_ProcessRtpPacket_outThreadFunction(void *arg);
};
 

#endif
