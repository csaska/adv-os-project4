#include "types.h"
#include "stat.h"
#include "defs.h"
#include "param.h"

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
    panic("kmalloc: requesing more than 4096 bytes");

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


void *mmap(void *addr, int length, int prot, int flags, int fd, int offset)
{
  return 0;
}

int munmap(void *addr, int length) {
  return 0;
}
