#include <stdio.h>

void printLoad(int32_t* address, int32_t value) {
  printf("Load instruction:\n");
  printf("  address = %#010x", address);
  printf("  value = %i\n", value);
}

void printLoadAddress(int32_t** address, int32_t* value) {
  printf("Load (address) instruction:\n");
  printf("  address = %#010x", address);
  printf("  value = %#010x\n", value);
}

void printStore(int32_t* address, int32_t value) {
  printf("Store instruction:\n");
  printf("  address = %#010x", address);
  printf("  value = %i\n", value);
}

void printStoreAddress(int32_t** address, int32_t* value) {
  printf("Store (address) instruction:\n");
  printf("  address = %#010x", address);
  printf("  value = %#010x\n", value);
}