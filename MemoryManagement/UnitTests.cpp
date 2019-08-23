
//////////////////////////////////////////////////////////////////////////
// DO NOT MODIFY THIS FILE.
//////////////////////////////////////////////////////////////////////////

#include "MemoryManagement.h"

// Catch provides the unit test framework for this assignment.
#define CATCH_CONFIG_RUNNER
#include "catch.h"

#include "AssignmentTestHarness.h"

#include <algorithm>
#include <iostream>

// The one and only test harness instance.
AssignmentTestHarness* g_pTestHarness = nullptr;


// main entry point
int main(int argc, char* const argv[])
{
	std::cout << "==================================================" << std::endl;
	std::cout << "  HORSE Module - Assignment 2: Memory Management" << std::endl;
	std::cout << "==================================================" << std::endl;

	g_pTestHarness = new AssignmentTestHarness;

	int result = Catch::Session().run(argc, argv);

	delete g_pTestHarness;
	g_pTestHarness = nullptr;

#ifdef SHU_ENABLE_LEAK_REPORT
	std::cout << "==================================================" << std::endl;
	std::cout << "          DISPLAYING MEMORY LEAK REPORT" << std::endl;
	std::cout << "==================================================" << std::endl;

	std::cout << "The catch unit testing framework will produce one leak in this report." << std::endl;
	std::cout << "Any others are worth looking into, did you release memory?." << std::endl;
	// This code shows any detected memory leaks.
	// NOTE that you will get lots of these if you don't provide more advance allocators.
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
	_CrtDumpMemoryLeaks();
#endif

	// Now pause.
	system("pause");
	return 0;
}


// Allocation holder for testing.
class AllocHolder {
public:
	AllocHolder()
		: m_pAllocator(nullptr)
		, m_pMemory(nullptr)
		, m_size(0)
		, m_align(0)
		, m_tag(0xFFFFffff)
	{}
	explicit AllocHolder(IMemoryAllocator* pAllocator, size_t size, size_t align, uint32_t tag, bool checkAlign, MemoryMappingType mappingCheck)
		: m_pAllocator(nullptr)
		, m_pMemory(nullptr)
		, m_size(0)
		, m_align(0)
		, m_tag(0xFFFFffff)
	{
		allocate(pAllocator, size, align, tag, checkAlign, mappingCheck);
	}

	// Deleted copy and assign
	AllocHolder(const AllocHolder&) = delete;
	AllocHolder& operator = (const AllocHolder&) = delete;

	void allocate(IMemoryAllocator* pAllocator, size_t size, size_t align, uint32_t tag, bool checkAlign, MemoryMappingType mappingCheck)
	{
		SHU_ASSERT(pAllocator);
		SHU_ASSERT(size >= sizeof(uint32_t)); // min allocation
											 //MT_ASSERT(is_power_two(align));

		m_pMemory = pAllocator->allocate(size, align);
		if (!m_pMemory) {
			// Breakpoints here can be useful
			std::cout << "Memory allocate fail!" << std::endl;
		}
		REQUIRE(m_pMemory);

		if (checkAlign)
		{
			bool bIsAligned = is_aligned(m_pMemory, align);
			if(!bIsAligned){
				// Breakpoints here can be useful
				std::cout << "Align check fail!" << std::endl;
			}
			REQUIRE(bIsAligned);
		}

		if (mappingCheck != MemoryMappingType::kUndefined)
		{
			bool bIsMapped = is_within_mapped_block(m_pMemory, mappingCheck);
			if (!bIsMapped) {
				// Breakpoints here can be useful
				std::cout << "Mapping check fail!" << std::endl;
			}
			REQUIRE(bIsMapped);
		}

		m_pAllocator = pAllocator;
		m_size = size;
		m_align = align;

		// memchecking tag.
		set_tag(tag);
	}
	void set_tag(uint32_t tag)
	{
		// store the tag
		m_tag = tag;
		// write into the memory
		auto p = static_cast<uint32_t*>(m_pMemory);
		p[0] = m_tag;
	}
	bool check_tag()
	{
		auto p = static_cast<const uint32_t*>(m_pMemory);
		return p[0] == m_tag;
	}
	void release()
	{
		if (m_pAllocator && m_pMemory)
		{
			bool bIsTagOk = check_tag();
			if (!bIsTagOk)
			{
				// Breakpoints here can be useful
				std::cout << "Tag check fail!" << std::endl;
			}
			REQUIRE(bIsTagOk);
			m_pAllocator->release(m_pMemory);
			m_pAllocator = nullptr;
			m_pMemory = nullptr;
			m_size = 0;
			m_align = 0;
		}
	}
	// splat the entire block with random data
	// if the allocations overlap then this will splat over other tags.
	void write_random()
	{
		auto p = static_cast<uint8_t*>(m_pMemory);
		for (size_t i = 0; i < m_size; ++i)
		{
			p[i] = (uint8_t)rand() % 256;
		}
		// replace the tag
		set_tag(m_tag);
	}
	// zero the entire block
	void write_zero()
	{
		auto p = static_cast<uint8_t*>(m_pMemory);
		for (size_t i = 0; i < m_size; ++i)
		{
			p[i] = 0;
		}
		// replace the tag
		set_tag(m_tag);
	}
	~AllocHolder()
	{
		// release must be explicit.
		// This allocs non-releasing tests on temporary allocations.
	}

private:
	IMemoryAllocator* m_pAllocator;
	void* m_pMemory;
	size_t m_size;
	size_t m_align;
	uint32_t m_tag;
};

// Test class for allocating in batches.
// Options for releasing in different orders.
class MultiAllocTester
{
public:
	MultiAllocTester(uint32_t kMaxAllocs)
		: m_allocs(kMaxAllocs)
		, m_kMaxAllocs(kMaxAllocs)
		, m_numAllocs(0)
	{

	}

	void make_multiple_allocs(IMemoryAllocator* pAllocator, size_t kNumAllocs, size_t kAllocTestSize, size_t kAllocAlignment, bool bCheckAlign, MemoryMappingType mappingCheck)
	{
		for (uint32_t i = 0; i < kNumAllocs; ++i)
		{
			AllocHolder& alloc = next_alloc();
			alloc.allocate(pAllocator, kAllocTestSize, kAllocAlignment, i, bCheckAlign, mappingCheck);
			alloc.write_random();
		}
	}
	
	void make_random_allocs(IMemoryAllocator* pAllocator, size_t kNumAllocs, const std::vector<size_t>& sizes, const std::vector<size_t>& aligns, uint32_t seed, bool bCheckAlign, MemoryMappingType mappingCheck)
	{
		std::srand(seed);
		for (uint32_t i = 0; i < kNumAllocs; ++i)
		{
			AllocHolder& alloc = next_alloc();

			uint32_t sizeIndex = std::rand() % sizes.size();
			uint32_t alignIndex = std::rand() % aligns.size();

			alloc.allocate(pAllocator, sizes.at(sizeIndex), aligns.at(alignIndex), i, bCheckAlign, mappingCheck);
			alloc.write_random();
		}
	}

	AllocHolder& next_alloc() {
		SHU_ASSERT(m_numAllocs < m_kMaxAllocs);
		return  m_allocs[m_numAllocs++];
	}

	void release_reverse_order()
	{
		for (int i = (int)m_numAllocs-1; i >= 0; --i)
		{
			m_allocs[i].release();
		}
		m_numAllocs = 0;
	}

	void release_forward_order()
	{
		for (uint32_t i = 0; i < m_numAllocs; ++i)
		{
			m_allocs[i].release();
		}
		m_numAllocs = 0;
	}

	void release_random_order(uint32_t seed)
	{
		// generate sequential indices in array
		std::vector<uint32_t> indices(m_numAllocs);
		for (uint32_t i = 0; i < m_numAllocs; ++i)
		{
			indices[i] = i;
		}

		// shuffle it according to seed.
		std::srand(seed);
		std::random_shuffle(indices.begin(), indices.end());

		// release in shuffled order.
		for (auto i : indices)
		{
			m_allocs[i].release();
		}
		m_numAllocs = 0;
	}

	void check_all_tags() {
		
		bool checksPassed = true;
		for (uint32_t i = 0; i < m_numAllocs; ++i)
		{
			if( ! m_allocs[i].check_tag() )
			{
				checksPassed = false;
				break;
			}
		}
		REQUIRE(checksPassed);
	}

	void clear() { m_numAllocs = 0; }
private:
	std::vector<AllocHolder> m_allocs;
	uint32_t m_kMaxAllocs;
	uint32_t m_numAllocs;
};


// In this test we have 3 sets of allocations.
// These are collections of small allocations per frame.
// They must be valid for a small number of frames but they are never released.
// This simulates something like a command buffer.
void multi_frame_allocation_test_helper(const size_t kMaxAllocs, IMemoryAllocator* pAllocator, bool bCheckAlign, MemoryMappingType mappingCheck)
{
	constexpr size_t kMaxFrames = 32; // Number of frames to test.
	constexpr size_t kBuffers = 3; // Number of frames each allocations is active for.

	MultiAllocTester testers[kBuffers] = {
		MultiAllocTester(kMaxAllocs),
		MultiAllocTester(kMaxAllocs),
		MultiAllocTester(kMaxAllocs)
	};

	std::vector<size_t> sizes{ 4,8,12,16,32,64 };
	std::vector<size_t> aligns{ 4, 8, 16, 32 };

	int idx = 0; // buffer index, e.g. double, triple buffering scheme.

	for (int i = 0; i < kMaxFrames; ++i)
	{
		int allocIdx = (idx + (kBuffers - 1)) % kBuffers; // begins at 2
		// make nex allocations
		testers[allocIdx].make_random_allocs(pAllocator, kMaxAllocs, sizes, aligns, 2345, bCheckAlign, mappingCheck);

		// check old ones and clear
		testers[idx].check_all_tags();
		testers[idx].clear();

		// signal the next frame.
		g_pTestHarness->signal(GameEventType::kEventNextFrame);

		idx = (idx + 1) % kBuffers; // next buffer.
	}
}

TEST_CASE("Test framework sanity checks.", "[sanity]")
{
	REQUIRE(1 == 1);
	REQUIRE_FALSE(1 == 2);
}

TEST_CASE("System Block Allocator", "[internal]")
{
	// Tests our internal system block allocators 
	// If these fail then it's possible you used too many system blocks.
	constexpr size_t kBlockTestSize = 1024 * 64;
	
	for (uint32_t i = 0; i < (uint32_t)MemoryMappingType::kMaxTypes; ++i)
	{
		MemoryMappingType mapType = (MemoryMappingType)i;
		void* pBlock = allocate_system_block(kBlockTestSize, mapType);
		REQUIRE(pBlock != nullptr);

		REQUIRE(is_within_mapped_block(pBlock, mapType));
		REQUIRE_FALSE(is_within_mapped_block(static_cast<uint8_t*>(pBlock) - 1, mapType));
		REQUIRE_FALSE(is_within_mapped_block(static_cast<uint8_t*>(pBlock) + kBlockTestSize, mapType));

		release_system_block(pBlock);
	}
}

TEST_CASE("GeneralHeap: Allocation (Without Alignment Check or mapping)", "[Easy]")
{
	constexpr size_t kMaxAllocs = 1024;
	MultiAllocTester tester(kMaxAllocs);
	tester.make_multiple_allocs(get_allocators().GeneralHeap, 1024, 1024, 16, false, MemoryMappingType::kUndefined);
	tester.release_reverse_order();
}

TEST_CASE("GeneralHeap: Allocations (With Alignment Check, without mapping)", "[Easy]")
{
	constexpr size_t kMaxAllocs = 1024 * 4;
	MultiAllocTester tester(kMaxAllocs);
	tester.make_multiple_allocs(get_allocators().GeneralHeap, 1024, 1024, 256, true, MemoryMappingType::kUndefined);
	tester.make_multiple_allocs(get_allocators().GeneralHeap, 1024, 64, 16, true, MemoryMappingType::kUndefined);
	tester.make_multiple_allocs(get_allocators().GeneralHeap, 1024, 256, 64, true, MemoryMappingType::kUndefined);
	tester.make_multiple_allocs(get_allocators().GeneralHeap, 1024, 128, 32, true, MemoryMappingType::kUndefined);
	tester.release_reverse_order();
}

TEST_CASE("SmallObject: Multiple Small Allocations of varied size (Without Alignment Check)", "[Medium]")
{
	constexpr size_t kMaxAllocs = 4096;
	MultiAllocTester tester1(kMaxAllocs);
	MultiAllocTester tester2(kMaxAllocs);

	std::vector<size_t> sizes{ 4,8,12,16,32,64 };
	std::vector<size_t> aligns{ 16 };

	tester1.make_random_allocs(get_allocators().SmallObject, kMaxAllocs, sizes, aligns, 1234, false, MemoryMappingType::kUndefined);

	for(int i = 0; i < 16; ++i)
	{
		tester2.make_random_allocs(get_allocators().SmallObject, kMaxAllocs, sizes, aligns, 5678, false, MemoryMappingType::kUndefined);
		tester2.release_random_order(4567);
	}

	tester1.release_random_order(3275);
}

// Remember that these systems don't interact with each other. They don't run in parallel.
// NOTE:  They never release their memory but once the test has finished the memory is never used again.
// Without a custom allocator, they will cause the windows memory debugger to complain.
TEST_CASE("ScratchSpace: Allocation System 1 (With Alignment Check)", "[Medium]")
{
	g_pTestHarness->signal(GameEventType::kEventFlushScratchSpace);

	constexpr size_t kMaxAllocs = 4;
	MultiAllocTester tester(kMaxAllocs);
	tester.make_multiple_allocs(get_allocators().ScratchSpace, kMaxAllocs, 4 * MB, 16, true, MemoryMappingType::kUndefined);
}

TEST_CASE("ScratchSpace: Allocation System 2 (With Alignment Check)", "[Medium]")
{
	g_pTestHarness->signal(GameEventType::kEventFlushScratchSpace);

	constexpr size_t kMaxAllocs = 1;
	MultiAllocTester tester(kMaxAllocs);
	tester.make_multiple_allocs(get_allocators().ScratchSpace, kMaxAllocs, 32 * MB, 16, true, MemoryMappingType::kUndefined);
}

TEST_CASE("ScratchSpace: Allocation System 3 (With Alignment Check)", "[Medium]")
{
	g_pTestHarness->signal(GameEventType::kEventFlushScratchSpace);

	constexpr size_t kMaxAllocs = 2;
	MultiAllocTester tester(kMaxAllocs);
	tester.make_multiple_allocs(get_allocators().ScratchSpace, kMaxAllocs, 16 * MB, 16, true, MemoryMappingType::kUndefined);
}


// In these tests you are required to subdivide system memory blocks.
TEST_CASE("SingleFrameCPU: Rapid short lived allocations (no release) (With Alignment Check)", "[Medium]")
{
	constexpr size_t kMaxAllocs = 1024;
	multi_frame_allocation_test_helper(kMaxAllocs, get_allocators().SingleFrameCPU, true, MemoryMappingType::kCPU);
}

TEST_CASE("SingleFrameGPU: Rapid short lived allocations (no release) (With Alignment Check)", "[Medium]")
{
	constexpr size_t kMaxAllocs = 1024;
	multi_frame_allocation_test_helper(kMaxAllocs, get_allocators().SingleFrameGPU, true, MemoryMappingType::kGPU);
}

// This test tries to allocate as if we are loading a level.
TEST_CASE("Level Loading Test 1", "[Medium]")
{

	constexpr size_t kMaxAllocs = 128;
	MultiAllocTester menuSystemAllocs(kMaxAllocs);
	MultiAllocTester levelAllocs(kMaxAllocs);


	/////////////////////////////////////////////////////////////////////
	// INIT Game loading the menu
	/////////////////////////////////////////////////////////////////////

	g_pTestHarness->signal(GameEventType::kEventGameInit);

	// load config files...
	menuSystemAllocs.make_multiple_allocs(get_allocators().LevelCPU, 5, 2 * KB, 16, true, MemoryMappingType::kCPU);
	menuSystemAllocs.make_multiple_allocs(get_allocators().LevelGPU, 4, 2 * MB, 16, true, MemoryMappingType::kGPU);
	
	/////////////////////////////////////////////////////////////////////
	// LOAD LEVEL 1 discard data from level 1 but keeping system data.
	/////////////////////////////////////////////////////////////////////

	g_pTestHarness->signal(GameEventType::kEventLevelBeginLoad);

	// load textures.
	levelAllocs.make_multiple_allocs(get_allocators().LevelGPU, 15, 8 * MB, 16, true, MemoryMappingType::kGPU);

	// load models.
	levelAllocs.make_multiple_allocs(get_allocators().LevelGPU, 21, 1 * MB, 16, true, MemoryMappingType::kGPU);

	g_pTestHarness->signal(GameEventType::kEventLevelLoadComplete);

	/////////////////////////////////////////////////////////////////////
	// Discard Level 1 but keeping system
	/////////////////////////////////////////////////////////////////////

	g_pTestHarness->signal(GameEventType::kEventLevelUnload);

	// Run tests to check our system data is still valid.
	menuSystemAllocs.check_all_tags();

	/////////////////////////////////////////////////////////////////////
	// LOAD LEVEL 2 discarding data from level 1 but keeping system data.
	/////////////////////////////////////////////////////////////////////

	g_pTestHarness->signal(GameEventType::kEventLevelBeginLoad);

	// load textures.
	levelAllocs.make_multiple_allocs(get_allocators().LevelGPU, 15, 8 * MB, 16, true, MemoryMappingType::kGPU);

	// load models.
	levelAllocs.make_multiple_allocs(get_allocators().LevelGPU, 23, 1 * MB, 16, true, MemoryMappingType::kGPU);

	g_pTestHarness->signal(GameEventType::kEventLevelLoadComplete);


	/////////////////////////////////////////////////////////////////////
	// Discard Level 2 but keeping system
	/////////////////////////////////////////////////////////////////////

	g_pTestHarness->signal(GameEventType::kEventLevelUnload);

	// Run tests to check our system data is still valid.
	menuSystemAllocs.check_all_tags();


	/////////////////////////////////////////////////////////////////////
	// Signal shutdown
	/////////////////////////////////////////////////////////////////////

	g_pTestHarness->signal(GameEventType::kEventGameShutdown);
}