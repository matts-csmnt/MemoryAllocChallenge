//////////////////////////////////////////////////////////////////////////
// DO NOT MODIFY THIS FILE.
//////////////////////////////////////////////////////////////////////////

#include "MemoryManagement.h"

#include <vector>

constexpr uint32_t kMaxSystemAllocations = 8;
constexpr size_t kSystemAllocAlignment = 256;

// Defines a "system" allocated memory block.
// These must be allocated by the "system" allocator functions.
// There are very few of these system blocks available so you must subdivide them.
struct SystemMemoryBlock
{
	void* m_pMemBlock;
	size_t m_size;
	MemoryMappingType m_type;
};

static SystemMemoryBlock g_systemAllocations[kMaxSystemAllocations];
static uint32_t g_systemAllocationCount = 0;

bool is_aligned(const void* ptr, const size_t kAlignment)
{
	SHU_ASSERT(ptr);
	uintptr_t pi(reinterpret_cast<uintptr_t>(ptr));
	return (pi & (kAlignment - 1)) == 0;
}

void* allocate_system_block(size_t size, MemoryMappingType mappingType)
{
	if (g_systemAllocationCount < kMaxSystemAllocations)
	{
		for (size_t i = 0; i < kMaxSystemAllocations; ++i)
		{
			auto& block(g_systemAllocations[i]);

			if(nullptr == block.m_pMemBlock)
			{
				++g_systemAllocationCount;
				block.m_pMemBlock = _aligned_malloc(size, kSystemAllocAlignment);
				block.m_size = size;
				block.m_type = mappingType;

				SHU_ASSERT(block.m_pMemBlock);
				SHU_ASSERT(is_aligned(block.m_pMemBlock, kSystemAllocAlignment));
				return block.m_pMemBlock;
			}
		}
	}
	return nullptr;
}

void release_system_block(void* ptr)
{
	SHU_ASSERT(ptr);
	for (size_t i = 0; i < kMaxSystemAllocations; ++i)
	{
		auto& block(g_systemAllocations[i]);
		if (block.m_pMemBlock == ptr)
		{
			_aligned_free(block.m_pMemBlock);
			block.m_pMemBlock = nullptr;
			block.m_size = 0;
			return;
		}
	}
	return;
}

bool is_within_mapped_block(const void* ptr, MemoryMappingType type)
{
	// check each block in turn:
	// is the pointer within a GPU mapped block.
	for (size_t i = 0; i < kMaxSystemAllocations; ++i)
	{
		auto& block(g_systemAllocations[i]);

		if (block.m_pMemBlock && block.m_type == type)
		{
			uintptr_t s = reinterpret_cast<uintptr_t>(block.m_pMemBlock);
			uintptr_t e = s + block.m_size;
			uintptr_t p = reinterpret_cast<uintptr_t>(ptr);

			if ((s <= p) && (p < e))
			{
				return true;
			}
		}
	}
	return false;
}

static MemoryAllocatorSet g_customAllocators;

void set_allocators(const MemoryAllocatorSet& allocatorSet)
{
	g_customAllocators = allocatorSet;
}

const MemoryAllocatorSet& get_allocators()
{
	return g_customAllocators;
}

