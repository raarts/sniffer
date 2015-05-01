#ifndef HEAP_SAFE_H
#define HEAP_SAFE_H

#if ( defined( __FreeBSD__ ) || defined ( __NetBSD__ ) )
# ifndef FREEBSD
#  define FREEBSD
# endif
#endif


#ifndef FREEBSD
#include <alloca.h>
#else
#include <sys/types.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "tools_inline.h"

#ifdef ENABLE_HEAPSAFE
#define HEAPSAFE_ALLOC_RESERVE			20
#define HEAPSAFE_SAFE_ALLOC_RESERVE		4

#define HEAPSAFE_BEGIN_MEMORY_CONTROL_BLOCK	"BMB"
#define HEAPSAFE_FREED_MEMORY_CONTROL_BLOCK	"FMB"
#define HEAPSAFE_END_MEMORY_CONTROL_BLOCK	"EMB"

#define HEAPSAFE_COPY_BEGIN_MEMORY_CONTROL_BLOCK(stringInfo) { \
	stringInfo[0] = HEAPSAFE_BEGIN_MEMORY_CONTROL_BLOCK[0]; \
	stringInfo[1] = HEAPSAFE_BEGIN_MEMORY_CONTROL_BLOCK[1]; \
	stringInfo[2] = HEAPSAFE_BEGIN_MEMORY_CONTROL_BLOCK[2]; }
#define HEAPSAFE_COPY_FREED_MEMORY_CONTROL_BLOCK(stringInfo) { \
	stringInfo[0] = HEAPSAFE_FREED_MEMORY_CONTROL_BLOCK[0]; \
	stringInfo[1] = HEAPSAFE_FREED_MEMORY_CONTROL_BLOCK[1]; \
	stringInfo[2] = HEAPSAFE_FREED_MEMORY_CONTROL_BLOCK[2]; }
#define HEAPSAFE_COPY_END_MEMORY_CONTROL_BLOCK(stringInfo) { \
	stringInfo[0] = HEAPSAFE_END_MEMORY_CONTROL_BLOCK[0]; \
	stringInfo[1] = HEAPSAFE_END_MEMORY_CONTROL_BLOCK[1]; \
	stringInfo[2] = HEAPSAFE_END_MEMORY_CONTROL_BLOCK[2]; }

#define HEAPSAFE_CMP_BEGIN_MEMORY_CONTROL_BLOCK(stringInfo) \
	(stringInfo[0] == HEAPSAFE_BEGIN_MEMORY_CONTROL_BLOCK[0] && \
	 stringInfo[1] == HEAPSAFE_BEGIN_MEMORY_CONTROL_BLOCK[1] && \
	 stringInfo[2] == HEAPSAFE_BEGIN_MEMORY_CONTROL_BLOCK[2])
#define HEAPSAFE_CMP_FREED_MEMORY_CONTROL_BLOCK(stringInfo) \
	(stringInfo[0] == HEAPSAFE_FREED_MEMORY_CONTROL_BLOCK[0] && \
	 stringInfo[1] == HEAPSAFE_FREED_MEMORY_CONTROL_BLOCK[1] && \
	 stringInfo[2] == HEAPSAFE_FREED_MEMORY_CONTROL_BLOCK[2])
#define HEAPSAFE_CMP_END_MEMORY_CONTROL_BLOCK(stringInfo) \
	(stringInfo[0] == HEAPSAFE_END_MEMORY_CONTROL_BLOCK[0] && \
	 stringInfo[1] == HEAPSAFE_END_MEMORY_CONTROL_BLOCK[1] && \
	 stringInfo[2] == HEAPSAFE_END_MEMORY_CONTROL_BLOCK[2])

#define MCB_STACK  HeapSafeCheck & _HeapSafeStack
#define SIZEOF_MCB (MCB_STACK ? sizeof(sHeapSafeMemoryControlBlockEx) : sizeof(sHeapSafeMemoryControlBlock))
 

enum eHeapSafeErrors {
	_HeapSafeErrorNotEnoughMemory =   1,
	_HeapSafeErrorBeginEnd        =   2,
	_HeapSafeErrorFreed           =   4,
	_HeapSafeErrorInAllocFce      =   8,
	_HeapSafeErrorAllocReserve    =  16,
	_HeapSafeErrorFillFF          =  32,
	_HeapSafeErrorInHeap          =  64,
	_HeapSafeSafeReserve          = 128,
	_HeapSafeStack                = 256
};

struct sHeapSafeMemoryControlBlock {
	char stringInfo[3];
	u_int32_t length;
	u_int32_t memory_type;
};

struct sHeapSafeMemoryControlBlockEx : public sHeapSafeMemoryControlBlock {
	u_int32_t memory_type_other;
};


void HeapSafeAllocError(int error);
void HeapSafeMemcpyError(const char *errorString, const char *file = NULL, unsigned int line = 0);
void HeapSafeMemsetError(const char *errorString, const char *file = NULL, unsigned int line = 0);
#endif

inline void *memcpy_heapsafe(void *destination, const void *destination_begin, const void *source, const void *source_begin, size_t length,
			     const char *file = NULL, unsigned int line = 0) {
#ifdef ENABLE_HEAPSAFE
	extern unsigned int HeapSafeCheck;
	if(HeapSafeCheck & _HeapSafeErrorBeginEnd) {
		bool error = false;
		sHeapSafeMemoryControlBlock *destination_beginMemoryBlock;
		u_int32_t destinationLength = 0;
		if(destination_begin) {
			destination_beginMemoryBlock = (sHeapSafeMemoryControlBlock*)((unsigned char*)destination_begin - SIZEOF_MCB);
			if(HEAPSAFE_CMP_BEGIN_MEMORY_CONTROL_BLOCK(destination_beginMemoryBlock->stringInfo)) {
				destinationLength = destination_beginMemoryBlock->length;
			} else {
				error = true;
				HeapSafeMemcpyError("destination corrupted (bad begin memory block)", file, line);
			}
		}
		sHeapSafeMemoryControlBlock *source_beginMemoryBlock;
		u_int32_t sourceLength = 0;
		if(source_begin) {
			source_beginMemoryBlock = (sHeapSafeMemoryControlBlock*)((unsigned char*)source_begin - SIZEOF_MCB);
			if(HEAPSAFE_CMP_BEGIN_MEMORY_CONTROL_BLOCK(source_beginMemoryBlock->stringInfo)) {
				sourceLength = source_beginMemoryBlock->length;
			} else {
				error = true;
				HeapSafeMemcpyError("source corrupted (bad begin memory block)", file, line);
			}
		}
		if(!error) {
			if(destination_begin) {
				if((unsigned char*)destination < (unsigned char*)destination_begin) {
					HeapSafeMemcpyError("negative offset of destination", file, line);
				}
				if((unsigned char*)destination - (unsigned char*)destination_begin + length > destinationLength) {
					HeapSafeMemcpyError("write after destination length", file, line);
				}
			}
			if(source_begin) {
				if((unsigned char*)source < (unsigned char*)source_begin) {
					HeapSafeMemcpyError("negative offset of source", file, line);
				}
				if((unsigned char*)source - (unsigned char*)source_begin + length > sourceLength) {
					HeapSafeMemcpyError("write after source length", file, line);
				}
			}
		}
	}
#endif
	return(memcpy(destination, source, length));
}

inline void *memcpy_heapsafe(void *destination, const void *source, size_t length,
			     const char *file = NULL, unsigned int line = 0) {
	return(memcpy_heapsafe(destination, destination, source, source, length,
			       file, line));
}

inline void *memset_heapsafe(void *ptr, void *ptr_begin, int value, size_t length,
			     const char *file = NULL, unsigned int line = 0) {
#ifdef ENABLE_HEAPSAFE
	extern unsigned int HeapSafeCheck;
	if(HeapSafeCheck & _HeapSafeErrorBeginEnd) {
		bool error = false;
		sHeapSafeMemoryControlBlock *ptr_beginMemoryBlock;
		u_int32_t ptrLength = 0;
		if(ptr_begin) {
			ptr_beginMemoryBlock = (sHeapSafeMemoryControlBlock*)((unsigned char*)ptr_begin - SIZEOF_MCB);
			if(HEAPSAFE_CMP_BEGIN_MEMORY_CONTROL_BLOCK(ptr_beginMemoryBlock->stringInfo)) {
				ptrLength = ptr_beginMemoryBlock->length;
			} else {
				error = true;
				HeapSafeMemsetError("ptr corrupted (bad begin memory block)", file, line);
			}
		}
		if(!error) {
			if(ptr_begin) {
				if((unsigned char*)ptr < (unsigned char*)ptr_begin) {
					HeapSafeMemsetError("negative offset of ptr", file, line);
				}
				if((unsigned char*)ptr - (unsigned char*)ptr_begin + length > ptrLength) {
					HeapSafeMemsetError("write after ptr length", file, line);
				}
			}
		}
	}
#endif
	return(memset(ptr, value, length));
}

inline void *memset_heapsafe(void *ptr, int value, size_t length,
			     const char *file = NULL, unsigned int line = 0) {
	return(memset_heapsafe(ptr, ptr, value, length,
			       file, line));
}

#ifdef ENABLE_HEAPSAFE
std::string getMemoryStat(bool all = false);
std::string addThousandSeparators(u_int64_t num);
void printMemoryStat(bool all = false);
#endif

void * operator new(size_t sizeOfObject, const char *memory_type1, int memory_type2 = 0);
void * operator new[](size_t sizeOfObject, const char *memory_type1, int memory_type2 = 0);


#define FILE_LINE (__FILE__, __LINE__)


#endif //HEAP_SAFE_H
