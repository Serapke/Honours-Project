#include <iostream>
#include <string.h>

using namespace std;

struct new_type {
  int hello;
  int goodbye;
};

int main() {
//  int var;
//  int* ptr;
//  int** pptr;
//
//  var = 3000;
//  ptr = &var;
//  pptr = &ptr;
//
//  printf("\nValue of var = %d\n", var);
//  printf("\nValue available at *ptr = %d\n", *ptr);
//  printf("\nValue available at **ptr  = %d\n", **pptr);

//  Struct pointers have to be cast to void pointers for loads and stores
//  new_type a;
//  a.hello = 1;
//  a.goodbye = 2;
//  void* p_a = (void*) &a;
//  void* p = p_a;

  int* heap_ptr = (int*) malloc(2*sizeof(int));
  cout << "after my malloc" << endl;
  cout << heap_ptr << endl;
  heap_ptr[0] = 1;
  cout << heap_ptr[0] << endl;
  int* new_heap_ptr = (int*) realloc(heap_ptr, 3*sizeof(int));
  cout << new_heap_ptr[0] << endl;
//  printf("\nAddress of heap_ptr = %p", heap_ptr);
//  printf("\nValue of heap_ptr = %d\n", heap_ptr[0]);
  free(new_heap_ptr);

  int* another_heap_ptr = (int*) calloc(2, sizeof(int));
  cout << (another_heap_ptr[0] == 0) << endl;
  cout << (another_heap_ptr[1] == 0) << endl;
  free(another_heap_ptr);


//  char str[] = "almost every programmer should know memset!";
//  memset (str,'-',6);
//  puts (str);

  return 0;
}



