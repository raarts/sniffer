#ifndef TCP_REASSEMBLY_H
#define TCP_REASSEMBLY_H

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <algorithm>

#include "sniff.h"
#include "pcap_queue_block.h"
#include "pstat.h"
#include "heap_safe.h"


extern int opt_tcpreassembly_pb_lock;
extern int opt_tcpreassembly_thread;


class TcpReassemblyDataItem {
public: 
	enum eDirection {
		DIRECTION_NA = 0,
		DIRECTION_TO_DEST,
		DIRECTION_TO_SOURCE
	};
	TcpReassemblyDataItem() {
		this->data = NULL;
		this->datalen = 0;
		this->time.tv_sec = 0;
		this->time.tv_usec = 0;
		this->ack = 0;
		this->direction = DIRECTION_NA;
	}
	TcpReassemblyDataItem(u_char *data, u_int32_t datalen, timeval time, u_int32_t ack = 0, eDirection direction = DIRECTION_NA) {
		if(data && datalen) {
			this->data = new FILE_LINE u_char[datalen + 1];
			memcpy_heapsafe(this->data, data, datalen, 
					__FILE__, __LINE__);
			this->data[datalen] = 0;
			this->datalen = datalen;
		} else {
			this->data = NULL;
			this->datalen = 0;
		}
		this->time = time;
		this->ack = ack;
		this->direction = direction;
	}
	TcpReassemblyDataItem(const TcpReassemblyDataItem &dataItem) {
		if(dataItem.data && dataItem.datalen) {
			this->data = new FILE_LINE u_char[dataItem.datalen + 1];
			memcpy_heapsafe(this->data, dataItem.data, dataItem.datalen, 
					__FILE__, __LINE__);
			this->data[dataItem.datalen] = 0;
			this->datalen = dataItem.datalen;
		} else {
			this->data = NULL;
			this->datalen = 0;
		}
		this->time = dataItem.time;
		this->ack = dataItem.ack;
		this->direction = dataItem.direction;
	}
	~TcpReassemblyDataItem() {
		if(this->data) {
			delete [] this->data;
		}
	}
	TcpReassemblyDataItem& operator = (const TcpReassemblyDataItem &dataItem) {
		if(this->data) {
			delete [] this->data;
		}
		if(dataItem.data && dataItem.datalen) {
			this->data = new FILE_LINE u_char[dataItem.datalen + 1];
			memcpy_heapsafe(this->data, dataItem.data, dataItem.datalen, 
					__FILE__, __LINE__);
			this->data[dataItem.datalen] = 0;
			this->datalen = dataItem.datalen;
		} else {
			this->data = NULL;
			this->datalen = 0;
		}
		this->time = dataItem.time;
		this->ack = dataItem.ack;
		this->direction = dataItem.direction;
		return(*this);
	}
	void setData(u_char *data, u_int32_t datalen, bool newAlloc = true) {
		if(this->data) {
			delete [] this->data;
		}
		if(data && datalen) {
			if(newAlloc) {
				this->data = new FILE_LINE u_char[datalen + 1];
				memcpy_heapsafe(this->data, data, datalen, 
						__FILE__, __LINE__);
				this->data[datalen] = 0;
			} else {
				this->data = data;
			}
			this->datalen = datalen;
		} else {
			this->data = NULL;
			this->datalen = 0;
		}
	}
	void setTime(timeval time) {
		this->time = time;
	}
	void setAck(u_int32_t ack) {
		this->ack = ack;
	}
	void setDirection(eDirection direction) {
		this->direction = direction;
	}
	void setDataTime(u_char *data, u_int32_t datalen, timeval time, bool newAlloc = true) {
		this->setData(data, datalen, newAlloc);
		this->setTime(time);
	}
	void setDataTimeAck(u_char *data, u_int32_t datalen, timeval time, u_int32_t ack, bool newAlloc = true) {
		this->setData(data, datalen, newAlloc);
		this->setTime(time);
		this->setAck(ack);
	}
	void clearData() {
		if(this->data) {
			delete [] this->data;
		}
		this->data = NULL;
		this->datalen = 0;
		this->ack = 0;
	}
	u_char *getData() {
		return(this->data);
	}
	string getDataString() {
		return(this->data ? (char*)this->data : "");
	}
	u_int32_t getDatalen() {
		return(this->datalen);
	}
	timeval getTime() {
		return(this->time);
	}
	u_int32_t getAck() {
		return(this->ack);
	}
	eDirection getDirection() {
		return(this->direction);
	}
	bool isFill() {
		return(this->data != NULL);
	}
private:
	u_char *data;
	u_int32_t datalen;
	timeval time;
	u_int32_t ack;
	eDirection direction;
};

class TcpReassemblyData {
public:
	TcpReassemblyData() {
		this->forceAppendExpectContinue = false;
	}
	void addData(u_char *data, u_int32_t datalen, timeval time, u_int32_t ack = 0, TcpReassemblyDataItem::eDirection direction = TcpReassemblyDataItem::DIRECTION_NA) {
		this->data.push_back(TcpReassemblyDataItem(data, datalen, time, ack, direction));
	}
	void addRequest(u_char *data, u_int32_t datalen, timeval time, u_int32_t ack = 0) {
		request.push_back(TcpReassemblyDataItem(data, datalen, time, ack));
	}
	void addResponse(u_char *data, u_int32_t datalen, timeval time, u_int32_t ack = 0) {
		response.push_back(TcpReassemblyDataItem(data, datalen, time, ack));
	}
	void addExpectContinue(u_char *data, u_int32_t datalen, timeval time, u_int32_t ack = 0) {
		expectContinue.push_back(TcpReassemblyDataItem(data, datalen, time, ack));
	}
	void addExpectContinueResponse(u_char *data, u_int32_t datalen, timeval time, u_int32_t ack = 0) {
		expectContinueResponse.push_back(TcpReassemblyDataItem(data, datalen, time, ack));
	}
	bool isFill();
public:
	vector<TcpReassemblyDataItem> data;
	vector<TcpReassemblyDataItem> request;
	vector<TcpReassemblyDataItem> response;
	vector<TcpReassemblyDataItem> expectContinue;
	vector<TcpReassemblyDataItem> expectContinueResponse;
	bool forceAppendExpectContinue;
};

class TcpReassemblyProcessData {
public:
	virtual void processData(u_int32_t ip_src, u_int32_t ip_dst,
				 u_int16_t port_src, u_int16_t port_dst,
				 TcpReassemblyData *data,
				 u_char *ethHeader, u_int32_t ethHeaderLength,
				 pcap_t *handle, int dlt, int sensor_id,
				 class TcpReassemblyLink *reassemblyLink,
				 bool debugSave) = 0;
	virtual void printContentSummary() {}
};

struct TcpReassemblyLink_id {
	TcpReassemblyLink_id(u_int32_t ip_src = 0, u_int32_t ip_dst = 0, 
			     u_int16_t port_src = 0, u_int16_t port_dst = 0) {
		this->ip_src = ip_src;
		this->ip_dst = ip_dst;
		this->port_src = port_src; 
		this->port_dst = port_dst;
	}
	void reverse() {
		u_int32_t tmp = this->ip_src;
		this->ip_src = this->ip_dst;
		this->ip_dst = tmp;
		tmp = this->port_src;
		this->port_src = this->port_dst;
		this->port_dst = tmp;
	}
	u_int32_t ip_src;
	u_int32_t ip_dst;
	u_int16_t port_src;
	u_int16_t port_dst;
	bool operator < (const TcpReassemblyLink_id& other) const {
		return((this->ip_src < other.ip_src) ? 1 : (this->ip_src > other.ip_src) ? 0 :
		       (this->ip_dst < other.ip_dst) ? 1 : (this->ip_dst > other.ip_dst) ? 0 :
		       (this->port_src < other.port_src) ? 1 : (this->port_src > other.port_src) ? 0 :
		       (this->port_dst < other.port_dst));
	}
};

class TcpReassemblyStream_packet {
public:
	enum eState {
		NA = 0,
		CHECK,
		FAIL
	};
	TcpReassemblyStream_packet() {
		time.tv_sec = 0;
		time.tv_usec = 0;
		memset(&header_tcp, 0, sizeof(header_tcp));
		next_seq = 0;
		data = NULL;
		datalen = 0;
		datacaplen = 0;
		block_store = NULL;
		block_store_index = 0;
		state = NA;
		//locked_packet = false;
	}
	TcpReassemblyStream_packet(const TcpReassemblyStream_packet &packet) {
		this->copyFrom(packet);
		if(!opt_tcpreassembly_pb_lock && packet.data) {
			this->data = new FILE_LINE u_char[packet.datacaplen];
			memcpy_heapsafe(this->data, packet.data, packet.datacaplen, 
					__FILE__, __LINE__);
		}
	}
	~TcpReassemblyStream_packet() {
		if(!opt_tcpreassembly_pb_lock && this->data) {
			delete [] this->data;
		}
	}
	TcpReassemblyStream_packet& operator = (const TcpReassemblyStream_packet &packet) {
		if(!opt_tcpreassembly_pb_lock && this->data) {
			delete [] this->data;
		}
		this->copyFrom(packet);
		if(!opt_tcpreassembly_pb_lock && packet.data) {
			this->data = new FILE_LINE u_char[packet.datacaplen];
			memcpy_heapsafe(this->data, packet.data, packet.datacaplen, 
					__FILE__, __LINE__);
		}
		return(*this);
	}
	void setData(timeval time, tcphdr2 header_tcp,
		     u_char *data, u_int32_t datalen, u_int32_t datacaplen,
		     pcap_block_store *block_store, int block_store_index) {
		this->time = time;
		this->header_tcp = header_tcp;
		this->next_seq = header_tcp.seq + datalen;
		if(opt_tcpreassembly_pb_lock) {
			this->data = data;
		} else {
			if(datacaplen) {
				this->data = new FILE_LINE u_char[datacaplen];
				memcpy_heapsafe(this->data, this->data,
						data, block_store ? NULL : data,
						datacaplen, 
						__FILE__, __LINE__);
			} else {
				this->data = NULL;
			}
		}
		this->datalen = datalen;
		this->datacaplen = datacaplen;
		this->block_store = block_store;
		this->block_store_index = block_store_index;
	}
private:
	void lock_packet() {
		if(opt_tcpreassembly_pb_lock && this->block_store/* && !locked_packet*/) {
			//static int countLock;
			//cout << "COUNT LOCK: " << (++countLock) << endl;
			this->block_store->lock_packet(this->block_store_index);
			//locked_packet = true;
		}
	}
	void unlock_packet() {
		if(opt_tcpreassembly_pb_lock && this->block_store/* && locked_packet*/) {
			//static int countUnlock;
			//cout << "COUNT UNLOCK: " << (++countUnlock) << endl;
			this->block_store->unlock_packet(this->block_store_index);
			//locked_packet = false;
		}
	}
	void copyFrom(const TcpReassemblyStream_packet &packet) {
		this->time = packet.time;
		this->header_tcp = packet.header_tcp;
		this->next_seq = packet.next_seq;
		this->data = packet.data;
		this->datalen = packet.datalen;
		this->datacaplen = packet.datacaplen;
		this->block_store = packet.block_store;
		this->block_store_index = packet.block_store_index;
		this->state = packet.state;
	}
	void cleanState() {
		this->state = NA;
	}
private:
	timeval time;
	tcphdr2 header_tcp;
	u_int32_t next_seq;
	u_char *data;
	u_int32_t datalen;
	u_int32_t datacaplen;
	pcap_block_store *block_store;
	int block_store_index;
	eState state;
	//bool locked_packet;
friend class TcpReassemblyStream_packet_var;
friend class TcpReassemblyStream;
friend class TcpReassemblyLink;
};

class TcpReassemblyStream_packet_var {
public:
	~TcpReassemblyStream_packet_var() {
		if(opt_tcpreassembly_pb_lock) {
			map<uint32_t, TcpReassemblyStream_packet>::iterator iter;
			for(iter = this->queuePackets.begin(); iter != this->queuePackets.end(); iter++) {
				iter->second.unlock_packet();
			}
		}
	}
	void push(TcpReassemblyStream_packet packet);
	u_int32_t getNextSeqCheck() {
		map<uint32_t, TcpReassemblyStream_packet>::iterator iter;
		for(iter = this->queuePackets.begin(); iter != this->queuePackets.end(); iter++) {
			if(iter->second.datalen &&
			   (iter->second.state == TcpReassemblyStream_packet::NA ||
			    iter->second.state == TcpReassemblyStream_packet::CHECK)) {
				return(iter->second.next_seq);
			}
		}
		return(0);
	}
	u_int32_t isFail() {
		map<uint32_t, TcpReassemblyStream_packet>::iterator iter;
		for(iter = this->queuePackets.begin(); iter != this->queuePackets.end(); iter++) {
			if(iter->second.state != TcpReassemblyStream_packet::FAIL) {
				return(false);
			}
		}
		return(true);
	}
private:
	void unlockPackets() {
		if(opt_tcpreassembly_pb_lock) {
			map<uint32_t, TcpReassemblyStream_packet>::iterator iter;
			for(iter = this->queuePackets.begin(); iter != this->queuePackets.end(); iter++) {
				iter->second.unlock_packet();
			}
		}
	}
	void cleanState() {
		map<uint32_t, TcpReassemblyStream_packet>::iterator iter;
		for(iter = this->queuePackets.begin(); iter != this->queuePackets.end(); iter++) {
			iter->second.cleanState();
		}
	}
private:
	map<uint32_t, TcpReassemblyStream_packet> queuePackets;
friend class TcpReassemblyStream;
friend class TcpReassemblyLink;
};

class TcpReassemblyStream {
public:
	enum eDirection {
		DIRECTION_NA = 0,
		DIRECTION_TO_DEST,
		DIRECTION_TO_SOURCE
	};
	enum eType {
		TYPE_DATA,
		TYPE_SYN_SENT,
		TYPE_SYN_RECV,
		TYPE_FIN,
		TYPE_RST
	};
	enum eHttpType {
		HTTP_TYPE_NA = 0,
		HTTP_TYPE_POST,
		HTTP_TYPE_GET,
		HTTP_TYPE_HEAD,
		HTTP_TYPE_HTTP
	};
	TcpReassemblyStream(class TcpReassemblyLink *link) {
		direction = DIRECTION_TO_DEST;
		type = TYPE_DATA;
		ack = 0;
		first_seq = 0;
		last_seq = 0;
		min_seq = 0;
		max_next_seq = 0;
		is_ok = false;
		completed_finally = false;
		exists_data = false;
		http_type = HTTP_TYPE_NA;
		http_header_length = 0;
		http_content_length = 0;
		http_ok = false;
		http_expect_continue = false;
		http_ok_expect_continue_post = false;
		http_ok_expect_continue_data = false;
		detect_ok_max_next_seq = 0;
		_ignore_expect_continue = false;
		_only_check_psh = false;
		_force_wait_for_next_psh = false;
		last_packet_at_from_header = 0;
		this->link = link;
		counterTryOk = 0;
	}
	void push(TcpReassemblyStream_packet packet);
	int ok(bool crazySequence = false, bool enableSimpleCmpMaxNextSeq = false, u_int32_t maxNextSeq = 0,
	       bool enableCheckCompleteContent = false, TcpReassemblyStream *prevHttpStream = NULL, bool enableDebug = false,
	       u_int32_t forceFirstSeq = 0, bool ignorePsh = false);
	bool ok2_ec(u_int32_t nextAck, bool enableDebug = false);
	u_char *complete(u_int32_t *datalen, timeval *time, bool check = false, bool unlockPackets = true,
			 size_t startIndex = 0, size_t *endIndex = NULL, bool breakIfPsh = false);
	bool saveCompleteData(bool unlockPackets = true, bool check = false, TcpReassemblyStream *prevHttpStream = NULL);
	void clearCompleteData();
	void unlockPackets();
	void printContent(int level  = 0);
	bool checkOkPost(TcpReassemblyStream *nextStream = NULL);
private:
	bool checkCompleteContent();
	bool checkContentIsHttpRequest();
	void cleanPacketsState() {
		map<uint32_t, TcpReassemblyStream_packet_var>::iterator iter;
		for(iter = this->queuePacketVars.begin(); iter != this->queuePacketVars.end(); iter++) {
			iter->second.cleanState();
		}
		this->ok_packets.clear();
	}
	u_int32_t getLastSeqFromNextStream();
	eDirection direction;
	eType type;
	u_int32_t ack;
	u_int32_t first_seq;
	u_int32_t last_seq;
	u_int32_t min_seq;
	u_int32_t max_next_seq;
	map<uint32_t, TcpReassemblyStream_packet_var> queuePacketVars;
	deque<d_u_int32_t> ok_packets;
	bool is_ok;
	bool completed_finally;
	bool exists_data;
	TcpReassemblyDataItem complete_data;
	eHttpType http_type;
	u_int32_t http_header_length;
	u_int32_t http_content_length;
	bool http_ok;
	bool http_ok_data_complete;
	bool http_expect_continue;
	bool http_ok_expect_continue_post;
	bool http_ok_expect_continue_data;
	u_int32_t detect_ok_max_next_seq;
	bool _ignore_expect_continue;
	bool _only_check_psh;
	bool _force_wait_for_next_psh;
	u_int64_t last_packet_at_from_header;
	TcpReassemblyLink *link;
	int counterTryOk;
friend class TcpReassemblyLink;
};

class TcpReassemblyLink {
public:
	enum eState {
		STATE_NA = 0,
		STATE_SYN_SENT,
		STATE_SYN_RECV,
		STATE_SYN_OK,
		STATE_SYN_FORCE_OK,
		STATE_RESET,
		STATE_CLOSE,
		STATE_CLOSED,
		STATE_CRAZY
	};
	class streamIterator {
		public:
			streamIterator(TcpReassemblyLink *link) {
				this->link = link;
				this->init();
			}
			bool init();
			bool next();
			bool nextAckInDirection();
			bool nextAckInReverseDirection();
			bool nextSeqInDirection();
			bool nextAckByMaxSeqInReverseDirection();
			void print();
			u_int32_t getMaxNextSeq();
		private:
			bool findSynSent();
			bool findFirstDataToDest();
		public:
			TcpReassemblyStream *stream;
			eState state;
		private:
			TcpReassemblyLink *link;
	};
	TcpReassemblyLink(class TcpReassembly *reassembly,
			  u_int32_t ip_src = 0, u_int32_t ip_dst = 0, 
			  u_int16_t port_src = 0, u_int16_t port_dst = 0,
			  u_char *packet = NULL, iphdr2 *header_ip = NULL,
			  pcap_t *handle = NULL, int dlt = 0, int sensor_id = 0) {
		this->reassembly = reassembly;
		this->ip_src = ip_src;
		this->ip_dst = ip_dst;
		this->port_src = port_src; 
		this->port_dst = port_dst;
		this->state = STATE_NA;
		this->forceOk = false;
		this->first_seq_to_dest = 0;
		this->first_seq_to_source = 0;
		this->rst = false;
		this->fin_to_dest = false;
		this->fin_to_source = false;
		this->_sync_queue = 0;
		this->_erase = 0;
		//this->created_at = getTimeMS();
		//this->last_packet_at = 0;
		this->created_at_from_header = 0;
		this->last_packet_at_from_header = 0;
		this->last_packet_process_cleanup_at = 0;
		this->last_ack = 0;
		this->exists_data = false;
		this->link_is_ok = 0;
		this->completed_offset = 0;
		this->direction_confirm = 0;
		this->ethHeader = NULL;
		this->ethHeaderLength = 0;
		if(packet && header_ip) {
			this->createEthHeader(packet, header_ip);
		}
		this->handle = handle;
		this->dlt = dlt;
		this->sensor_id = sensor_id;
		for(int i = 0; i < 2; i++) {
			this->remainData[i] = NULL;
			this->remainDataLength[i] = 0;
		}
	}
	~TcpReassemblyLink();
	bool push(TcpReassemblyStream::eDirection direction,
		  timeval time, tcphdr2 header_tcp, 
		  u_char *data, u_int32_t datalen, u_int32_t datacaplen,
		  pcap_block_store *block_store, int block_store_index) {
		if(datalen) {
			this->exists_data = true;
		}
		if(this->state == STATE_CRAZY) {
			return(this->push_crazy(
				direction, time, header_tcp, 
				data, datalen, datacaplen,
				block_store, block_store_index));
		} else {
			return(this->push_normal(
				direction, time, header_tcp, 
				data, datalen, datacaplen,
				block_store, block_store_index));
		}
	}
	bool push_normal(
		  TcpReassemblyStream::eDirection direction,
		  timeval time, tcphdr2 header_tcp, 
		  u_char *data, u_int32_t datalen, u_int32_t datacaplen,
		  pcap_block_store *block_store, int block_store_index);
	bool push_crazy(
		  TcpReassemblyStream::eDirection direction,
		  timeval time, tcphdr2 header_tcp, 
		  u_char *data, u_int32_t datalen, u_int32_t datacaplen,
		  pcap_block_store *block_store, int block_store_index);
	int okQueue(int final = 0, bool enableDebug = false, 
		    bool checkCompleteContent = false, bool ignorePsh = false) {
		if(this->state == STATE_CRAZY) {
			return(this->okQueue_crazy(final, enableDebug));
		} else {
			return(this->okQueue_normal(final, enableDebug, checkCompleteContent, ignorePsh));
		}
	}
	int okQueue_normal(int final = 0, bool enableDebug = false, 
			   bool checkCompleteContent = false, bool ignorePsh = false);
	int okQueue_crazy(int final = 0, bool enableDebug = false);
	void complete(bool final = false, bool eraseCompletedStreams = false) {
		if(this->state == STATE_CRAZY) {
			this->complete_crazy(final, eraseCompletedStreams);
		} else {
			this->complete_normal(final);
		}
	}
	void complete_normal(bool final = false);
	void complete_crazy(bool final = false, bool eraseCompletedStreams = false);
	streamIterator createIterator();
	TcpReassemblyStream *findStreamBySeq(u_int32_t seq) {
		for(size_t i = 0; i < this->queueStreams.size(); i++) {
			map<uint32_t, TcpReassemblyStream_packet_var>::iterator iter;
			iter = this->queueStreams[i]->queuePacketVars.find(seq);
			if(iter != this->queueStreams[i]->queuePacketVars.end()) {
				return(this->queueStreams[i]);
			}
		}
		return(NULL);
	}
	TcpReassemblyStream *findStreamByMinSeq(u_int32_t seq, bool dataOnly = false, 
						u_int32_t not_ack = 0, TcpReassemblyStream::eDirection direction = TcpReassemblyStream::DIRECTION_NA) {
		map<uint32_t, TcpReassemblyStream*>::iterator iter;
		for(iter = this->queue_by_ack.begin(); iter != this->queue_by_ack.end(); iter++) {
			if(iter->second->min_seq == seq &&
			   (!not_ack || iter->second->ack != not_ack) &&
			   (direction == TcpReassemblyStream::DIRECTION_NA || iter->second->direction == direction)) {
				return(iter->second);
			}
		}
		if(!dataOnly && this->queue_nul_by_ack.size()) {
			iter = this->queue_nul_by_ack.end();
			do {
				--iter;
				if(iter->second->min_seq == seq &&
				   (!not_ack || iter->second->ack != not_ack) &&
				   (direction == TcpReassemblyStream::DIRECTION_NA || iter->second->direction == direction)) {
					return(iter->second);
				}
			} while(iter != this->queue_nul_by_ack.begin());
		}
		return(NULL);
	}
	TcpReassemblyStream *findFlagStreamByAck(u_int32_t ack) {
		map<uint32_t, TcpReassemblyStream*>::iterator iter;
		iter = this->queue_flags_by_ack.find(ack);
		if(iter != this->queue_flags_by_ack.end()) {
			return(iter->second);
		}
		return(NULL);
	}
	TcpReassemblyStream *findFinalFlagStreamByAck(u_int32_t ack, TcpReassemblyStream::eDirection direction) {
		map<uint32_t, TcpReassemblyStream*>::iterator iter;
		iter = this->queue_flags_by_ack.find(ack);
		if(iter != this->queue_flags_by_ack.end() &&
		   iter->second->direction == direction) {
			return(iter->second);
		}
		return(NULL);
	}
	TcpReassemblyStream *findFinalFlagStreamBySeq(u_int32_t seq, TcpReassemblyStream::eDirection direction) {
		map<uint32_t, TcpReassemblyStream*>::iterator iter;
		for(iter = this->queue_flags_by_ack.begin(); iter != this->queue_flags_by_ack.end(); iter++) {
			if(iter->second->direction == direction &&
			   iter->second->min_seq >= seq) {
				return(iter->second);
			}
		}
		return(NULL);
	}
	bool existsFinallyUncompletedDataStream() {
		map<uint32_t, TcpReassemblyStream*>::iterator iter;
		for(iter = this->queue_by_ack.begin(); iter != this->queue_by_ack.end(); iter++) {
			if(iter->second->exists_data &&
			   !iter->second->completed_finally) {
				return(true);
			}
		}
		return(false);
	}
	void cleanup(u_int64_t act_time);
	void printContent(int level  = 0);
	void setRemainData(u_char *data, u_int32_t datalen, TcpReassemblyDataItem::eDirection direction);
	void clearRemainData(TcpReassemblyDataItem::eDirection direction);
	u_char *getRemainData(TcpReassemblyDataItem::eDirection direction);
	u_int32_t getRemainDataLength(TcpReassemblyDataItem::eDirection direction);
private:
	void lock_queue() {
		while(ATOMIC_TEST_AND_SET(&this->_sync_queue, 1)) usleep(100);
	}
	void unlock_queue() {
		ATOMIC_CLEAR(&this->_sync_queue);
	}
	void pushpacket(TcpReassemblyStream::eDirection direction,
		        TcpReassemblyStream_packet packet);
	void setLastSeq(TcpReassemblyStream::eDirection direction, 
			u_int32_t lastSeq);
	void switchDirection();
	void createEthHeader(u_char *packet, iphdr2 *header_ip);
private:
	TcpReassembly *reassembly;
	u_int32_t ip_src;
	u_int32_t ip_dst;
	u_int16_t port_src;
	u_int16_t port_dst;
	eState state;
	bool forceOk;
	u_int32_t first_seq_to_dest;
	u_int32_t first_seq_to_source;
	bool rst;
	bool fin_to_dest;
	bool fin_to_source;
	map<uint32_t, TcpReassemblyStream*> queue_by_ack;
	map<uint32_t, TcpReassemblyStream*> queue_flags_by_ack;
	map<uint32_t, TcpReassemblyStream*> queue_nul_by_ack;
	deque<TcpReassemblyStream*> queueStreams;
	volatile int _sync_queue;
	volatile int _erase;
	//u_int64_t created_at;
	//u_int64_t last_packet_at;
	u_int64_t created_at_from_header;
	u_int64_t last_packet_at_from_header;
	u_int64_t last_packet_process_cleanup_at;
	u_int32_t last_ack;
	bool exists_data;
	int link_is_ok;
	size_t completed_offset;
	int direction_confirm;
	vector<TcpReassemblyStream*> ok_streams;
	u_char *ethHeader;
	u_int32_t ethHeaderLength;
	pcap_t *handle;
	int dlt; 
	int sensor_id;
	u_char *remainData[2];
	u_int32_t remainDataLength[2];
friend class TcpReassembly;
friend class TcpReassemblyStream;
};

class TcpReassembly {
public:
	enum eType {
		http,
		webrtc,
		ssl
	};
	struct sPacket {
		pcap_pkthdr header; 
		iphdr2 *header_ip; 
		u_char *packet;
		pcap_block_store *block_store; 
		int block_store_index;
		pcap_t *handle; 
		int dlt; 
		int sensor_id;
	};
public:
	TcpReassembly(eType type);
	~TcpReassembly();
	void push(pcap_pkthdr *header, iphdr2 *header_ip, u_char *packet,
		  pcap_block_store *block_store = NULL, int block_store_index = 0,
		  pcap_t *handle = NULL, int dlt = 0, int sensor_id = 0);
	void cleanup(bool all = false);
	void cleanup_simple(bool all = false);
	void setEnableHttpForceInit(bool enableHttpForceInit = true) {
		this->enableHttpForceInit = enableHttpForceInit;
	}
	void setEnableCrazySequence(bool enableCrazySequence = true) {
		this->enableCrazySequence = enableCrazySequence;
	}
	void setEnableWildLink(bool enableWildLink = true) {
		this->enableWildLink = enableWildLink;
	}
	void setEnableIgnorePairReqResp(bool enableIgnorePairReqResp = true) {
		this->enableIgnorePairReqResp = enableIgnorePairReqResp;
	}
	void setEnableDestroyStreamsInComplete(bool enableDestroyStreamsInComplete = true) {
		this->enableDestroyStreamsInComplete = enableDestroyStreamsInComplete;
	}
	void setEnableAllCompleteAfterZerodataAck(bool enableAllCompleteAfterZerodataAck = true) {
		this->enableAllCompleteAfterZerodataAck = enableAllCompleteAfterZerodataAck;
	}
	void setEnableCleanupThread(bool enableCleanupThread = true) {
		this->enableCleanupThread = enableCleanupThread;
		this->createCleanupThread();
	}
	void setEnablePacketThread(bool enablePacketThread = true) {
		this->enablePacketThread = enablePacketThread;
		this->createPacketThread();
	}
	void setDataCallback(TcpReassemblyProcessData *dataCallback) {
		this->dataCallback = dataCallback;
	}
	/*
	bool enableStop();
	*/
	void printContent();
	void printContentSummary();
	void setDoPrintContent() {
		this->doPrintContent = true;
	}
	void setIgnoreTerminating(bool ignoreTerminating);
	void addLog(const char *logString);
	bool isActiveLog() {
		return(this->log != NULL);
	}
	void prepareCleanupPstatData();
	double getCleanupCpuUsagePerc(bool preparePstatData = false);
	void preparePacketPstatData();
	double getPacketCpuUsagePerc(bool preparePstatData = false);
	string getCpuUsagePerc();
	bool check_ip(u_int32_t ip, u_int16_t port = 0) {
		if(type == http || type == webrtc) {
			extern vector<u_int32_t> httpip;
			extern vector<d_u_int32_t> httpnet;
			extern vector<u_int32_t> webrtcip;
			extern vector<d_u_int32_t> webrtcnet;
			vector<u_int32_t> *_ip = (type == http ? &httpip : &webrtcip);
			vector<d_u_int32_t> *_net  = (type == http ? &httpnet : &webrtcnet);
			if(!_ip->size() && !_net->size()) {
				return(true);
			}
			if(_ip->size()) {
				vector<u_int32_t>::iterator iterIp;
				iterIp = std::lower_bound(_ip->begin(), _ip->end(), ip);
				if(iterIp != _ip->end() && ((*iterIp) & ip) == (*iterIp)) {
					return(true);
				}
			}
			if(_net->size()) {
				for(size_t i = 0; i < _net->size(); i++) {
					if((*_net)[i][0] == ip >> (32 - (*_net)[i][1]) << (32 - (*_net)[i][1])) {
						return(true);
					}
				}
			}
		} else if(type == ssl) {
			extern map<d_u_int32_t, string> ssl_ipport;
			map<d_u_int32_t, string>::iterator iter = ssl_ipport.find(d_u_int32_t(ip, port));
			return(iter != ssl_ipport.end());
		}
		return(false);
	}
	bool check_port(u_int16_t port, u_int32_t ip = 0) {
		if(type == http || type == webrtc) {
			extern char *httpportmatrix;
			extern char *webrtcportmatrix;
			return(type == http ? httpportmatrix[port] : webrtcportmatrix[port]);
		} else if(type == ssl) {
			extern map<d_u_int32_t, string> ssl_ipport;
			map<d_u_int32_t, string>::iterator iter = ssl_ipport.find(d_u_int32_t(ip, port));
			return(iter != ssl_ipport.end());
		}
		return(false);
	}
	eType getType() {
		return(type);
	}
	string getTypeString(bool upper = false) {
		string str = type == http ? "http" :
			     type == webrtc ? "webrtc" : 
			     type == ssl ? "ssl" : "";
		if(upper) {
			std::transform(str.begin(), str.end(), str.begin(), ::toupper);
		}
		return(str);
	}
	void setLinkTimeout(u_int32_t linkTimeout) {
		this->linkTimeout = linkTimeout;
	}
private:
	void _push(pcap_pkthdr *header, iphdr2 *header_ip, u_char *packet,
		   pcap_block_store *block_store, int block_store_index,
		   pcap_t *handle, int dlt, int sensor_id);
	void createCleanupThread();
	void createPacketThread();
	void *cleanupThreadFunction(void *);
	void *packetThreadFunction(void *);
	void lock_links() {
		while(ATOMIC_TEST_AND_SET(&this->_sync_links, 1)) usleep(100);
	}
	void unlock_links() {
		ATOMIC_CLEAR(&this->_sync_links);
	}
private:
	eType type;
	map<TcpReassemblyLink_id, TcpReassemblyLink*> links;
	volatile int _sync_links;
	bool enableHttpForceInit;
	bool enableCrazySequence;
	bool enableWildLink;
	bool enableIgnorePairReqResp;
	bool enableDestroyStreamsInComplete;
	bool enableAllCompleteAfterZerodataAck;
	bool enableCleanupThread;
	bool enablePacketThread;
	TcpReassemblyProcessData *dataCallback;
	u_int64_t act_time_from_header;
	u_int64_t last_time;
	u_int64_t last_cleanup_call_time_from_header;
	u_int64_t last_erase_links_time;
	bool doPrintContent;
	pthread_t cleanupThreadHandle;
	pthread_t packetThreadHandle;
	int cleanupThreadId;
	int packetThreadId;
	bool terminated;
	bool ignoreTerminating;
	FILE *log;
	pstat_data cleanupThreadPstatData[2];
	pstat_data packetThreadPstatData[2];
	u_long lastTimeLogErrExceededMaximumAttempts;
	u_long _cleanupCounter;
	u_int32_t linkTimeout;
	SafeAsyncQueue<sPacket> packetQueue;
friend class TcpReassemblyLink;
friend void *_TcpReassembly_cleanupThreadFunction(void* arg);
friend void *_TcpReassembly_packetThreadFunction(void* arg);
};

#endif
