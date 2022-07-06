#include "log.h"

namespace server {

const char* LogLevel::ToString(LogLevel::Level level) {

    switch (level) {
#define XX(name) case LogLevel::name: return #name; break;
        XX(FATAL);
        XX(ALERT);
        XX(CRIT);
        XX(ERROR);
        XX(WARN);
        XX(NOTICE);
        XX(INFO);
        XX(DEBUG);
        XX(NOTSET);
#undef XX
        default:
            return "NOTSET"; break;
    }
    return "NOTSET";
}

LogLevel::Level LogLevel::FromString(const std::string& str) {
#define XX(level, name) if (str == #name) { return LogLevel::level; }
    XX(FATAL, FATAL);
    XX(ALERT,ALERT);
    XX(CRIT, CRIT);
    XX(ERROR, ERROR);
    XX(WARN, WARN);
    XX(NOTICE, NOTICE);
    XX(INFO, INFO);
    XX(DEBUG, DEBUG);
    XX(NOTSET, NOTSET);   
#undef XX

    return LogLevel::NOTSET;
}

LogEvent::LogEvent(const std::string& logger_name, LogLevel::Level level, const char* file, uint32_t line, uint64_t elapse
                  , uint32_t thread_id, uint64_t fiber_id, time_t time, const std::string& thread_name) 
                  : level_(level), file_(file), line_(line), elapse_(elapse), thread_id_(thread_id), fiber_id_(fiber_id)
                  , time_(time), thread_name_(thread_name), logger_name_(logger_name) {
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

LogFormatter::LogFormatter(const std::string &pattern)
    : pattern_(pattern) {
    init();
}

class MessageFormatItem : public LogFormatter::FormatItem {
public:
    MessageFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->GetContent();
    }
};

class LevelFormatItem : public LogFormatter::FormatItem {
public:
    LevelFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << LogLevel::ToString(event->GetLevel());
    }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
public:
    ElapseFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->GetElapse();
    }
};

class LoggerNameFormatItem : public LogFormatter::FormatItem {
public:
    LoggerNameFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->GetLoggerName();
    }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
public:
    ThreadIdFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->GetThreadId();
    }
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
public:
    FiberIdFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->GetFiberId();
    }
};

class ThreadNameFormatItem : public LogFormatter::FormatItem {
public:
    ThreadNameFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->GetThreadName();
    }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
public:
    DateTimeFormatItem(const std::string& format = "%Y-%m-%d %H:%M:%S")
        :m_format(format) {
        if(m_format.empty()) {
            m_format = "%Y-%m-%d %H:%M:%S";
        }
    }

    void format(std::ostream& os, LogEvent::ptr event) override {
        struct tm tm;
        time_t time = event->GetTime();
        localtime_r(&time, &tm);
        char buf[64];
        strftime(buf, sizeof(buf), m_format.c_str(), &tm);
        os << buf;
    }
private:
    std::string m_format;
};

class FileNameFormatItem : public LogFormatter::FormatItem {
public:
    FileNameFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->GetFile();
    }
};

class LineFormatItem : public LogFormatter::FormatItem {
public:
    LineFormatItem(const std::string &str) {}
    void format(std::ostream &os, LogEvent::ptr event) override {
        os << event->GetLine();
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
        :string_(str) {}
    void format(std::ostream& os, LogEvent::ptr event) override {
        os << string_;
    }
private:
    std::string string_;
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

void LogFormatter::init() {
    std::vector<std::pair<int, std::string>> patterns;
    // 临时存储常规字符串
    std::string tmp;
    // 日期格式字符串，默认把位于%d后面的大括号对里的全部字符都当作格式字符，不校验格式是否合法
    std::string dateformat;
    // 是否解析出错
    bool error = false;
    
    // 是否正在解析常规字符，初始时为true
    bool parsing_string = true;
    // 是否正在解析模板字符，%后面的是模板字符
    // bool parsing_pattern = false;

    size_t i = 0;
    while(i < pattern_.size()) {
        std::string c = std::string(1, pattern_[i]);
        if(c == "%") {
            if(parsing_string) {
                if(!tmp.empty()) {
                    patterns.push_back(std::make_pair(0, tmp));
                }
                tmp.clear();
                parsing_string = false; // 在解析常规字符时遇到%，表示开始解析模板字符
                // parsing_pattern = true;
                i++;
                continue;
            } else {
                patterns.push_back(std::make_pair(1, c));
                parsing_string = true; // 在解析模板字符时遇到%，表示这里是一个%转义
                // parsing_pattern = false;
                i++;
                continue;
            }
        } else { // not %
            if(parsing_string) { // 持续解析常规字符直到遇到%，解析出的字符串作为一个常规字符串加入patterns
                tmp += c;
                i++;
                continue;
            } else { // 模板字符，直接添加到patterns中，添加完成后，状态变为解析常规字符，%d特殊处理
                patterns.push_back(std::make_pair(1, c));
                parsing_string = true; 
                // parsing_pattern = false;

                // 后面是对%d的特殊处理，如果%d后面直接跟了一对大括号，那么把大括号里面的内容提取出来作为dateformat
                if(c != "d") {
                    i++;
                    continue;
                }
                i++;
                if(i < pattern_.size() && pattern_[i] != '{') {
                    continue;
                }
                i++;
                while( i < pattern_.size() && pattern_[i] != '}') {
                    dateformat.push_back(pattern_[i]);
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
    } // end while(i < m_pattern.size())

    if(error) {
        error_ = true;
        return;
    }

    // 模板解析结束之后剩余的常规字符也要算进去
    if(!tmp.empty()) {
        patterns.push_back(std::make_pair(0, tmp));
        tmp.clear();
    }

    // for debug 
    // std::cout << "patterns:" << std::endl;
    // for(auto &v : patterns) {
    //     std::cout << "type = " << v.first << ", value = " << v.second << std::endl;
    // }
    // std::cout << "dataformat = " << dateformat << std::endl;
    
    static std::map<std::string, std::function<FormatItem::ptr(const std::string& str)> > s_format_items = {
#define XX(str, C)  {#str, [](const std::string& fmt) { return FormatItem::ptr(new C(fmt));} }

        XX(m, MessageFormatItem),           // m:消息
        XX(p, LevelFormatItem),             // p:日志级别
        XX(c, LoggerNameFormatItem),        // c:日志器名称
//        XX(d, DateTimeFormatItem),          // d:日期时间
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

    for(auto &v : patterns) {
        if(v.first == 0) {
            items_.push_back(FormatItem::ptr(new StringFormatItem(v.second)));
        } else if( v.second =="d") {
            items_.push_back(FormatItem::ptr(new DateTimeFormatItem(dateformat)));
        } else {
            auto it = s_format_items.find(v.second);
            if(it == s_format_items.end()) {
                std::cout << "[ERROR] LogFormatter::init() " << "pattern: [" << pattern_ << "] " << 
                "unknown format item: " << v.second << std::endl;
                error = true;
                break;
            } else {
                items_.push_back(it->second(v.second));
            }
        }
    }

    if(error) {
        error_ = true;
        return;
    }
}

std::string LogFormatter::format(LogEvent::ptr event) {
    std::stringstream ss;
    for (auto& i : items_) {
        i->format(ss, event);
    }
    return ss.str();
}

std::ostream& LogFormatter::format(std::ostream &os, LogEvent::ptr event) {
    for (auto& i : items_) {
        i->format(os, event);
    }
    return os;
}

LogAppender::LogAppender(LogFormatter::ptr default_formatter)
    : default_formatter_(default_formatter) {
}

void LogAppender::SetLogFormatter(LogFormatter::ptr val) {
    std::lock_guard<std::mutex> locker(mtx_);
    formatter_ = val;
}

LogFormatter::ptr LogAppender::GetLogFormatter() {
    std::lock_guard<std::mutex> locker(mtx_);
    if (formatter_) return formatter_; 
    return default_formatter_;
}

StdoutLogAppender::StdoutLogAppender() 
    : LogAppender(LogFormatter::ptr(new LogFormatter)){
}

void StdoutLogAppender::log(LogEvent::ptr event) {
    if (formatter_) {
        formatter_->format(event);
    } else {
        default_formatter_->format(event);
    }
}

FileLogAppender::FileLogAppender() 
    : LogAppender(LogFormatter::ptr(new LogFormatter)){
}

void FileLogAppender::log(LogEvent::ptr event) {
    uint64_t now_time = event->GetTime();
    if (now_time >= last_open_time_ + 3) {
        reopen();
        if (reopen_error_) {
            std::cout << "log file " << file_name_ << " open failed" << std::endl;
        }
        last_open_time_ = now_time;
    }
    if (reopen_error_) {
        return;
    }
    std::lock_guard<std::mutex> locker(mtx_);
    if(formatter_) {
        if(!formatter_->format(file_stream_, event)) {
            std::cout << "[ERROR] FileLogAppender::log() format error" << std::endl;
        }
    } else {
        if(!default_formatter_->format(file_stream_, event)) {
            std::cout << "[ERROR] FileLogAppender::log() format error" << std::endl;
        }
    }
}

bool FileLogAppender::reopen() {
    std::lock_guard<std::mutex> locker(mtx_);
    if (file_stream_) {
        file_stream_.close();
    }
    file_stream_.open(file_name_, std::ios::app);
    reopen_error_ = !file_stream_;
    return !reopen_error_;
}

Logger::Logger(const std::string &name) 
    : log_name_(name)
    , level_(LogLevel::INFO) 
    , create_time_(GetElapsedMS()){
}

void Logger::addAppender(LogAppender::ptr appender) {
    std::lock_guard<std::mutex> locker(mtx_);
    appenders_.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
    std::lock_guard<std::mutex> locker(mtx_);
    for (auto it = appenders_.begin(); it != appenders_.end(); it++) {
        if (appender == *it) {
            appenders_.erase(it);
            break;
        }
    }
}

void Logger::clearAppenders() {
    std::lock_guard<std::mutex> locker(mtx_);
    appenders_.clear();
}

void Logger::log(LogEvent::ptr event) {
    if (event->GetLevel() <= level_) {
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

}