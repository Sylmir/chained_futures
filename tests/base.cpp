#include <cassert>
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

  assert(f1.get() == 25);
  assert(f2.get() == 50);
  assert(f3.get() == 50.0f);
  assert(f4.get() == 1);
  assert(f5.get() == false);
  assert(f6.get() == 25);
  return 0;
}
