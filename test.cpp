#include <iostream>
#include <stdlib.h>
#include <chrono>
#include <pthread.h>
#include <semaphore.h>

using namespace std;
using namespace std::chrono;

const int M_X = 10;
const int M_Y = 10;
const int N = 50;
volatile int target;
int* data;
char *heap_matrix;
int N_PERFORM = 100;
sem_t lock;
sem_t empty, full;


void setup() {
  data = (int*) malloc(sizeof(int));
  heap_matrix = (char*) malloc(M_X * M_Y * sizeof(char));
}

void test_stack_allocation(char matrix[][M_Y]) {
  for (int i = 0; i < M_X; i++) {
    for (int j = 0; j < M_Y; j++) {
      matrix[i][j] = (i * M_Y + j) % 128;
    }
  }
}

char *test_heap_allocation() {
  for (int i = 0; i < M_X; i++) {
    for (int j = 0; j < M_Y; j++) {
      *(heap_matrix + i * M_Y + j) = (i * M_Y + j) % 128;
    }
  }
}

void *produce(void *arg) {
  for (int i = 1; i <= N; i++) {
    sem_wait(&empty);
    *data = i;
    sem_post(&full);
  }
  return NULL;
}

void *consume(void *arg) {
  int total = 0;
  for (int i = 0; i < N; i++) {
    sem_wait(&full);
    total += *data;
    sem_post(&empty);
  }
  return NULL;
}

bool test_stack_correctness(char matrix[][M_Y]) {
  for (int i = 0; i < M_X; i++) {
    for (int j = 0; j < M_Y; j++) {
      if (matrix[i][j] != (i * M_Y + j) % 128) {
        cout << i * M_Y + j << "  " << matrix[i][j] << endl;
        return false;
      }
    }
  }
  return true;
}

bool test_heap_correctness(char *matrix) {
  bool correct = true;
  for (int i = 0; i < M_X; i++) {
    for (int j = 0; j < M_Y; j++) {
      if (*(matrix + i * M_Y + j) != (i * M_Y + j) % 128) {
        correct = false;
      }
    }
  }
  free(matrix);
  return correct;
}

void *test_multithreaded_correctness(void *arg) {
  for (int i = 0; i < N; i++) {
    sem_wait(&lock);
    target++;
    sem_post(&lock);
  }
  return NULL;
}

int main() {
  char stack_matrix[M_X][M_Y];
  pthread_t thread[10];

  setup();

  // single-threaded performance
  auto start = steady_clock::now();
  for (int i = 0; i < N_PERFORM; i++) {
    test_stack_allocation(stack_matrix);
  }
  auto duration = duration_cast<microseconds>(steady_clock::now()-start);
  printf("%s on average took %ld (μs).\n", "test_stack_allocation", duration.count()/N_PERFORM);

  start = steady_clock::now();
  for (int i = 0; i < N_PERFORM; i++) {
    heap_matrix = test_heap_allocation();
  }
  duration = duration_cast<microseconds>(steady_clock::now()-start);
  printf("%s on average took %ld (μs).\n", "test_heap_allocation", duration.count()/N_PERFORM);

  // multi-threaded performance
  sem_init(&empty, 0, 1);
  sem_init(&full, 0, 0);
  start = steady_clock::now();
  for (int i = 0; i < N_PERFORM; i++) {
    pthread_create(&thread[0], NULL, produce, NULL);
    pthread_create(&thread[1], NULL, consume, NULL);
    pthread_join(thread[0], NULL);
    pthread_join(thread[1], NULL);
  }
  duration = duration_cast<microseconds>(steady_clock::now()-start);
  printf("%s on average took %ld (μs).\n", "producer-consumer", duration.count()/N_PERFORM);

  // single-threaded correctness
  bool result = test_stack_correctness(stack_matrix);
  printf("%s returned with a value: %s\n", "test_stack_correctness", result ? "true" : "false");

  result = test_heap_correctness(heap_matrix);
  printf("%s returned with a value: %s\n", "test_heap_correctness", result ? "true" : "false");

  // multi-threaded correctness
  target = 0;
  sem_init(&lock, 0, 1);
  for (int i = 0; i < 10; i++) {
    pthread_create(&thread[i], NULL, test_multithreaded_correctness, NULL);
  }
  for (int i = 0; i < 10; i++) {
    pthread_join(thread[i], NULL);
  }
  printf("Final counter value was %d (should be %d)\n", target, 10*N);
  return 0;
}

