#include <iostream>
#include "functions.h"

using namespace std;

void* my_malloc(size_t size) {
  if (DEBUG) {
    cout << "\tMalloc simple:" << endl;
  }
  return (void*) (atomic_increment("bt_break", size)-size);
}

void my_free(void* ptr) {

}

void* my_realloc(void* ptr, size_t size) {
  return my_malloc(size);
}

void* my_calloc(size_t num, size_t size) {
  return my_malloc(num * size);
}
