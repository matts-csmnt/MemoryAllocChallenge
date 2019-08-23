#include "AssignmentTestHarness.h"
#include <fstream>
#include <iomanip>

#define DATALOGGING_ON 1

AssignmentTestHarness::AssignmentTestHarness()
{
	// TODO: any setup or initialization here.
	m_pStackAllocator = new StackAllocator(32 * MB, MemoryMappingType::kUndefined);
	m_pCPUMFAllocator = new MultiFrameAllocator(159 * KB, MemoryMappingType::kCPU);
	m_pGPUMFAllocator = new MultiFrameAllocator(159 * KB, MemoryMappingType::kGPU);

	//level tests
	m_pCPULevelStack = new StackAllocator(10 * KB, MemoryMappingType::kCPU);
	m_pRollbackGPU = new RollbackStackAllocator(156 * MB, MemoryMappingType::kGPU);

	//OBJECT POOL
	m_pSmallObjectPool = new ObjectPoolManager(4096 * 2, MemoryMappingType::kUndefined);

	// TODO: request any system allocations you intend on subdividing.
	// Note: done by the allocator

	// TODO: Then assign your allocators and pass them to the test suite.

	// Here is an example...
	// NOTE THAT THIS WILL FAIL MANY TESTS
	m_memAllocSet.GeneralHeap = &m_alignedMalloc;
	m_memAllocSet.SmallObject = m_pSmallObjectPool;	
	m_memAllocSet.ScratchSpace = m_pStackAllocator;
	m_memAllocSet.SingleFrameCPU = m_pCPUMFAllocator;
	m_memAllocSet.SingleFrameGPU = m_pGPUMFAllocator;
	m_memAllocSet.LevelCPU = m_pCPULevelStack;
	m_memAllocSet.LevelGPU = m_pRollbackGPU;
	set_allocators(m_memAllocSet);


}

void AssignmentTestHarness::signal(GameEventType evt)
{
	// NOTE: Unit tests will call this function with a variety of useful signals.
	// By intercepting them you can tailor your memory system behavior accordingly.
	// In many tests, allocated memory is need for only a small number of frames or a a specific period of time.
	switch (evt)
	{
	case GameEventType::kEventFlushScratchSpace:	// signaled when a system has finished with scratch memory.
		reinterpret_cast<IMemoryAllocatorX*>(m_memAllocSet.ScratchSpace)->handle_signals((int)GameEventType::kEventFlushScratchSpace);
		break;
	case GameEventType::kEventGameInit:
		break;
	case GameEventType::kEventLevelBeginLoad:		// signaled when a "level" begins to load.
		reinterpret_cast<IMemoryAllocatorX*>(m_memAllocSet.LevelGPU)->handle_signals((int)GameEventType::kEventLevelBeginLoad);
	case GameEventType::kEventLevelLoadComplete:	// signaled when "level" loading is complete.
		break;
	case GameEventType::kEventLevelUnload:
		reinterpret_cast<IMemoryAllocatorX*>(m_memAllocSet.LevelGPU)->handle_signals((int)GameEventType::kEventLevelUnload);
		break;
	case GameEventType::kEventGameShutdown:
		break;
	case GameEventType::kEventNextFrame:			// signaled when a frame is finished
		reinterpret_cast<IMemoryAllocatorX*>(m_memAllocSet.SingleFrameCPU)->handle_signals((int)GameEventType::kEventNextFrame);
		reinterpret_cast<IMemoryAllocatorX*>(m_memAllocSet.SingleFrameGPU)->handle_signals((int)GameEventType::kEventNextFrame);
		break;
	}
}


AssignmentTestHarness::~AssignmentTestHarness()
{
	// TODO: any tear down shutdown code here.
	delete m_pRollbackGPU;
	delete m_pCPULevelStack;
	delete m_pGPUMFAllocator;
	delete m_pCPUMFAllocator;
	delete m_pStackAllocator;
	delete m_pSmallObjectPool;
}

//=====================================================
//======= Implementations for Custom Allocators =======
//=====================================================
#pragma region CUSTOM ALLOCATOR IMPLEMENTATIONS

#pragma region IMemoryAllocator Extended Base - Measurement Helpers
void IMemoryAllocatorX::measure_usage(size_t size)
{
	maxSpaceUsed += size;
	++maxNumActiveAllocations;

	if (size > largestAllocation)
		largestAllocation = size;
}

void IMemoryAllocatorX::output_all_data(const char* cn)
{
	//early out if we already have data
	if (get_isDataOutputted())
		return;

	std::ofstream datalog("datalog.csv", std::fstream::app);
	datalog << cn << ",\n"
		<< "largest allocation:," << largestAllocation << " B,\n"
		<< "max space used (active allocations):," << lastMaxSpaceUsed << " B," << (float)(lastMaxSpaceUsed / KB) << " KB," << (float)(lastMaxSpaceUsed / MB) << " MB,\n"
		<< "max space used (all):," << maxSpaceUsed << " B," << (float)(maxSpaceUsed / KB) << " KB," << (float)(maxSpaceUsed / MB) << " MB,\n"
		<< "max active allocations (active allocations):," << lastMaxNumActiveAllocations << ",\n"
		<< "max active allocations (all):," << maxNumActiveAllocations << ",\n\n";
	datalog.close();

	//set flag if this is the first data to be outputted
	set_isDataOutputted();
}

void IMemoryAllocatorX::set_maxSpaceUsed(size_t s)
{
	maxSpaceUsed = s;
}

void IMemoryAllocatorX::set_lastMaxSpaceUsed(size_t s)
{
	lastMaxSpaceUsed = s;
}
#pragma endregion

#pragma region Stack Allocator - ALL PURPOSE
void* StackAllocator::allocate(size_t size, size_t alignment) {

	//if no memory grabbed - get it
	if (!get_memblock())
	{
		//TEST MEMORY SIZE
		int blocksize = get_memorySize();
		set_spaceRemaining(blocksize);
		set_memblock((uint8_t*)allocate_system_block(blocksize, get_memoryType()));

		reset_memory_loc();
	}

	size_t sR = get_spaceRemaining();
	void* ret_p = nullptr;

	//override the alignment?
	//if (get_alignment_override() == false)
	//{
		//align the first element
		void* pCur = (void*)get_memLoc();
		ret_p = std::align(alignment, size, pCur, sR);
	//}
	//else
	//{
	//	ret_p = (void*)get_memLoc();
	//	sR -= size;
	//}

	//test for memory used up
	SHU_ASSERT(ret_p != nullptr)
	SHU_ASSERT((sR) >= size)

	set_spaceRemaining(sR);

	//check if is in cpu space
	SHU_ASSERT(is_within_mapped_block(get_memLoc(), get_memoryType()))

	//measure alignment offset
#if DATALOGGING_ON == 1
	ptrdiff_t alignOffset = (uint8_t*)ret_p - get_memLoc();
#endif

	//inc memory address by size for next time
	set_memLoc((uint8_t*)ret_p + size);

	//log stats
#if DATALOGGING_ON == 1
	measure_usage(alignOffset + size);
#endif
	return ret_p;
}

void StackAllocator::release(void* ptr) {
	if(memblock != nullptr)
		release_system_block(ptr);
}

void StackAllocator::handle_signals(int sig) {
	//flush scratch space...
	switch (sig)
	{
	case 0:
		reset_memory_loc();

		//reset our memory usage to find size of active allocations only
		if (get_maxSpaceUsed() > get_lastMaxSpaceUsed())
		{
			set_lastMaxSpaceUsed(get_maxSpaceUsed());
		}

		set_maxSpaceUsed(0);
	}
}

StackAllocator::~StackAllocator() {
	//log stats
#if DATALOGGING_ON == 1
	std::string name = "Stack Allocator: ";
	switch (get_memoryType())
	{
	case MemoryMappingType::kCPU:
		name += "CPU";
		break;
	case MemoryMappingType::kGPU:
		name += "GPU";
		break;
	default:
		name += "UNDEFINED";
		break;
	}
	output_all_data(name.c_str());

	std::ofstream datalog("datalog.csv", std::fstream::app);
	datalog << "memory size:," << get_memorySize() << ",\n"
		<< "space remaining:," << get_spaceRemaining() << ",\n\n";
	datalog.close();

#endif
	//release whole chunk of memory
	release(memblock);
}
#pragma endregion

#pragma region Stack Allocator - With Marker Rollback
void RollbackStackAllocator::rollback_to_marker(){
	SHU_ASSERT(get_marker() != nullptr);

	//find how much space we will have left when reset and set it
	ptrdiff_t diff = get_memLoc() - get_marker();
	set_spaceRemaining(get_spaceRemaining() + diff);

	//set active memory location to the marker we placed
	set_memLoc(get_marker());
}

void RollbackStackAllocator::handle_signals(int sig) {
	switch (sig)
	{
	case 2:	//Begin load level
		place_marker();
		break;
	case 4: //unload level
		rollback_to_marker();
		break;
	}
}
#pragma endregion

#pragma region CPU Stack Allocator - UNUSED AS BASE HAS BEEN EXTENDED
void* CPUStackAllocator::allocate(size_t size, size_t alignment) {

	//if no memory grabbed - get it
	if (!get_memblock())
	{
		//TEST MEMORY SIZE
		int blocksize = MB * 1;	//1mb of memory	
		set_spaceRemaining(blocksize);
		set_memblock((uint8_t*)allocate_system_block(blocksize, MemoryMappingType::kCPU));

		reset_memory_loc();
	}

	size_t sR = get_spaceRemaining();
	void* ret_p = nullptr;

	//align the first element
	void* pCur = (void*)get_memLoc();
	ret_p = std::align(alignment, size, pCur, sR);

	//test for memory used up
	SHU_ASSERT(ret_p != nullptr)
	SHU_ASSERT((sR) >= size)

	set_spaceRemaining(sR);

	//check if is in cpu space
	SHU_ASSERT(is_within_mapped_block(get_memLoc(), MemoryMappingType::kCPU))

	set_memLoc((uint8_t*)ret_p + size);	//inc memory address by size for next time

	//log stats
#if DATALOGGING_ON == 1
	measure_usage(size);
#endif
	return ret_p;
}

void CPUStackAllocator::release(void* ptr) {
	release_system_block(ptr);
}

CPUStackAllocator::~CPUStackAllocator(){
	//log stats
#if DATALOGGING_ON == 1
	output_all_data("CPU Stack Allocator");
#endif
}
#pragma endregion

#pragma region GPU Stack Allocator - UNUSED AS BASE HAS BEEN EXTENDED
void* GPUStackAllocator::allocate(size_t size, size_t alignment) {

	//if no memory grabbed - get it
	if (!get_memblock())
	{
		//TEST MEMORY SIZE
		int blocksize = MB * 1;	//1mb of memory	
		set_spaceRemaining(blocksize);
		set_memblock((uint8_t*)allocate_system_block(blocksize, MemoryMappingType::kGPU));

		reset_memory_loc();
	}

	size_t sR = get_spaceRemaining();
	void* ret_p = nullptr;

	void* pCur = (void*)get_memLoc();
	ret_p = std::align(alignment, size, pCur, sR);

	//test for memory used up
	SHU_ASSERT(ret_p != nullptr)
		SHU_ASSERT((sR) >= size)

		set_spaceRemaining(sR);

	//check if is in gpu space
	SHU_ASSERT(is_within_mapped_block(get_memLoc(), MemoryMappingType::kGPU))

		set_memLoc((uint8_t*)ret_p + size);	//inc memory address by size for next time
	return ret_p;
}

void GPUStackAllocator::release(void* ptr) {
	release_system_block(ptr);
}

GPUStackAllocator::~GPUStackAllocator() {
	//log stats
#if DATALOGGING_ON == 1
	output_all_data("GPU Stack Allocator");
#endif
}
#pragma endregion

#pragma region Multi / Active Frame Stack Allocator 
void* MultiFrameAllocator::allocate(size_t size, size_t alignment) {

	//if no memory grabbed - get it
	if (!get_memblock())
	{
		//TEST MEMORY SIZE
		//int blocksize = KB * 159;	//minimum needed for "SingleFrameCPU: Rapid short lived allocations (no release) (With Alignment Check)"
		int blocksize = get_memorySize();
		set_spaceRemaining(blocksize);
		set_memblock((uint8_t*)allocate_system_block(blocksize, get_memoryType()));

		reset_memory_loc();
	}

	size_t sR = get_spaceRemaining();
	void* ret_p = nullptr;

	//align the first element
	void* pCur = (void*)get_memLoc();
	ret_p = std::align(alignment, size, pCur, sR);

	//test for memory used up
	SHU_ASSERT(ret_p != nullptr)
	SHU_ASSERT((sR) >= size)

	set_spaceRemaining(sR);

	//check if is in cpu space
	SHU_ASSERT(is_within_mapped_block(get_memLoc(), get_memoryType()))

	//measure alignment offset
#if DATALOGGING_ON == 1
	ptrdiff_t alignOffset = (uint8_t*)ret_p - get_memLoc();
#endif

	//inc memory address by size for next time
	set_memLoc((uint8_t*)ret_p + size);

	//log stats
#if DATALOGGING_ON == 1
	measure_usage(alignOffset + size);
#endif
	return ret_p;
}

void MultiFrameAllocator::handle_signals(int s)
{
	switch (s)
	{
	case 6:
		//how many frames do we want active data
		//loop back to buffer start when we hit that
		inc_frame_count();
		if (get_frame_count() > 3)
		{
			reset_memory_loc();
			reset_frame_count();

			//reset our memory usage to find size of active allocations only
			if (get_maxSpaceUsed() > get_lastMaxSpaceUsed())
			{
				set_lastMaxSpaceUsed(get_maxSpaceUsed());
			}

			set_maxSpaceUsed(0);
		}
		break;
	}
}

MultiFrameAllocator::~MultiFrameAllocator() {
	//log stats
#if DATALOGGING_ON == 1
	std::string name = "Ring Allocator: ";
	switch (get_memoryType())
	{
	case MemoryMappingType::kCPU:
		name += "CPU";
		break;
	case MemoryMappingType::kGPU:
		name += "GPU";
		break;
	default:
		name += "UNDEFINED";
		break;
	}
	output_all_data(name.c_str());
#endif
}
#pragma endregion

#pragma region CPU MF Allocator - UNUSED
void* CPUMFAllocator::allocate(size_t size, size_t alignment) {

	//if no memory grabbed - get it
	if (!get_memblock())
	{
		//TEST MEMORY SIZE
		int blocksize = KB * 159;	//minimum needed for "SingleFrameCPU: Rapid short lived allocations (no release) (With Alignment Check)"
		set_spaceRemaining(blocksize);
		set_memblock((uint8_t*)allocate_system_block(blocksize, MemoryMappingType::kCPU));

		reset_memory_loc();
	}

	size_t sR = get_spaceRemaining();
	void* ret_p = nullptr;

	//align the first element
	void* pCur = (void*)get_memLoc();
	ret_p = std::align(alignment, size, pCur, sR);

	//test for memory used up
	SHU_ASSERT(ret_p != nullptr)
	SHU_ASSERT((sR) >= size)

	set_spaceRemaining(sR);

	//check if is in cpu space
	SHU_ASSERT(is_within_mapped_block(get_memLoc(), MemoryMappingType::kCPU))

	set_memLoc((uint8_t*)ret_p + size);	//inc memory address by size for next time

		//log stats
#if DATALOGGING_ON == 1
	measure_usage(size);
#endif
	return ret_p;
}

void CPUMFAllocator::handle_signals(int s)
{
	switch (s)
	{
	case 6:
		//how many frames do we want active data
		//loop back to buffer start when we hit that
		inc_frame_count();
		if (get_frame_count() > 3)
		{
			reset_memory_loc();
			reset_frame_count();

			//reset our memory usage to find size of active allocations only
			if (get_maxSpaceUsed() > get_lastMaxSpaceUsed())
			{
				set_lastMaxSpaceUsed(get_maxSpaceUsed());
			}

			set_maxSpaceUsed(0);
		}
		break;
	}
}

CPUMFAllocator::~CPUMFAllocator() {
	//log stats
#if DATALOGGING_ON == 1
	//const char* classname = __CLASS__;
	output_all_data("CPU Ring Allocator");

	std::ofstream datalog("datalog.csv", std::fstream::app);
	datalog << "space remaining:," << get_spaceRemaining() << ",\n\n";
	datalog.close();

#endif
}
#pragma endregion

#pragma region GPU MF Allocator - UNUSED
void* GPUMFAllocator::allocate(size_t size, size_t alignment) {

	//if no memory grabbed - get it
	if (!get_memblock())
	{
		//TEST MEMORY SIZE
		//int blocksize = KB * 159;	//minimum needed for "SingleFrameCPU: Rapid short lived allocations (no release) (With Alignment Check)"
		int blocksize = MB * 1;		//test size
		set_spaceRemaining(blocksize);
		set_memblock((uint8_t*)allocate_system_block(blocksize, MemoryMappingType::kGPU));

		reset_memory_loc();
	}

	size_t sR = get_spaceRemaining();
	void* ret_p = nullptr;

	//align the first element
	void* pCur = (void*)get_memLoc();
	ret_p = std::align(alignment, size, pCur, sR);

	//test for memory used up
	SHU_ASSERT(ret_p != nullptr)
		SHU_ASSERT((sR) >= size)

		set_spaceRemaining(sR);

	//check if is in cpu space
	SHU_ASSERT(is_within_mapped_block(get_memLoc(), MemoryMappingType::kGPU))

		set_memLoc((uint8_t*)ret_p + size);	//inc memory address by size for next time

			//log stats
#if DATALOGGING_ON == 1
	measure_usage(size);
#endif
	return ret_p;
}

GPUMFAllocator::~GPUMFAllocator() {
	//log stats
#if DATALOGGING_ON == 1
	output_all_data("GPU Ring Allocator");

	std::ofstream datalog("datalog.csv", std::fstream::app);
	datalog << "space remaining:," << get_spaceRemaining() << ",\n\n";
	datalog.close();

#endif
}
#pragma endregion

#pragma region Free List Allocator - DRAFT IDEA SMALL OBJECT TEST
//void * ObjectPoolManager::allocate(size_t size, size_t alignment)
//{
//	//if no memory grabbed - get it
//	if (!get_memblock())
//	{
//		//TEST MEMORY SIZE
//		int blocksize = get_memorySize();
//		set_spaceRemaining(blocksize);
//		set_memblock((uint8_t*)allocate_system_block(blocksize, get_memoryType()));
//
//		reset_memory_loc();
//
//		//push initial node to the free list
//		alloc_node block;
//		block.size = blocksize;
//		block.blockLoc = get_memLoc();
//		freeList.push_back(block);
//	}
//
//	//get the starting pointer for the allocation, after storage data
//	void* ptr = get_memLoc();
//	alloc_node* block = nullptr;
//
//	if (size > 0)
//	{
//		// check if the last element in the freeList contains spare space
//		alloc_node* n = &freeList.back();
//		SHU_ASSERT(n != nullptr);
//		if(n)
//			if (n->size >= size)
//			{
//				block = n;
//			}
//
//		// we found something
//		if (block)
//		{
//			// Can we split the block?
//			if ((block->size - size) >= MinAllocSize)
//			{
//				alloc_node* new_block;
//				
//				//create a new element with decreased size
//				new_block->blockLoc = block->blockLoc + size;
//				new_block->size = block->size - size;
//
//				//modify the existing block to contain the new allocation space
//				block->size = size;
//
//				freeList.push_back(*new_block);
//			}
//		}
//
//	} //else NULL
//
//	return ptr;
//}
//void ObjectPoolManager::release(void * ptr)
//{
//}
//ObjectPoolManager::~ObjectPoolManager()
//{
//}
//
////list management
//void ObjectPoolManager::defrag_free_list()
//{
//}
//void ObjectPoolManager::add_node_block(void * addr, size_t size)
//{
//	alloc_node *blk;
//
//	// align the start addr of our block to the next pointer aligned addr
//	//blk = (void *)align_up((uintptr_t)addr, sizeof(void*));
//	blk = (void*)get_memLoc();
//
//	// calculate actual size - mgmt overhead
//	blk->size = (uintptr_t)addr + size - (uintptr_t)blk
//		- ALLOC_HEADER_SZ;
//
//	//and now our giant block of memory is added to the list!
//	list_add(&blk->node, &freeList);
//}
#pragma endregion

#pragma region Object Pool
constexpr size_t kDSize = sizeof(ObjectPoolManager::dataPack);
constexpr size_t kMemOffset = offsetof(ObjectPoolManager::dataPack, d);

void * ObjectPoolManager::allocate(size_t size, size_t alignment)
{
		//if no memory grabbed - get it
		if (!get_memblock())
		{
			//TEST MEMORY SIZE
			int blocksize = get_memorySize();
			set_spaceRemaining(blocksize);
			set_memblock((uint8_t*)allocate_system_block(blocksize, get_memoryType()));	
			reset_memory_loc();
			
			//record where the block ends
			endOfBlock = get_memLoc() + blocksize;

			//init the pool - splat it across the requested data
			allocationPool = new(get_memLoc()) dataPack[MaxAllocations - 1];
			
			// The first one is available.
			firstAvailable = &allocationPool[0];

			// Each pack of data points to the next.
			for (int i(0); i < MaxAllocations - 1; i++)
			{
				allocationPool[i].setNext(&allocationPool[i + 1]);
				allocationPool[i].live = 0;
			}

			// The last one terminates the list.
			allocationPool[MaxAllocations - 1].setNext(nullptr);
		}

		//find our first free element
		uint8_t* ret_p = (uint8_t*)add_data();

		//check we are not out of range
		SHU_ASSERT((uint8_t*)ret_p <= (uint8_t*)&allocationPool[MaxAllocations-1]);

		//return the address of first pool location + offset for pointer to next
		ret_p += kMemOffset;

		//check if is in chosen space and in range
		SHU_ASSERT(is_within_mapped_block(ret_p, get_memoryType()));

		return ret_p;
}

void ObjectPoolManager::release(void * ptr)
{
	//free whatever was stored in the data
	dataPack* dpp = (dataPack*)((uint8_t*)ptr - kMemOffset);
	dpp->live = 0;

	dpp->setNext(firstAvailable);
	firstAvailable = dpp;
}

ObjectPoolManager::~ObjectPoolManager()
{
#if DATALOGGING_ON == 1
	size_t usedNodes(0);
	for (int i(0); i < MaxAllocations - 1; i++)
	{
		if (allocationPool[i].used)
			++usedNodes;
	}

	output_all_data("Object Pool Allocator");

	std::ofstream datalog("datalog.csv", std::fstream::app);
	datalog << "used nodes:," << usedNodes << ",\n"
	<< "max nodes:," << MaxAllocations << ",\n\n";
	datalog.close();
#endif

	//delete[] allocationPool;
	release_system_block(get_memblock());
}

ObjectPoolManager::dataPack* ObjectPoolManager::add_data()
{
	// Make sure the pool isn't full.
	SHU_ASSERT(firstAvailable != nullptr);

	// Remove it from the available list.
	dataPack* newPack = firstAvailable;
	firstAvailable = newPack->getNext();
	newPack->live = 1;

#if DATALOGGING_ON == 1
	//record this as used
	newPack->used = 1;
#endif

	return newPack;
}
#pragma endregion

#pragma endregion
