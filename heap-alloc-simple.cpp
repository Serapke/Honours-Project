#include <iostream>
#include <atomic>
#include "functions.h"

using namespace std;

atomic<int> bt_break;

void* my_malloc(size_t size) {
  if (DEBUG) {
    cout << "\tMalloc simple:" << endl;
  }
  int old = bt_break;
  bt_break += size;
  return (void*) old;
//  return (void*) (atomic_increment("bt_break", size)-size);
}

void my_free(void* ptr) {

}

void* my_realloc(void* ptr, size_t size) {
  return my_malloc(size);
}

void* my_calloc(size_t num, size_t size) {
  return my_malloc(num * size);
}

