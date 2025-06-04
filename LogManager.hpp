#ifndef LOGMANAGER_HPP
#define LOGMANAGER_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "FastLogger.hpp"
#include "NonCopyMovable.hpp"
#include "Singleton.hpp"

namespace SNJ {
  class LogManager : public Singleton<LogManager> {
   public:
    friend class Singleton<LogManager>;

    std::shared_ptr<FastLogger> CreateLogger(std::string_view baseFileName) {
      std::string logFilePath = generateLogFileName(baseFileName);
      auto        logger      = std::make_shared<FastLogger>(logFilePath);

      // Store the logger as a weak pointer
      std::lock_guard<std::mutex> lock(_loggerMutex);
      _loggers.push_back(logger);

      return logger;
    }

    void StartLogging(bool __startAsync = true) {
      if (_mKeepLogging.load(std::memory_order_acquire)) {
        return;  // Logging already started
      }

      _mKeepLogging = true;

      if (__startAsync) {
        _loggingThread = std::thread([this]() { LoggingLoop(); });
      } else {
        LoggingLoop();
      }
    }

    void StopLogging() {
      _mKeepLogging = false;
      if (_loggingThread.joinable()) {
        _loggingThread.join();
      }
    }

   private:
    LogManager(std::string_view logsDir) : _logsDir(std::move(logsDir)), _mKeepLogging(false) {
      // Ensure logs directory exists
      std::filesystem::create_directories(_logsDir);
    }

    ~LogManager() {
      StopLogging();
    }

    void LoggingLoop() {
      while (_mKeepLogging.load(std::memory_order_relaxed)) {
        std::lock_guard<std::mutex> lock(_loggerMutex);

        // Call ConsumeAndWriteLogs() for active loggers
        for (const auto& weakLogger : _loggers) {
          if (auto logger = weakLogger.lock()) {
            logger->ConsumeAndWriteLogs();
          }
        }

        // Remove expired loggers safely using erase-remove idiom
        _loggers.erase(std::remove_if(_loggers.begin(), _loggers.end(),
                                      [](const std::weak_ptr<FastLogger>& logger) { return logger.expired(); }),
                       _loggers.end());

        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Avoid busy waiting
      }
    }

    std::string generateLogFileName(std::string_view baseFileName) {
      auto        now    = std::chrono::system_clock::now();
      std::time_t now_c  = std::chrono::system_clock::to_time_t(now);
      std::tm     now_tm = *std::localtime(&now_c);

      std::ostringstream oss;
      oss << _logsDir << "/" << baseFileName << "_" << std::put_time(&now_tm, "%Y-%m-%d") << ".log";
      return oss.str();
    }

    std::string                            _logsDir;
    std::atomic<bool>                      _mKeepLogging;
    std::vector<std::weak_ptr<FastLogger>> _loggers;
    std::mutex                             _loggerMutex;
    std::thread                            _loggingThread;
  };
}  // namespace SNJ

#endif  // LOGMANAGER_HPP
