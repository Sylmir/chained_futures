/* Check that function parameters are properly decayed when passed to a
 * chained_future. We check array to pointer conversion and reference
 * wrapping.
 */

#include <cassert>

#include <atomic>
#include <utility>

#include "chained_future/chained_future.h"

int f(const char* s) {
  return printf("%s\n", s);
}

void g(int& ref) {
  ref += 2;
}

int main() {
  char buffer[] = "Hello World !";
  chained_future<int> f1 = chained_futures::async(f, buffer);
  assert(f1.get() == sizeof(buffer));

  int ref = 0;
  chained_future<void> f2 = chained_futures::async(g, std::ref(ref));
  f2.get();
  /* Memory is ordered due to the fact get() synchronizes-with async. */
  assert(ref == 2);
  return 0;
}
