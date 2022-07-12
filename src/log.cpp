#include "log.h"

namespace server {

const char* LogLevel::ToString(LogLevel::Level level) {
    switch (level) {
#define XX(name) case LogLevel::name: return #name;
    XX(FATAL);
    XX(ALERT);
    XX(CRIT);
    XX(ERROR);
    XX(WARN);
    XX(NOTICE);
    XX(INFO);
    XX(DEBUG);
#undef XX
    }
    return "NOTSET";
}

LogLevel::Level LogLevel::FromString(const std::string& str) {
#define XX(level, v) if (#v == str) return LogLevel::level;
    XX(FATAL, FATAL);
    XX(ALERT, ALERT);
    XX(CRIT, CRIT);
    XX(ERROR, WARN);
    XX(WARN, WARN);
    XX(NOTICE, NOTICE);
    XX(INFO, INFO);
    XX(DEBUG, DEBUG);
    XX(NOTSET, NOTSET);
#undef XX
    return LogLevel::NOTSET;
}

LogEvent::LogEvent(const std::string& log_name, LogLevel::Level level, const char* file, uint32_t line, uint64_t elapse
        , uint32_t thread_id, uint64_t fiber_id, time_t time, const std::string& thread_name) 
        : level_(level)
        , file_(file)
        , line_(line)
        , elapse_(elapse)
        , thread_id_(thread_id)
        , fiber_id_(fiber_id)
        , time_(time)
        , thread_name_(thread_name) 
        , log_name_(log_name) {
    
}
void LogEvent::printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

void LogEvent::vprintf(const char *fmt, va_list ap) {
    char *buf = nullptr;
    int len = vasprintf(&buf, fmt, ap);
    if(len != -1) {
        ss_ << std::string(buf, len);
        free(buf);
    } 
}

class MessageFormatItem : public LogFormatter::FormatItem {
public:
    MessageFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->getContent();
    }
};

class LevelFormatItem : public LogFormatter::FormatItem {
public:
    LevelFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << LogLevel::ToString(event->getLevel());
    }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
public:
    ElapseFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->getElapse();
    }
};

class LoggerNameFormatItem : public LogFormatter::FormatItem {
public:
    LoggerNameFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->getLogName();
    }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
public:
    ThreadIdFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->getThreadId();
    }
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
public:
    FiberIdFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->getFiberId();
    }
};

class ThreadNameFormatItem : public LogFormatter::FormatItem {
public:
    ThreadNameFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->getThreadName();
    }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
public:
    DateTimeFormatItem(const std::string& format = "%Y-%m-%d %H:%M:%S")
        :format_(format) {
        if(format_.empty()) {
            format_ = "%Y-%m-%d %H:%M:%S";
        }
    }

    void format(std::ostream& os, LogEvent::ptr event) override {
        struct tm tm;
        time_t time = event->getTime();
        localtime_r(&time, &tm);
        char buf[64];
        strftime(buf, sizeof(buf), format_.c_str(), &tm);
        os << buf;
    }
private:
    std::string format_;
};

class FileNameFormatItem : public LogFormatter::FormatItem {
public:
    FileNameFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->getFile();
    }
};

class LineFormatItem : public LogFormatter::FormatItem {
public:
    LineFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->getLine();
    }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
public:
    NewLineFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << std::endl;
    }
};

class StringFormatItem : public LogFormatter::FormatItem {
public:
    StringFormatItem(const std::string& str)
        :m_string(str) {}
    void format(std::ostream& os, LogEvent::ptr event) override {
        os << m_string;
    }
private:
    std::string m_string;
};

class TabFormatItem : public LogFormatter::FormatItem {
public:
    TabFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << "\t";
    }
};

class PercentSignFormatItem : public LogFormatter::FormatItem {
public:
    PercentSignFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << "%";
    }
};

LogFormatter::LogFormatter(const std::string &pattern) : pattern_(pattern) {
    init();
}

void LogFormatter::init() {
    uint32_t i = 0;
    bool parse_normal = true;
    std::string tmp;
    std::vector<std::pair<int, std::string>> patterns;
    std::string date_format;
    // 是否解析出错
    bool error = false;

    while (i < pattern_.size()) {
        char c = pattern_[i];
        if (c == '%') {
            if (parse_normal) {
                parse_normal = false;
                if (!tmp.empty()) {
                    patterns.push_back(std::make_pair(0, tmp));
                    tmp.clear();
                }
                i++;
                continue;
            } else {
                parse_normal = true;
                patterns.push_back(std::make_pair(1, std::string(1, c)));
                i++;
                continue;
            }
        } else {
            if (parse_normal) {
                tmp.push_back(c);
                i++;
                continue;
            } else { // 模版字符
                patterns.push_back(std::make_pair(1, std::string(1, c)));
                parse_normal = true;
                if(c != 'd') {
                    i++;
                    continue;
                }
                i++;
                if(i < pattern_.size() && pattern_[i] != '{') {
                    continue;
                }
                i++;
                while( i < pattern_.size() && pattern_[i] != '}') {
                    date_format.push_back(pattern_[i]);
                    i++;
                }
                if(pattern_[i] != '}') {
                    // %d后面的大括号没有闭合，直接报错
                    std::cout << "[ERROR] LogFormatter::init() " << "pattern: [" << pattern_ << "] '{' not closed" << std::endl;
                    error = true;
                    break;
                }
                i++;
                continue;
            }
        }
    }

    if(error) {
        error_ = true;
        return;
    }

    if(!tmp.empty()) {
        patterns.push_back(std::make_pair(0, tmp));
        tmp.clear();
    }

    static std::map<std::string, std::function<FormatItem::ptr(const std::string& str)>> s_format_items = {
#define XX(str, C) {#str, [](const std::string& fmt) { return FormatItem::ptr(new C(fmt)); }}
        XX(m, MessageFormatItem),           // m:消息
        XX(p, LevelFormatItem),             // p:日志级别
        XX(c, LoggerNameFormatItem),        // c:日志器名称
        XX(r, ElapseFormatItem),            // r:累计毫秒数
        XX(f, FileNameFormatItem),          // f:文件名
        XX(l, LineFormatItem),              // l:行号
        XX(t, ThreadIdFormatItem),          // t:编程号
        XX(F, FiberIdFormatItem),           // F:协程号
        XX(N, ThreadNameFormatItem),        // N:线程名称
        XX(%, PercentSignFormatItem),       // %:百分号
        XX(T, TabFormatItem),               // T:制表符
        XX(n, NewLineFormatItem),           // n:换行符
#undef XX
    };

    for (auto& x : patterns) {
        if(x.first == 0) {
            items_.push_back(FormatItem::ptr(new StringFormatItem(x.second)));
        } else if (x.second =="d") {
            items_.push_back(FormatItem::ptr(new DateTimeFormatItem(date_format)));
        } else {
            auto it = s_format_items.find(x.second);
            if(it == s_format_items.end()) {
                std::cout << "[ERROR] LogFormatter::init() " << "pattern: [" << pattern_ << "] " << 
                "unknown format item: " << x.second << std::endl;
                error = true;
                break;
            } else {
                items_.push_back(it->second(x.second));
            }
        }
    }
}

std::string LogFormatter::format(LogEvent::ptr event) {
    std::stringstream ss;
    for(auto &i : items_) {
        i->format(ss, event);
    }
    return ss.str();
}

std::ostream& LogFormatter::format(std::ostream& os, LogEvent::ptr event) {
    for(auto &i:items_) {
        i->format(os, event);
    }
    return os;
}

LogAppender::LogAppender(LogFormatter::ptr default_formatter) 
    : default_formatter_(default_formatter) {
}

void LogAppender::setFormatter(LogFormatter::ptr val) {
    std::lock_guard<std::mutex> locker(mtx_);
    formatter_ = val;
}

LogFormatter::ptr LogAppender::getFormatter() {
    std::lock_guard<std::mutex> locker(mtx_);
    return formatter_ ? formatter_ : default_formatter_;
}

StdoutLogAppender::StdoutLogAppender() 
    : LogAppender(LogFormatter::ptr(new LogFormatter())){
}

void StdoutLogAppender::log(LogEvent::ptr event) {
    if (formatter_) {
        formatter_->format(std::cout, event);
    } else {
        default_formatter_->format(std::cout, event);
    }
}

FileLogAppender::FileLogAppender(const std::string& file) 
    : LogAppender(LogFormatter::ptr(new LogFormatter()))
    , filename_(file) {
    reopen();
    if(reopen_error_) {
        std::cout << "reopen file " << filename_ << " error" << std::endl;
    }
}

void FileLogAppender::log(LogEvent::ptr event) {
    uint64_t now = event->getTime();
    if (now > last_time_ + 3) {
        reopen();
        if(reopen_error_) {
            std::cout << "reopen file " << filename_ << " error" << std::endl;
        }  
        last_time_ = now;      
    }
    if (reopen_error_) {
        return;
    }
    std::lock_guard<std::mutex> locker(mtx_);
    if (formatter_) {
        if(!formatter_->format(filestream_, event)) {
            std::cout << "[ERROR] FileLogAppender::log() format error" << std::endl;
        }
    } else {
        if(!default_formatter_->format(filestream_, event)) {
            std::cout << "[ERROR] FileLogAppender::log() format error" << std::endl;
        }
    }
}

bool FileLogAppender::reopen() {
    std::lock_guard<std::mutex> locker(mtx_);
    if (filestream_) {
        filestream_.close();
    }
    filestream_.open(filename_, std::ios::app);
    reopen_error_ = !filestream_;
    return !reopen_error_;
}

Logger::Logger(const std::string &name) 
    : name_(name)
    , level_(LogLevel::INFO)
    , create_time_(server::GetElapsedMS()){
}

void Logger::addAppender(LogAppender::ptr appender) {
    std::lock_guard<std::mutex> locker(mtx_);
    appenders_.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
    std::lock_guard<std::mutex> locker(mtx_);
    for (auto it = appenders_.begin(); it != appenders_.end(); it++) {
        if (*it == appender) {
            appenders_.erase(it);
            break;
        }
    }
}

void Logger::clearAppender() {
    std::lock_guard<std::mutex> locker(mtx_);
    appenders_.clear();
}

void Logger::log(LogEvent::ptr event) {
    if (event->getLevel() <= level_) {
        for (auto& it : appenders_) {
            it->log(event);
        }
    }
}

LogEventWrap::LogEventWrap(Logger::ptr logger, LogEvent::ptr event) 
    : logger_(logger)
    , event_(event) {
}

LogEventWrap::~LogEventWrap() {
     logger_->log(event_);
}

LoggerManager::LoggerManager() {
    root_.reset(new Logger("root"));
    root_->addAppender(LogAppender::ptr(new StdoutLogAppender));
    loggers_[root_->getLogName()] = root_;
    init();
}

void LoggerManager::init() {
    // TODO
}

Logger::ptr LoggerManager::getLogger(const std::string &name) {
    std::lock_guard<std::mutex> locker(mtx_);
    auto it = loggers_.find(name);
    if(it != loggers_.end()) {
        return it->second;
    }
    Logger::ptr logger(new Logger(name));
    loggers_[name] = logger;
    return logger;
}

} // namespace server