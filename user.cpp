#include <iostream>
#include <string.h>
#include <thread>
#include <chrono>
#include <stdlib.h>

using namespace std;

struct new_type {
  int hello;
  int goodbye;
};

const int global_constant = 400;
int global_value = 500;
int *global_pointer = &global_value;


void test_globals() {
  int initial = global_value;
  global_value++;
  cout << initial+1 << " is equal to " << global_value << endl;
}

void test_struct() {
  new_type a;
  a.hello = 1;
  a.goodbye = 2;
  new_type* p_a = &a;
  cout << p_a->hello << endl;
}

void test_single_threaded_heap() {
  int var;
  int* ptr;
  int** pptr;

  var = 3000;

  ptr = (int*) malloc(sizeof(int));
  ptr[0] = 3001;
  pptr = &ptr;
  printf("\nValue of var = %d\n", var);
  printf("\nValue of heap variable = %d\n", ptr[0]);


  printf("\nValue available at *ptr = %d\n", *ptr);
  printf("\nValue available at **ptr  = %d\n", **pptr);
  free(ptr);
}

void print_args(int id, int id2) {
  cout << (id == 0 && id2 == 4) << endl;
}

void call_calloc() {
  int* heap_ptr = (int*) calloc(2, sizeof(int));
  cout << (heap_ptr[0] == 0) << endl;
  free(heap_ptr);
}

void test_one_thread() {
  thread t;
  int a = 4;
  t = thread(print_args, 0, a);
  t.join();
}

void test_one_thread_heap() {
  thread t;
  t = thread(call_calloc);
  t.join();
}

void test_multiple_threads(int n) {
  thread t[n];
  for (int i = 0; i < n; i++) {
    if (i % 2 == 0) {
      t[i] = thread(print_args, 0, 4);
    } else {
      t[i] = thread(call_calloc);
    }
  }
  for (int i = 0; i < n; i++) {
    t[i].join();
  }
}

void test_multi_threaded_heap() {
  test_one_thread();
  test_one_thread_heap();
  test_multiple_threads(5);
}

void test_mem_ops() {
  char str[] = "almost every programmer should know memset!";
  memset (str,'-',6);
  puts (str);
}

int main() {

//  test_globals();
//  test_struct();
//  test_single_threaded_heap();
//  test_multi_threaded_heap();

  return 0;
}
