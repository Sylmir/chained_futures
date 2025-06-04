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

template<typename R, typename T>
class chained_future_callback : public chained_future_callback_base<T> {
public:
  chained_future_callback(chained_future<R> future, std::function<R(T)> function) :
    _future(future), _function(function) {

  }

  void run(T t) final override {
    std::thread th(static_cast<void(&)(chained_future_res_t<R(*)(T), T>, std::function<R(T)>&&, T&&)>(chained_futures::launch), _future, _function, t);
    th.detach();
  }

private:
  chained_future<R> _future;
  std::function<R(T)> _function;
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

  template<typename R>
  chained_future<R> chain(std::function<R(T)> f) {
    chained_future<R> future;
    _callbacks.push_back(new chained_future_callback<R, T>(future, f));
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

namespace chained_futures {
  template<typename F, typename... Args>
  chained_future_res_t<F, Args...> async(std::launch policy, F&& f,
      Args&&... args) {
    (void)policy;
    chained_future_res_t<F, Args...> result;
    std::thread t(static_cast<void (&)(chained_future_res_t<F, Args...>, F&&, Args&&...)>(chained_futures::launch), result,
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
    future._state->resolve(std::forward<F>(f)(std::forward<Args>(args)...));
  }
}


template<typename T>
class chained_future {
public:
  chained_future() : _state(std::make_shared<chained_future_state<T>>()) { }

  template<typename R>
  chained_future<R> chain(std::function<R(T)> f) {
    return _state->chain(f);
  }

  template<typename R>
  chained_future<R> chain(R (*fn)(T)) {
    std::function<R(T)> fun = fn;
    return _state->chain(fun);
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
