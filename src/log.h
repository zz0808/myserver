#ifndef __LOG_H__
#define __LOH_H__

#include "utils.h"
#include "singleton.h"
#include <string>
#include <memory>
#include <sstream>
#include <cstdarg>
#include <vector>
#include <fstream>
#include <iostream>
#include <mutex>
#include <list>
#include <utility>
#include <map>
#include <functional>

// 获取root日志器
#define LOG_ROOT() server::LoggerMgr::GetInstance()->getRoot()

// 获取指定名称的日志器
#define LOG_NAME(name) server::LoggerMgr::GetInstance()->getLogger(name)

#define LOG_LEVEL(logger , level) \
    if (level <= logger->getLevel()) \
        server::LogEventWrap(logger, server::LogEvent::ptr(new server::LogEvent(logger->getLogName(), \
        level, __FILE__, __LINE__, server::GetElapsedMS() - logger->getCreateTime(), \
        server::GetThreadId(), server::GetFiberId(), time(0), server::GetThreadName()))).getLogEvent()->getSS()

#define LOG_FATAL(logger) LOG_LEVEL(logger, server::LogLevel::FATAL)
#define LOG_ALERT(logger) LOG_LEVEL(logger, server::LogLevel::ALERT)
#define LOG_CRIT(logger) LOG_LEVEL(logger, server::LogLevel::CRIT)
#define LOG_ERROR(logger) LOG_LEVEL(logger, server::LogLevel::ERROR)
#define LOG_WARN(logger) LOG_LEVEL(logger, server::LogLevel::WARN)
#define LOG_NOTICE(logger) LOG_LEVEL(logger, server::LogLevel::NOTICE)
#define LOG_INFO(logger) LOG_LEVEL(logger, server::LogLevel::INFO)
#define LOG_DEBUG(logger) LOG_LEVEL(logger, server::LogLevel::DEBUG)

#define LOG_FMT_LEVEL(logger, level, fmt, ...) \
    if(level <= logger->getLevel()) \
        server::LogEventWrap(logger, server::LogEvent::ptr(new server::LogEvent(logger->getLogName(), \
            level, __FILE__, __LINE__, server::GetElapsedMS() - logger->getCreateTime(), \
            server::GetThreadId(), server::GetFiberId(), time(0), \
            server::GetThreadName()))).getLogEvent()->printf(fmt, __VA_ARGS__)

#define LOG_FMT_FATAL(logger, fmt, ...) LOG_FMT_LEVEL(logger, server::LogLevel::FATAL, fmt, __VA_ARGS__)
#define LOG_FMT_ALERT(logger, fmt, ...) LOG_FMT_LEVEL(logger, server::LogLevel::ALERT, fmt, __VA_ARGS__)
#define LOG_FMT_CRIT(logger, fmt, ...) LOG_FMT_LEVEL(logger, server::LogLevel::CRIT, fmt, __VA_ARGS__)
#define LOG_FMT_ERROR(logger, fmt, ...) LOG_FMT_LEVEL(logger, server::LogLevel::ERROR, fmt, __VA_ARGS__)
#define LOG_FMT_WARN(logger, fmt, ...) LOG_FMT_LEVEL(logger, server::LogLevel::WARN, fmt, __VA_ARGS__)
#define LOG_FMT_NOTICE(logger, fmt, ...) LOG_FMT_LEVEL(logger, server::LogLevel::NOTICE, fmt, __VA_ARGS__)
#define LOG_FMT_INFO(logger, fmt, ...) LOG_FMT_LEVEL(logger, server::LogLevel::INFO, fmt, __VA_ARGS__)
#define LOG_FMT_DEBUG(logger, fmt, ...) LOG_FMT_LEVEL(logger, server::LogLevel::DEBUG, fmt, __VA_ARGS__)

namespace server {

// 日志级别
class LogLevel {
public:
    enum Level {
        /// 致命情况，系统不可用
        FATAL  = 0,
        /// 高优先级情况，例如数据库系统崩溃
        ALERT  = 100,
        /// 严重错误，例如硬盘错误
        CRIT   = 200,
        /// 错误
        ERROR  = 300,
        /// 警告
        WARN   = 400,
        /// 正常但值得注意
        NOTICE = 500,
        /// 一般信息
        INFO   = 600,
        /// 调试信息
        DEBUG  = 700,
        /// 未设置
        NOTSET = 800,
    };

    static const char* ToString(LogLevel::Level level);
    static LogLevel::Level FromString(const std::string& str); 
};

// 日志事件
class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> ptr;

    LogEvent(const std::string& log_name, LogLevel::Level level_, const char* file, uint32_t line, uint64_t elapse
            , uint32_t thread_id, uint64_t fiber_id, time_t time, const std::string& thread_name);

    LogLevel::Level getLevel() const { return level_; }
    std::string getContent() const { return ss_.str(); }
    std::string getFile() const { return file_; }
    uint32_t getLine() const { return line_; }
    uint64_t getElapse() const { return elapse_; }
    uint32_t getThreadId() const { return thread_id_; }
    uint64_t getFiberId() const { return fiber_id_; }
    time_t getTime() const { return time_; }
    const std::string& getThreadName() const { return thread_name_; }
    const std::string& getLogName() const { return log_name_; }
    std::stringstream& getSS() { return ss_; }
    void printf(const char *fmt, ...);
    void vprintf(const char *fmt, va_list ap);

private:
    LogLevel::Level level_;
    std::stringstream ss_;
    const char* file_ = nullptr;
    uint32_t line_ = 0;
    uint64_t elapse_ = 0;
    uint32_t thread_id_ = 0;
    uint64_t fiber_id_ = 0;
    time_t time_;
    std::string thread_name_;
    std::string log_name_;
};

// 日志格式
class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;

    LogFormatter(const std::string &pattern = "%d{%Y-%m-%d %H:%M:%S} [%rms]%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n");

    void init();
    bool isError() const { return error_; }
    std::string format(LogEvent::ptr event);
    std::ostream& format(std::ostream& os, LogEvent::ptr event);
    std::string getPattern() const { return pattern_; }

public:
    class FormatItem {
    public:
        typedef std::shared_ptr<FormatItem> ptr;

        virtual ~FormatItem() {}
        virtual void format(std::ostream &os, LogEvent::ptr event) = 0;
    };
private:
    /// 日志格式模板
    std::string pattern_;
    /// 解析后的格式模板数组
    std::vector<FormatItem::ptr> items_;
    /// 是否出错
    bool error_ = false;

};

// 日志输出地
class LogAppender {
public:
    typedef std::shared_ptr<LogAppender> ptr;
    
    LogAppender(LogFormatter::ptr default_formatter);
    virtual ~LogAppender() {}
    void setFormatter(LogFormatter::ptr val);
    LogFormatter::ptr getFormatter();
    virtual void log(LogEvent::ptr event) = 0;

protected:
    std::mutex mtx_;
    LogFormatter::ptr formatter_;
    LogFormatter::ptr default_formatter_;
};

class StdoutLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;

    StdoutLogAppender();
    void log(LogEvent::ptr event) override;
};

class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;

    FileLogAppender(const std::string &file);
    void log(LogEvent::ptr event) override;
    bool reopen();

private:
    std::string filename_;
    std::ofstream filestream_;
    uint64_t last_time_ = 0;
    bool reopen_error_ = false;
};

// 日志器类
class Logger {
public:
    typedef std::shared_ptr<Logger> ptr;

    Logger(const std::string &name="default");
    const std::string& getLogName() const { return name_; }
    uint64_t getCreateTime() const { return create_time_; }
    void setLevel(LogLevel::Level level) { level_ = level; }
    LogLevel::Level getLevel() const { return level_; }
    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);
    void clearAppender();
    void log(LogEvent::ptr event);

private:
    std::mutex mtx_;
    std::string name_;
    LogLevel::Level level_;
    std::list<LogAppender::ptr> appenders_;
    uint64_t create_time_;
};

// 日志事件包装器，方便宏定义，内部包含日志事件和日志器
class LogEventWrap {
public:
    LogEventWrap(Logger::ptr logger, LogEvent::ptr event);
    ~LogEventWrap();
    LogEvent::ptr getLogEvent() const { return event_; }

private:
    Logger::ptr logger_;
    LogEvent::ptr event_;
};

//  日志器管理类
class LoggerManager{
public:
    LoggerManager();
    void init();
    Logger::ptr getLogger(const std::string &name);
    Logger::ptr getRoot() { return root_; }

private:
    std::mutex mtx_;
    std::map<std::string, Logger::ptr> loggers_;
    Logger::ptr root_;
};

typedef server::Singleton<LoggerManager> LoggerMgr;

} // namespace server

#endif