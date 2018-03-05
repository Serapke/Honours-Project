#include <mutex>
#include <unistd.h>
#include <iostream>

#define align4(x) (((((x)-1)>>2)<<2)+4)
#define BLOCK_SIZE 20

using namespace std;

struct block {
  size_t size;
  struct block* next;
  struct block* prev;
  void *ptr;
  int free;
  char data[1];           // indicates the end of the meta-data
};
typedef struct block* p_block;

void *base = NULL;
std::mutex mtx;
uintptr_t current_break;

void* set_brk(int incr) {
  return sbrk(incr);
//  if (current_break == 0) {
//    current_break = (uintptr_t) sbrk(0);
//  }
//  uintptr_t old_break = current_break;
//  current_break += incr;
//  return (void*) old_break;
}

/*
 * find_block returns a pointer to a fitting block, or NULL if none where found.
 * After the execution, the argument 'last' points to the last visited block.
 */
p_block find_block(p_block *last, size_t size) {
  p_block b = (p_block) base;
  while (b && !(b->free && b->size >= size)) {
    *last = b;
    b = b->next;
  }
  return b;
}

p_block extend_heap(p_block last, size_t s) {
  cout << "Extend heap: (" << (void*) last << ", " << s << ")" << endl;
  void* sb;
  p_block b;
  b = (p_block) set_brk(0);
  sb = set_brk(BLOCK_SIZE + s);
  if (sb == (void*) -1) {
    return NULL;
  }
  b->size = s;
  b->next = NULL;
  b->prev = last;
  b->ptr = b->data;
  if (last) {
    last->next = b;
  }
  b->free = 0;
  return b;
}

/*
 * split_block cuts the block passed in argument to make data block of the wanted size.
 * Assumes 's' is already aligned.
 */
void split_block(p_block b, size_t s) {
  p_block new_block;
  new_block = (p_block ) b->data + s;
  new_block->size = b->size - s - BLOCK_SIZE;
  new_block->next = b->next;
  new_block->prev = b;
  new_block->free = 1;
  new_block->ptr = new_block->data;
  b->size = s;
  b->next = new_block;
  if (new_block->next) {
    new_block->next->prev = new_block;
  }
}

/*
 * fusion merges a block and its successor.
 */
p_block fusion(p_block b) {
  if (b->next && b->next->free) {
    b->size += BLOCK_SIZE + b->next->size;
    b->next = b->next->next;
    if (b->next) {
      b->next->prev = b;
    }
  }
  return b;
}

/*
 * get_block gets a block from a pointer
 */
p_block get_block(void *p) {
  char *tmp;
  tmp = (char*) p;
  tmp -= BLOCK_SIZE;
  return (p_block) tmp;
}

/* Valid address for free */
int valid_heap_addr(void* p) {
  if (base) {
    if (p > base && p < set_brk(0)) {
      return p == (get_block(p)->ptr);
    }
    return -1;
  }
  return -1;
}

void copy_block(p_block src, p_block dst) {
  int *sdata, *ddata;
  size_t i;
  sdata = (int*) src->ptr;
  ddata = (int*) dst->ptr;
  for (i = 0; i * 4 < src->size && i * 4 < dst->size; i++) {
    ddata[i] = sdata[i];
  }
}

void* my_malloc(size_t size) {
  printf("%d",size);
  return (void*) 0;
//  p_block b, last;
//  size_t s;
//  s = align4(size);
//  mtx.lock();
//  if (base) {
//    /* First find a block */
//    last = (p_block) base;
//    b = find_block(&last, s);
//    if (b) {
//      /* Can we split */
//      if ((b->size - s) >= (BLOCK_SIZE + 4)) {
//        split_block(b, s);
//      }
//      b->free = 0;
//    } else {
//      /* No fitting block, extend the heap */
//      b = extend_heap(last, s);
//      if (!b) {
//        mtx.unlock();
//        return NULL;
//      }
//    }
//  } else {
//    /* First time */
//    b = extend_heap(NULL, s);
//    if (!b) {
//      mtx.unlock();
//      return NULL;
//    }
//    base = b;
//  }
//  mtx.unlock();
//  return b->data;
}

//void free(void* ptr) {
//  p_block b;
//  mtx.lock();
//  if (valid_heap_addr(ptr)) {
//    b = get_block(ptr);
//    b->free = 1;
//    /* Fusion with previous if possible */
//    if (b->prev && b->prev->free) {
//      b = fusion(b->prev);
//    }
//    /* Then fusion with next */
//    if (b->next) {
//      fusion(b);
//    } else {
//      /* Free the end of the heap */
//      if (b->prev) {
//        b->prev->next = NULL;
//      } else {
//        /* No more blocks */
//        base = NULL;
//      }
//      sbrk(-(b->size + BLOCK_SIZE));
//    }
//  }
//  mtx.unlock();
//}
//
//void* realloc(void* ptr, size_t size) {
//  size_t s;
//  p_block b, new_block;
//  void *newp;
//  if (!ptr) {
//    return malloc(size);
//  }
//  mtx.lock();
//  if (valid_heap_addr(ptr)) {
//    s = align4(size);
//    b = get_block(ptr);
//    if (b->size >= s) {
//      if (b->size - s >= (BLOCK_SIZE + 4)) {
//        split_block(b, s);
//      }
//    } else {
//      /* Try fusion with next block if possible */
//      if (b->next && b->next->free && (b->size + BLOCK_SIZE + b->next->size) >= s) {
//        fusion(b);
//        if (b->size - s >= (BLOCK_SIZE + 4))
//          split_block(b, s);
//      } else {
//        /* Realloc with a new block */
//        newp = malloc(s);
//        if (!newp) {
//          mtx.unlock();
//          return NULL;
//        }
//        new_block = get_block(newp);
//        copy_block(b, new_block);
//        free(ptr);
//        mtx.unlock();
//        return newp;
//      }
//    }
//    mtx.unlock();
//    return ptr;
//  }
//  mtx.unlock();
//  return NULL;
//}
//
//void* calloc(size_t num, size_t size) {
//  int* new_block;
//  size_t s4, i;
//  new_block = (int*) malloc(num * size);
//  if (new_block) {
//    s4 = align4(num * size) << 2;
//    for (i = 0; i < s4; i++) {
//      new_block[i] = 0;
//    }
//  }
//  return new_block;
//}
