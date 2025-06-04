#ifndef SNJ_FAST_LOGGER_HPP
#define SNJ_FAST_LOGGER_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>

#include "NonCopyMovable.hpp"
#include "SPSCQueue.hpp"

namespace SNJ {

  enum class LogLevel : std::uint8_t {
    DEBUG,  ///< Debug-level messages.
    INFO,   ///< Informational messages.
    ERROR,  ///< Error-level messages.
    FATAL   ///< Fatal-level messages.
  };

  inline static std::string LogLevelToString(LogLevel __logLevel) {
    switch (__logLevel) {
      case LogLevel::DEBUG:
        return "DEBUG";
      case LogLevel::INFO:
        return "INFO";
      case LogLevel::ERROR:
        return "ERROR";
      case LogLevel::FATAL:
        return "FATAL";
    }
    return "INVALID";
  }

  inline static LogLevel LogLevelStrToEnum(const std::string &logLevelStr) {
    static const std::unordered_map<std::string, LogLevel> logLevelMap = {
        {"DEBUG", LogLevel::DEBUG},
        {"INFO", LogLevel::INFO},
        {"ERROR", LogLevel::ERROR},
        {"FATAL", LogLevel::FATAL}
    };

    auto it = logLevelMap.find(logLevelStr);
    return (it != logLevelMap.end()) ? it->second : LogLevel::FATAL;
}

  
  class BaseLogFormatter {
   protected:
    constexpr BaseLogFormatter(std::string_view __formatString) : _mFormatString(__formatString) {}
    virtual ~BaseLogFormatter() noexcept = default;

    std::string_view _mFormatString;

   public:
    virtual void Evaluate(const char* __data, std::ostringstream& __stream) const = 0;
  };

  template <size_t... N>
  struct StringLiteral {
    static constexpr size_t TotalSize = (N + ... + 0);  // Calculate total size including null terminator
    char                    Value[TotalSize];

    // Constructor to concatenate string literals
    constexpr StringLiteral(const char (&... str)[N]) {
      char* dest = Value;
      ((std::copy_n(str, N - 1, dest), dest += (N - 1)), ...);  // Concatenation
      *dest = '\0';                                             // Null-terminate the final string
    }
  };

  template <size_t... N>
  constexpr StringLiteral<N...> makeStringLiteral(const char (&... str)[N]) {
    return StringLiteral<N...>(str...);
  }

  template <StringLiteral FormatString, class... CArgs>
  class LogFormatter : public BaseLogFormatter {
   public:
    inline static LogFormatter<FormatString, CArgs...> instance{};

    constexpr LogFormatter() : BaseLogFormatter(FormatString.Value) {}

    template <class T>
    const char* PrintData(const char* __data, std::ostringstream& __stream) const {
      if constexpr (std::is_same_v<std::decay_t<T>, std::string> || std::is_same_v<T, const char*>) {
        std::string str(__data);
        __stream << str;
        return __data + str.size() + 1;
      } else {
        __stream << *(reinterpret_cast<const T*>(__data));
        return __data + sizeof(T);
      }
    }

    template <typename... Args>
    typename std::enable_if<sizeof...(Args) == 0>::type Format(const char* __data, const char* __formatStr,
                                                               std::ostringstream& __stream) const {
      __stream << __formatStr;
    }

    template <class Arg, class... Args>
    void Format(const char* __data, const char* __formatStr, std::ostringstream& __stream) const {
      const char* placeholder = strstr(__formatStr, "{}");
      if (!placeholder) {
        __stream << (__formatStr);
        return;
      }
      __stream.write(__formatStr, placeholder - __formatStr);
      __data = PrintData<Arg>(__data, __stream);
      Format<Args...>(__data, placeholder + 2, __stream);
    }

    void Evaluate(const char* __data, std::ostringstream& __stream) const override {
      Format<std::decay_t<CArgs>...>(__data, BaseLogFormatter::_mFormatString.data(), __stream);
    }
  };

  struct LogMessage {
    BaseLogFormatter* _mFormatter;         ///< Pointer to the formatter for the message.
    char              _mDataBuffer[1024];  ///< Buffer to store message data.

    template <class T>
    char* CopyData(char* __buffer, T& __data) {
      if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
        memcpy(__buffer, __data.c_str(), __data.size());
        __buffer[__data.size()] = '\0';
        return __buffer + __data.size() + 1;
      } else if constexpr (std::is_same_v<std::decay_t<T>, const char*>) {
        std::size_t length = strlen(__data);
        memcpy(__buffer, __data, length);
        __buffer[length] = '\0';
        return __buffer + length + 1;
      } else {
        *(reinterpret_cast<std::decay_t<T>*>(__buffer)) = __data;
        return __buffer + sizeof(T);
      }
    }

    template <class Arg, class... Args>
    void CopyArgs(char* __buffer, Arg& __arg, Args&... __args) {
      __buffer = CopyData(__buffer, __arg);
      CopyArgs(__buffer, __args...);
    }

    template <class Arg>
    void CopyArgs(char* __buffer, Arg& __arg) {
      __buffer = CopyData(__buffer, __arg);
    }

    LogMessage() noexcept = default;

    LogMessage(BaseLogFormatter* __formatter, LogLevel __logLevel) {
      _mFormatter                                = __formatter;
      *reinterpret_cast<LogLevel*>(_mDataBuffer) = __logLevel;
    }

    template <class... Args>
    LogMessage(BaseLogFormatter* __formatter, LogLevel __logLevel, Args&&... __args)
        : LogMessage(__formatter, __logLevel) {
      CopyArgs(_mDataBuffer + sizeof(LogLevel), __args...);
    }
  };

  using MessageQueue = SPSCQueue<LogMessage>;
  class ThreadScopedQueueManager {
   public:
    class ThreadScopedQueue {
     public:
      ThreadScopedQueue(std::shared_ptr<ThreadScopedQueueManager> __threadScopedQueueManager)
          : _mThreadScopedQueueManager(__threadScopedQueueManager) {
        _mThreadScopedQueueManager->RegisterScopedQueue(this);
      }

      MessageQueue& GetMessageQueue() { return _mMessageQueue; }

      MAKE_NON_COPYABLE(ThreadScopedQueue);

      ~ThreadScopedQueue() { _mThreadScopedQueueManager->UnRegisterThreadScopedQueue(this); }

     private:
      std::shared_ptr<ThreadScopedQueueManager> _mThreadScopedQueueManager;
      MessageQueue                              _mMessageQueue;
    };

   public:
    void RegisterScopedQueue(ThreadScopedQueue* __threadScopedQueue) {
      std::lock_guard<std::mutex> lock(_mLock);
      _mThreadScopedQueues.insert(__threadScopedQueue);
    }

    void UnRegisterThreadScopedQueue(ThreadScopedQueue* __threadScopedQueue) {
      if (!__threadScopedQueue->GetMessageQueue().IsEmpty()) {
        sleep(5);
      }
      std::lock_guard<std::mutex> lock(_mLock);
      _mThreadScopedQueues.erase(__threadScopedQueue);
    }

    template <class TCallback>
    void ForEachQueue(TCallback __callback) {
      std::lock_guard<std::mutex> lock(_mLock);
      for (auto threadScopedQueue : _mThreadScopedQueues) {
        __callback(threadScopedQueue->GetMessageQueue());
      }
    }

   private:
    std::mutex                             _mLock;
    std::unordered_set<ThreadScopedQueue*> _mThreadScopedQueues;
  };

  inline MessageQueue& GetThreadScopedMessageQueue(std::shared_ptr<ThreadScopedQueueManager> __threadScopedQueueManager) {
    thread_local ThreadScopedQueueManager::ThreadScopedQueue sThreadScopedQueue(__threadScopedQueueManager);
    return sThreadScopedQueue.GetMessageQueue();
  }

  class FastLogger {
   public:
    FastLogger(std::string_view __logFileName)
        : _mFileStream(__logFileName.data()),
          _mThreadScopedQueueManager(std::make_shared<ThreadScopedQueueManager>()) {}

    MAKE_NON_COPYABLE(FastLogger);
    MAKE_NON_MOVABLE(FastLogger);

    ~FastLogger() noexcept {
      _mFileStream.flush();
      _mFileStream.close();
    }

    template <class... Args>
    void Log(BaseLogFormatter* __formatter, LogLevel __logLevel, Args&&... __args) {
      if (__logLevel >= _mLogLevel) {
        GetThreadScopedMessageQueue(_mThreadScopedQueueManager)
            .Enqueue(__formatter, __logLevel, std::forward<Args>(__args)...);
      }
    }

    void SetLogLevel(LogLevel __logLevel) { _mLogLevel = __logLevel; }

    void ConsumeAndWriteLogs() noexcept {
      _mThreadScopedQueueManager->ForEachQueue([this](auto& queue) {
        LogMessage message;
        while (queue.Dequeue(message) != false) {
          std::ostringstream oss;
          auto               now   = std::chrono::system_clock::now();
          std::time_t        now_c = std::chrono::system_clock::to_time_t(now);
          std::tm            tm_buf;
          localtime_r(&now_c, &tm_buf);
          oss << "[" << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "] ";
          auto logLevel = *reinterpret_cast<const LogLevel*>(message._mDataBuffer);
          oss << "[" << LogLevelToString(logLevel) << "] ";
          message._mFormatter->Evaluate(message._mDataBuffer + sizeof(LogLevel), oss);
          _mFileStream << oss.str() << "\n";
          _mFileStream.flush();
        }
      });
    }
    LogLevel                                  _mLogLevel{LogLevel::INFO};
    std::ofstream                             _mFileStream;  ///< File stream for logging.
    std::shared_ptr<ThreadScopedQueueManager> _mThreadScopedQueueManager;
  };

  template <StringLiteral FormatString, class... Args>
  inline static void WriteLog(std::shared_ptr<FastLogger> __logger, LogLevel __logLevel, Args&&... __args) {
    __logger->Log(&LogFormatter<FormatString, Args...>::instance, __logLevel, std::forward<Args>(__args)...);
  }

#define FAST_LOG(logger, logLevel, formatString, ...) \
  SNJ::WriteLog<SNJ::makeStringLiteral(__PRETTY_FUNCTION__, ":", formatString)>(logger, logLevel, ##__VA_ARGS__);

#define LOG_DEBUG(logger, formatString, ...) FAST_LOG(logger, SNJ::LogLevel::DEBUG, formatString, ##__VA_ARGS__)

#define LOG_INFO(logger, formatString, ...) FAST_LOG(logger, SNJ::LogLevel::INFO, formatString, ##__VA_ARGS__)

#define LOG_ERROR(logger, formatString, ...) FAST_LOG(logger, SNJ::LogLevel::ERROR, formatString, ##__VA_ARGS__)

#define LOG_FATAL(logger, formatString, ...) FAST_LOG(logger, SNJ::LogLevel::FATAL, formatString, ##__VA_ARGS__)

}  // namespace SNJ

#endif
