#include <cassert>

#include <atomic>
#include <chrono>
#include <thread>

#include "chained_future/chained_future.h"

int f() {
  std::this_thread::sleep_for(std::chrono::seconds(2));
  return 12;
}

std::atomic<int> f_result = 0;
std::atomic<bool> ran_g = false;

void g(int result) {
  f_result.store(result, std::memory_order_release);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  ran_g.store(true, std::memory_order_release);
}

int h() {
  assert(ran_g.load(std::memory_order_acquire));
  return 25;
}

int main() {
  chained_future<int> f1 = chained_futures::async(f);
  chained_future<void> f2 = f1.chain(g);
  chained_future<int> f3 = f2.chain(h);

  assert(f1.get() == 12);
  f2.get();
  assert(ran_g.load(std::memory_order_acquire));
  assert(f_result.load(std::memory_order_acquire) == 12);
  assert(f3.get() == 25);

  return 0;
}
