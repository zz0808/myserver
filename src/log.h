#ifndef __LOG_H__
#define __LOG_H__

#include <string>
#include <memory>
#include <vector>
#include <utility>

namespace server {

class LogLevel {
public:
    enum Level {
        UNKNOWN = 0,
        INFO,
        DEBUG,
        WARN,
        ERROR,
        FATAL
    };
    static std::string ToString(LogLevel::Level level);
    static LogLevel::Level FromString(const std::string& str);
};

class LogEvent {
public:
    typedef std::shared_ptr<LogEvent> ptr;

    LogEvent(const std::string& filename, LogLevel::Level level, uint32_t line, uint32_t thread_id, 
        uint64_t fiber_id, uint64_t elapse, time_t time, const std::string& log_file, const std::string thread_name);
    ~LogEvent() {}

    const std::string& getFilename() const { return filename_; }
    LogLevel::Level getLevel() const { return level_; }
    uint32_t getLine() const { return line_; }
    uint32_t getThreadId() const { return thread_id_; }
    uint64_t getFiberId() const { return fiber_id_; }
    uint64_t getElapse() const { return elapse_; }
    time_t getTime() const { return time_; }
    const std::string& getLogFile() const { return log_file_; }
    const std::string getThreadName() const { return thread_name_; }

private:
    std::string filename_;
    LogLevel::Level level_;
    uint32_t line_;
    uint32_t thread_id_;
    uint64_t fiber_id_;
    uint64_t elapse_;
    time_t time_;
    std::string log_file_;
    std::string thread_name_;
};

class LogFormatter {
public:
    typedef std::shared_ptr<LogFormatter> ptr;
    LogFormatter(const std::string& pattern = "%d{%Y-%m-%d %H:%M:%S} [%rms]%T%t%T%N%T%F%T[%p]%T[%c]%T%f:%l%T%m%n");
    ~LogFormatter() {}
    bool isError() const { return is_error_; }
    const std::string& getPattern() const { return pattern_; }

public:
    class FormatItem {
    public:
        typedef std::shared_ptr<FormatItem> ptr;
        FormatItem() {}
        virtual ~FormatItem() {}
        // virtual void format() = 0;
    };

private:
    std::string pattern_;
    bool is_error_;
    std::vector<FormatItem::ptr> items_;
    void init();

};

} // namespace server

#endif