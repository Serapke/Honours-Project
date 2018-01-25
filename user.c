#include <stdio.h>
#include <stdlib.h>

int c = 3;

int main() {
  c = 2;
//  int i = 5;
//  int j = 2;
  int* r;
  int* p = malloc(2*sizeof(int));
//  printf("\n%i\n\n", *p);
//  *p = c;
  int i = 4;
  r = p;
  *r = 3;
  *(p+1) = 2;
//  printf("\n%i\n\n", *p);
//  j = 4;
//  i = j + 5;
//  for (int i = 0; i < 20; i++) {
//    a[i] = a[i * 4] + 5;
//    if (i > 3)
//      break;
//  }
  return 0;
}

