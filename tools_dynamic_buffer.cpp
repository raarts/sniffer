#include <syslog.h>

#include "voipmonitor.h"
#include "tools.h"
#include "calltable.h"

#include "tools_dynamic_buffer.h"

#define MIN(x,y) ((x) < (y) ? (x) : (y))


extern int opt_pcap_dump_tar_sip_use_pos;
extern int opt_pcap_dump_tar_rtp_use_pos;
extern int opt_pcap_dump_tar_graph_use_pos;


/* not tested - obsolete ?
void DynamicBuffer::cout(bool itemSeparator) {
	DynamicBufferItem *iter = this->first;
	while(iter) {
		std::string str = std::string((char*)iter->buffer, iter->length);
		std::cout << str;
		if(itemSeparator) {
			std::cout << '|';
		}
		iter = iter->next;
	}
}

u_char *DynamicBuffer::getConcatBuffer() {
	u_int32_t size = this->getSize();
	if(!size) {
		return(NULL);
	}
	u_char *concatBuffer = new FILE_LINE u_char[size + 1];
	u_int32_t length = 0;
	DynamicBufferItem *iter = this->first;
	while(iter) {
		memcpy(concatBuffer + length, iter->buffer, iter->length);
		length += iter->length;
		iter = iter->next;
	}
	concatBuffer[length] = 0;
	return(concatBuffer);
}

void DynamicBufferTar::write(const char *fileName, int time) {
	u_int32_t tarBufferSize = this->getSize();
	if(tarBufferSize) {
		u_char *concatTarBuffer = this->getConcatBuffer();
		if(concatTarBuffer) {
			
			//tarQueue->add(fileName, time, (char*)concatTarBuffer, tarBufferSize);
			delete [] concatTarBuffer;
		}
	}
}
*/


CompressStream::CompressStream(eTypeCompress typeCompress, u_int32_t compressBufferLength, u_int32_t maxDataLength) {
	this->typeCompress = typeCompress;
	this->compressBufferLength = compressBufferLength;
	this->compressBufferBoundLength = 0;
	this->compressBuffer = NULL;
	this->decompressBufferLength = compressBufferLength;
	this->decompressBuffer = NULL;
	this->maxDataLength = maxDataLength;
	this->zipStream = NULL;
	this->zipStreamDecompress = NULL;
	#ifdef HAVE_LIBLZMA
	this->lzmaStream = NULL;
	this->lzmaStreamDecompress = NULL;
	#endif //HAVE_LIBLZMA
	#ifdef HAVE_LIBLZ4
	this->lz4Stream = NULL;
	this->lz4StreamDecode = NULL;
	#endif //HAVE_LIBLZ4
	this->zipLevel = Z_DEFAULT_COMPRESSION;
	this->lzmaLevel = 6;
	this->processed_len = 0;
	this->sendParameter_client = 0;
	this->sendParameter_sshchannel = NULL;
}

CompressStream::~CompressStream() {
	this->termCompress();
	this->termDecompress();
}

void CompressStream::setZipLevel(int zipLevel) {
	this->zipLevel = zipLevel;
}

void CompressStream::setLzmaLevel(int lzmaLevel) {
	this->lzmaLevel = lzmaLevel;
}

void CompressStream::setSendParameters(int client, void *sshchannel) {
	this->sendParameter_client = client;
	this->sendParameter_sshchannel = sshchannel;
}

void CompressStream::initCompress() {
	switch(this->typeCompress) {
	case compress_na:
		break;
	case lzma:
#ifdef HAVE_LIBLZMA
		if(!this->lzmaStream) {
			this->lzmaStream = new FILE_LINE lzma_stream;
			memset_heapsafe(this->lzmaStream, 0, sizeof(lzma_stream));
			int ret = lzma_easy_encoder(this->lzmaStream, this->lzmaLevel, LZMA_CHECK_CRC64);
			if(ret == LZMA_OK) {
				createCompressBuffer();
			} else {
				char error[1024];
				snprintf(error, sizeof(error), "lzma_easy_encoder error: %d", (int) ret);
				this->setError(error);
				break;
			}
		}
		break;
#endif
	case zip:
	case gzip:
		if(!this->zipStream) {
			this->zipStream =  new FILE_LINE z_stream;
			this->zipStream->zalloc = Z_NULL;
			this->zipStream->zfree = Z_NULL;
			this->zipStream->opaque = Z_NULL;
			if((this->typeCompress == zip ?
			     deflateInit(this->zipStream, this->zipLevel) :
			     deflateInit2(this->zipStream, this->zipLevel, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY)) == Z_OK) {
				createCompressBuffer();
			} else {
				deflateEnd(this->zipStream);
				this->setError("zip initialize failed");
				break;
			}
		}
		break;
	case lz4:
		if(!this->compressBuffer) {
			createCompressBuffer();
		}
		break;
	case lz4_stream:
		#ifdef HAVE_LIBLZ4
		if(!this->lz4Stream) {
			this->lz4Stream = LZ4_createStream();
			createCompressBuffer();
		}
		#endif //HAVE_LIBLZ4
		break;
	case snappy:
		if(!this->compressBuffer) {
			createCompressBuffer();
		}
		break;
	}
}

void CompressStream::initDecompress(u_int32_t dataLen) {
	switch(this->typeCompress) {
	case compress_na:
		break;
	case lzma:
#ifdef HAVE_LIBLZMA 
		if(!this->lzmaStreamDecompress) {
			this->lzmaStreamDecompress = new FILE_LINE lzma_stream;
			memset_heapsafe(this->lzmaStreamDecompress, 0, sizeof(lzma_stream));
			int ret = lzma_stream_decoder(this->lzmaStreamDecompress, UINT64_MAX, LZMA_CONCATENATED);
			if(ret == LZMA_OK) {
				createDecompressBuffer(this->decompressBufferLength);
			} else {
				char error[1024];
				snprintf(error, sizeof(error), "lzma_stream_decoder error: %d", (int) ret);
				this->setError(error);
			}
		}
		break;
#endif
	case zip:
	case gzip:
		if(!this->zipStreamDecompress) {
			this->zipStreamDecompress =  new FILE_LINE z_stream;
			this->zipStreamDecompress->zalloc = Z_NULL;
			this->zipStreamDecompress->zfree = Z_NULL;
			this->zipStreamDecompress->opaque = Z_NULL;
			this->zipStreamDecompress->avail_in = 0;
			this->zipStreamDecompress->next_in = Z_NULL;
			if((this->typeCompress == zip ?
			     inflateInit(this->zipStreamDecompress) :
			     inflateInit2(this->zipStreamDecompress, MAX_WBITS + 16)) == Z_OK) {
				createDecompressBuffer(this->decompressBufferLength);
			} else {
				inflateEnd(this->zipStreamDecompress);
				this->setError("unzip initialize failed");
			}
		}
		break;
	case lz4:
		createDecompressBuffer(dataLen);
		break;
	case lz4_stream:
		#ifdef HAVE_LIBLZ4
		if(!this->lz4StreamDecode) {
			this->lz4StreamDecode = LZ4_createStreamDecode();
		}
		createDecompressBuffer(dataLen);
		#endif //HAVE_LIBLZ4
		break;
	case snappy:
		createDecompressBuffer(dataLen);
		break;
	}
}

void CompressStream::termCompress() {
	if(this->zipStream) {
		deflateEnd(this->zipStream);
		delete this->zipStream;
		this->zipStream = NULL;
	}
	#ifdef HAVE_LIBLZMA
	if(this->lzmaStream) {
		lzma_end(this->lzmaStream);
		delete this->lzmaStream;
		this->lzmaStream = NULL;
	}
	#endif //ifdef HAVE_LIBLZMA
	#ifdef HAVE_LIBLZ4
	if(this->lz4Stream) {
		LZ4_freeStream(this->lz4Stream);
		this->lz4Stream = NULL;
	}
	#endif //ifdef HAVE_LIBLZ4
	if(this->compressBuffer) {
		delete [] this->compressBuffer;
		this->compressBuffer = NULL;
	}
}

void CompressStream::termDecompress() {
	if(this->zipStreamDecompress) {
		inflateEnd(this->zipStreamDecompress);
		delete this->zipStreamDecompress;
		this->zipStreamDecompress = NULL;
	}
	#ifdef HAVE_LIBLZMA
	if(this->lzmaStreamDecompress) {
		lzma_end(this->lzmaStreamDecompress);
		delete this->lzmaStreamDecompress;
		this->lzmaStreamDecompress = NULL;
	}
	#endif //ifdef HAVE_LIBLZMA
	#ifdef HAVE_LIBLZ4
	if(this->lz4StreamDecode) {
		LZ4_freeStreamDecode(this->lz4StreamDecode);
		this->lz4StreamDecode = NULL;
	}
	#endif //HAVE_LIBLZ4
	if(this->decompressBuffer) {
		delete [] this->decompressBuffer;
		this->decompressBuffer = NULL;
	}
}

bool CompressStream::compress(char *data, u_int32_t len, bool flush, CompressStream_baseEv *baseEv) {
	if(this->isError()) {
		return(false);
	}
	if(!(len || (flush && this->processed_len))) {
		return(true);
	}
	switch(this->typeCompress) {
	case compress_na:
		if(!baseEv->compress_ev(data, len, len)) {
			this->setError("compress_ev failed");
			return(false);
		}
		break;
	case lzma:
#ifdef HAVE_LIBLZMA
		{
		if(!this->lzmaStream) {
			this->initCompress();
		}
		this->lzmaStream->avail_in = len;
		this->lzmaStream->next_in = (unsigned char*)data;
		do {
			this->lzmaStream->avail_out = this->compressBufferLength;
			this->lzmaStream->next_out = (unsigned char*)this->compressBuffer;
			int rslt = lzma_code(this->lzmaStream, flush ? LZMA_FINISH : LZMA_RUN);
			if(rslt == LZMA_OK || rslt == LZMA_STREAM_END) {
				int have = this->compressBufferLength - this->zipStream->avail_out;
				if(!baseEv->compress_ev(this->compressBuffer, have, 0)) {
					this->setError("lzma compress_ev failed");
					return(false);
				}
			} else {
				this->setError("lzma compress failed");
				return(false);
			}
		} while(this->lzmaStream->avail_out == 0);
		this->processed_len += len;
		}
		break;
#endif
	case zip: 
	case gzip: {
		if(!this->zipStream) {
			this->initCompress();
		}
		this->zipStream->avail_in = len;
		this->zipStream->next_in = (unsigned char*)data;
		do {
			this->zipStream->avail_out = this->compressBufferLength;
			this->zipStream->next_out = (unsigned char*)this->compressBuffer;
			int deflateRslt = deflate(this->zipStream, flush ? Z_FINISH : Z_NO_FLUSH);
			if(deflateRslt == Z_OK || deflateRslt == Z_STREAM_END) {
				int have = this->compressBufferLength - this->zipStream->avail_out;
				if(!baseEv->compress_ev(this->compressBuffer, have, 0)) {
					this->setError("zip compress_ev failed");
					return(false);
				}
			} else {
				this->setError(string("zip compress failed") + 
					       (this->zipStream->msg ?
						 string(" ") + this->zipStream->msg :
						 ""));
				return(false);
			}
		} while(this->zipStream->avail_out == 0);
		this->processed_len += len;
		}
		break;
	case lz4: {
		#ifdef HAVE_LIBLZ4
		if(!this->compressBuffer) {
			this->initCompress();
		}
		size_t compressLength = LZ4_compress(data, this->compressBuffer, len);
		if(compressLength > 0) {
			if(baseEv->compress_ev(this->compressBuffer, compressLength, len)) {
				this->processed_len += len;
			} else {
				this->setError("lz4 compress_ev failed");
				return(false);
			}
		} else {
			this->setError("lz4 compress failed");
			return(false);
		}
		#endif //HAVE_LIBLZ4
		}
		break;
	case lz4_stream: {
		#ifdef HAVE_LIBLZ4
		if(!this->lz4Stream) {
			this->initCompress();
		}
		u_int32_t pos = 0;
		while(pos < len) {
			u_int32_t inputLen = min(this->compressBufferLength, len - pos);
			u_int32_t have = LZ4_compress_continue(this->lz4Stream, data + pos, this->compressBuffer, inputLen);
			if(have > 0) {
				if(!baseEv->compress_ev(this->compressBuffer, have, inputLen)) {
					this->setError("lz4 compress_ev failed");
					return(false);
				}
			} else {
				break;
			}
			pos += this->compressBufferLength;
		}
		this->processed_len += len;
		#endif //HAVE_LIBLZ4
		}
		break;
	case snappy: {
#ifdef HAVE_LIBSNAPPY
		if(!this->compressBuffer) {
			this->initCompress();
		}
		size_t compressLength = this->compressBufferBoundLength;
		snappy_status snappyRslt = snappy_compress(data, len, this->compressBuffer, &compressLength);
		switch(snappyRslt) {
		case SNAPPY_OK:
			if(baseEv->compress_ev(this->compressBuffer, compressLength, len)) {
				this->processed_len += len;
			} else {
				this->setError("snappy compress_ev failed");
				return(false);
			}
			break;
		case SNAPPY_INVALID_INPUT:
			this->setError("snappy compress failed - invalid input");
			return(false);
		case SNAPPY_BUFFER_TOO_SMALL:
			this->setError("snappy compress failed - buffer is too small");
			return(false);
		default:
			this->setError("snappy compress failed -  unknown error");
			return(false);
		}
#endif
		}
		break;
	}
	return(true);
}

bool CompressStream::decompress(char *data, u_int32_t len, u_int32_t decompress_len, bool flush, CompressStream_baseEv *baseEv, u_int32_t *use_len) {
	if(sverb.chunk_buffer > 2) {
		cout << "decompress data " << len << " " << decompress_len << endl;
		for(u_int32_t i = 0; i < min(len, (u_int32_t)max(sverb.chunk_buffer, 200)); i++) {
			cout << (int)(unsigned char)data[i] << ",";
		}
		cout << endl;
	}
	if(this->isError()) {
		return(false);
	}
	if(!len) {
		return(true);
	}
	switch(this->typeCompress) {
	case compress_na:
		if(!baseEv->decompress_ev(data, len)) {
			this->setError("decompress_ev failed");
			return(false);
		}
		break;
	case lzma:
#ifdef HAVE_LIBLZMA
		if(!this->lzmaStreamDecompress) {
			this->initDecompress(0);
		}
		this->lzmaStreamDecompress->avail_in = len;
		this->lzmaStreamDecompress->next_in = (unsigned char*)data;
		do {
			this->lzmaStreamDecompress->avail_out = this->decompressBufferLength;
			this->lzmaStreamDecompress->next_out = (unsigned char*)this->decompressBuffer;
			int rslt = lzma_code(this->lzmaStreamDecompress, flush ? LZMA_FINISH : LZMA_RUN);
			if(rslt == LZMA_OK || rslt == LZMA_STREAM_END) {
				int have = this->decompressBufferLength - this->lzmaStreamDecompress->avail_out;
				if(!baseEv->decompress_ev(this->decompressBuffer, have)) {
					this->setError("lzma decompress_ev failed");
					return(false);
				}
			} else {
				this->setError("lzma decompress failed");
				return(false);
			}
		} while(this->lzmaStreamDecompress->avail_out == 0);
		break;
#endif
	case zip:
	case gzip:
		if(!this->zipStreamDecompress) {
			this->initDecompress(0);
		}
		this->zipStreamDecompress->avail_in = len;
		this->zipStreamDecompress->next_in = (unsigned char*)data;
		do {
			this->zipStreamDecompress->avail_out = this->decompressBufferLength;
			this->zipStreamDecompress->next_out = (unsigned char*)this->decompressBuffer;
			int inflateRslt = inflate(this->zipStreamDecompress, Z_NO_FLUSH);
			if(inflateRslt == Z_OK || inflateRslt == Z_STREAM_END) {
				int have = this->decompressBufferLength - this->zipStreamDecompress->avail_out;
				if(!baseEv->decompress_ev(this->decompressBuffer, have)) {
					this->setError("zip decompress_ev failed");
					return(false);
				}
			} else {
				this->setError(string("zip decompress failed") + 
					       (this->zipStreamDecompress->msg ?
						 string(" ") + this->zipStreamDecompress->msg :
						 ""));
				return(false);
			}
		} while(this->zipStreamDecompress->avail_out == 0);
		if(use_len) {
			*use_len = len - this->zipStreamDecompress->avail_in;
		}
		break;
	case lz4:
		#ifdef HAVE_LIBLZ4
		if(!this->decompressBuffer || !this->maxDataLength) {
			this->initDecompress(decompress_len);
		}
		if(LZ4_decompress_fast(data, this->decompressBuffer, decompress_len)) {
			if(!baseEv->decompress_ev(this->decompressBuffer, decompress_len)) {
				this->setError("lz4 decompress_ev failed");
				return(false);
			}
		} else {
			this->setError("lz4 decompress failed");
			return(false);
		}
		#endif //HAVE_LIBLZ4
		break;
	case lz4_stream:
		#ifdef HAVE_LIBLZ4
		if(!this->lz4StreamDecode) {
			this->initDecompress(decompress_len);
		}
		if(!this->decompressBuffer || !this->maxDataLength) {
			this->createDecompressBuffer(decompress_len);
		}
		if(LZ4_decompress_safe_continue(this->lz4StreamDecode, data, this->decompressBuffer, len, this->decompressBufferLength) > 0) {
			if(!baseEv->decompress_ev(this->decompressBuffer, decompress_len)) {
				this->setError("lz4 decompress_ev failed");
				return(false);
			}
		} else {
			this->setError("lz4 decompress failed");
			return(false);
		}
		#endif //HAVE_LIBLZ4
		break;
	case snappy: {
#ifdef HAVE_LIBSNAPPY
		if(!this->decompressBuffer || !this->maxDataLength) {
			this->initDecompress(decompress_len);
		}
		size_t decompressLength = this->decompressBufferLength;
		snappy_status snappyRslt = snappy_uncompress(data, len, this->decompressBuffer, &decompressLength);
		switch(snappyRslt) {
		case SNAPPY_OK:
			if(!baseEv->decompress_ev(this->decompressBuffer, decompress_len)) {
				this->setError("snappy decompress_ev failed");
				return(false);
			}
			break;
		case SNAPPY_INVALID_INPUT:
			this->setError("snappy decompress failed - invalid input");
			return(false);
		case SNAPPY_BUFFER_TOO_SMALL:
			this->setError("snappy decompress failed - buffer is too small");
			return(false);
		default:
			this->setError("snappy decompress failed - unknown error");
			return(false);
		}
#endif
		}
		break;
	}
	return(true);
}

void CompressStream::createCompressBuffer() {
	if(this->compressBuffer) {
		return;
	}
	switch(this->typeCompress) {
	case compress_na:
		break;
	case zip:
	case gzip:
	case lzma:
		this->compressBuffer = new FILE_LINE char[this->compressBufferLength];
		break;
	case lz4:
	case lz4_stream:
		#ifdef HAVE_LIBLZ4
		if(this->maxDataLength) {
			this->compressBufferLength = this->maxDataLength;
		}
		this->compressBufferBoundLength = LZ4_compressBound(this->compressBufferLength);
		this->compressBuffer = new FILE_LINE char[this->compressBufferBoundLength];
		#endif //HAVE_LIBLZ4
		break;
	case snappy:
#ifdef HAVE_LIBSNAPPY
		if(this->maxDataLength) {
			this->compressBufferLength = this->maxDataLength;
		}
		this->compressBufferBoundLength = snappy_max_compressed_length(this->compressBufferLength);
		this->compressBuffer = new FILE_LINE char[this->compressBufferBoundLength];
#endif
		break;
	}
}

void CompressStream::createDecompressBuffer(u_int32_t bufferLen) {
	if(this->decompressBuffer) {
		if(this->decompressBufferLength >= bufferLen) {
			return;
		} else {
			delete [] this->decompressBuffer;
			this->decompressBuffer = NULL;
		}
	}
	switch(this->typeCompress) {
	case compress_na:
		break;
	case zip:
	case gzip:
	case lzma:
		this->decompressBuffer = new FILE_LINE char[this->decompressBufferLength];
		break;	
	case lz4:
	case lz4_stream:
	case snappy:
		this->decompressBufferLength = max(this->maxDataLength, bufferLen);
		this->decompressBuffer = new FILE_LINE char[this->decompressBufferLength];
		break;
	}
}

extern int _sendvm(int socket, void *channel, const char *buf, size_t len, int mode);
bool CompressStream::compress_ev(char *data, u_int32_t len, u_int32_t decompress_len) {
	if(this->sendParameter_client || this->sendParameter_sshchannel) {
		if(_sendvm(this->sendParameter_client, this->sendParameter_sshchannel, data, len, 0) == -1) {
			this->setError("send error");
			return(false);
		}
	}
	return(true);
}

CompressStream::eTypeCompress CompressStream::convTypeCompress(const char *typeCompress) {
	char _compress_method[10];
	strncpy(_compress_method, typeCompress, sizeof(_compress_method));
	strlwr(_compress_method, sizeof(_compress_method));
	if(!strcmp(_compress_method, "zip")) {
		return(CompressStream::zip);
	} else if(!strcmp(_compress_method, "snappy")) {
		return(CompressStream::snappy);
	} else if(!strcmp(_compress_method, "lz4")) {
		return(CompressStream::lz4);
	} else if(!strcmp(_compress_method, "lz4_stream")) {
		return(CompressStream::lz4_stream);
	}
	return(CompressStream::compress_na);
}

ChunkBuffer::ChunkBuffer(int time, u_int32_t chunk_fix_len, Call *call, int typeContent) {
	this->time = time;
	this->call = call;
	this->typeContent = typeContent;
	this->chunkBuffer_countItems = 0;
	this->len = 0;
	this->chunk_fix_len = chunk_fix_len;
	this->compress_orig_data_len = 0;
	this->lastChunk = NULL;
	this->compressStream = NULL;
	this->chunkIterateProceedLen = 0;
	this->closed = false;
	this->decompressError = false;
	this->name = NULL;
	this->_sync_chunkBuffer = 0;
	this->_sync_compress = 0;
	this->last_add_time = 0;
	this->last_add_time_tar = 0;
	this->last_tar_time = 0;
	this->chunk_buffer_size = 0;
	if(call &&
	   (typeContent == FileZipHandler::pcap_sip ? opt_pcap_dump_tar_sip_use_pos :
	    typeContent == FileZipHandler::pcap_rtp ? opt_pcap_dump_tar_rtp_use_pos :
	    typeContent == FileZipHandler::graph_rtp ? opt_pcap_dump_tar_graph_use_pos : 0)) {
		call->incChunkBuffers();
	}
}

ChunkBuffer::~ChunkBuffer() {
	if(sverb.tar > 2) {
		syslog(LOG_NOTICE, "chunkbufer destroy: %s %lx %i %i", 
		       this->getName().c_str(), (long)this,
		       this->time, this->time % TAR_MODULO_SECONDS);
	}
	for(list<sChunk>::iterator it = chunkBuffer.begin(); it != chunkBuffer.end(); it++) {
		it->deleteChunk(this);
	}
	if(this->compressStream) {
		delete this->compressStream;
	}
	if(this->name) {
		delete this->name;
	}
	if(call &&
	   (typeContent == FileZipHandler::pcap_sip ? opt_pcap_dump_tar_sip_use_pos :
	    typeContent == FileZipHandler::pcap_rtp ? opt_pcap_dump_tar_rtp_use_pos :
	    typeContent == FileZipHandler::graph_rtp ? opt_pcap_dump_tar_graph_use_pos : 0)) {
		call->decChunkBuffers();
	}
}

void ChunkBuffer::setTypeCompress(CompressStream::eTypeCompress typeCompress, u_int32_t compressBufferLength, u_int32_t maxDataLength) {
	switch(typeCompress) {
	case CompressStream::zip:
	case CompressStream::lz4:
	case CompressStream::lz4_stream:
	case CompressStream::snappy:
		this->compressStream = new FILE_LINE CompressStream(typeCompress, compressBufferLength, maxDataLength);
		break;
	default:
		break;
	}
}

void ChunkBuffer::setZipLevel(int zipLevel) {
	if(this->compressStream) {
		this->compressStream->setZipLevel(zipLevel);
	}
}

void ChunkBuffer::setName(const char *name) {
	if(this->name) {
		delete this->name;
		this->name = NULL;
	}
	if(!name || !*name) {
		return;
	}
	this->name = new FILE_LINE char[strlen(name) + 1];
	strcpy(this->name, name);
}

#include <stdio.h>

void ChunkBuffer::add(char *data, u_int32_t datalen, bool flush, u_int32_t decompress_len, bool directAdd) {
	if(!datalen) {
		return;
	}
	if(sverb.chunk_buffer > 2) {
		if(directAdd) {
			cout << "add compress data " << datalen << endl;
			for(u_int32_t i = 0; i < min(datalen, (u_int32_t)max(sverb.chunk_buffer, 200)); i++) {
				cout << (int)(unsigned char)data[i] << ",";
			}
			cout << endl;
		} else {
			cout << "add source data " << datalen << endl;
			for(u_int32_t i = 0; i < min(datalen, (u_int32_t)max(sverb.chunk_buffer, 200)); i++) {
				cout << (int)(unsigned char)data[i] << ",";
			}
			cout << endl;
		}
	}
	eAddMethod addMethod = add_na;
	if(directAdd) {
		if(this->chunk_fix_len) {
			if(this->compressStream->typeCompress == CompressStream::zip) {
				addMethod = add_fill_fix_len;
			} else {
				addMethod = add_fill_chunks;
			}
		} else {
			addMethod = add_simple;
		}
	} else {
		if(this->compressStream) {
			addMethod = add_compress;
		} else if(this->chunk_fix_len) {
			addMethod = add_fill_fix_len;
		} else {
			addMethod = add_simple;
		}
	}
	if(addMethod == add_compress) {
		this->lock_compress();
		this->compressStream->compress(data, datalen, flush, this);
		this->compress_orig_data_len += datalen;
		this->unlock_compress();
		return;
	}
	this->lock_chunkBuffer();
	switch(addMethod) {
	case add_simple: {
		sChunk chunk;
		chunk.chunk = new FILE_LINE char[datalen];
		memcpy_heapsafe(chunk.chunk, data, datalen,
				__FILE__, __LINE__);
		chunk.len = datalen;
		chunk.decompress_len = decompress_len;
		this->chunkBuffer.push_back(chunk);
		++this->chunkBuffer_countItems;
		this->len += datalen;
		__sync_fetch_and_add(&this->chunk_buffer_size, datalen);
		__sync_fetch_and_add(&ChunkBuffer::chunk_buffers_sumsize, datalen);
		}
		break;
	case add_fill_chunks: {
		if(sverb.chunk_buffer > 1) {
			cout << "chunkpos_add " << (this->lastChunk ? this->lastChunk->len : 0) << " / " << this->chunkBuffer_countItems << endl;
		}
		u_int32_t allcopied = 0;
		for(int i = 0; i < 2; i++) {
			char *_data;
			u_int32_t _len;
			sChunkLen chunkLen;
			if(i == 0) {
				chunkLen.len = datalen;
				chunkLen.decompress_len = decompress_len;
				_data = (char*)&chunkLen;
				_len = sizeof(chunkLen);
			} else {
				_data = data;
				_len = datalen;
			}
			u_int32_t pos = 0;
			while(pos < _len) {
				if(!this->lastChunk ||
				   this->lastChunk->len == this->chunk_fix_len) {
					sChunk chunk;
					chunk.chunk = new FILE_LINE char[this->chunk_fix_len];
					chunk.len = 0;
					chunk.decompress_len = (u_int32_t)-1;
					this->chunkBuffer.push_back(chunk);
					++this->chunkBuffer_countItems;
					this->lastChunk = &(*(--this->chunkBuffer.end()));
				}
				u_int32_t copied = min(_len - pos, this->chunk_fix_len - this->lastChunk->len);
				memcpy_heapsafe(this->lastChunk->chunk + this->lastChunk->len, this->lastChunk->chunk,
						_data + pos, i ? _data : NULL,
						copied,
						__FILE__, __LINE__);
				this->lastChunk->len += copied;
				allcopied += copied;
				pos +=copied;
			}
		}
		this->len += allcopied;
		__sync_fetch_and_add(&this->chunk_buffer_size, allcopied);
		__sync_fetch_and_add(&ChunkBuffer::chunk_buffers_sumsize, allcopied);
		}
		break;
	case add_fill_fix_len: {
		u_int32_t copied = 0;
		do {
			if(!(this->len % this->chunk_fix_len)) {
				sChunk chunk;
				chunk.chunk = new FILE_LINE char[this->chunk_fix_len];
				this->chunkBuffer.push_back(chunk);
				++this->chunkBuffer_countItems;
				this->lastChunk = &(*(--this->chunkBuffer.end()));
			}
			int whattocopy = MIN(this->chunk_fix_len - this->len % this->chunk_fix_len, datalen - copied);
			memcpy_heapsafe(this->lastChunk->chunk + this->len % this->chunk_fix_len, this->lastChunk->chunk,
					data + copied, data,
					whattocopy,
					__FILE__, __LINE__);
			copied += whattocopy;
			this->len += whattocopy;
			this->lastChunk->len += whattocopy;
			__sync_fetch_and_add(&this->chunk_buffer_size, whattocopy);
			__sync_fetch_and_add(&ChunkBuffer::chunk_buffers_sumsize, whattocopy);
		} while(datalen > copied);
		}
		break;
	case add_compress:
	case add_na:
		break;
	}
	this->unlock_chunkBuffer();
	extern volatile unsigned int glob_last_packet_time;
	this->last_add_time = glob_last_packet_time;
}

void ChunkBuffer::close() {
	if(sverb.tar > 2) {
		syslog(LOG_NOTICE, "chunkbufer close: %s %lx %i %i", 
		       this->getName().c_str(), (long)this,
		       this->time, this->time % TAR_MODULO_SECONDS);
	}
	this->closed = true;
}

bool ChunkBuffer::compress_ev(char *data, u_int32_t len, u_int32_t decompress_len) {
	this->add(data, len, false, decompress_len, true);
	return(true);
}

bool ChunkBuffer::decompress_ev(char *data, u_int32_t len) {
 	decompress_chunkbufferIterateEv->chunkbuffer_iterate_ev(data, len, this->decompress_pos);
	this->decompress_pos += len;
	if(sverb.chunk_buffer > 2) {
		cout << "decompress ev " << len << " " << this->decompress_pos << " " << endl;
		for(u_int32_t i = 0; i < min(len, (u_int32_t)max(sverb.chunk_buffer, 200)); i++) {
			cout << (int)(unsigned char)data[i] << ",";
		}
		cout << endl;
	}
	return(true);
}

void ChunkBuffer::chunkIterate(ChunkBuffer_baseIterate *chunkbufferIterateEv, bool freeChunks, bool enableContinue, u_int32_t limitLength) {
	if(sverb.chunk_buffer > 1) {
		cout << "### start chunkIterate " << this->chunkIterateProceedLen << endl;
	}
	if(!enableContinue) {
		this->chunkIterateProceedLen = 0;
		if(sverb.chunk_buffer > 1) {
			cout << "### reset chunkIterateProceedLen" << endl;
		}
	}
	size_t counterIterator = 0;
	size_t sizeChunkBuffer = chunkBuffer_countItems;
	u_int32_t chunkIterateProceedLen_start = this->chunkIterateProceedLen;
	if(this->compressStream) {
		this->decompress_chunkbufferIterateEv = chunkbufferIterateEv;
		this->decompress_pos = 0;
		if(!enableContinue) {
			this->chunkIterateCompleteBufferInfo.init();
		}
		bool _break = false;
		for(list<sChunk>::iterator it = chunkBuffer.begin(); counterIterator < sizeChunkBuffer && !_break;) {
			if(counterIterator++) ++it;
			if(!it->chunk || counterIterator <= this->chunkIterateCompleteBufferInfo.chunkIndex) {
				continue;
			}
			if(sverb.chunk_buffer > 1) {
				cout << "### chunkIterate 01" << endl;
			}
			if(it->decompress_len == (u_int32_t)-1) {
				while(this->chunkIterateCompleteBufferInfo.chunkPos < it->len && !_break) {
					if(!this->chunkIterateCompleteBufferInfo.counter) {
						this->chunkIterateCompleteBufferInfo.chunkLen = *(sChunkLen*)it->chunk;
						++this->chunkIterateCompleteBufferInfo.counter;
						this->chunkIterateCompleteBufferInfo.bufferLen = this->chunkIterateCompleteBufferInfo.chunkLen.len;
						this->chunkIterateCompleteBufferInfo.bufferPos = 0;
						this->chunkIterateCompleteBufferInfo.addPos(sizeof(sChunkLen));
					}
					if(!this->chunkIterateCompleteBufferInfo.bufferPos) {
						if(this->chunkIterateCompleteBufferInfo.bufferLen <= it->len - this->chunkIterateCompleteBufferInfo.chunkPos) {
							if(this->chunkIterateCompleteBufferInfo.counter % 2) {
								if(sverb.chunk_buffer > 1) {
									cout << "chunkpos_dec " << this->chunkIterateCompleteBufferInfo.chunkPos << " / " << counterIterator << endl;
								}
								if(!this->compressStream->decompress(it->chunk + this->chunkIterateCompleteBufferInfo.chunkPos, 
												     this->chunkIterateCompleteBufferInfo.chunkLen.len, 
												     this->chunkIterateCompleteBufferInfo.chunkLen.decompress_len, 
												     this->closed && counterIterator == sizeChunkBuffer &&
												      this->chunkIterateCompleteBufferInfo.allPos + this->chunkIterateCompleteBufferInfo.chunkLen.len == this->len, 
												     this)) {
									syslog(LOG_ERR, "chunkbuffer decompress error in %s", this->getName().c_str());
									this->decompressError = true;
									return;
								}
								this->chunkIterateProceedLen += this->chunkIterateCompleteBufferInfo.chunkLen.decompress_len;
								this->chunkIterateCompleteBufferInfo.bufferLen = sizeof(sChunkLen);
								this->chunkIterateCompleteBufferInfo.addPos(this->chunkIterateCompleteBufferInfo.chunkLen.len);
								if(sverb.chunk_buffer > 1) { 
									cout << " d1 " << this->chunkIterateProceedLen - chunkIterateProceedLen_start << endl;
								}
							} else {
								this->chunkIterateCompleteBufferInfo.chunkLen = *(sChunkLen*)(it->chunk + this->chunkIterateCompleteBufferInfo.chunkPos);
								this->chunkIterateCompleteBufferInfo.bufferLen = this->chunkIterateCompleteBufferInfo.chunkLen.len;
								this->chunkIterateCompleteBufferInfo.addPos(sizeof(sChunkLen));
								if(sverb.chunk_buffer > 1) { 
									cout << "chunkLen " << this->chunkIterateCompleteBufferInfo.chunkLen.len << " / " << this->chunkIterateCompleteBufferInfo.chunkLen.decompress_len << endl;
									cout << "chunkpos_len " << this->chunkIterateCompleteBufferInfo.chunkPos << " / " << counterIterator << endl;
								}
							}
							++this->chunkIterateCompleteBufferInfo.counter;
							this->chunkIterateCompleteBufferInfo.bufferPos = 0;
						} else {
							this->chunkIterateCompleteBufferInfo.buffer = 
								this->chunkIterateCompleteBufferInfo.counter % 2 ?
								 new FILE_LINE char[this->chunkIterateCompleteBufferInfo.bufferLen] :
								 (char*)&(this->chunkIterateCompleteBufferInfo.chunkLenBuff);
							u_int32_t copied = it->len - this->chunkIterateCompleteBufferInfo.chunkPos;
							if(sverb.chunk_buffer > 1) { 
								cout << (this->chunkIterateCompleteBufferInfo.counter % 2 ? "chunkpos_decI " : "chunkpos_lenI ")
								     << this->chunkIterateCompleteBufferInfo.chunkPos << " / " << this->chunkIterateCompleteBufferInfo.bufferPos << " / " << counterIterator << endl;
							}
							memcpy_heapsafe(this->chunkIterateCompleteBufferInfo.buffer, 
									this->chunkIterateCompleteBufferInfo.buffer != (char*)&(this->chunkIterateCompleteBufferInfo.chunkLenBuff) ?
										this->chunkIterateCompleteBufferInfo.buffer : NULL,
									it->chunk + this->chunkIterateCompleteBufferInfo.chunkPos, it->chunk,
									copied,
									__FILE__, __LINE__);
							this->chunkIterateCompleteBufferInfo.bufferPos += copied;
							this->chunkIterateCompleteBufferInfo.addPos(copied);
						}
					} else {
						u_int32_t copied = min(it->len - this->chunkIterateCompleteBufferInfo.chunkPos, 
								       this->chunkIterateCompleteBufferInfo.bufferLen - this->chunkIterateCompleteBufferInfo.bufferPos);
						if(sverb.chunk_buffer > 1) { 
							cout << (this->chunkIterateCompleteBufferInfo.counter % 2 ? "chunkpos_dec2 " : "chunkpos_len2 ")
							     << this->chunkIterateCompleteBufferInfo.chunkPos << " / " << this->chunkIterateCompleteBufferInfo.bufferPos << " / " << counterIterator << endl;
						}
						memcpy_heapsafe(this->chunkIterateCompleteBufferInfo.buffer + this->chunkIterateCompleteBufferInfo.bufferPos, 
								this->chunkIterateCompleteBufferInfo.buffer != (char*)&(this->chunkIterateCompleteBufferInfo.chunkLenBuff) ?
									this->chunkIterateCompleteBufferInfo.buffer : NULL,
								it->chunk + this->chunkIterateCompleteBufferInfo.chunkPos, it->chunk,
								copied,
								__FILE__, __LINE__);
						this->chunkIterateCompleteBufferInfo.bufferPos += copied;
						this->chunkIterateCompleteBufferInfo.addPos(copied);
						if(this->chunkIterateCompleteBufferInfo.bufferPos == this->chunkIterateCompleteBufferInfo.bufferLen) {
							 if(this->chunkIterateCompleteBufferInfo.counter % 2) {
								if(!this->compressStream->decompress(this->chunkIterateCompleteBufferInfo.buffer, 
												     this->chunkIterateCompleteBufferInfo.chunkLen.len, 
												     this->chunkIterateCompleteBufferInfo.chunkLen.decompress_len, 
												     this->closed && counterIterator == sizeChunkBuffer &&
												      this->chunkIterateCompleteBufferInfo.allPos == this->len, 
												     this)) {
									syslog(LOG_ERR, "chunkbuffer decompress error in %s", this->getName().c_str());
									this->decompressError = true;
									return;
								}
								this->chunkIterateProceedLen += this->chunkIterateCompleteBufferInfo.chunkLen.decompress_len;
								this->chunkIterateCompleteBufferInfo.bufferLen = sizeof(sChunkLen);
								if(sverb.chunk_buffer > 1) { 
									cout << " d2 " << this->chunkIterateProceedLen - chunkIterateProceedLen_start << endl;
								}
								
							} else {
								this->chunkIterateCompleteBufferInfo.chunkLen = *(sChunkLen*)this->chunkIterateCompleteBufferInfo.buffer;
								this->chunkIterateCompleteBufferInfo.bufferLen = this->chunkIterateCompleteBufferInfo.chunkLen.len;
								if(sverb.chunk_buffer > 1) { 
									cout << "chunkLen " << this->chunkIterateCompleteBufferInfo.chunkLen.len << " / " << this->chunkIterateCompleteBufferInfo.chunkLen.decompress_len << endl;
								}
							}
							++this->chunkIterateCompleteBufferInfo.counter;
							this->chunkIterateCompleteBufferInfo.bufferPos = 0;
							if(this->chunkIterateCompleteBufferInfo.buffer != (char*)&(this->chunkIterateCompleteBufferInfo.chunkLenBuff)) {
								delete [] this->chunkIterateCompleteBufferInfo.buffer;
							}
						}
					}
					if(!(this->chunkIterateCompleteBufferInfo.counter % 2) &&
					   limitLength && 
					   this->chunkIterateProceedLen - chunkIterateProceedLen_start >= limitLength) {
						if(sverb.chunk_buffer > 1) { 
							cout << "break" << endl;
						}
						_break = true;
					}
				}
				if(this->chunkIterateCompleteBufferInfo.chunkPos >= it->len && it->len == this->chunk_fix_len) {
					++this->chunkIterateCompleteBufferInfo.chunkIndex;
					this->chunkIterateCompleteBufferInfo.chunkPos = 0;
					if(freeChunks) {
						it->deleteChunk(this);
					}
				}
			} else {
				if(!this->compressStream->decompress(it->chunk, it->len, it->decompress_len, 
								     this->closed && counterIterator == sizeChunkBuffer,
								     this)) {
					syslog(LOG_ERR, "chunkbuffer decompress error in %s", this->getName().c_str());
					this->decompressError = true;
					return;
				}
				this->chunkIterateProceedLen += it->decompress_len;
				if(freeChunks) {
					it->deleteChunk(this);
				}
				if(limitLength && 
				   this->chunkIterateProceedLen - chunkIterateProceedLen_start >= limitLength) {
					break;
				}
				if(!this->closed && counterIterator >= sizeChunkBuffer + 1) {
					break;
				}
			}
		}
		chunkbufferIterateEv->chunkbuffer_iterate_ev(NULL, 0 , this->decompress_pos);
		if(!enableContinue) {
			this->compressStream->termDecompress();
		}
	} else {
		u_int32_t pos = 0;
		if(this->closed) {
			for(list<sChunk>::iterator it = chunkBuffer.begin(); counterIterator < sizeChunkBuffer;) {
				if(counterIterator++) ++it;
				if(it->chunk) {
					chunkbufferIterateEv->chunkbuffer_iterate_ev(it->chunk, it->len, 0);
					this->chunkIterateProceedLen += it->len;
					if(freeChunks) {
						it->deleteChunk(this);
					}
					pos += it->len;
				}
			}
			chunkbufferIterateEv->chunkbuffer_iterate_ev(NULL, 0, pos);
		} else {
			for(list<sChunk>::iterator it = chunkBuffer.begin(); counterIterator < sizeChunkBuffer;) {
				if(counterIterator++) ++it;
				if(!it->chunk) {
					continue;
				}
				chunkbufferIterateEv->chunkbuffer_iterate_ev(it->chunk, it->len, pos);
				this->chunkIterateProceedLen += it->len;
				if(freeChunks) {
					it->deleteChunk(this);
				}
				pos += it->len;
				if(limitLength && 
				   this->chunkIterateProceedLen - chunkIterateProceedLen_start >= limitLength) {
					break;
				}
				if(!this->closed && counterIterator >= sizeChunkBuffer - 1) {
					break;
				}
			}
			chunkbufferIterateEv->chunkbuffer_iterate_ev(NULL, 0, pos);
		}
	}
	if(sverb.chunk_buffer > 1) { 
		cout << "### end chunkIterate " << this->chunkIterateProceedLen << endl;
	}
}

u_int32_t ChunkBuffer::getChunkIterateSafeLimitLength(u_int32_t limitLength) {
	u_int32_t safeLimitLength = 0;
	size_t counterIterator = 0;
	size_t sizeChunkBuffer = chunkBuffer_countItems;
	if(this->compressStream) {
		for(list<sChunk>::iterator it = chunkBuffer.begin(); counterIterator < sizeChunkBuffer;) {
			if(counterIterator++) ++it;
			if(!it->chunk) {
				continue;
			}
			if(it->decompress_len == (u_int32_t)-1) {
				return(limitLength);
			} else {
				if(safeLimitLength + it->decompress_len >= limitLength) {
					break;
				}
				if(!this->closed && counterIterator >= sizeChunkBuffer + 1) {
					break;
				}
				safeLimitLength += it->decompress_len;
			}
		}
	} else {
		if(this->closed) {
			return(limitLength - this->chunkIterateProceedLen);
		} else {
			for(list<sChunk>::iterator it = chunkBuffer.begin(); counterIterator < sizeChunkBuffer;) {
				if(counterIterator++) ++it;
				if(!it->chunk) {
					continue;
				}
				if(safeLimitLength + it->len >= limitLength) {
					break;
				}
				if(!this->closed && counterIterator >= sizeChunkBuffer - 1) {
					break;
				}
				safeLimitLength += it->len;
			}
		}
	}
	return(safeLimitLength);
}

void ChunkBuffer::addTarPosInCall(u_int64_t pos) {
	if(call) {
		call->addTarPos(pos, typeContent);
	}
}

volatile u_int64_t ChunkBuffer::chunk_buffers_sumsize = 0;
