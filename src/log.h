#ifndef LOG_H_
#define LOG_H_

#include "utils.h"
#include "singleton.h"
#include <string>
#include <memory>
#include <sstream>
#include <cstdarg>
#include <vector>
#include <utility>
#include <iostream>
#include <map>
#include <functional>
#include <mutex>
#include <iostream>
#include <fstream>
#include <yaml-cpp/yaml.h>
#include <list>
#include <map>

namespace dserver {

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
        NOTSET = 800
    };

    static LogLevel::Level FromString(const std::string& str);
    static const char* ToString(Level Level);
};

// 日志事件
class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> ptr;

    /**
     * @brief 日志事件构造函数
     * 
     * @param log_name 日志器名称
     * @param level 日志级别
     * @param file 日志文件
     * @param line 当前行
     * @param elapse 程序启动时间
     * @param thread_id_ 线程id
     * @param fiber_id 协程id
     * @param time 当前时间
     * @param thread_name 线程名称
     */
    LogEvent(const std::string& log_name, LogLevel::Level level, const char* file, uint32_t line, 
            uint64_t elapse, uint32_t thread_id, uint64_t fiber_id, time_t time, const std::string& thread_name);

    LogLevel::Level GetLevel() const { return level_; }
    const std::string& GetContent() const { return ss_.str(); }
    std::string GetFile() const { return file_; }
    uint32_t GetLine() const { return line_; }
    uint64_t GetElapse() const { return elapse_; }
    uint32_t GetThreadId() const { return thread_id_; }
    uint64_t GetFiberId() const { return fiber_id_; }
    time_t GetTime() const { return time_; }
    const std::string& GetThreadName() const { return thread_name_; }
    std::stringstream& GetSS() { return ss_; }
    const std::string& GetLogName() const { return logger_name_; }

    void printf(const char *fmt, ...);
    void vprintf(const char *fmt, va_list ap);

private:
    LogLevel::Level level_;
    std::stringstream ss_;
    const char* file_ = nullptr;
    uint32_t line_ = 0;
    uint32_t thread_id_ = 0;
    uint64_t fiber_id_ = 0;
    uint64_t elapse_ = 0;
    time_t time_;
    std::string thread_name_ = nullptr;
    std::string logger_name_ = nullptr;
};

// 日志格式化
class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;

    LogFormatter(const std::string &pattern = "%d{%Y-%m-%d %H:%M:%S} [%rms]%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n");

    void init();
    bool is_error() { return error_; }
    std::string format(LogEvent::ptr event);
    std::ostream &format(std::ostream &os, LogEvent::ptr event);
    std::string GetPattern() const { return pattern_; }

public:
    class FormatItem {
    public:
        typedef std::shared_ptr<FormatItem> ptr;
        virtual ~FormatItem() {}
        virtual void format(std::ostream& os, LogEvent::ptr event) = 0;
    };
private:
    std::string pattern_ = nullptr;
    std::vector<FormatItem::ptr> items_;
    bool error_ = false;
};

// 日志输出
class LogAppender {
public:
    typedef std::shared_ptr<LogAppender> ptr;

    LogAppender(LogFormatter::ptr default_formatter);
    virtual ~LogAppender() {}

    void SetFormatter(LogFormatter::ptr val);
    LogFormatter::ptr GetFormatter();

    virtual void log(LogEvent::ptr event) = 0;
    virtual std::string toYamlString() = 0;

protected:
    std::mutex mtx_;
    LogFormatter::ptr formatter_;
    LogFormatter::ptr default_formatter_;
};

// 控制台日志输出
class StdoutLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<StdoutLogAppender> ptr;

    StdoutLogAppender();
    void log(LogEvent::ptr event) override;
    std::string toYamlString() override;
};

// 文件日志输出
class FileLogAppender : public LogAppender {
public:
    typedef std::shared_ptr<FileLogAppender> ptr;

    FileLogAppender(const std::string& file);
    void log(LogEvent::ptr event) override;
    std::string toYamlString() override;  
    bool reopen();

private:
    std::string file_name_;
    /// 文件流
    std::ofstream file_stream_;
    /// 上次重打打开时间
    uint64_t last_time_ = 0;
    /// 文件打开错误标识
    bool reopen_error_ = false;
};

// 日志器类
class Logger {
public:
    typedef std::shared_ptr<Logger> ptr;

    Logger(const std::string& name = "default");
    const std::string& GetName() const { return name_; }
    uint64_t GetCreateTime() const { return create_time_; }
    void SetLevel(LogLevel::Level level) { level_ = level; }
    LogLevel::Level GetLevel() const { return level_; }
    void add_appender(LogAppender::ptr appender);
    void del_appender(LogAppender::ptr appender);
    void clear_appender();
    void log(LogEvent::ptr event);
    std::string ToYamlString();

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
    LogEvent::ptr GetLogEvent() const { return event_; }

private:
    Logger::ptr logger_;
    LogEvent::ptr event_;
};

// 日志器管理类
class LoggerManager {
public:
    LoggerManager();
    //  初始化，主要是结合配置模块实现日志模块初始化
    void init();
    // 如果找不到，就创建一个新的
    Logger::ptr GetLogger(const std::string& name);
    Logger::ptr GetRootLogger() { return root_logger_; }
    std::string ToYamlString();

private:
    std::mutex mtx_;
    std::map<std::string, Logger::ptr> loggers_;
    Logger::ptr root_logger_;
};

/// 日志器管理类单例
typedef dserver::Singleton<LoggerManager> LoggerMgr;

} // namespace dserver

#endif