#include <stddef.h>

typedef unsigned char byte;

void* memcpy(void* dest, const void* src, size_t n) {
  // Typecast src and dest addresses to (char *)
  byte *csrc = (byte*) src;
  byte *cdest = (byte*) dest;

  // Copy contents of src[] to dest[]
  for (int i=0; i<n; i++) {
    cdest[i] = csrc[i];
  }
  return dest;
}

void* memset (void* ptr, int value, size_t n) {
  byte* p = (byte*) ptr;

  /*
   *  c should only be a byte's worth of information anyway, but let's mask out
   *  everything else just in case.
   *
   *  https://github.com/Smattr/memset/blob/master/memset.c
   */
  byte val = value & 0xff;

  while (n--) {
    *p++ = val;
  }
  return ptr;
}

void* memset (void* ptr, char value, size_t n) {
  return memset(ptr, (int) value, n);
}
