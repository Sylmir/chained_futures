#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>

template<typename T>
class chained_future;

template<typename F, typename... Args>
using chained_future_res_t = chained_future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;

namespace chained_futures {
  template<typename F, typename... Args>
  chained_future_res_t<F, Args...> async(std::launch policy, F&& f,
      Args&&... args);

  template<typename F, typename... Args>
  chained_future_res_t<F, Args...> async(F&& f, Args&&... args);

  template<typename F, typename... Args>
  void launch(chained_future_res_t<F, Args...> future, F&& f, Args&&... args);
}

template<typename T>
class chained_future_callback_base {
public:
  virtual ~chained_future_callback_base() { }
  virtual void run(T t) = 0;
};

template<>
class chained_future_callback_base<void> {
public:
  virtual ~chained_future_callback_base() { }
  virtual void run() = 0;
};

template<typename F, typename T>
class chained_future_callback : public chained_future_callback_base<T> {
public:
  chained_future_callback(chained_future_res_t<F, T> future, F f) :
    _future(future), _f(f) {

  }

  void run(T t) final override {
    std::thread th(static_cast<void(&)(chained_future_res_t<F, T>, F&&, T&&)>(chained_futures::launch), _future, _f, t);
    th.detach();
  }

private:
  chained_future_res_t<F, T> _future;
  F _f;
};

template<typename F>
class chained_future_callback<F, void> : public chained_future_callback_base<void> {
public:
  chained_future_callback(chained_future_res_t<F> future, F f) :
    _future(future), _f(f) {

  }

  void run() final override {
    std::thread th(static_cast<void(&)(chained_future_res_t<F>, F&&)>(chained_futures::launch), _future, _f);
    th.detach();
  }

private:
  chained_future_res_t<F> _future;
  F _f;
};

template<typename T>
class chained_future_state {
public:
  ~chained_future_state() {
    for (chained_future_callback_base<T>* callback: _callbacks) {
      delete callback;
    }
  }

  void resolve(T const& t) {
    _value = t;
    set_resolved();
  }

  void resolve(T&& t) {
    _value = std::move(t);
    set_resolved();
  }

  template<typename F>
  chained_future_res_t<F, T> chain(F f) {
    chained_future_res_t<F, T> future;
    _callbacks.push_back(new chained_future_callback<F, T>(future, f));
    run_callbacks();
    return future;
  }

  T& get() {
    std::unique_lock<std::mutex> lck(_m);
    while (!_ready) {
      _cv.wait(lck);
    }

    return _value;
  }

private:
  T _value;
  std::vector<chained_future_callback_base<T>*> _callbacks;

  void run_callbacks() {
    std::unique_lock<std::mutex> lck(_m);
    if (!_ready) {
      return;
    }

    for (chained_future_callback_base<T>* callback: _callbacks) {
      callback->run(_value);
    }

    _callbacks.clear();
  }

  void set_resolved() {
    _m.lock();
    _ready = true;
    _cv.notify_all();
    _m.unlock();

    run_callbacks();
  }

  std::mutex _m;
  std::condition_variable _cv;
  bool _ready = false;
};

template<>
class chained_future_state<void> {
public:
  ~chained_future_state() {
    for (chained_future_callback_base<void>* callback: _callbacks) {
      delete callback;
    }
  }

  void resolve() {
    set_resolved();
  }

  template<typename F>
  chained_future_res_t<F> chain(F f) {
    chained_future_res_t<F> future;
    _callbacks.push_back(new chained_future_callback<F, void>(future, f));
    run_callbacks();
    return future;
  }

  void get() {
    std::unique_lock<std::mutex> lck(_m);
    while (!_ready) {
      _cv.wait(lck);
    }
  }

private:
  std::vector<chained_future_callback_base<void>*> _callbacks;

  void run_callbacks() {
    std::unique_lock<std::mutex> lck(_m);
    if (!_ready) {
      return;
    }

    for (chained_future_callback_base<void>* callback: _callbacks) {
      callback->run();
    }

    _callbacks.clear();
  }

  void set_resolved() {
    _m.lock();
    _ready = true;
    _cv.notify_all();
    _m.unlock();

    run_callbacks();
  }

  std::mutex _m;
  std::condition_variable _cv;
  bool _ready = false;
};


namespace chained_futures {
  template<typename F, typename... Args>
  chained_future_res_t<F, Args...> async(std::launch policy, F&& f,
      Args&&... args) {
    (void)policy;
    chained_future_res_t<F, Args...> result;
    std::thread t(static_cast<void (&)(chained_future_res_t<F, Args...>, std::decay_t<F>&&, std::decay_t<Args>&&...)>(chained_futures::launch), result,
	f, args...);
    t.detach();
    return result;
  }

  template<typename F, typename... Args>
  chained_future_res_t<F, Args...> async(F&& f, Args&&... args) {
    return chained_futures::async(std::launch::async | std::launch::deferred,
	std::forward<F>(f), std::forward<Args>(args)...);
  }

  template<typename F, typename... Args>
  void launch(chained_future_res_t<F, Args...> future, F&& f, Args&&... args) {
    if constexpr (std::is_void_v<typename chained_future_res_t<F, Args...>::result_type>) {
      std::forward<F>(f)(std::forward<Args>(args)...);
      future._state->resolve();
    } else {
      future._state->resolve(std::forward<F>(f)(std::forward<Args>(args)...));
    }
  }
}


template<typename T>
class chained_future {
public:
  typedef T result_type;

  chained_future() : _state(std::make_shared<chained_future_state<T>>()) { }

  template<typename F>
  chained_future_res_t<F, T> chain(F f) {
    return _state->chain(f);
  }

  T& get() {
    return _state->get();
  }

private:
  std::shared_ptr<chained_future_state<T>> get_state() {
    return _state;
  }

  template<typename U>
  friend class chained_future_callback_base;

  template<typename F, typename... Args>
  friend void chained_futures::launch(chained_future_res_t<F, Args...>, F&&, Args&&...);

  std::shared_ptr<chained_future_state<T>> _state;
};

template<>
class chained_future<void> {
public:
  typedef void result_type;

  chained_future () : _state(std::make_shared<chained_future_state<void>>()) { }

  template<typename F>
  chained_future_res_t<F> chain(F f) {
    return _state->chain(f);
  }

  void get() {
    _state->get();
  }

private:
  std::shared_ptr<chained_future_state<void>> get_state() {
    return _state;
  }

  template<typename U>
  friend class chained_future_callback_base;

  template<typename F, typename... Args>
  friend void chained_futures::launch(chained_future_res_t<F, Args...>, F&&, Args&&...);

  std::shared_ptr<chained_future_state<void>> _state;
};
