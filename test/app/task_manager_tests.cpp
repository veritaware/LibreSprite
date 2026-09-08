// Copyright (C) 2026 Veritaware
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

// TaskManager's constructor builds a ui::Timer, which asserts a live
// ui::Manager exists (ui::Timer's owner defaults to Manager::getDefault()).
#define TEST_GUI
#include "tests.h"

#include "app/task_manager.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

using namespace app;

namespace {

// Repeatedly pumps the TaskManager and sleeps briefly until `pred()` is
// true or `timeout` elapses. Needed for anything that runs on one of
// TaskManager's real background worker threads (addTask()), which
// delayed() alone doesn't need.
template<typename Pred>
bool waitUntil(Pred&& pred, std::chrono::milliseconds timeout = std::chrono::seconds(3))
{
  auto deadline = std::chrono::steady_clock::now() + timeout;
  do {
    TaskManager::instance().pump();
    if (pred())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  } while (std::chrono::steady_clock::now() < deadline);
  return pred();
}

class TaskManagerTest : public ::testing::Test {
protected:
  void TearDown() override {
    // Each test gets a fresh TaskManager (and fresh background threads),
    // so state from one test can't leak into the next.
    TaskManager::cleanup();
  }
};

} // namespace

TEST_F(TaskManagerTest, DelayedCallbacksRunOnPumpInFifoOrder)
{
  std::vector<int> order;

  TaskManager::instance().delayed([&] { order.push_back(1); });
  TaskManager::instance().delayed([&] { order.push_back(2); });
  TaskManager::instance().delayed([&] { order.push_back(3); });

  EXPECT_TRUE(order.empty()); // nothing runs until pump()

  TaskManager::instance().pump();

  ASSERT_EQ(3u, order.size());
  EXPECT_EQ(1, order[0]);
  EXPECT_EQ(2, order[1]);
  EXPECT_EQ(3, order[2]);
}

TEST_F(TaskManagerTest, DelayedCallbacksQueuedDuringAPumpWaitForTheNextOne)
{
  std::vector<int> order;

  TaskManager::instance().delayed([&] {
    order.push_back(1);
    // Queuing another delayed task from inside a callback must not run it
    // within this same pump() - see the maxTasks snapshot in onTick().
    TaskManager::instance().delayed([&] { order.push_back(2); });
  });

  TaskManager::instance().pump();
  ASSERT_EQ(1u, order.size());
  EXPECT_EQ(1, order[0]);

  TaskManager::instance().pump();
  ASSERT_EQ(2u, order.size());
  EXPECT_EQ(2, order[1]);
}

TEST_F(TaskManagerTest, AddTaskResultsAreDeliveredInProductionOrder)
{
  // Regression test for 0d75475c9: a single recurring task that produces
  // several results over multiple invocations must have them delivered to
  // the consumer in the order they were produced, not reversed.
  std::mutex mutex;
  std::vector<int> received;
  std::atomic<int> nextValue{0};

  TaskManager::instance().addTask<int>(
    [&](std::atomic_bool& isAlive) -> int {
      int v = nextValue.fetch_add(1);
      if (v >= 4)
        isAlive = false;
      return v;
    },
    [&](int&& v) {
      std::lock_guard<std::mutex> guard(mutex);
      received.push_back(v);
    });

  ASSERT_TRUE(waitUntil([&] {
    std::lock_guard<std::mutex> guard(mutex);
    return received.size() >= 5;
  }));

  std::lock_guard<std::mutex> guard(mutex);
  ASSERT_EQ(5u, received.size());
  for (int i = 0; i < 5; ++i)
    EXPECT_EQ(i, received[i]);
}

TEST_F(TaskManagerTest, TaskHandleAbortFlipsIsAliveAndStopsFurtherWork)
{
  std::atomic<int> invocations{0};

  TaskHandle handle = TaskManager::instance().addTask<int>(
    [&](std::atomic_bool&) -> int {
      return invocations.fetch_add(1);
    },
    [&](int&&) {});

  // Let it run at least once so we know the worker thread picked it up.
  ASSERT_TRUE(waitUntil([&] { return invocations.load() > 0; }));

  handle.abort();

  int countAtAbort = invocations.load();
  // Give it a moment to (not) keep running.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  TaskManager::instance().pump();

  EXPECT_LE(invocations.load(), countAtAbort + 1); // at most one in-flight iteration finishes
  EXPECT_TRUE(waitUntil([&] { return handle.done(); }));
}

TEST_F(TaskManagerTest, TaskMonitorAbortsOnScopeExit)
{
  std::atomic_bool aliveFlagSeen{true};
  std::atomic<int> invocations{0};

  {
    TaskMonitor monitor;
    monitor = TaskManager::instance().addTask<int>(
      [&](std::atomic_bool& isAlive) -> int {
        invocations.fetch_add(1);
        aliveFlagSeen = (bool)isAlive;
        return 0;
      },
      [&](int&&) {});

    ASSERT_TRUE(waitUntil([&] { return invocations.load() > 0; }));
  } // monitor destroyed here -> aborts the task

  int countAtDestruction = invocations.load();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  TaskManager::instance().pump();

  EXPECT_LE(invocations.load(), countAtDestruction + 1);
}

TEST_F(TaskManagerTest, AThrowingWorkerMarksTheTaskDone)
{
  TaskHandle handle = TaskManager::instance().addTask<int>(
    [](std::atomic_bool&) -> int {
      throw std::runtime_error("boom");
    },
    [](int&&) {});

  EXPECT_TRUE(waitUntil([&] { return handle.done(); }));
}

TEST_F(TaskManagerTest, SingleShotAddTaskOverloadRunsOnceAndCallsTheAborterOnAbort)
{
  std::atomic<int> invocations{0};
  std::atomic_bool aborterCalled{false};

  TaskHandle handle = TaskManager::instance().addTask<int>(
    [&]() -> int {
      invocations.fetch_add(1);
      return 42;
    },
    [](int&&) {},
    [&] { aborterCalled = true; });

  ASSERT_TRUE(waitUntil([&] { return handle.done(); }));
  EXPECT_EQ(1, invocations.load());

  // Aborting an already-finished task still runs the aborter (it doesn't
  // check isDone), but must not crash or re-run the worker.
  handle.abort();
  EXPECT_TRUE(aborterCalled.load());
  EXPECT_EQ(1, invocations.load());
}

TEST_F(TaskManagerTest, CleanupJoinsThreadsAndInstanceRecreatesAFreshManager)
{
  std::atomic<int> invocations{0};
  TaskManager::instance().addTask<int>(
    [&](std::atomic_bool& isAlive) -> int {
      invocations.fetch_add(1);
      isAlive = false;
      return 0;
    },
    [](int&&) {});

  ASSERT_TRUE(waitUntil([&] { return invocations.load() > 0; }));

  // cleanup() must return only once every worker thread has been joined.
  TaskManager::cleanup();

  // A fresh instance() must work normally afterwards.
  std::vector<int> order;
  TaskManager::instance().delayed([&] { order.push_back(1); });
  TaskManager::instance().pump();
  ASSERT_EQ(1u, order.size());
}
