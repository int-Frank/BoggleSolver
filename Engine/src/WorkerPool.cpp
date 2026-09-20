//@group Misc/impl

#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <vector>
#include <queue>

#include "WorkerPool.h"

namespace Engine
{
  struct WorkerPoolTask
  {
    WorkerPoolCallback function;
    WorkerPoolCallback postFunction; // runs on main thread
    WorkerPoolCallback freeFunction; // runs on main thread
    void * pUserData;
  };

  class WorkerPool : public IWorkerPool
  {
    std::atomic<uint32_t> m_threadsWaiting;
    std::atomic<bool> m_shouldQuit;
    std::mutex m_queuedTasksMutex;
    std::mutex m_queuedPostTasksMutex;
    std::condition_variable m_cv;
    std::queue<WorkerPoolTask> m_queuedTasks;
    std::queue<WorkerPoolTask> m_queuedPostTasks;
    std::vector<std::thread> m_workerThreads;

  public:

    WorkerPool(int a_totalThreads)
    {
      if (a_totalThreads < 1)
        a_totalThreads = 1;

      for (int i = 0; i < a_totalThreads; i++)
      {
        m_workerThreads.emplace_back([this]
          {
            while (!m_shouldQuit)
            {
              ++m_threadsWaiting;
              WorkerPoolTask temp = {};

              {
                std::unique_lock<std::mutex> lock(m_queuedTasksMutex);

                m_cv.wait(lock,
                  [this] { return (!m_queuedTasks.empty()) || m_shouldQuit; });
                --m_threadsWaiting;

                if (m_shouldQuit)
                  break;

                temp = m_queuedTasks.front();
                m_queuedTasks.pop();
              }

              temp.function(temp.pUserData);

              if (temp.postFunction != nullptr)
              {
                std::unique_lock<std::mutex> lock(m_queuedPostTasksMutex);

                // TODO push_back can fail. It should return a ErrorCode and handled here.
                // If push_back throws, this thread will never increment threadsWaiting,
                // and will seem as if it is always working.
                m_queuedPostTasks.push(temp);
              }
              else if (temp.freeFunction != nullptr)
              {
                temp.freeFunction(temp.pUserData);
              }
            }
          });
      }
    }

    ~WorkerPool() override
    {
      {
        std::unique_lock<std::mutex> lock(m_queuedTasksMutex);
        m_shouldQuit = true;
      }

      m_cv.notify_all();
      for (std::thread & worker : m_workerThreads)
        worker.join();

      // Any tasks left in these queues were never run to completion, so their
      // postFunction (if any) is skipped, but we still need to release pUserData.
      while (!m_queuedTasks.empty())
      {
        WorkerPoolTask & task = m_queuedTasks.front();
        if (task.freeFunction != nullptr)
          task.freeFunction(task.pUserData);
        m_queuedTasks.pop();
      }

      while (!m_queuedPostTasks.empty())
      {
        WorkerPoolTask & task = m_queuedPostTasks.front();
        if (task.freeFunction != nullptr)
          task.freeFunction(task.pUserData);
        m_queuedPostTasks.pop();
      }
    }

    ErrorCode AddTask(WorkerPoolCallback a_func, void * a_pUserData, WorkerPoolCallback a_freeFunction, WorkerPoolCallback a_postFunction) override
    {
      WorkerPoolTask temp{};
      temp.freeFunction = a_freeFunction;
      temp.function = a_func;
      temp.postFunction = a_postFunction;
      temp.pUserData = a_pUserData;

      {
        std::unique_lock<std::mutex> lock(m_queuedTasksMutex);
        m_queuedTasks.push(temp);
      }

      m_cv.notify_one();
      return ErrorCode::None;
    }

    uint32_t DoPostWork(uint32_t a_processLimit) override
    {
      uint32_t doneTasks = 0;
      for (uint32_t i = 0; i < a_processLimit; i++)
      {
        bool newTask = false;
        WorkerPoolTask temp = {};
        {
          std::unique_lock<std::mutex> lock(m_queuedPostTasksMutex);

          if (!m_queuedPostTasks.empty())
          {
            temp = m_queuedPostTasks.front();
            m_queuedPostTasks.pop();
            newTask = true;
          }
        }

        if (!newTask)
          break;

        temp.postFunction(temp.pUserData);
        if (temp.freeFunction != nullptr)
        {
          temp.freeFunction(temp.pUserData);
        }

        doneTasks++;
      }
      return doneTasks;
    }

    bool HasActiveWorkers() override
    {
      std::unique_lock<std::mutex> lock(m_queuedTasksMutex);
      return (!m_queuedTasks.empty() || (m_threadsWaiting != (uint32_t)m_workerThreads.size()));
    }
  };

  IWorkerPool * IWorkerPool::Create(int a_totalThreads)
  {
    return new WorkerPool(a_totalThreads);
  }
}