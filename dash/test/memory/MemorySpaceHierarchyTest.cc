
#include <dash/allocator/SymmetricAllocator.h>
#include <dash/allocator/LocalAllocator.h>
#include <dash/allocator/EpochSynchronizedAllocator.h>


using namespace dash;

TEST_F(MemorySpaceHierarchyTest, SymmetricUnbalancedTest)
{
  using value_type  = int;

  using LocalAllocT = LocalAllocator<value_type>;
  using GlobMemT    = GlobStaticMem<value_type, LocalAllocT>;
  using GlobAllocT  = SymmetricAllocator<value_type>;

  GlobMemT   g_memspace(100);
  GlobAllocT g_alloc(&g_memspace);

}

TEST_F(MemorySpaceHierarchyTest, EpochSyncedUnbalancedTest)
{
  using value_type  = int;

  using LocalAllocT = LocalAllocator<value_type>;
  using GlobMemT    = GlobHeapMem<value_type, LocalAllocT>;
  using GlobAllocT  = EpochSynchronizedAllocator<value_type>;
}

TEST_F(MemorySpaceHierarchyTest, MultipleAllocatorTest)
{
  using value_type  = int;

  using LocalAllocT = LocalAllocator<value_type>;
  using GlobMemT    = GlobStaticMem<value_type, LocalAllocT>;
  using GlobAllocT  = SymmetricAllocator<value_type>;

  GlobMemT   g_memspace(100);
  GlobAllocT g_alloc_a(&g_memspace);
  GlobAllocT g_alloc_b(&g_memspace);

  auto g_mem_a = g_alloc_a.allocate(80);
  auto g_mem_b = g_alloc_b.allocate(20);
}
