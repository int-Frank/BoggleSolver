#ifndef IWORKERPOOL_H
#define IWORKERPOOL_H

#include <cstdint>

#include "ErrorCode.h"

namespace Engine
{
  // We use a plain old function pointer instead of a std::function<void(void*)>. 
  // This allows the implementation of the Worker Pool to use minimal memory allocations.
  // A consequence is the capture clause of a lambda must be empty.
  typedef void (*WorkerPoolCallback)(void *) noexcept;

  class IWorkerPool
  {
  public:

    // Returns nullptr on failure
    static IWorkerPool * Create(int a_totalThreads);

    virtual ~IWorkerPool() = default;

    virtual ErrorCode AddTask(WorkerPoolCallback func,
                              void * pUserData = nullptr,
                              WorkerPoolCallback freeFunction = nullptr,
                              WorkerPoolCallback postFunction = nullptr) = 0;

    // This must be run on the main thread, handles marshalling work back from worker threads if required
    // The parameter can be used to limit how much work is done each time this is called
    // Returns number of tasks processed.
    virtual uint32_t DoPostWork(uint32_t processLimit = 0xFFFFFFFF) = 0;

    // Returns true if there are workers currently processing tasks or if workers should be processing tasks
    virtual bool HasActiveWorkers() = 0;
  };
}

#endif
