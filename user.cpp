#include <iostream>
#include <string.h>
#include <thread>

using namespace std;

struct new_type {
  int hello;
  int goodbye;
};

//void test_single_threaded_heap() {
//  int* heap_ptr = (int*) malloc(2*sizeof(int));
//  cout << "after my malloc" << endl;
//  cout << heap_ptr << endl;
//  heap_ptr[0] = 1;
//  cout << heap_ptr[0] << endl;
//  int* new_heap_ptr = (int*) realloc(heap_ptr, 3*sizeof(int));
//  cout << new_heap_ptr[0] << endl;
//  free(new_heap_ptr);
//
//  int* another_heap_ptr = (int*) calloc(2, sizeof(int));
//  cout << (another_heap_ptr[0] == 0) << endl;
//  cout << (another_heap_ptr[1] == 0) << endl;
//  free(another_heap_ptr);
//}

void thread_function1() {
  int* heap_ptr = (int*) malloc(2*sizeof(int));
  heap_ptr[0] = 1;
  cout << (heap_ptr[0] == 1) << endl;
  free(heap_ptr);
}

void thread_function2() {
  int* heap_ptr = (int*) calloc(2, sizeof(int));
  cout << (heap_ptr[0] == 0) << endl;
  free(heap_ptr);
}

void test_multi_threaded_heap() {
  thread t[20];

  for (int i = 0; i < 20; i++) {
    if (i % 2 == 0) {
      t[i] = thread(thread_function1);
    } else {
      t[i] = thread(thread_function2);
    }
  }

  for (int i = 0; i < 20; i++) {
    t[i].join();
  }
}

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

//  test_single_threaded_heap();
  test_multi_threaded_heap();


//  char str[] = "almost every programmer should know memset!";
//  memset (str,'-',6);
//  puts (str);

  return 0;
}





