#include <stdio.h>
void compute(int n, int m)
{
  m++;
  int i = 1;
  int sum = 0;
  int product;
  if (m <= 0)
    product = 1;
  else
    product = m;
  while (i < n) {
    sum = sum + i;
    product = product * i;
    i = i + 1;
  }
  if (product > 100) {
    sum *= 2;
  }
  if (sum > 200) {
    product = 200;
  }
  sum = 100;
  printf("sum = %d\n", sum);
  printf("product = %d\n", product);
}

int main()
{
  int n, m;
  scanf("%d %d", &n, &m);
  compute(n, m);
  return 0;
}
