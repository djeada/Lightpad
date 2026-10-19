#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#ifdef __linux__
#include <pthread.h>
#endif

std::mutex mutex;
std::condition_variable condition;
std::atomic<bool> betaReady{false};
bool released = false;

struct WorkerState {
  int id;
  float samples[2][2];
};

void worker(int id) {
  WorkerState payload{id, {{float(id), 2}, {3, 4}}};
#ifdef __linux__
  pthread_setname_np(pthread_self(), id == 1 ? "alpha" : "beta");
#endif
  if (id == 2) {
    std::unique_lock<std::mutex> lock(mutex);
    betaReady = true;
    condition.wait(lock, [] { return released; });
  } else {
    while (!betaReady)
      std::this_thread::yield();
    volatile int inspectionMarker = payload.id; // CPP_THREAD_INSPECT
    {
      std::lock_guard<std::mutex> lock(mutex);
      released = true;
    }
    condition.notify_all();
  }
  std::printf("cpp-thread-debug-ok %d\n", payload.id);
}

int main() {
  std::thread alpha(worker, 1);
  std::thread beta(worker, 2);
  alpha.join();
  beta.join();
}
