#include <iostream>

//int c = 3;
int main() {
//  c = 2;
  int var;
  int* ptr;
  int** pptr;
  
  var = 3000;
  ptr = &var;
  pptr = &ptr;

  printf("\nValue of var = %d\n", var);
  printf("\nValue available at *ptr = %d\n", *ptr);
  printf("\nValue available at **ptr  = %d\n", **pptr);
//  int* p = (int*) malloc(2*sizeof(int));
//  printf("\n%i\n\n", *p);
//  *p = c;
//  int i = 4;
//  r = p;
//  *r = 3;
//  *(p+1) = 2;
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

