#if !defined(LIBROBOCOL_SYNC_H)
#define LIBROBOCOL_SYNC_H

// Usage:
// In the scope you want to be syncronized, put:
// auto l = lock();
#define SYNCHRONIZED_CLASS                           \
    Mutex accessM;                              \
    LockGuardMutex lock()               \
    {                                                \
        return {accessM}; \
    }

#define STATIC_SYNCHRONIZED_CLASS                    \
    static Mutex accessM;      \
    static LockGuardMutex lock()        \
    {                                                \
        return {accessM}; \
    }


// Implementation of a mutex that chooses between std::mutex (if present) and the wii/gamecube specific libogc mutex implementation

#ifdef GEKKO // Macro present when code is compiled for the GC and Wii

#include <mutex.h>
#include <system_error>

namespace librobocol
{

class mutex
{
    mutex_t handle;

public:
    mutex()
    {
        int err = -999;
        if ((err = LWP_MutexInit(&handle, false)) < 0) 
        {
            throw std::error_code(err, std::system_category());
        }
    }
    mutex(const mutex&) = delete;

    void lock()
    {
        int err = -999;
        if ((err = LWP_MutexLock(handle)) < 0) 
        {
            throw std::error_code(err, std::system_category());
        }
    }

    bool try_lock()
    {
        int err = -999;
        if ((err = LWP_MutexTryLock(handle)) < 0 || err == 1) 
        {
            return false;
        }
        return true;
    }

    void unlock()
    {
        int err = -999;
        if ((err = LWP_MutexLock(handle)) < 0) 
        {
            throw std::error_code(err, std::system_category());
        }
    }
};

template <typename MutexT>
class lock_guard
{
    MutexT& mutex;

public:
    lock_guard(MutexT& _mutex) : mutex(_mutex) 
    {
        mutex.lock();
    }

    ~lock_guard()
    {
        mutex.unlock();
    }
};

}

using Mutex = librobocol::mutex;

template <typename MutexT>
using LockGuard = librobocol::lock_guard<MutexT>;

using LockGuardMutex = librobocol::lock_guard<Mutex>;

#else

// Just use the standard libraries' version when present

#include <mutex>

using Mutex = std::mutex;

template <typename MutexT>
using LockGuard = std::lock_guard<MutexT>;

using LockGuardMutex = std::lock_guard<Mutex>;

#endif

#endif // if !defined(LIBROBOCOL_SYNC_H)