
#include <dart.h>

#include "dart_malloc.h"
#include "dart_memarea.h"
#include "dart_mempool.h"
#include "dart_teams_impl.h"
#include "shmem_logger.h"


/* TO IMPLEMENT */
/*
dart_ret_t dart_gptr_getaddr(const dart_gptr_t gptr, void *addr);
dart_ret_t dart_gptr_setaddr(dart_gptr_t *gptr, void *addr);
dart_ret_t dart_gptr_setunit(dart_gptr_t *gptr, dart_unit_t);

dart_ret_t dart_memfree(dart_gptr_t gptr);

dart_ret_t dart_team_memalloc_aligned(dart_team_t teamid, 
				      size_t nbytes, dart_gptr_t *gptr);
dart_ret_t dart_team_memfree(dart_team_t teamid, dart_gptr_t gptr);
*/


dart_ret_t dart_gptr_getaddr(const dart_gptr_t gptr, void **addr)
{
  dart_mempoolptr pool;
  int poolid;
  char *base;
  void *ptr; 

  poolid = gptr.segid;
  
  pool = dart_memarea_get_mempool_by_id(poolid);
  if(!pool ) 
    return DART_ERR_OTHER;

  base = (char*)(pool->base_addr);
  ptr = base + gptr.addr_or_offs.offset;
  (*addr) = ptr; 

  return DART_OK;
}


//
// unaligned allocation in the mempool of 
// DART_TEAM_All (to make the memory accessibly 
// by all units)
//
dart_ret_t dart_memalloc(size_t nbytes,
			 dart_gptr_t *gptr)
{
  dart_unit_t myid;
  dart_mempoolptr pool;
  dart_membucket bucket;
  int poolid;

  // 1. find the right pool id in DART_TEAM_ALL for the allocation
  // 2. get the mempool using this pool id
  // 3. perform the allocation
  // 4. set gptr members

  if( !gptr ) 
    return DART_ERR_INVAL;

  // todo: this is not always 0
  poolid = 0;
  pool = dart_memarea_get_mempool_by_id(poolid);
  
  if( !pool ) 
    return DART_ERR_OTHER;

  bucket = pool->bucket;
  if( !bucket ) 
    return DART_ERR_OTHER;
  
  void *addr;
  addr = dart_membucket_alloc(bucket, nbytes);
  if( addr==((void*)0) )
    {
      ERROR("Could not alloc memory in mempool%s", "");
      return DART_ERR_OTHER;
    }

  dart_myid(&myid);
  
  gptr->unitid  = myid;
  gptr->segid   = poolid;
  gptr->addr_or_offs.offset = 
    ((char*)addr)-((char*)pool->base_addr);

  return DART_OK;
}


