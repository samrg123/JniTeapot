#pragma once

#include <errno.h>
#include <sys/mman.h>
#include <string.h>

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
			
			static inline uint memoryBlockCount;
			static inline uint memoryBlockReserveCount;
		#endif

		class Arena;
		
		class Region {
			friend Arena;
			friend Memory;
			
			Block* block;
			uint32 position;
		};
		
        class Arena {
        	private:
	            friend Memory;
	    
	            Block* currentBlock;
	            Block* reservedBlock;
	
				#if ENABLE_MEMORY_STATS
	                uint32 arenaBytes;
	                uint32 arenaPadBytes;
	                uint32 arenaUnusedBytes; //bytes at end of blocks - includes reserve block
	                uint arenaBlockCount;    //includes reserve block
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
		        
		        void* ExtendBlockWithT(Block* block, uint32 tBytes, uint8 alignment) {
					void* tPos = ByteOffset(block, block->position);
					uint32 freeBytes = block->FreeBytes();
			
					uint8 alignmentOffset = AlignUpOffsetPow2(tPos, alignment);
					uint32 alignedBytes = tBytes + alignmentOffset;
			
					if(freeBytes < alignedBytes) return nullptr;
			
					tPos = ByteOffset(tPos, alignmentOffset);
					block->position+= alignedBytes;
			
					#if ENABLE_MEMORY_STATS
						block->padBytes+= alignmentOffset;
				
						arenaPadBytes+= alignmentOffset;
						arenaUnusedBytes-= alignedBytes;
				
						memoryPadBytes+= alignmentOffset;
						memoryUnusedBytes-= alignedBytes;
                    #endif
						
					return tPos;
		        }
		
				//Note: calls func(void* chunk, uint32 bytes) for each chunk in the arena while 'chunk != stopBlock'
		        template<typename T>
		    	inline void ForEachBlockExclusive(const Block* stopBlock, const T& func) const {
					Block* block = currentBlock;
					while(block != stopBlock) {
						func(ByteOffset(block, sizeof(Block)), block->position - sizeof(Block));
						block = block->previousBlock;
					}
		        }
		        
		        template<typename T>
				static T DefaultTranslator(const void* element) { return *(const T*)element; }
				
		
				// translates chunk with 'translator(void*)' and copies over results to buffer
				//Note: this is designed to be used inside 'ForEach' which iterates chunks in reverse
				// 		so instead of passing in the base buffer address and iterating forward we pass
				// 		in the end buffer address and iterate backwards to preserve chunk ordering
		        template<typename TranslatorFuncT>
		        inline void TranslateChunkToBuffer(const TranslatorFuncT& translator, void*& bufferEnd,
												   void* chunk, uint32 chunkBytes,
												   uint32 bufferStride, uint32 chunkStride) const {
					
					using T = decltype(translator(nullptr));
		        	
		        	//position bufferData at start of chunk
					void* bufferDataEnd = bufferEnd;
					void* bufferData = ByteOffset(bufferDataEnd, -bufferStride*(chunkBytes/chunkStride));
					
					//set next tail at start of this chunk
					bufferEnd = bufferData;
					
					//copy chunk into the buffer
					while(bufferData < bufferDataEnd) {
						*(T*)bufferData = translator(chunk);
						
						bufferData = ByteOffset(bufferData, bufferStride);
						chunk = ByteOffset(chunk, chunkStride);
					}
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

					void* tPosition = ExtendBlockWithT(currentBlock, bytes, alignment);
					if(!tPosition) {

						//check to see if we can reuse the reserved block
						if(reservedBlock) {

							tPosition = ExtendBlockWithT(reservedBlock, bytes, alignment);
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

					if(zeroMemory) memset(tPosition, 0, bytes);
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
		        
		        inline bool IsEmptyRegion(Region r) {
				    return r.block == currentBlock && r.position == currentBlock->position;
				}
		        
		        inline Region CreateRegion() {
		        	Region region;
			        region.block = currentBlock;
			        region.position = currentBlock->position;
		        	return region;
		        }
		        
		        inline void FreeRegion(const Region& region) {
		        	
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
		        
		        //Note: calls func(void* chunk, uint32 bytes) for each chunk in the arena
				//Warn: ForEach iterates blocks in reverse order from when they were pushed to the region
				template<typename T>
				inline void ForEach(const T& func) const { ForEachBlockExclusive(&emptyBlock, func); }
		
				//Note: calls func(void* chunk, uint32 bytes) for each chunk in the region
		        //Warn: ForEachRegion iterates blocks in reverse order from when they were pushed to the region
                template<typename T>
                inline void ForEachRegion(const Region& region, const T& func) const {
					ForEachBlockExclusive(region.block, func);
					func(ByteOffset(region.block, region.position), region.block->position - region.position);
				}
				
				//Note: Copies whole arena of type 'T' to contiguous buffer preserving the order in which elements were pushed
				template<typename T>
				inline void CopyToBuffer(uint32 numElements, void* buffer,
										 uint32 bufferStride = sizeof(T), uint32 arenaStride = sizeof(T)) const {
					
					void* bufferEnd = ByteOffset(buffer, bufferStride*numElements);
		        	ForEach([&](void* chunk, uint32 bytes) {
						TranslateChunkToBuffer(DefaultTranslator<T>, bufferEnd, chunk, bytes, bufferStride, arenaStride);
		        	});
		        }
		
				//Note: Copies Region of type 'T' to contiguous buffer preserving the order in which elements were pushed
				template<typename T>
				inline void CopyRegionToBuffer(const Region& region, uint32 numElements, void* buffer,
											   uint32 bufferStride = sizeof(T), uint32 regionStride = sizeof(T)) const {
					
					void* bufferEnd = ByteOffset(buffer, bufferStride*numElements);
					ForEachRegion(region, [&](void* chunk, uint32 bytes) {
						TranslateChunkToBuffer(DefaultTranslator<T>, bufferEnd, chunk, bytes, bufferStride, regionStride);
					});
				}
		
				//Note: Copies arena to contiguous buffer of type 'T' preserving the order in which elements were pushed
				// 		by translating each element with 'translator(void* element)->T'
				template<typename TranslatorFuncT>
				inline void TranslateToBuffer(uint32 numElements, void* buffer, uint32 arenaStride,
											  const TranslatorFuncT& translator,
											  uint32 bufferStride = sizeof(decltype(translator(nullptr)))) const {
					
					void* bufferEnd = ByteOffset(buffer, bufferStride*numElements);
					ForEach([&](void* chunk, uint32 bytes) {
						TranslateChunkToBuffer(translator, bufferEnd, chunk, bytes, bufferStride, arenaStride);
					});
		        }
		
				//Note: Copies Region to contiguous buffer of type 'T' preserving the order in which elements were pushed
				// 		by translating each element with 'translator(void* element)->T'
				template<typename TranslatorFuncT>
				inline void TranslateRegionToBuffer(const Region& region, uint32 numElements, void* buffer, uint32 regionStride,
													const TranslatorFuncT& translator,
													uint32 bufferStride = sizeof(decltype(translator(nullptr)))) const {
					
					void* bufferEnd = ByteOffset(buffer, bufferStride*numElements);
					ForEachRegion(region, [&](void* chunk, uint32 bytes) {
						TranslateChunkToBuffer(translator, bufferEnd, chunk, bytes, bufferStride, regionStride);
					});
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
		        	Region region = {};
		        	region.block = (Block*)&emptyBlock;
					FreeRegion(region);
					Pack();
				}
        };

        static inline Arena temporaryArena = Arena(0);
};