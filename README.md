# Chained Futures

Futures are a synchronization construct in concurrent programming that allows
one task to synchronize with the completion of another, asynchronous, task.

A future can be represented as an empty box tied to the completion of a task.
Once this task completes and produces a value, the box is filled with this
value. Futures are equipped with a blocking operation, traditionally called
`get`, that waits until a value is present in the box, and then returns it.
Filling the box is called "resolving" the future.

C++11 introduced futures under `std::future<T>` in the header `future`. This
implementation lacks a feature called "chaining".

In futures terminology, chaining refers to the action of attaching a
continuation to the resolution of a future. When the future is resolved with
a value `X`, the continuation attached to it is ran with the value `X` as parameter.
Attaching the continuation produces another future, that will be resolved with
the value produced by executing the continuation with `X`.

This repository proposes the construction `chained_future<T>`, a superset of
`std::future<T>` that is equipped with an operation called `chain`. This operation
takes as parameter a callable object (any object that supports `operator(T)`),
and returns a `chained_future<U>` that will be resolved with the value produced
by executing the callable once the initial `chained_future<T>` is resolved.

Just like `std::future<T>`s are created through `std::async`, a
`chained_future<T>` can be created through `chained_futures::async`, whose
semantics mirror those of `std::async`.

# Example

```cpp
#include <cstdio>
#include <chrono>
#include <thread>

#include "chained_future/chained_future.h"

int f(int a, int b) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    return a + b;
}

float g(int a) {
    return a * 2.5f;
}

int main() {
    /* Asynchronously execute f with parameters 12 and 13.
     * The result, once available, will be stored in f1 and accessible with
     * f1.get().
     */
    chained_future<int> f1 = chained_futures::async(&f, 12, 13);

    /* Execute function g with the result of asynchronous f(12, 13).
     * Result will be stored in f2 and accessible with f2.get().
     * g will not be called until f1 is resolved.
     */
    chained_future<float> f2 = f1.chain(g);

    /* prints "25 62.5\n". */
    printf("%d %f\n", f1.get(), f2.get());
    return 0;
}
```

Build with:

```sh
g++ -Wall -Wextra -std=c++17 -I /path/to/chained_future/include \ 
  -o <your_exec> <your_source>
```

# Dependencies

This project is self-contained and relies only on the C++ STL for threading
support.

# Motivation

Chaining on futures was motivated by my work on the
[REPENTOGON Launcher](https://github.com/TeamREPENTOGON/Launcher) where the
need for asynchronicity is primordial to prevent a GUI from freezing in some
contexts, such as downloads and execution of child processes. In addition,
being able to react to the completion of such asynchronous operations is
needed to provide information to the user, in a context where the only code
running is the user event loop of the grapĥical framework and the asynchronous
tasks. In such a scenario, access to the future is difficult, if not impossible,
and asynchronous reactions become invaluable in writing concise and clear code.

# Limitations and future work

This repository evolves as I find time to work on it. In this initial state,
there is no support for `std::launch::deferred` (which silently falls back
to `std::launch::async`). Asynchronous operations always create threads
instead of relying on a thread pool for better resource usage, and raw
mutexes are used instead of more lightweight synchronization primitives.
