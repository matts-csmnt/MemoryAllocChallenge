#pragma once
//////////////////////////////////////////////////////////////////////////
// MODIFY THIS FILE AS YOU WISH
//////////////////////////////////////////////////////////////////////////

#include "MemoryManagement.h"

#include <cstdlib>
#include <memory>
#include <list>

#pragma region IMemoryAllocator Extended Base
//Extended IMemoryAllocator for testing and data gathering / signal handling
class IMemoryAllocatorX : public IMemoryAllocator
{
public:
	virtual void* allocate(size_t size, size_t alignment) = 0;
	virtual void release(void* ptr) = 0;

	//CUSTOM FOR HANDLING SIGNALS
	virtual void handle_signals(int sig) {  };

	//CUSTOM - for measurements
	void measure_usage(size_t size);
	void output_all_data(const char* cn);

	//CUSTOM - for measuring ring buffer sizes etc
	void set_maxSpaceUsed(size_t s);
	void set_lastMaxSpaceUsed(size_t s);

	const size_t get_maxSpaceUsed() { return maxSpaceUsed; };
	const size_t get_lastMaxSpaceUsed() { return lastMaxSpaceUsed; };

	void set_isDataOutputted() { isDataOutputted = true; };
	bool get_isDataOutputted() { return isDataOutputted; };

	void reset_maxNumActiveAllocations() { if(maxNumActiveAllocations > lastMaxNumActiveAllocations) lastMaxNumActiveAllocations = maxNumActiveAllocations; maxNumActiveAllocations = 0; };
	bool get_maxNumActiveAllocations() { return maxNumActiveAllocations; };

private:
	//CUSTOM - helper members - measuring memory
	size_t maxSpaceUsed = 0;
	size_t largestAllocation = 0;
	size_t lastMaxSpaceUsed = 0;
	size_t maxNumActiveAllocations = 0;
	size_t lastMaxNumActiveAllocations = 0;

	//CUSTOM - bool for skipping repeat outputs on destruction
	bool isDataOutputted = false;
};
#pragma endregion

#pragma region Unalligned Malloc
// A trivial allocator using malloc
class MallocAllocator : public IMemoryAllocatorX
{
public:
	virtual void* allocate(size_t size, size_t alignment) { return malloc(size); (void)alignment; }
	virtual void release(void* ptr) { free(ptr); }
};
#pragma endregion

#pragma region Alligned Malloc
// A trivial allocator using aligned malloc
class AlignedMallocAllocator : public IMemoryAllocatorX
{
public:
	virtual void* allocate(size_t size, size_t alignment) { return _aligned_malloc(size, alignment); (void)alignment; }
	virtual void release(void* ptr) { _aligned_free(ptr); }
};
#pragma endregion

#pragma region Stack Allocator - ALL PURPOSE
// A stack allocator using aligned malloc
class StackAllocator : public IMemoryAllocatorX
{
public:
	StackAllocator() = default;
	StackAllocator(size_t size, MemoryMappingType type, bool overrideAlign = false) : memorySize(size), memoryType(type), alignOverride(overrideAlign) {};

	virtual void* allocate(size_t size, size_t alignment);

	virtual void release(void* ptr);

	virtual void handle_signals(int sig);

	//accessors/setters
	const size_t get_spaceRemaining() const { return spaceRemaining; };
	uint8_t* get_memblock() const { return memblock; };
	uint8_t* get_memLoc() const { return memLoc; };

	void set_spaceRemaining(size_t s) { spaceRemaining = s; };
	void set_memblock(uint8_t* m) { memblock = m; };
	void set_memLoc(uint8_t* m) { memLoc = m; };

	//reset
	void reset_memory_loc() { memLoc = memblock; };

	//size and type of memory access/set
	const size_t get_memorySize() { return memorySize; };
	void set_memorySize(size_t s) { memorySize = s; };

	const MemoryMappingType get_memoryType() { return memoryType; };
	void set_memoryType(MemoryMappingType mt) { memoryType = mt; };

	bool get_alignment_override() { return alignOverride; };

	~StackAllocator();

private:
	size_t spaceRemaining;
	//Initial memory address
	uint8_t* memblock = nullptr;
	//address of next free space
	uint8_t* memLoc = memblock;

	//size of memory we need to initialise the block as using
	size_t memorySize = 0;

	//memory type that can be altered on creation
	MemoryMappingType memoryType = MemoryMappingType::kUndefined;

	//override alignments?
	bool alignOverride = false;

};
#pragma endregion

#pragma region Stack Allocator - With Marker Rollback
class RollbackStackAllocator : public StackAllocator {
public:
	RollbackStackAllocator() = default;
	RollbackStackAllocator(size_t size, MemoryMappingType type) { set_memorySize(size); set_memoryType(type); };

	void place_marker() { rollback_marker = get_memLoc(); };
	void rollback_to_marker();
	uint8_t* get_marker() { return rollback_marker; };

	void handle_signals(int sig);
private:
	uint8_t* rollback_marker = nullptr;	//where to roll back to if we need to use rollback
};
#pragma endregion

#pragma region CPU Stack Allocator - UNUSED
class CPUStackAllocator : public StackAllocator {
public:
	CPUStackAllocator() = default;
	CPUStackAllocator(size_t memory) { set_memorySize(memory); };

	virtual void* allocate(size_t size, size_t alignment);
	void release(void* ptr);

	~CPUStackAllocator();
};
#pragma endregion

#pragma region GPU Stack Allocator - UNUSED
class GPUStackAllocator : public StackAllocator {
public:
	virtual void* allocate(size_t size, size_t alignment);
	virtual void release(void* ptr);

	~GPUStackAllocator();
};
#pragma endregion

#pragma region Ring / Active Frame Stack Allocator
class MultiFrameAllocator : public StackAllocator {
public:
	MultiFrameAllocator() = default;
	MultiFrameAllocator(size_t memory, MemoryMappingType type) { set_memorySize(memory); set_memoryType(type); };

	void* allocate(size_t size, size_t alignment);

	void handle_signals(int);
	void inc_frame_count() { ++frameCount; };
	void reset_frame_count() { frameCount = 0; };
	const size_t get_frame_count() { return frameCount; };

	~MultiFrameAllocator();
private:
	size_t frameCount = 0;
};
#pragma endregion

#pragma region CPU Multi Frame - UNUSED
class CPUMFAllocator : public CPUStackAllocator {
public:
	CPUMFAllocator() = default;
	CPUMFAllocator(size_t memory) { set_memorySize(memory); };

	void* allocate(size_t size, size_t alignment);

	void handle_signals(int);
	void inc_frame_count() { ++frameCount; };
	void reset_frame_count() { frameCount = 0; };
	const size_t get_frame_count() { return frameCount; };

	~CPUMFAllocator();
private:
	size_t frameCount = 0;
};
#pragma endregion

#pragma region GPU Multi Frame Allocator - UNUSED
class GPUMFAllocator : public CPUStackAllocator {
public:
	virtual void* allocate(size_t size, size_t alignment);

	~GPUMFAllocator();
};
#pragma endregion

#pragma region Object Pool
class ObjectPoolManager : public StackAllocator {
public:
	ObjectPoolManager() = default;
	ObjectPoolManager(size_t maxAllocs, MemoryMappingType type) { MaxAllocations = maxAllocs; set_memorySize(maxAllocs * sizeof(dataPack)); set_memoryType(type); };

	virtual void* allocate(size_t size, size_t alignment);
	virtual void release(void* ptr);

	void handle_signals(int sig) {};

	~ObjectPoolManager();

	//64 byte data elements w a pointer
	struct dataPack {
		/*uint8_t* prev = nullptr;*/
		dataPack* next = nullptr;
		bool live : 1;
		bool used : 2;
		double_t d[8];

		dataPack* getNext() const { return next; }
		void setNext(dataPack* n) { next = n; }
	};

private:
	size_t MaxAllocations = 0;
	dataPack* allocationPool;
	uint8_t* endOfBlock;

	//free element finder
	dataPack* firstAvailable;
	dataPack* add_data();
};
#pragma endregion

//Free List - Attempted, unfinished
#pragma region Free List Allocator - DRAFT IDEA SMALL OBJECT TEST
//class ObjectPoolManager : public StackAllocator {
//public:
//	ObjectPoolManager() = default;
//	ObjectPoolManager(size_t size, MemoryMappingType type) { set_memorySize(size); set_memoryType(type); };
//
//	virtual void* allocate(size_t size, size_t alignment);
//	virtual void release(void* ptr);
//
//	~ObjectPoolManager();
//
//private:
//	// linked list structs
//	typedef struct dll_head {
//		//doubly linked list head to link allocatons
//		struct dll_head *next;
//		struct dll_head *previous;
//	};
//
//	typedef struct alloc_node {
//		//dll_head node;		//linked list links
//		size_t size;		//size of this allocation
//		uint8_t* blockLoc;	//location of this alloc
//	};
//
//	//find the offset in bytes of where the alloc should begin
//	//const size_t ALLOC_HEADER_SZ = offsetof(alloc_node, blockLoc);
//
//	//blocks can be of sizes 4,8,12,16,32,64
//	const size_t MinAllocSize = 4;
//
//	//free list
//	std::list<alloc_node> freeList;
//
//	//list management
//	void defrag_free_list();
//	void add_node_block(void*, size_t);
//
//};
#pragma endregion

// Modify this test harness to setup your allocators 
// and pass them to the test suite.
class AssignmentTestHarness
{
public:
	AssignmentTestHarness();
	void signal(GameEventType evt);
	~AssignmentTestHarness();

	//collection of allocators
	MemoryAllocatorSet m_memAllocSet;

private:
	MallocAllocator m_simpleAllocator;
	AlignedMallocAllocator m_alignedMalloc;

	//Things with constuction parameters
	StackAllocator* m_pSmallObjStackAllocator;
	StackAllocator* m_pStackAllocator;
	MultiFrameAllocator* m_pCPUMFAllocator;
	MultiFrameAllocator* m_pGPUMFAllocator;

	//Level tests - CPU used for system data - GPU used for unloaded/loaded levels
	StackAllocator* m_pCPULevelStack;
	RollbackStackAllocator* m_pRollbackGPU;

	ObjectPoolManager* m_pSmallObjectPool;
};
