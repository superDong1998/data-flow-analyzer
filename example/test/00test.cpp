// #include <iostream>
#include <random>
#include <stdio.h>
#define N 10
#define M 10
int main() {
  int data[N][M] = {0};
  int data2[N] = {0};
  int result[N][M] = {0};
  int sum = 0;

  for (int i = 1; i < N; i++) {
    for (int j = 1; j < M; j++) {
      data[i][j] = data[i-1][j-1] + i + j;
    }
  }
  
  for (int i = 1; i < N; i++) {
    for (int j = 1; j < M; j++) {
      result[i][j] = data[i][j] * 2 + result[1][j-1];
    }
  }

  for (int k = 1; k < N; k++) {
    data2[k] = data2[k - 1] + k*3;
  }

  // return sum + data2[N-1];
  return sum + result[M-1][N-1];
  return sum + data[M-1][N-1];
  // for (int j = 1; j < N; j++) {
  //   data2[j] = data2[j - 1] + j * 3;
  //   sum += data2[j];
  // }

  // printf("sum is: %d\n", sum);
  // std::cout << "sum:" << sum << std::endl;
}