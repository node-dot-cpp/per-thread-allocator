/* -------------------------------------------------------------------------------
 * Copyright (c) 2018, OLogN Technologies AG
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * -------------------------------------------------------------------------------
 * 
 * Per-thread bucket allocator
 * 
 * v.1.00    May-09-2018    Initial release
 * 
 * -------------------------------------------------------------------------------*/

 
#ifndef SERIALIZABLE_ALLOCATOR_KEEPING_PAGES_H
#define SERIALIZABLE_ALLOCATOR_KEEPING_PAGES_H

#include <cstdlib>
#include <cstring>
#include <limits>

#include "bucket_allocator_common.h"
#include "page_allocator.h"


class SerializableAllocatorBase;
extern thread_local SerializableAllocatorBase g_AllocManager;


constexpr size_t ALIGNMENT = 2 * sizeof(uint64_t);
constexpr uint8_t ALIGNMENT_EXP = sizeToExp(ALIGNMENT);
static_assert( ( 1 << ALIGNMENT_EXP ) == ALIGNMENT, "" );
constexpr size_t ALIGNMENT_MASK = expToMask(ALIGNMENT_EXP);
static_assert( 1 + ALIGNMENT_MASK == ALIGNMENT, "" );

constexpr size_t BLOCK_SIZE = 4 * 1024;
constexpr uint8_t BLOCK_SIZE_EXP = sizeToExp(BLOCK_SIZE);
constexpr size_t BLOCK_SIZE_MASK = expToMask(BLOCK_SIZE_EXP);
static_assert( ( 1 << BLOCK_SIZE_EXP ) == BLOCK_SIZE, "" );
static_assert( 1 + BLOCK_SIZE_MASK == BLOCK_SIZE, "" );


//#define USE_ITEM_HEADER
#define USE_SOUNDING_PAGE_ADDRESS

#ifdef USE_SOUNDING_PAGE_ADDRESS
template<class BasePageAllocator, size_t bucket_cnt_exp>
class SoundingAddressPageAllocator : public BasePageAllocator
{
	static constexpr size_t reservation_size_exp = 23; // that is, one-time reservation is 2^reservation_size_exp
	static constexpr size_t reservation_size = 1 << reservation_size_exp;
	static constexpr size_t bucket_cnt = 1 << bucket_cnt_exp;
	static_assert( reservation_size_exp >= bucket_cnt_exp + BLOCK_SIZE_EXP, "revise implementation" );
	static constexpr size_t pages_per_bucket_exp = reservation_size_exp - bucket_cnt_exp - BLOCK_SIZE_EXP;
	static constexpr size_t pages_per_bucket = 1 << pages_per_bucket_exp;

	struct MemoryBlockHeader
	{
		MemoryBlockListItem block;
		MemoryBlockHeader* next;
	};
//	MemoryBlockHeader* memoryBlockHead = nullptr;
	
	struct PageBlockDescriptor
	{
		PageBlockDescriptor* next = nullptr;
		void* blockAddress = nullptr;
//		size_t usageMask = 0;
		uint16_t nextToUse[ bucket_cnt ];
//		static_assert( sizeof( size_t ) * 8 >= bucket_cnt, "revise implementation" );
		static_assert( UINT16_MAX > pages_per_bucket , "revise implementation" );
	};
	PageBlockDescriptor pageBlockListStart;
	PageBlockDescriptor* pageBlockListCurrent;
	PageBlockDescriptor* indexHead[bucket_cnt];

	void* getNextBlock()
	{
//		void* pages = this->getFreeBlockNoCache( BLOCK_SIZE * bucket_cnt ); // TODO: replace by reservation, if possible
//		void* pages = this->AllocateAddressSpace( BLOCK_SIZE * bucket_cnt );
		void* pages = this->AllocateAddressSpace( reservation_size );
//		MemoryBlockHeader* h = reinterpret_cast<MemoryBlockHeader*>( pages );
//		h->next = memoryBlockHead;
//		memoryBlockHead = h;
		return pages;
	}

	void* createNextBlockAndGetPage( size_t reasonIdx )
	{
		assert( reasonIdx < bucket_cnt );
		PageBlockDescriptor* pb = new PageBlockDescriptor; // TODO: consider using our own allocator
		pb->blockAddress = getNextBlock();
//printf( "createNextBlockAndGetPage(): descriptor allocated at 0x%zx; block = 0x%zx\n", (size_t)(pb), (size_t)(pb->blockAddress) );
//		pb->usageMask = ((size_t)1) << reasonIdx;
		memset( pb->nextToUse, 0, sizeof( uint16_t) * bucket_cnt );
		pb->next = nullptr;
		pb->nextToUse[ reasonIdx ] = 1;
		pageBlockListCurrent->next = pb;
		pageBlockListCurrent = pb;
//		void* ret = idxToPageAddr( pb->blockAddress, reasonIdx );
		void* ret = idxToPageAddr( pb->blockAddress, reasonIdx, 0 );
//	printf("createNextBlockAndGetPage(): before commit, %zd, 0x%zx -> 0x%zx\n", reasonIdx, (size_t)(pb->blockAddress), (size_t)(ret) );
//		void* ret2 = this->CommitMemory( ret, BLOCK_SIZE );
		this->CommitMemory( ret, BLOCK_SIZE );
*reinterpret_cast<uint8_t*>(ret) += 1; // test write
//	printf("createNextBlockAndGetPage(): after commit 0x%zx\n", (size_t)(ret2) );
		return ret;
	}

	void resetLists()
	{
		pageBlockListStart.blockAddress = nullptr;
//		pageBlockListStart.usageMask = 0;
		for ( size_t i=0; i<bucket_cnt; ++i )
			pageBlockListStart.nextToUse[i] = pages_per_bucket; // thus triggering switching to a next block whatever bucket is selected
		pageBlockListStart.next = nullptr;

		pageBlockListCurrent = &pageBlockListStart;
		for ( size_t i=0; i<bucket_cnt; ++i )
			indexHead[i] = pageBlockListCurrent;
	}

public:
	static constexpr size_t reserverdSizeAtPageStart() { return sizeof( MemoryBlockHeader ); }

public:
//	SoundingAddressPageAllocator( BasePageAllocator& pageAllocator_ ) : pageAllocator( pageAllocator_ ) {}
	SoundingAddressPageAllocator() {}

//	static FORCE_INLINE size_t addressToIdx( void* ptr ) { return ( (uintptr_t)(ptr) >> BLOCK_SIZE_EXP ) & ( bucket_cnt - 1 ); }
//	static FORCE_INLINE size_t addressToIdx( void* ptr ) { return ( (uintptr_t)(ptr) >> (reservation_size_exp - bucket_cnt_exp) ) & ( bucket_cnt - 1 ); }
	static FORCE_INLINE size_t addressToIdx( void* ptr ) 
	{ 
		// TODO: make sure computations are optimal
		uintptr_t padr = (uintptr_t)(ptr) >> BLOCK_SIZE_EXP;
		constexpr uintptr_t meaningfulBitsMask = ( 1 << (bucket_cnt_exp + pages_per_bucket_exp) ) - 1;
		uintptr_t meaningfulBits = padr & meaningfulBitsMask;
		return meaningfulBits >> pages_per_bucket_exp;
	}
/*	static FORCE_INLINE void* idxToPageAddr( void* blockptr, size_t idx ) 
	{ 
		assert( idx < bucket_cnt );
		uintptr_t startAsIdx = addressToIdx( blockptr );
		void* ret = (void*)( ( ( ( (uintptr_t)(blockptr) >> ( BLOCK_SIZE_EXP + bucket_cnt_exp ) ) << bucket_cnt_exp ) + idx + (( idx < startAsIdx ) << bucket_cnt_exp) ) << BLOCK_SIZE_EXP );
		assert( addressToIdx( ret ) == idx );
		return ret;
	}*/
	static FORCE_INLINE void* idxToPageAddr( void* blockptr, size_t idx, size_t pagesUsed ) 
	{ 
		assert( idx < bucket_cnt );
		uintptr_t startAsIdx = addressToIdx( blockptr );
		assert( ( (uintptr_t)(blockptr) & BLOCK_SIZE_MASK ) == 0 );
		uintptr_t startingPage =  (uintptr_t)(blockptr) >> BLOCK_SIZE_EXP;
		uintptr_t basePage =  ( startingPage >> (reservation_size_exp - BLOCK_SIZE_EXP) ) << (reservation_size_exp - BLOCK_SIZE_EXP);
		uintptr_t baseOffset = startingPage - basePage;
//		uintptr_t stepOffset = baseOffset & (pages_per_bucket - 1);
//		bool below = pagesUsed < stepOffset;
		bool below = (idx << pages_per_bucket_exp) + pagesUsed < baseOffset;
		uintptr_t ret = basePage + (idx << pages_per_bucket_exp) + pagesUsed + (below << (pages_per_bucket_exp + bucket_cnt_exp));
		ret <<= BLOCK_SIZE_EXP;
		assert( addressToIdx( (void*)( ret ) ) == idx );
		assert( (uint8_t*)blockptr <= (uint8_t*)ret && (uint8_t*)ret < (uint8_t*)blockptr + reservation_size );
		return (void*)( ret );

/*		void* ret = (void*)( ( ( ( ( (uintptr_t)(blockptr) >> reservation_size_exp ) << bucket_cnt_exp ) + idx + (( idx < startAsIdx ) << bucket_cnt_exp) ) << ( reservation_size_exp - bucket_cnt_exp ) ) + ( pagesUsed << BLOCK_SIZE_EXP ) );
		assert( addressToIdx( ret ) == idx );
		assert( (uint8_t*)blockptr <= (uint8_t*)ret && (uint8_t*)ret < (uint8_t*)blockptr + reservation_size );
		return ret;*/
	}
	static FORCE_INLINE size_t getOffsetInPage( void * ptr ) { return (uintptr_t)(ptr) & BLOCK_SIZE_MASK; }
	static FORCE_INLINE void* ptrToPageStart( void * ptr ) { return (void*)( ( (uintptr_t)(ptr) >> BLOCK_SIZE_EXP ) << BLOCK_SIZE_EXP ); }

	void initialize( uint8_t blockSizeExp )
	{
		BasePageAllocator::initialize( blockSizeExp );
		resetLists();
	}

	void* getPage( size_t idx )
	{
		assert( idx < bucket_cnt );
		assert( indexHead[idx] );
		if ( indexHead[idx]->nextToUse[idx] < pages_per_bucket )
		{
			void* ret = idxToPageAddr( indexHead[idx]->blockAddress, idx, indexHead[idx]->nextToUse[idx] );
			++(indexHead[idx]->nextToUse[idx]);
			this->CommitMemory( ret, BLOCK_SIZE );
*reinterpret_cast<uint8_t*>(ret) += 1; // test write
			return ret;
		}
		else if ( indexHead[idx]->next == nullptr ) // next block is to be created
		{
			assert( indexHead[idx] == pageBlockListCurrent );
			void* ret = createNextBlockAndGetPage( idx );
			indexHead[idx] = pageBlockListCurrent;
			assert( indexHead[idx]->next == nullptr );
*reinterpret_cast<uint8_t*>(ret) += 1; // test write
			return ret;
		}
		else // next block is just to be used first time
		{
			indexHead[idx] = indexHead[idx]->next;
			assert( indexHead[idx]->blockAddress );
//			assert( ( indexHead[idx]->usageMask & ( ((size_t)1) << idx ) ) == 0 );
			assert( indexHead[idx]->nextToUse[idx] == 0 );
//			indexHead[idx]->usageMask |= ((size_t)1) << idx;
//			void* ret = idxToPageAddr( indexHead[idx]->blockAddress, idx );
			void* ret = idxToPageAddr( indexHead[idx]->blockAddress, idx, indexHead[idx]->nextToUse[idx] );
			++(indexHead[idx]->nextToUse[idx]);
//	printf("getPage(): before commit, %zd, 0x%zx -> 0x%zx\n", idx, (size_t)(indexHead[idx]->blockAddress), (size_t)(ret) );
//			void* ret2 = this->CommitMemory( ret, BLOCK_SIZE );
//	printf("getPage(): after commit 0x%zx\n", (size_t)(ret2) );
			this->CommitMemory( ret, BLOCK_SIZE );
*reinterpret_cast<uint8_t*>(ret) += 1; // test write
			return ret;
		}
	}

	void freePage( MemoryBlockListItem* chk )
	{
		assert( false );
		// TODO: decommit
	}

	void deinitialize()
	{
//printf( "Entering deinitialize()...\n" );
		PageBlockDescriptor* next = pageBlockListStart.next;
		while( next )
		{
//printf( "in block 0x%zx about to delete 0x%zx of size 0x%zx\n", (size_t)( next ), (size_t)( next->blockAddress ), BLOCK_SIZE * bucket_cnt );
			assert( next->blockAddress );
			this->freeChunkNoCache( reinterpret_cast<MemoryBlockListItem*>( next->blockAddress ), reservation_size );
			PageBlockDescriptor* tmp = next->next;
			delete next;
			next = tmp;
		}
		resetLists();
		BasePageAllocator::deinitialize();
	}
};
#endif


class SerializableAllocatorBase
{
protected:
	static constexpr size_t MaxBucketSize = BLOCK_SIZE / 4;
	static constexpr size_t BucketCountExp = 4;
	static constexpr size_t BucketCount = 1 << BucketCountExp;
	void* buckets[BucketCount];
	static constexpr size_t large_block_idx = 0xFF;

	struct ChunkHeader
	{
		MemoryBlockListItem block;
		ChunkHeader* next;
		size_t idx;
	};
	
#ifdef USE_SOUNDING_PAGE_ADDRESS
	SoundingAddressPageAllocator<PageAllocatorWithCaching, BucketCountExp> pageAllocator;
#else
	PageAllocatorWithCaching pageAllocator;

	ChunkHeader* nextPage = nullptr;

	FORCE_INLINE
	ChunkHeader* getChunkFromUsrPtr(void* ptr)
	{
		return reinterpret_cast<ChunkHeader*>(alignDownExp(reinterpret_cast<uintptr_t>(ptr), BLOCK_SIZE_EXP));
	}
#endif

#ifdef USE_ITEM_HEADER
	struct ItemHeader
	{
		uint8_t idx;
		uint8_t reserved[7];
	};
	static_assert( sizeof( ItemHeader ) == 8, "" );
#endif // USE_ITEM_HEADER

protected:
	static constexpr
	FORCE_INLINE size_t indexToBucketSize(uint8_t ix)
	{
		return 1ULL << (ix + 3);
	}

#if defined(_MSC_VER)
#if defined(_M_IX86)
	static
		FORCE_INLINE uint8_t sizeToIndex(uint32_t sz)
	{
		unsigned long ix;
		uint8_t r = _BitScanReverse(&ix, sz - 1);
		return (sz <= 8) ? 0 : static_cast<uint8_t>(ix - 2);
	}
#elif defined(_M_X64)
	static
	FORCE_INLINE uint8_t sizeToIndex(uint64_t sz)
	{
		unsigned long ix;
		uint8_t r = _BitScanReverse64(&ix, sz - 1);
		return (sz <= 8) ? 0 : static_cast<uint8_t>(ix - 2);
	}
#else
#error Unknown 32/64 bits architecture
#endif

#elif defined(__GNUC__)
#if defined(__i386__)
	static
		FORCE_INLINE uint8_t sizeToIndex(uint32_t sz)
	{
		uint32_t ix = __builtin_clzl(sz - 1);
		return (sz <= 8) ? 0 : static_cast<uint8_t>(29ul - ix);
	}
#elif defined(__x86_64__)
	static
		FORCE_INLINE uint8_t sizeToIndex(uint64_t sz)
	{
		uint64_t ix = __builtin_clzll(sz - 1);
		return (sz <= 8) ? 0 : static_cast<uint8_t>(61ull - ix);
	}
#else
#error Unknown 32/64 bits architecture
#endif	

#else
#error Unknown compiler
#endif
	
public:
	SerializableAllocatorBase() { initialize(); }
	SerializableAllocatorBase(const SerializableAllocatorBase&) = delete;
	SerializableAllocatorBase(SerializableAllocatorBase&&) = default;
	SerializableAllocatorBase& operator=(const SerializableAllocatorBase&) = delete;
	SerializableAllocatorBase& operator=(SerializableAllocatorBase&&) = default;

	void enable() {}
	void disable() {}


	FORCE_INLINE void* allocateInCaseNoFreeBucket( size_t sz, uint8_t szidx )
	{
#ifdef USE_SOUNDING_PAGE_ADDRESS
		uint8_t* block = reinterpret_cast<uint8_t*>( pageAllocator.getPage( szidx ) );
		constexpr size_t memStart = alignUpExp( SoundingAddressPageAllocator<PageAllocatorWithCaching, BucketCountExp>::reserverdSizeAtPageStart(), ALIGNMENT_EXP );
#else
		uint8_t* block = reinterpret_cast<uint8_t*>( pageAllocator.getFreeBlock( BLOCK_SIZE ) );
		constexpr size_t memStart = alignUpExp( sizeof( ChunkHeader ), ALIGNMENT_EXP );
		ChunkHeader* h = reinterpret_cast<ChunkHeader*>( block );
		h->idx = szidx;
		h->next = nextPage;
		nextPage = h;
#endif
		uint8_t* mem = block + memStart;
		size_t bucketSz = indexToBucketSize( szidx ); // TODO: rework
		assert( bucketSz >= sizeof( void* ) );
#ifdef USE_ITEM_HEADER
		bucketSz += sizeof(ItemHeader);
#endif // USE_ITEM_HEADER
		size_t itemCnt = (BLOCK_SIZE - memStart) / bucketSz;
		assert( itemCnt );
		for ( size_t i=bucketSz; i<(itemCnt-1)*bucketSz; i+=bucketSz )
			*reinterpret_cast<void**>(mem + i) = mem + i + bucketSz;
		*reinterpret_cast<void**>(mem + (itemCnt-1)*bucketSz) = nullptr;
		buckets[szidx] = mem + bucketSz;
#ifdef USE_ITEM_HEADER
		reinterpret_cast<ItemHeader*>( mem )->idx = szidx;
		return mem + sizeof( ItemHeader );
#else
		return mem;
#endif // USE_ITEM_HEADER
	}

	NOINLINE void* allocateInCaseTooLargeForBucket(size_t sz)
	{
#ifdef USE_ITEM_HEADER
		constexpr size_t memStart = alignUpExp( sizeof( ChunkHeader ) + sizeof( ItemHeader ), ALIGNMENT_EXP );
#elif defined USE_SOUNDING_PAGE_ADDRESS
		constexpr size_t memStart = alignUpExp( sizeof( size_t ), ALIGNMENT_EXP );
#else
		constexpr size_t memStart = alignUpExp( sizeof( ChunkHeader ), ALIGNMENT_EXP );
#endif // USE_ITEM_HEADER
		size_t fullSz = alignUpExp( sz + memStart, BLOCK_SIZE_EXP );
		MemoryBlockListItem* block = pageAllocator.getFreeBlock( fullSz );

#ifdef USE_ITEM_HEADER
#else
		size_t* h = reinterpret_cast<size_t*>( block );
		*h = sz;
#endif

//		usedNonBuckets.pushFront(chk);
#ifdef USE_ITEM_HEADER
		( reinterpret_cast<ItemHeader*>( reinterpret_cast<uint8_t*>(block) + memStart ) - 1 )->idx = large_block_idx;
#endif // USE_ITEM_HEADER
		return reinterpret_cast<uint8_t*>(block) + memStart;
	}

	FORCE_INLINE void* allocate(size_t sz)
	{
		if ( sz <= MaxBucketSize )
		{
			uint8_t szidx = sizeToIndex( sz );
			assert( szidx < BucketCount );
			if ( buckets[szidx] )
			{
				void* ret = buckets[szidx];
				buckets[szidx] = *reinterpret_cast<void**>(buckets[szidx]);
#ifdef USE_ITEM_HEADER
				reinterpret_cast<ItemHeader*>( ret )->idx = szidx;
				return reinterpret_cast<uint8_t*>(ret) + sizeof( ItemHeader );
#else
				return ret;
#endif // USE_ITEM_HEADER
			}
			else
				return allocateInCaseNoFreeBucket( sz, szidx );
		}
		else
			return allocateInCaseTooLargeForBucket( sz );

		return nullptr;
	}

	FORCE_INLINE void deallocate(void* ptr)
	{
		if(ptr)
		{
#ifdef USE_ITEM_HEADER
			ItemHeader* ih = reinterpret_cast<ItemHeader*>(ptr) - 1;
			if ( ih->idx != large_block_idx )
			{
				uint8_t idx = ih->idx;
				*reinterpret_cast<void**>( ih ) = buckets[idx];
				buckets[idx] = ih;
			}
			else
			{
				ChunkHeader* ch = getChunkFromUsrPtr( ptr );
//				assert( reinterpret_cast<uint8_t*>(ch) == reinterpret_cast<uint8_t*>(ih) );
				pageAllocator.freeChunk( reinterpret_cast<MemoryBlockListItem*>(ch) );
			}
#elif defined USE_SOUNDING_PAGE_ADDRESS
			size_t offsetInPage = SoundingAddressPageAllocator<PageAllocatorWithCaching, BucketCountExp>::getOffsetInPage( ptr );
			if ( offsetInPage > alignUpExp( sizeof( size_t ), ALIGNMENT_EXP ) )
			{
				uint8_t idx = SoundingAddressPageAllocator<PageAllocatorWithCaching, BucketCountExp>::addressToIdx( ptr );
				*reinterpret_cast<void**>( ptr ) = buckets[idx];
				buckets[idx] = ptr;
			}
			else
			{
				void* pageStart = SoundingAddressPageAllocator<PageAllocatorWithCaching, BucketCountExp>::ptrToPageStart( ptr );
				MemoryBlockListItem* h = reinterpret_cast<MemoryBlockListItem*>(pageStart);
				h->size = *reinterpret_cast<size_t*>(pageStart);
				h->sizeIndex = 0xFFFFFFFF; // TODO: address properly!!!
				h->prev = nullptr;
				h->next = nullptr;
				pageAllocator.freeChunk( reinterpret_cast<MemoryBlockListItem*>(h) );
			}
#else
			ChunkHeader* h = getChunkFromUsrPtr( ptr );
			if ( h->idx != large_block_idx )
			{
				*reinterpret_cast<void**>( ptr ) = buckets[h->idx];
				buckets[h->idx] = ptr;
			}
			else
				pageAllocator.freeChunk( reinterpret_cast<MemoryBlockListItem*>(h) );
#endif // USE_ITEM_HEADER
		}
	}
	
	const BlockStats& getStats() const { return pageAllocator.getStats(); }
	
	void printStats()
	{
		pageAllocator.printStats();
	}

	void initialize(size_t size)
	{
		initialize();
	}

	void initialize()
	{
		memset( buckets, 0, sizeof( void* ) * BucketCount );
		pageAllocator.initialize( BLOCK_SIZE_EXP );
	}

	void deinitialize()
	{
#ifdef USE_SOUNDING_PAGE_ADDRESS
		// ...
#else
		while ( nextPage )
		{
			ChunkHeader* next = nextPage->next;
			pageAllocator.freeChunk( reinterpret_cast<MemoryBlockListItem*>(nextPage) );
			nextPage = next;
		}
#endif // USE_SOUNDING_PAGE_ADDRESS
		pageAllocator.deinitialize();
	}

	~SerializableAllocatorBase()
	{
		deinitialize();
	}
};

#endif //SERIALIZABLE_ALLOCATOR_KEEPING_PAGES_H