// SPDX-License-Identifier: BSD-3-Clause
//
// Regression test for a lost wakeup in the STDThread pool destructor.
//
// ~vtkSMPThreadPool sets Joining and notifies each worker's condition variable.
// A worker that has just evaluated its wait predicate (Joining still false) but
// has not yet blocked misses a notification sent without its mutex held, sleeps
// forever, and the destructor never returns from std::thread::join. The pool
// singleton is destroyed at process exit, so the symptom is a process that
// finishes its work and then hangs.
//
// The window is hit when a pool is destroyed while its workers are still on
// their way into the wait: right after construction, or right after their last
// job. Both are repeated here; a watchdog fails the run if a destructor blocks.

#include <SMP/STDThread/vtkSMPThreadPool.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>

int main(int argc, char* argv[])
{
  using Pool = vtk::detail::smp::vtkSMPThreadPool;
  const int iterations = argc > 1 ? std::atoi(argv[1]) : 2000;
  if (iterations < 1)
  {
    std::cerr << "usage: " << argv[0] << " [iterations >= 1]" << std::endl;
    return EXIT_FAILURE;
  }
  constexpr int jobs = 64;
  // One iteration takes milliseconds, so no progress for this long is a hang.
  constexpr std::chrono::seconds stallTimeout{ 30 };

  std::atomic<int> iteration{ 0 };
  std::mutex doneMutex;
  std::condition_variable doneCondition;
  bool done = false;
  std::thread watchdog{ [&]
    {
      std::unique_lock<std::mutex> lock{ doneMutex };
      int last = -1;
      while (!doneCondition.wait_for(lock, stallTimeout, [&] { return done; }))
      {
        if (iteration.load() == last)
        {
          std::cerr << "FAIL: thread pool destructor blocked at iteration " << last << " of "
                    << iterations << std::endl;
          std::_Exit(EXIT_FAILURE);
        }
        last = iteration.load();
      }
    } };

  int status = EXIT_SUCCESS;
  for (; iteration < iterations && status == EXIT_SUCCESS; ++iteration)
  {
    {
      Pool idle; // destroyed while its new workers are entering the wait
    }

    std::atomic<int> completed{ 0 };
    {
      Pool active;
      auto proxy = active.AllocateThreads();
      for (int job = 0; job < jobs; ++job)
      {
        proxy.DoJob([&completed] { completed.fetch_add(1, std::memory_order_relaxed); });
      }
      proxy.Join();
      // destroyed while its workers are returning to the wait after their last job
    }
    if (completed.load() != jobs)
    {
      std::cerr << "FAIL: " << completed.load() << " of " << jobs << " jobs ran at iteration "
                << iteration.load() << std::endl;
      status = EXIT_FAILURE;
    }
  }

  {
    std::lock_guard<std::mutex> lock{ doneMutex };
    done = true;
  }
  doneCondition.notify_one();
  watchdog.join();

  if (status == EXIT_SUCCESS)
  {
    std::cout << "thread pool shutdown: destroyed " << 2 * iterations << " pools" << std::endl;
  }
  return status;
}
