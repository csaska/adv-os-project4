#include "types.h"
#include "stat.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"

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

void
insert_after(struct mmregion *node, struct mmregion *new_node)
{
  if (node == NULL)
    panic("mmap: insert_after program error");

  new_node->rnext = node->rnext;
  node->rnext = new_node;
}

void*
find_region(struct proc *curproc, void *addr, int length)
{
  struct mmregion *curr = curproc->mmregion_head;
  while(curr) {
    if (curr->addr == addr && curr->length == length)
      return curr;
    curr = curr->rnext;
  }
  return 0;
}

struct mmregion*
find_free_region(struct proc *curproc, void *addr, int length)
{
  // TODO: what if mapping can fit in the middle of region?


  struct mmregion *curr = curproc->mmregion_head;
  struct mmregion *candidate = NULL;
  int candidate_delta = 0;
  while(curr) {
    if (curr->rfree && curr->rsize >= length) {
      // get the abs delta of curr address to addr hint
      int delta;
      if ((uint)curr->addr > (uint)addr)
        delta = (uint)curr->addr - (uint)addr;
      else
        delta = (uint)addr - (uint)curr->addr;

      // list in increasing order, will never find better candidate
      if (candidate && delta > candidate_delta)
        break;

      candidate = curr;
      candidate_delta = delta;

      // best possible candidate or hint wasn't given
      if (candidate_delta == 0 || addr == 0)
        break;
    }
    curr = curr->rnext;
  }
  return candidate;
}

struct mmregion*
create_region(struct proc *curproc, void *addr, int length)
{
  // Allocate kernel space for new mmapped region entry
  // TODO: check for errors
  // TODO: test_4 8000 allocation get's overwritten
  //  struct mmregion *node = (struct mmregion*)kmlloc(sizeof(struct mmregion));
  struct mmregion *new_region = (struct mmregion*)kalloc();

  // New mmregion starts at address allocated in the heap
  uint sz = curproc->sz;
  new_region->addr = (void*)sz;
  new_region->length = length;
  new_region->rnext = NULL;
  new_region->rfree = 0;
  new_region->rsize = PGROUNDUP(length);

  // Allocate user space
  if ((sz = allocuvm(curproc->pgdir, sz, sz + new_region->rsize)) == 0)
    return 0;
  curproc->sz = sz;

  // Add mmregion to proc's mmregion list in order
  struct mmregion *curr = curproc->mmregion_head;
  if (curr == NULL || (uint)curr->addr > (uint)new_region->addr) {
    curproc->mmregion_head = new_region;
  } else {
    while(curr->rnext != NULL && (uint)curr->rnext->addr < (uint)new_region->addr) {
        curr = curr->rnext;
    }
    insert_after(curr, new_region);
  }

  return new_region;
}

void
merge_free_regions(struct proc *curproc)
{

}

void*
mmap(void *addr, int length, int prot, int flags, int fd, int offset)
{
  if (length < 1)
    return 0;

  // TODO: if hit failure, undo free any allocations

  struct proc *curproc = myproc();

  // Try to re-use a freed, previously mmap'd region
  struct mmregion *free_region = find_free_region(curproc, addr, length);
  if (free_region) {
    // update region and return start address
    // TODO: does region need to be split into smaller pieces?
    // TODO: call map region
    return free_region->addr;
  }

  struct mmregion *new_region = create_region(curproc, addr, length);

  return new_region->addr;
}

int
munmap(void *addr, int length)
{
  struct proc *curproc = myproc();

  struct mmregion *mmregion = find_region(curproc, addr, length);
  if (mmregion == NULL)
    return -1;

  // clear data previously mapped to region
  memset(addr, 0, length);

  // clear region state
  mmregion->rfree = 1;

  merge_free_regions(curproc);

  return 0;

  // TODO: call deallocuvm and kfree to free data in vm.c
  //  deallocuvm(curproc->pgdir, (uint)addr + length, (uint)addr);
  //  kfree(currproc->);
}
