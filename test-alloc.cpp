#include <iostream>
#include <stdlib.h>
#include <chrono>
#include <pthread.h>
#include <semaphore.h>

using namespace std;
using namespace std::chrono;

const int N_THREADS = 10;
const int N_PERFORM = 100;
const int N = 50;
const int SIZE = 1024;    // 1 kB


void testA(int alloc_size) {
  char *mem[N];
  for (int i = 0; i < N; i++) {
    mem[i] = (char*) malloc(alloc_size);
  }
  for (int i = 0; i < N; i++) {
    free(mem[i]);
  }
}

void testB(int alloc_size) {
  char *mem[N];
  for (int i = 0; i < N; i++) {
    mem[i] = (char*) malloc((i+1)*alloc_size);
  }
  for (int i = 0; i < N; i++) {
    free(mem[i]);
  }
}

void testC(bool same_sized) {
  if (same_sized) {
    for (int k = 0; k < N / 5; k++) {
      testA(SIZE / 2);
    }
  } else {
    for (int k = 0; k < N / 5; k++) {
      testB(SIZE / 2);
    }
  }
}


void testD(bool same_sized) {
  char *mem[N];
  for (int i = 0; i < N; i++) {
    mem[i] = (char*) malloc(SIZE);
  }
  for (int i = 0; i < N; i++) {
    if (i % 2 == 0)
      free(mem[i]);
  }
  testC(same_sized);
  for (int i = 0; i < N; i++) {
    if (i % 2 != 0)
      free(mem[i]);
  }
}

void *testA_parallel(void* arg) {
  testA(SIZE);
  return NULL;
}

void *testB_parallel(void* arg) {
  testB(SIZE);
  return NULL;
}

int main() {
  pthread_t thread[N_THREADS];

  // Test A: allocate memory of a single size, then free

  auto start = steady_clock::now();
  for (int i = 0; i < N_PERFORM; i++) {
    testA(SIZE);
  }
  auto duration = duration_cast<microseconds>(steady_clock::now()-start);
  printf("%s took %ld (μs).\n", "Test A", duration.count()/N_PERFORM);

  // Test B: allocate memory of different sizes, then free

  start = steady_clock::now();
  for (int i = 0; i < N_PERFORM; i++) {
    testB(SIZE);
  }
  duration = duration_cast<microseconds>(steady_clock::now()-start);
  printf("%s took %ld (μs).\n", "Test B", duration.count()/N_PERFORM);

  // Test C: allocate, free and repeat (for single and different sized)

  start = steady_clock::now();
  for (int i = 0; i < N_PERFORM; i++) {
    testC(true);
  }
  duration = duration_cast<microseconds>(steady_clock::now()-start);
  printf("%s took %ld (μs).\n", "Test C (single size)", duration.count()/N_PERFORM);

  start = steady_clock::now();
  for (int i = 0; i < N_PERFORM; i++) {
    testC(false);
  }
  duration = duration_cast<microseconds>(steady_clock::now()-start);
  printf("%s took %ld (μs).\n", "Test C (diff size)", duration.count()/N_PERFORM);

  // Test D: allocate memory of different sizes, free half of it, then allocate and free in a loop

  start = steady_clock::now();
  for (int i = 0; i < N_PERFORM; i++) {
    testD(true);
  }
  duration = duration_cast<microseconds>(steady_clock::now()-start);
  printf("%s took %ld (μs).\n", "Test D (single size)", duration.count()/N_PERFORM);

  start = steady_clock::now();
  for (int i = 0; i < N_PERFORM; i++) {
    testD(false);
  }
  duration = duration_cast<microseconds>(steady_clock::now()-start);
  printf("%s took %ld (μs).\n", "Test D (diff size)", duration.count()/N_PERFORM);

  // Test E: two threads to allocate memory in parallel

  start = steady_clock::now();
  for (int i = 0; i < N_PERFORM; i++) {
    for (int i = 0; i < 2; i++) {
      pthread_create(&thread[i], NULL, testA_parallel, NULL);
    }
    for (int i = 0; i < 2; i++) {
      pthread_join(thread[i], NULL);
    }
  }
  duration = duration_cast<microseconds>(steady_clock::now()-start);
  printf("%s took %ld (μs).\n", "2 parallel threads test (same sized)", duration.count()/N_PERFORM);

  start = steady_clock::now();
  for (int i = 0; i < N_PERFORM; i++) {
    for (int i = 0; i < 2; i++) {
      pthread_create(&thread[i], NULL, testB_parallel, NULL);
    }
    for (int i = 0; i < 2; i++) {
      pthread_join(thread[i], NULL);
    }
  }
  duration = duration_cast<microseconds>(steady_clock::now()-start);
  printf("%s took %ld (μs).\n", "2 parallel threads test (different sized)", duration.count()/N_PERFORM);

  // Test F: many threads allocate memory in parallel

  start = steady_clock::now();
  for (int i = 0; i < N_PERFORM; i++) {
    for (int i = 0; i < N_THREADS; i++) {
      pthread_create(&thread[i], NULL, testA_parallel, NULL);
    }
    for (int i = 0; i < N_THREADS; i++) {
      pthread_join(thread[i], NULL);
    }
  }
  duration = duration_cast<microseconds>(steady_clock::now()-start);
  printf("%d %s took %ld (μs).\n", N_THREADS, "parallel threads test (same sized)", duration.count()/N_PERFORM);

  start = steady_clock::now();
  for (int i = 0; i < N_PERFORM; i++) {
    for (int i = 0; i < N_THREADS; i++) {
      pthread_create(&thread[i], NULL, testB_parallel, NULL);
    }
    for (int i = 0; i < N_THREADS; i++) {
      pthread_join(thread[i], NULL);
    }
  }
  duration = duration_cast<microseconds>(steady_clock::now()-start);
  printf("%d %s took %ld (μs).\n", N_THREADS, "parallel threads test (different sized)", duration.count()/N_PERFORM);

  return 0;
}
