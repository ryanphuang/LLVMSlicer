#include <stdio.h>
#include <stdlib.h>
void compute(int n, int *s, int *p)
{
  int j = 0;
  int *i = &j;
  int sum = 0;
  int product = 1;
  while (*i < n) {
    sum = sum + *i;
    product = product * *i;
    *i = *i + 1;
  }
  printf("sum = %d\n", sum);
  printf("product = %d\n", product);
  *s = sum;
  *p = product;
}

int main()
{
  int n;
  scanf("%d", &n);
  int s, p;
  compute(n, &s, &p);
  return 0;
}
