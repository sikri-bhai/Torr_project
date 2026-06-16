#pragma once

#include <windows.h>
#include <process.h>

#include <functional>

using namespace std;

class Mutex {
public:
    Mutex() { InitializeCriticalSection(&cs_); }
    ~Mutex() { DeleteCriticalSection(&cs_); }
    void lock() { EnterCriticalSection(&cs_); }
    void unlock() { LeaveCriticalSection(&cs_); }
    CRITICAL_SECTION& native() { return cs_; }

private:
    CRITICAL_SECTION cs_;
};

class LockGuard {
public:
    explicit LockGuard(Mutex& m) : m_(m) { m_.lock(); }
    ~LockGuard() { m_.unlock(); }

private:
    Mutex& m_;
};

class UniqueLock {
public:
    explicit UniqueLock(Mutex& m) : m_(m), locked_(true) { m_.lock(); }
    ~UniqueLock() {
        if (locked_) m_.unlock();
    }
    void unlock() {
        if (locked_) {
            m_.unlock();
            locked_ = false;
        }
    }
    Mutex& mutex() { return m_; }

private:
    Mutex& m_;
    bool locked_;
};

class CondVar {
public:
    CondVar() { InitializeConditionVariable(&cv_); }

    void wait(UniqueLock& lk) { SleepConditionVariableCS(&cv_, &lk.mutex().native(), INFINITE); }

    template <typename Pred>
    void wait(UniqueLock& lk, Pred pred) {
        while (!pred()) wait(lk);
    }

    void notify_one() { WakeConditionVariable(&cv_); }
    void notify_all() { WakeAllConditionVariable(&cv_); }

private:
    CONDITION_VARIABLE cv_;
};

class Thread {
public:
    Thread() = default;
    explicit Thread(function<void()> fn) { start(move(fn)); }

    void start(function<void()> fn) {
        auto* job = new function<void()>(move(fn));
        h_ = (HANDLE)_beginthreadex(nullptr, 0, &Thread::trampoline, job, 0, nullptr);
    }

    void join() {
        if (h_) {
            WaitForSingleObject(h_, INFINITE);
            CloseHandle(h_);
            h_ = nullptr;
        }
    }

    bool joinable() const { return h_ != nullptr; }

    void detach() {
        if (h_) {
            CloseHandle(h_);
            h_ = nullptr;
        }
    }

private:
    HANDLE h_ = nullptr;

    static unsigned __stdcall trampoline(void* arg) {
        auto* job = static_cast<function<void()>*>(arg);
        (*job)();
        delete job;
        return 0;
    }
};

inline void sleepMs(int ms) { Sleep(ms); }
inline void sleepSec(int sec) { Sleep(sec * 1000); }
