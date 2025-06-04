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

struct functor {
  int operator()(bool x) {
    return x ? 50 : 25;
  }
};

int main() {
  chained_future<int> f1 = chained_futures::async(&f, 12, 13);
  chained_future<int> f2 = f1.chain(g);
  chained_future<float> f3 = f2.chain(std::move(h));
  std::function<int(float)> fn = [](float x) -> int {
      return x == 50.f ? 1 : 0;
  };
  chained_future<int> f4 = f3.chain(fn);
  chained_future<bool> f5 = f4.chain([](int x) -> bool {
      return x % 2 == 0;
  });
  chained_future<int> f6 = f5.chain(functor());

  printf("%d %d %f %d %d %d\n", f1.get(), f2.get(), f3.get(),
      f4.get(), f5.get(), f6.get());
  return 0;
}
