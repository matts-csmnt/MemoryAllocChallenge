#pragma once
//////////////////////////////////////////////////////////////////////////
// DO NOT MODIFY THIS FILE.
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// In Game Engines, memory is an carefully managed resource.
// Low level memory management systems in consoles and on the PC can differ significantly.
// In this assignment you are tasked with writing an improved memory management system for a theoretical game.
// The calling system will make many thousands of memory allocation requests through some memory callbacks.
// The final tests require you to allocate within special "memory mapped" blocks that you have requested from the GPU.
//////////////////////////////////////////////////////////////////////////


// If the following define is set the the test suit generates a leak report.
// This will be turned when the assignment is marked.
#define SHU_ENABLE_LEAK_REPORT

	// This define enables the
	// see : https://docs.microsoft.com/en-us/visualstudio/debugger/finding-memory-leaks-using-the-crt-library?view=vs-2017
	#define _CRTDBG_MAP_ALLOC

#include <stdlib.h>  
#include <crtdbg.h>

// Standard fixed size ints.
#include <cstdint>

// Assertion macro.
#define SHU_ASSERT(x) if(!(x)){ __debugbreak(); abort();}

// useful memory size constants
constexpr size_t KB = 1024; // 1 KB
constexpr size_t MB = 1024 * 1024; // 1 MB

// Check a pointer is aligned to a specified power 2 boundary.
bool is_aligned(const void* ptr, const size_t kAlignment);

// Fictional memory mapping classifications
enum class MemoryMappingType
{
	kCPU,
	kGPU,

	kMaxTypes, // last one.

	kUndefined = 0xFFFF // use to disable memory mapping checks in tests.
};

// Allocates a system block with the specified mapping type.
// returns a pointer to the start of the block.
void* allocate_system_block(size_t size, MemoryMappingType mappingType);

// Releases a system block previously allocated with allocate_system_block
void release_system_block(void* ptr);

// Checks to see if a memory address resides within a system block with the specified mapping.
bool is_within_mapped_block(const void* ptr, MemoryMappingType type);


// "Game" events interface.
// Unit tests will signal these during tests.
enum class GameEventType
{
	kEventFlushScratchSpace // signaled when a system has finished with scratch memory.
	, kEventGameInit
	, kEventLevelBeginLoad  // signaled when a "level" begins to load.
	, kEventLevelLoadComplete // signaled when "level" loading is complete.
	, kEventLevelUnload
	, kEventGameShutdown
	, kEventNextFrame // signaled when a frame is finished
	, kMaxEventTypes,
};


// Custom Allocator Interface
class IMemoryAllocator
{
public:
	virtual void* allocate(size_t size, size_t alignment) = 0;
	virtual void release(void* ptr) = 0;
};

// A collection of allocators for different use cases.
// The unit test suite will run tests on any of these.
// TODO: It's your job to provide these.
// Note that NONE of these allocators are required to be thread safe.
struct MemoryAllocatorSet {
	IMemoryAllocator* GeneralHeap;	// General purpose aligned memory : typically a malloc variant.
	IMemoryAllocator* SmallObject;	// Rapid small allocations and deallocations of required for mixed duration (no alignment required)
	IMemoryAllocator* ScratchSpace;	// Temporary scratch space for systems (typically large allocations used for a short period of time)
	IMemoryAllocator* SingleFrameCPU;	// Small temporary allocations required for a very short time : kMemoryMapping_CPU
	IMemoryAllocator* SingleFrameGPU;	// Small temporary allocations required for a very short time: kMemoryMapping_GPU
	IMemoryAllocator* LevelCPU; // Allocations which are required for a level but can be released all at once. : kMemoryMapping_CPU
	IMemoryAllocator* LevelGPU; // Allocations which are required for a level but can be released all at once. : kMemoryMapping_GPU
}; 

// Sets the collection of allocators.
void set_allocators(const MemoryAllocatorSet& allocatorSet);

// Returns the collection of allocators.
const MemoryAllocatorSet& get_allocators();