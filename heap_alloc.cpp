#include <mutex>
#include <unistd.h>
#include <iostream>
#include <map>
#include "functions.h"

#define align4(x) (((((x)-1)>>2)<<2)+4)

using namespace std;

struct block {
  size_t size;
  struct block* next;
  struct block* prev;
  void *addr;        // address to block on Bigtable
  int free;
};
typedef struct block* p_block;
#define BLOCK_SIZE sizeof(struct block)
#define MIN_ALLOC_SIZE 32

p_block head = NULL;
map<void*, p_block> lookup_table;

uintptr_t current_bt_break;
uintptr_t initial_bt_break;

extern mutex mtx;

/*
 * Increments Bigtable break by the provided amount.
 * When called first time, sets the current break of Bigtable to the local heap break.
 * If incr = 0, returns the current break of Bigtable.
 */
void* set_bt_brk(int incr) {
  // if uninitialised, set to sbrk(0)
  if (current_bt_break == 0) {
    current_bt_break = (uintptr_t) sbrk(0);
    initial_bt_break = current_bt_break;
  }
  uintptr_t old_break = current_bt_break;
  current_bt_break += incr;
  return (void*) old_break;
}

unsigned long long getHeapBegin() {
  return static_cast<int>(initial_bt_break);
}

unsigned long long getHeapEnd() {
  return static_cast<int>(current_bt_break);
}

/*
 * find_block returns a pointer to a fitting block, or NULL if none where found.
 * After the execution, the argument 'last' points to the last visited block.
 */
p_block find_block(p_block *last, size_t size) {
  p_block b = head;
  while (b && !(b->free && b->size >= size)) {
    *last = b;
    b = b->next;
  }
  return b;
}

p_block extend_heap(p_block last, size_t s) {
//  cout << "Extend heap: (" << (void*) last << ", " << s << ")" << endl;
  void* sb;
  void* b;
  b = set_bt_brk(0);        // get the address of the current bigtable break
  sb = set_bt_brk(s);                 // increment bigtable break
  if (sb == (void*) -1) {
    return NULL;
  }
  p_block new_block = (p_block) malloc(BLOCK_SIZE);
  new_block->size = s;
  new_block->addr = b;
  new_block->next = NULL;
  new_block->prev = last;
  new_block->free = 0;
  if (last) {
    last->next = new_block;
  }
  lookup_table.insert({new_block->addr, new_block});
  return new_block;
}

/*
 * split_block cuts the block passed as argument to make data block of the wanted size.
 * Assumes 's' is already aligned.
 */
void split_block(p_block b, size_t s) {
  p_block new_block = (p_block) malloc(BLOCK_SIZE);
  new_block->size = b->size - s;
  new_block->next = b->next;
  new_block->prev = b;
  new_block->free = 1;
  new_block->addr = (void*) ((char*) b->addr + s);
  b->size = s;
  b->next = new_block;
  if (new_block->next) {
    new_block->next->prev = new_block;
  }
  lookup_table.insert({new_block->addr, new_block});
}

/*
 * fusion merges a block and its successor.
 */
p_block fusion(p_block b) {
  if (b->next && b->next->free) {
    p_block next_block = b->next;
    b->size += next_block->size;
    b->next = next_block->next;
    if (b->next) {
      b->next->prev = b;
    }
    auto it = lookup_table.find(next_block->addr);
    if (it != lookup_table.end()) {
      lookup_table.erase(it);
    }
    free(next_block);
  }
  return b;
}

/*
 * get_block gets a block from a Bigtable address
 */
p_block get_block(void *p) {
  auto it = lookup_table.find(p);
  if (it != lookup_table.end()) {
    return it->second;
  }
  return NULL;
}

/* Valid address for free */
bool valid_heap_addr(void* p) {
//  cout << "valid heap addr" << endl;
  if (head) {
    if (p >= head->addr && p < set_bt_brk(0)) {
      p_block b = get_block(p);
      return p == (b == NULL ? NULL : b->addr);
    }
    return false;
  }
  return false;
}

void copy_block(p_block src, p_block dst) {
  int *sdata, *ddata;
  int value;
  size_t i;
  sdata = (int*) src->addr;
  ddata = (int*) dst->addr;
  for (i = 0; i * 4 < src->size && i * 4 < dst->size; i++) {
    unsigned long long* a = (unsigned long long*) &sdata[i];
    unsigned long long b = (unsigned long long) a;
    value = get(b);
    a = (unsigned long long*) &ddata[i];
    b = (unsigned long long) a;
    put(b, value);
  }
}

void* my_malloc(size_t size) {
  p_block b, last;
  size_t s;
  s = align4(size);
  mtx.lock();
  if (head) {
    /* First find a block */
    last = head;
    b = find_block(&last, s);
    if (b) {
      /* Can we split */
      if ((b->size - s) >= MIN_ALLOC_SIZE) {
        split_block(b, s);
      }
      b->free = 0;
    } else {
      /* No fitting block, extend the heap */
      b = extend_heap(last, s);
      if (!b) {
        mtx.unlock();
        return NULL;
      }
    }
  } else {
    /* First time */
    b = extend_heap(NULL, s);
    if (!b) {
      mtx.unlock();
      return NULL;
    }
    head = b;
  }
  if (DEBUG)
    cout << "\tmalloc returned address: " << b->addr << endl;
  mtx.unlock();
  return b->addr;
}

void my_free(void* ptr) {
  if (DEBUG)
    cout << "\tfree called with: " << ptr << endl;
  p_block b;
  mtx.lock();
  if (valid_heap_addr(ptr)) {
    b = get_block(ptr);
    b->free = 1;
    /* Fusion with previous if possible */
    if (b->prev && b->prev->free) {
      b = fusion(b->prev);
    }
    /* Then fusion with next */
    if (b->next) {
      fusion(b);
    } else {
      /* Free the end of the heap */
      if (b->prev) {
        b->prev->next = NULL;
      } else {
        /* No more blocks */
        head = NULL;
      }
      set_bt_brk(-b->size);
      auto it = lookup_table.find(b->addr);
      if (it != lookup_table.end()) {
        lookup_table.erase(it);
      }
      free(b);
    }
  }
  mtx.unlock();
}

void* my_realloc(void* ptr, size_t size) {
  size_t s;
  p_block b, new_block;
  void *newp;
  if (!ptr) {
    return my_malloc(size);
  }
  mtx.lock();
  if (valid_heap_addr(ptr)) {
    s = align4(size);
    b = get_block(ptr);
    if (b->size >= s) {
      if (b->size - s >= MIN_ALLOC_SIZE) {
        split_block(b, s);
      }
    } else {
      /* Try fusion with next block if possible */
      if (b->next && b->next->free && (b->size + b->next->size) >= s) {
        fusion(b);
        if (b->size - s >= MIN_ALLOC_SIZE)
          split_block(b, s);
      } else {
        /* Realloc with a new block */
        newp = my_malloc(s);
        if (!newp) {
          mtx.unlock();
          return NULL;
        }
        new_block = get_block(newp);
        copy_block(b, new_block);
        my_free(ptr);
        mtx.unlock();
        return newp;
      }
    }
    if (DEBUG)
      cout << "\trealloc returned address: " << b->addr << endl;
    mtx.unlock();
    return ptr;
  }
  mtx.unlock();
  return NULL;
}

void* my_calloc(size_t num, size_t size) {
  int* new_block;
  size_t s4, i;
  new_block = (int*) my_malloc(num * size);
  if (new_block) {
    s4 = align4(num * size) >> 2;
    for (i = 0; i < s4; i++) {
      unsigned long long* a = (unsigned long long*) &new_block[i];
      unsigned long long b = (unsigned long long) a;
      put(b, 0ULL);
    }
  }
  return new_block;
}

