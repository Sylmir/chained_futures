#include <cmath>
#include <cstdio>

#include <chrono>
#include <thread>

#include "chained_future/chained_future.h"

int f(int a, int b) {
  std::this_thread::sleep_for(std::chrono::seconds(2));
  return a + b;
}

int g(int a) {
  return a * 2;
}

float h(int a) {
  return std::ceil(a);
}

int main() {
  chained_future<int> f1 = chained_futures::async(&f, 12, 13);
  chained_future<int> f2 = f1.chain(std::move(g));
  chained_future<float> f3 = f2.chain(std::move(h));

  printf("%d %d %f\n", f1.get(), f2.get(), f3.get());
  return 0;
}
