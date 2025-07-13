#include <cassert>

#include <atomic>
#include <chrono>
#include <thread>

#include "chained_future/chained_future.h"

class Foo {
  public:
    int f() {
      std::this_thread::sleep_for(std::chrono::seconds(2));
      return _a + _b;
    }

    void g() {
      std::this_thread::sleep_for(std::chrono::seconds(2));
      _c = 1;
    }

    inline int get_c() const {
      return _c;
    }

  private:
    int _a = 12, _b = 13;
    int _c = 0;
};

int f(int a) {
  return a + 2;
}

static std::atomic<bool> g_called = false;
void g(Foo* foo) {
  assert(foo->get_c() == 1);
  g_called.store(true, std::memory_order_release);
}

int main() {
  Foo foo;
  chained_future<int> f1 = chained_futures::async(&Foo::f, &foo);
  chained_future<int> f2 = f1.chain(f);

  chained_future<void> g1 = chained_futures::async(&Foo::g, &foo);
  chained_future<void> g2 = g1.chain(std::bind_front(g, &foo));

  assert(f2.get() == 27);
  g2.get();
  assert(g_called.load(std::memory_order_acquire));

  return 0;
}
