#ifndef LOG_H_
#define LOG_H_

#include "utils.h"
#include <string>
#include <memory>
#include <sstream>
#include <cstdarg>
#include <vector>
#include <iostream>
#include <map>
#include <functional>
#include <mutex>
#include <fstream>
#include <list>

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

    LogEvent(const std::string& logger_name, LogLevel::Level level, const char* file, uint32_t line, uint64_t elapse
            , uint32_t thread_id, uint64_t fiber_id, time_t time, const std::string& thread_name);

    LogLevel::Level GetLevel() const { return level_; }
    std::string GetContent() const { return ss_.str(); }
    const char* GetFile() const { return file_; }
    uint64_t GetElapse() const { return elapse_; }
    uint32_t GetThreadId() const { return thread_id_; }
    uint64_t GetFiberId() const { return fiber_id_; }
    time_t GetTime() const { return time_; }
    uint32_t GetLine() const { return line_; }
    const std::string& GetThreadName() const { return thread_name_; }
    const std::string& GetLoggerName() const { return logger_name_; }
    std::stringstream& GetSS() { return ss_; }
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
    std::string logger_name_;
};

// 日志格式化
class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;

    LogFormatter(const std::string &pattern = "%d{%Y-%m-%d %H:%M:%S} [%rms]%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n");
    void init();
    bool isError() const { return error_; }
    std::string format(LogEvent::ptr event);
    std::ostream &format(std::ostream &os, LogEvent::ptr event);
    std::string GetPattern() const { return pattern_; }

public:
    class FormatItem {
    public:
        typedef std::shared_ptr<FormatItem> ptr;
        virtual ~FormatItem() {}
        virtual void format(std::ostream &os, LogEvent::ptr event) = 0;
    };

private:
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
    virtual ~LogAppender();
    void SetLogFormatter(LogFormatter::ptr val);
    LogFormatter::ptr GetLogFormatter();
    virtual void log(LogEvent::ptr event) = 0;

protected:
    std::mutex mtx_;
    LogFormatter::ptr formatter_;
    // 默认日志格式器
    LogFormatter::ptr default_formatter_;
};

// 输出到控制台
class StdoutLogAppender : public LogAppender{
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;

    StdoutLogAppender();
    void log(LogEvent::ptr event) override;

private:

};

// 输出到文件
class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;

    FileLogAppender();
    void log(LogEvent::ptr event) override;
    bool reopen();
    
private:
    std::string file_name_;
    std::ofstream file_stream_;
    uint64_t last_open_time_ = 0;
    bool reopen_error_ = false;
};

// 日志器类
class Logger {
public:
    typedef std::shared_ptr<Logger> ptr;

    Logger(const std::string &name="default");
    const std::string& GetLogName() const { return log_name_; }
    const uint64_t& GetCreateTime() const { return create_time_; }
    void setLevel(LogLevel::Level level) { level_ = level; }
    LogLevel::Level getLevel() const { return level_; }
    void addAppender(LogAppender::ptr appender);
    void delAppender(LogAppender::ptr appender);
    void clearAppenders();
    void log(LogEvent::ptr event);

private:
    std::mutex mtx_;
    LogLevel::Level level_;
    std::string log_name_;
    std::list<LogAppender::ptr> appenders_;
    uint64_t create_time_ = 0;
};

// 包装器
class LogEventWrap{
public:
    LogEventWrap(Logger::ptr logger, LogEvent::ptr event);
    ~LogEventWrap();
    LogEvent::ptr GetLogEvent() const { return event_; }

private:
    /// 日志器
    Logger::ptr logger_;
    /// 日志事件
    LogEvent::ptr event_;
};

} // namespace server


#endif 