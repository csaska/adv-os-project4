#include "types.h"
#include "stat.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "mman.h"

#define NULL 0

typedef long Align;

union header {
  struct {
    union header *ptr;
    uint size;
  } s;
  Align x;
};

typedef union header Header;

static Header base;
static Header *freep;

static Header*
morecore(uint nu)
{
  char *p;
  Header *hp;

  if(nu < 4096)
    nu = 4096;

  // replace sbrk with kalloc
  p = kalloc();

  if(p == (char*)-1)
    return 0;
  hp = (Header*)p;
  hp->s.size = nu;
  kmfree((void*)(hp + 1));
  return freep;
}

void*
kmalloc(uint nbytes)
{
  if (nbytes > 4096)
    panic("kmalloc: requesting more than 4096 bytes");

  Header *p, *prevp;
  uint nunits;

  nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
  if((prevp = freep) == 0){
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }
  for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
    if(p->s.size >= nunits){
      if(p->s.size == nunits)
        prevp->s.ptr = p->s.ptr;
      else {
        p->s.size -= nunits;
        p += p->s.size;
        p->s.size = nunits;
      }
      freep = prevp;
      return (void*)(p + 1);
    }
    if(p == freep)
      if((p = morecore(nunits)) == 0)
        return 0;
  }
}

void
kmfree(void *addr)
{
  Header *bp, *p;

  bp = (Header*)addr - 1;
  for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
    if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
      break;
  if(bp + bp->s.size == p->s.ptr){
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  } else
    bp->s.ptr = p->s.ptr;
  if(p + p->s.size == bp){
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  } else
    p->s.ptr = bp;
  freep = p;
}

int
has_next(struct mmregion *node)
{
  if (node->rnext)
    return 1;
  return 0;
}

struct mmregion*
next(struct mmregion *node)
{
  return node->rnext;
}

void
insert_after(struct mmregion *node, struct mmregion *new_node)
{
  if (node == NULL)
    panic("mmap: insert_after program error");

  new_node->rnext = node->rnext;
  node->rnext = new_node;
}

struct mmregion*
get_region(struct proc *curproc, void *addr, int length)
{
  struct mmregion *curr = curproc->mmregion_head;
  while(curr) {
    if (curr->addr == (uint)addr && curr->length == length)
      return curr;
    curr = next(curr);
  }
  return 0;
}

void*
find_best_fit(struct mmregion* node, void *addr, int length)
{
  uint lb = (uint)node->addr;
  uint ub = lb + node->rsize;

  if ((uint)addr <= lb)
    return (void*)lb;

  if ((uint)addr > ub)
    return (void*)(ub - length);

  if ((uint)addr + length <= ub)
    return addr;

  return (void*)(ub - length);
}


struct mmregion*
create_region(struct proc *curproc, void *addr, int length)
{
  // Allocate kernel space for new mmapped region entry
  // TODO: test_2 linked list entry doesn't persist when using kmalloc but works when using kalloc
  // TODO: test_5 hangs when using kmalloc, works with kalloc
  //  struct mmregion *new_region = (struct mmregion*)kmalloc(sizeof(struct mmregion));
  struct mmregion *new_region = (struct mmregion*)kalloc();

  new_region->rnext = NULL;
  new_region->rfree = 0;
  new_region->rsize = PGROUNDUP(length);
  new_region->rtype = MAP_ANONYMOUS;

  new_region->addr = (uint)addr;
  new_region->length = length;
  new_region->prot = 0;
  new_region->fd = -1;

  // Add mmregion to proc's mmregion list in order
  struct mmregion *curr = curproc->mmregion_head;
  if (curr == NULL || (uint)curr->addr > (uint)new_region->addr) {
    curproc->mmregion_head = new_region;
    if (curr != NULL) {
      curproc->mmregion_head->rnext = curr;
    }
  } else {
    while(has_next(curr) && (uint)next(curr)->addr < (uint)new_region->addr) {
        curr = curr->rnext;
    }
    insert_after(curr, new_region);
  }
  return new_region;
}

struct mmregion*
get_free_region(struct proc *curproc, void *addr, int length)
{
  struct mmregion *curr = curproc->mmregion_head;
  struct mmregion *candidate = NULL;
  int candidate_delta = 0;
  uint candidate_addr = 0;
  while(curr) {
    if (curr->rfree && curr->rsize >= length) {
      uint raddr = (uint)find_best_fit(curr, addr, PGROUNDUP(length));

      // get the abs delta of curr address to addr hint
      int delta;
      if (raddr > (uint)addr)
        delta = raddr - (uint)addr;
      else
        delta = (uint)addr - raddr;

      // list in increasing order, will never find better candidate
      if (candidate && delta > candidate_delta)
        break;

      candidate = curr;
      candidate_delta = delta;
      candidate_addr = raddr;

      // best possible candidate or hint wasn't given
      if (candidate_delta == 0 || addr == 0)
        break;
    }
    curr = next(curr);
  }

  if (candidate == 0)
    return 0;

  // check if we need to split candidate region into two regions
  if (candidate_addr != (uint)candidate->addr) {
    struct mmregion *new_region = create_region(curproc, (void*)candidate_addr, length);
    candidate->rsize -= PGROUNDUP(length);
    return new_region;
  }
  if (candidate->rsize - PGROUNDUP(length) >= PGSIZE) {
    candidate->rsize -= PGROUNDUP(length);
    candidate->addr += PGROUNDUP(length);
    struct mmregion *new_region = create_region(curproc, (void*)candidate_addr, length);
    return new_region;
  }
  candidate->rfree = 0;
  candidate->length = length;
  return candidate;
}

int
allocate_region_addr(struct mmregion* mmregion)
{
  struct proc *curproc = myproc();

  // Allocate user space
  uint oldsz = (uint)mmregion->addr;
  uint newsz = oldsz + mmregion->rsize;

  // We are now lazy loading page. Call allocuvm when processing page fault interrupt now instead in trap.c
  //  if (allocuvm(curproc->pgdir, oldsz, newsz) == 0)
  //    return -1;

  // Update curproc size
  if (newsz > curproc->sz)
    curproc->sz = newsz;

  return 0;
}

int
deallocate_region_addr(struct mmregion* mmregion)
{
  struct proc *curproc = myproc();

  uint lb = (uint)mmregion->addr;
  uint ub = lb + mmregion->rsize;

  // Deallocate user space
  if (deallocuvm(curproc->pgdir, ub, lb) == 0)
    return -1;

  // TODO: how much of the data structure should we deallocate?

  return 0;
}

void
merge_free_regions(struct proc *curproc)
{
  struct mmregion *curr = curproc->mmregion_head;
  while(curr) {
    if (curr->rfree && has_next(curr) && next(curr)->rfree) {
      struct mmregion* next_region = next(curr);

      curr->rsize += next_region->rsize;
      curr->rnext = next_region->rnext;

      // Deallocate now unused node in list
      kfree((char*)next_region);
    }
    curr = next(curr);
  }
}

void*
mmap(void *addr, int length, int prot, int flags, int fd, int offset)
{
  if (length < 1)
    return 0;

  if (flags == MAP_ANONYMOUS && fd != -1)
    return 0;

  if (flags == MAP_FILE && fd < 0)
    return 0;

  if (flags != 0 && flags != MAP_ANONYMOUS && flags != MAP_FILE)
    return 0;

  struct proc *curproc = myproc();

  struct mmregion *region = 0;
  if ((region = get_free_region(curproc, addr, length)) == 0) {
    // Create and allocate a new region
    region = create_region(curproc, (void*)curproc->sz, length);

    // Always start new region by growing address space(void*)curproc->sz;
    curproc->sz = curproc->sz + PGROUNDUP(length);
  }

  // Set flags, offset, and prot
  region->rtype = flags;
  region->offset = offset;
  region->prot = prot;

  // Set fd
  if (flags == MAP_FILE) {
    // TODO: check if fd corresponds to something other than an inode?
    if ((fd = fdalloc(curproc->ofile[fd])) < 0)
      return 0;
    filedup(curproc->ofile[fd]);
    region->fd = fd;
  }

  return (void*)region->addr;
}

int
munmap(void *addr, int length)
{
  struct proc *curproc = myproc();

  struct mmregion *region = get_region(curproc, addr, length);
  if (region == NULL)
    return -1;

  // clear data previously mapped to region one page at a time
  //  uint a = (uint)addr;
  //  for(; a < (uint)addr + length; a += PGSIZE) {
  //    pte_t *pte = walkpgdir(curproc->pgdir, (char*)a, 0);
  //    // address hasn't been used yet or hasn't been written
  //    if (pte == NULL)
  //      continue 0;

  //    // TODO: only write page if dirty bit is set

  //    memset(addr, 0, PGSIZE);
  //  }

  // deallocate physical memory so process can no longer access
  if (deallocate_region_addr(region) == -1)
    return -1;
  switchuvm(curproc);

  // close file if we opened one
  if (region->rtype == MAP_FILE) {
    fileclose(curproc->ofile[region->fd]);
    curproc->ofile[region->fd] = 0;
  }

  // clear region state
  region->rfree = 1;

  merge_free_regions(curproc);

  return 0;
}

int
msync(void *start_addr, int length)
{
  struct proc *curproc = myproc();

  struct mmregion *region = get_region(curproc, start_addr, length);
  if (region == NULL)
    return -1;

  uint a = (uint)start_addr;
  uint offset = region->offset;
  for(; a < (uint)start_addr + length;) {
    pte_t *pte = walkpgdir(curproc->pgdir, (char*)a, 0);
    // address hasn't been used yet or hasn't been written
    if (!pte || (*pte & PTE_D) == 0)
      continue;

    // write contents of page back to file
    if (fileseek(curproc->ofile[region->fd], offset) == -1)
      return -1;
    if (filewrite(curproc->ofile[region->fd], (char*)a, PGSIZE) == -1)
      return -1;

    a += PGSIZE;
    offset += PGSIZE;
  }
  return 0;
}