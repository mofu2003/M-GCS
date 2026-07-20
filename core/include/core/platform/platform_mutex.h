/**
 * platform_mutex.h - 跨平台互斥锁、unique_lock 和条件变量
 *
 * Problem: MinGW with --enable-threads=win32 does not support std::mutex.
 * Solution: Thin wrapper that uses CRITICAL_SECTION + CONDITION_VARIABLE
 * on Windows, std::mutex / std::condition_variable elsewhere.
 *
 * Usage:
 *   #include "core/platform/platform_mutex.h"
 *   mg::core::platform::Mutex mtx;
 *   mg::core::platform::LockGuard lk(mtx);
 *
 *   mg::core::platform::Mutex mtx;
 *   mg::core::platform::ConditionVariable cv;
 *   mg::core::platform::UniqueLock lk(mtx);
 *   cv.wait(lk, [&]{ return ready; });
 */

#ifndef MGCORE_PLATFORM_MUTEX_H
#define MGCORE_PLATFORM_MUTEX_H

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <chrono>

  namespace mg::core::platform {

  // 跨平台互斥锁（Windows 下使用 CRITICAL_SECTION）
  class Mutex {
  public:
      Mutex()  { InitializeCriticalSection(&cs_); }
      ~Mutex() { DeleteCriticalSection(&cs_); }

      Mutex(const Mutex&) = delete;
      Mutex& operator=(const Mutex&) = delete;

      void lock()   { EnterCriticalSection(&cs_); }
      void unlock() { LeaveCriticalSection(&cs_); }

      CRITICAL_SECTION& native() { return cs_; }
      const CRITICAL_SECTION& native() const { return cs_; }

  private:
      CRITICAL_SECTION cs_;
  };

  // RAII 风格的锁守卫，构造时加锁，析构时解锁
  class LockGuard {
  public:
      explicit LockGuard(Mutex& m) : mutex_(m) { mutex_.lock(); }
      ~LockGuard() { mutex_.unlock(); }

      LockGuard(const LockGuard&) = delete;
      LockGuard& operator=(const LockGuard&) = delete;

  private:
      Mutex& mutex_;
  };

  // 可手动加解锁的灵活锁守卫，支持条件变量等待
  class UniqueLock {
  public:
      explicit UniqueLock(Mutex& m) : mutex_(m) { mutex_.lock(); }
      ~UniqueLock() { if (locked_) mutex_.unlock(); }

      UniqueLock(const UniqueLock&) = delete;
      UniqueLock& operator=(const UniqueLock&) = delete;

      void lock()   { mutex_.lock(); locked_ = true; }
      void unlock() { mutex_.unlock(); locked_ = false; }
      bool owns_lock() const { return locked_; }
      Mutex& mutex() const { return mutex_; }

  private:
      Mutex& mutex_;
      bool locked_ = true;
  };

  // 跨平台条件变量（Windows 下使用 CONDITION_VARIABLE），支持等待和超时
  class ConditionVariable {
  public:
      ConditionVariable()  { InitializeConditionVariable(&cv_); }
      ~ConditionVariable() = default;

      ConditionVariable(const ConditionVariable&) = delete;
      ConditionVariable& operator=(const ConditionVariable&) = delete;

      // 无条件等待，直到被唤醒
      void wait(UniqueLock& lk) {
          SleepConditionVariableCS(&cv_, &lk.mutex().native(), INFINITE);
      }

      // 带谓词的等待，仅在条件满足时返回
      template<typename Pred>
      void wait(UniqueLock& lk, Pred pred) {
          while (!pred()) {
              SleepConditionVariableCS(&cv_, &lk.mutex().native(), INFINITE);
          }
      }

      // 唤醒一个等待线程
      void notify_one() { WakeConditionVariable(&cv_); }
      // 唤醒所有等待线程
      void notify_all() { WakeAllConditionVariable(&cv_); }

      // 带超时的等待，超时返回 false，条件满足返回 true
      template<typename Rep, typename Period, typename Pred>
      bool wait_for(UniqueLock& lk, const std::chrono::duration<Rep, Period>& timeout, Pred pred) {
          auto abs_timeout = std::chrono::steady_clock::now() + timeout;
          while (!pred()) {
              auto remaining = abs_timeout - std::chrono::steady_clock::now();
              if (remaining <= std::chrono::duration<Rep, Period>::zero()) {
                  return pred();  // one final check
              }
              auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count();
              if (ms <= 0) return pred();
              SleepConditionVariableCS(&cv_, &lk.mutex().native(), static_cast<DWORD>(ms));
          }
          return true;
      }

  private:
      CONDITION_VARIABLE cv_;
  };

  } // namespace mg::core::platform

#else
  #include <mutex>
  #include <condition_variable>

  namespace mg::core::platform {

  // POSIX 平台直接使用标准库类型
  using Mutex = std::mutex;

  // RAII 风格的锁守卫
  class LockGuard {
  public:
      explicit LockGuard(Mutex& m) : guard_(m) {}
      ~LockGuard() = default;

      LockGuard(const LockGuard&) = delete;
      LockGuard& operator=(const LockGuard&) = delete;

  private:
      std::lock_guard<Mutex> guard_;
  };

  using UniqueLock = std::unique_lock<Mutex>;
  using ConditionVariable = std::condition_variable;

  } // namespace mg::core::platform

#endif // _WIN32

#endif /* MGCORE_PLATFORM_MUTEX_H */
