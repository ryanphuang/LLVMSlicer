#include <stdio.h>
int __ai_state_;
void compute(int n)
{
  int i = 1;
  int sum = 0;
  int product = 1;
  while (i < n) {
    sum = sum + i;
    product = product * i;
    i = i + 1;
  }
  printf("sum = %d\n", sum);
  printf("product = %d\n", product);
  __ai_state_ = 0;
}

int main()
{
  int n;
  scanf("%d", &n);
  compute(n);
  return 0;
}
