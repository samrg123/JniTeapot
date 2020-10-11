#pragma once

#include <errno.h>
#include <sys/mman.h>

#include "types.h"
#include "util.h"
#include "customAssert.h"
#include "panic.h"

#define ENABLE_MEMORY_STATS 1

class Memory {
		
    private:

		// TODO: consider passing in blockSize or querying system for page size
		static constexpr uint kMinBlockSize = KB(4); // size of a normal linux page

	    struct Block {
            Block* previousBlock;
            uint32 bytes;
            uint32 position;
   
			#if ENABLE_MEMORY_STATS
                uint32 padBytes;
			#endif

            inline uint32 FreeBytes() { return bytes - position; }
        };
		
		static inline Block emptyBlock = {};
		
    public:
		
		#if ENABLE_MEMORY_STATS
			static inline uint32 memoryBytes;
			static inline uint32 memoryPadBytes;
			static inline uint32 memoryUnusedBytes;
			static inline uint32 memoryBlockReservedBytes;
			
			static inline uint32 memoryBlockCount;
			static inline uint32 memoryBlockReserveCount;
		#endif

		class Arena;
		
		class Region {
			friend Arena;
			friend Memory;
			
			private:
				Block* block;
				uint32 position;
				
				constexpr Region(Block* block, uint32 position): block(block), position(position) {}
				
			public:
				constexpr Region(): block(&emptyBlock), position(0) {}
		};
		
	private:
		static inline const Region kEmptyRegion;
		
	public:
		
        class Arena: NoCopyClass {
        	private:
	            friend Memory;
	    
	            Block* currentBlock;
	            Block* reservedBlock;
	
				#if ENABLE_MEMORY_STATS
	                uint32 arenaBytes;		 //usedBytes + unusedBytes
	                uint32 arenaPadBytes;	 //unusedBytes caused from block alignment
	                uint32 arenaUnusedBytes; //unused bytes at end of blocks + reserveBlock bytes
	                uint32 arenaBlockCount;  //number of blocks (includes reserveBlock)
				#endif
				
		        inline Block* CreateBlock(uint32 minSize, Block* previousBlock) {
			        uint32 blockBytes = Max(sizeof(Block) + minSize, kMinBlockSize);
			        RUNTIME_ASSERT(blockBytes > sizeof(Block),
						        	"Block Allocation is too small for header { arena: %p,  bytes: %d, sizeof(Block): %d } ",
						        	this, blockBytes, sizeof(Block));
			
			        // Note: this memory is page aligned and zeroed
			        Block* block = (Block*)mmap(nullptr,
			                                    blockBytes,
			                                    PROT_READ|PROT_WRITE,
			                                    MAP_ANONYMOUS|MAP_PRIVATE,
			                                    -1, // some linux variations require fd to be -1
			                                    0   // offset must be 0 with MAP_ANONYMOUS
			                                   );
			
			        if(block == MAP_FAILED) {
				        Panic("Failed to allocate memory block { arena: %p, bytes requested: %d, Linux errno: %d } ",
				        	  this, blockBytes, errno);
			        }
			
			        block->bytes = blockBytes;
			        block->position = sizeof(Block);
			        block->previousBlock = previousBlock;
			        
			        #if ENABLE_MEMORY_STATS
			            uint32 freeBytes = blockBytes - sizeof(Block);
			        
			            arenaBlockCount++;
				        arenaBytes+= blockBytes;
				        arenaUnusedBytes+= freeBytes;

				        memoryBlockCount++;
				        memoryBytes+= blockBytes;
				        memoryUnusedBytes+= freeBytes;
			        #endif
			
			        return block;
		        }
		
				inline void FreeBlock(Block* block) {
			
			        #if ENABLE_MEMORY_STATS
			            RUNTIME_ASSERT(memoryBlockCount && arenaBlockCount,
			                        	"Trying to free more blocks than allocated { arena: %p, memoryBlockCount: %d, arenaBlockCount: %d }",
			                        	this, memoryBlockCount, arenaBlockCount);
				
			            uint32 freeBytes = block->FreeBytes();
				
				        arenaBlockCount--;
				        arenaBytes-= block->bytes;
				        arenaUnusedBytes-= freeBytes;
				        arenaPadBytes-= block->padBytes;
				
				        memoryBlockCount--;
				        memoryBytes-= block->bytes;
				        memoryUnusedBytes-= freeBytes;
				        memoryPadBytes-= block->padBytes;
			        #endif
			
			        int error = munmap(block, block->bytes);
			        RUNTIME_ASSERT(!error,
						        	"Failed to free Memory block { arena: %p, block: %p, bytes: %d, Linux errno: %d }",
						        	this, block, block->bytes, errno);
		        }
	
				inline Block* CreateBlockWithT(Block* previousBlock, uint32 tBytes, uint8 alignment, void** tPtr) {
			
					uint8 alignmentOffset = AlignUpOffsetPow2((void *)sizeof(Block), alignment);
					uint32 alignedSize = tBytes+alignmentOffset;
			
					// Note: AllocateBlock is page aligned and zero
					Block* newBlock = CreateBlock(alignedSize, previousBlock);
					newBlock->position+= alignedSize;
			
					*tPtr = ByteOffset(newBlock, sizeof(Block)+alignmentOffset);
			
					#if ENABLE_MEMORY_STATS
						newBlock->padBytes+= alignmentOffset;
				
						arenaPadBytes+= alignmentOffset;
						arenaUnusedBytes-= alignedSize;
				
						memoryPadBytes+= alignmentOffset;
						memoryUnusedBytes-= alignedSize;
					#endif
			
					return newBlock;
				}
		        
		        void* ExtendBlockWithT(Block* block, uint32 tBytes, uint8 alignment, bool zeroMemory) {
					void* tPos = ByteOffset(block, block->position);
					uint32 freeBytes = block->FreeBytes();
			
					uint8 alignmentOffset = AlignUpOffsetPow2(tPos, alignment);
					uint32 alignedBytes = tBytes + alignmentOffset;
			
					if(freeBytes < alignedBytes) return nullptr;
			
					tPos = ByteOffset(tPos, alignmentOffset);
					block->position+= alignedBytes;
			
					//TODO: consider keeping dirty flag around for block and only zeroing if new region has been written to
					if(zeroMemory) FillMemory(tPos, 0, tBytes);
					
					#if ENABLE_MEMORY_STATS
						block->padBytes+= alignmentOffset;
				
						arenaPadBytes+= alignmentOffset;
						arenaUnusedBytes-= alignedBytes;
				
						memoryPadBytes+= alignmentOffset;
						memoryUnusedBytes-= alignedBytes;
                    #endif
					
					return tPos;
		        }
		        
        	public:
		
				inline Arena(uint32 prealocatedBytes = 0): reservedBlock(nullptr) {
                    #if ENABLE_MEMORY_STATS
                        arenaBytes = 0;
                        arenaPadBytes = 0;
                        arenaUnusedBytes = 0;
                        arenaBlockCount = 0;
                    #endif
		            
					currentBlock = prealocatedBytes ? CreateBlock(prealocatedBytes, &emptyBlock) : &emptyBlock;
				}
	    		
				void* PushBytes(size_t bytes, bool zeroMemory = false, uint8 alignment = 1) {
					RUNTIME_ASSERT(IsPow2Safe(alignment), "Alignment must be a power of 2. { alignment: %d }", alignment);
					RUNTIME_ASSERT(currentBlock, "Null arena block - should be initialized to emptyBlock! { arena: %p } ", this);

					void* tPosition = ExtendBlockWithT(currentBlock, bytes, alignment, zeroMemory);
					if(!tPosition) {

						//check to see if we can reuse the reserved block
						if(reservedBlock) {

							tPosition = ExtendBlockWithT(reservedBlock, bytes, alignment, zeroMemory);
							if(tPosition) {
								#if ENABLE_MEMORY_STATS
									memoryBlockReserveCount--;
									memoryBlockReservedBytes-= reservedBlock->bytes;
								#endif

								reservedBlock->previousBlock = currentBlock;
								currentBlock = reservedBlock;
								reservedBlock = nullptr;
								
							} else currentBlock = CreateBlockWithT(currentBlock, bytes, alignment, &tPosition);

						} else currentBlock = CreateBlockWithT(currentBlock, bytes, alignment, &tPosition);
					}

					return tPosition;
				}
        		
		        template<class T>
		        T* PushStruct(bool zeroMemory = false, uint8 alignment = 1) { return (T*)PushBytes(sizeof(T), zeroMemory, alignment); }

                inline void Reserve(uint32 bytes) {
                
		            //check current block
                    if(bytes < currentBlock->FreeBytes()) return;
                    
                    //check reserve block
                    if(reservedBlock) {
                        if(bytes < reservedBlock->FreeBytes()) return;
                        FreeBlock(reservedBlock);
                    }
                
                    //create new reserve block
                    reservedBlock = CreateBlock(bytes, currentBlock);
		        }
		        
		        inline bool IsEmptyRegion(Region r) const {
				    return r.block == currentBlock && r.position == currentBlock->position;
				}
		        
		        inline Region CreateRegion() const { return Region(currentBlock, currentBlock->position); }
		        
		        //TODO: make  a FreeRegion that can take in a startRegion and endRegion and use ForEachRegion to free blocks in range and merge start and stop block if needed
		        
		        //Free all blocks from current position in arena up into and including 'region'
		        inline void FreeBaseRegion(const Region& region) {
		        	
			        RUNTIME_ASSERT(currentBlock != &emptyBlock || region.block == &emptyBlock, "Trying to free past start of arena { Arena: %p }", this);
			        RUNTIME_ASSERT(region.block == &emptyBlock || region.position >= sizeof(Block),
			                    	"Region position is smaller than block header { Arena: %p, regionPosition: %d, sizeof(Block): %d }",
			                    	this, region.position, sizeof(Block));
			        
			        Block *popBlock = currentBlock;
			        while(popBlock != region.block) {
			        	
			        	currentBlock = popBlock->previousBlock;

				        // TODO: make sure this gets optimized into 2 loops
						if(!reservedBlock && currentBlock == region.block) {

							#if ENABLE_MEMORY_STATS
								arenaPadBytes-= popBlock->padBytes;
								arenaUnusedBytes+= popBlock->position - sizeof(Block);
							
								memoryBlockReserveCount++;
								memoryPadBytes-= popBlock->padBytes;
								memoryUnusedBytes+= popBlock->position - sizeof(Block);
								memoryBlockReservedBytes+= popBlock->bytes;
							
								popBlock->padBytes = 0;
							#endif

							reservedBlock = popBlock;
							reservedBlock->position = sizeof(Block);
							break;
						}
				    
			        	FreeBlock(popBlock);
			        	popBlock = currentBlock;
			        }
			        
			        RUNTIME_ASSERT(region.position <= currentBlock->bytes,
						        	"Region position is out of bounds of block { block: %p, regionPosition: %d, blockBytes: %d }",
			                        currentBlock, region.position, currentBlock->bytes);
			        
			        RUNTIME_ASSERT(currentBlock != &emptyBlock || region.position == 0,
						        	"non-zero region position for emptyBlock { arena: %p, regionPosition: %d }",
						        	this, region.position);
			        
					currentBlock->position = region.position;
		        }
			       
		        inline void Pack() {
			        if(reservedBlock) {
				
				        #if ENABLE_MEMORY_STATS
			        	    memoryBlockReserveCount--;
			        	    memoryBlockReservedBytes-= reservedBlock->bytes;
						#endif

				        FreeBlock(reservedBlock);
				        reservedBlock = nullptr;
			        }
		        }
		
				~Arena() {
                    FreeBaseRegion(kEmptyRegion);
					Pack();
				}
		
				//Note: translates whole arena to contiguous buffer using 'translator(void* regionElement, void* bufferElement)'
				template <typename TranslatorFuncT>
				inline void TranslateToBuffer(uint32 numElements, void* buffer,
											  uint32 regionStride, uint32 bufferStride,
											  const TranslatorFuncT& translator) {
			
					Memory::TranslateRegionsToBuffer(kEmptyRegion, CreateRegion(),
													 numElements, buffer,
													 regionStride, bufferStride,
													 translator);
				}
				
				//Note: Copies whole arena of type 'ArenaT' to contiguous buffer
				template <typename ArenaT>
				inline void CopyToBuffer(uint32 numElements, void* buffer,
										 uint32 regionStride=sizeof(ArenaT), uint32 bufferStride=sizeof(ArenaT)) const {
					
					Memory::CopyRegionsToBuffer<ArenaT>(kEmptyRegion, CreateRegion(),
														numElements, buffer,
														regionStride, bufferStride);
				}
        };

        static inline Arena temporaryArena = Arena(0);
		
		//Note: calls 'func(void* chunk, uint32 chunkBytes)' for each chunk in [startRegion, stopRegion] inclusive
		//Warn: ForEachRegion iterates blocks in reverse order from when they were pushed to the region
		template<typename FuncT>
		static inline void ForEachRegion(const Region& startRegion, const Region& stopRegion, const FuncT& func) {
			
			Block *startBlock = startRegion.block,
				  *stopBlock  = stopRegion.block;
			
			RUNTIME_ASSERT(startBlock == &emptyBlock || stopBlock != &emptyBlock, "non-empty region range has empty stopRegion");
			
			if(startBlock == stopBlock) {
				func(ByteOffset(startBlock, startRegion.position), stopRegion.position - startRegion.position);
			} else {
				
				//invoke on last block
				func(ByteOffset(stopBlock, sizeof(Block)), stopRegion.position - sizeof(Block));
				
				//invoke on intermediate blocks
				for(stopBlock = stopBlock->previousBlock; stopBlock != startBlock; stopBlock = stopBlock->previousBlock) {
					RUNTIME_ASSERT(stopBlock != &emptyBlock, "Malformed Region range! - walked back to emptyBlock before hitting startBlock");
					func(ByteOffset(stopBlock, sizeof(Block)), stopBlock->position - sizeof(Block));
				}

				//invoke on first block
				func(ByteOffset(startBlock, startRegion.position), startBlock->position - startRegion.position);
			}
		}
		
		//Note: invokes 'translator(void* regionElement, void* bufferElement)' for each regionElement in [startRegion, stopRegion]
		// 		and each bufferElement in a contiguous buffer
		template<typename TranslatorFuncT>
		static inline void TranslateRegionsToBuffer(const Region& startRegion, const Region& stopRegion, uint32 numElements, void* buffer,
													uint32 regionStride, uint32 bufferStride, const TranslatorFuncT& translator) {
			
			void* bufferEnd = ByteOffset(buffer, bufferStride*numElements);
			
			//Note: ForEachRegion iterates chunks backwards so we translate buffer in reverse to maintain chunk ordering
			ForEachRegion(startRegion, stopRegion,
				          [&](void *chunk, uint32 chunkBytes) {
				
							  //position bufferData at start of chunk
							  void *bufferDataEnd = bufferEnd;
							  void *bufferData = ByteOffset(bufferDataEnd, -bufferStride*(chunkBytes/regionStride));
				
							  //set next tail at start of this chunk
							  bufferEnd = bufferData;
				
							  //translate chunk to buffer
							  while(bufferData < bufferDataEnd) {
					
								  translator(chunk, bufferData);
					
								  bufferData = ByteOffset(bufferData, bufferStride);
								  chunk=ByteOffset(chunk, regionStride);
							  }
						  });
		}
		
		//Note: Copies Region of type 'RegionT' to contiguous buffer
		template <typename RegionT>
		static inline void CopyRegionsToBuffer(const Region& startRegion, const Region& stopRegion, uint32 numElements, void* buffer,
											   uint32 regionStride=sizeof(RegionT), uint32 bufferStride=sizeof(RegionT)) {
			
			//TODO: Make sure that translate function compiles int '__builtin_memcpy'
			// 		if not, consider invoking CopyMemory to instead (will prevent constructors from being called)
			
			TranslateRegionsToBuffer(startRegion, stopRegion,
				                     numElements, buffer,
				                     regionStride, bufferStride,
									 [](void *src, void *dst) { *(RegionT*)dst = *(RegionT*)src; });
		}
};