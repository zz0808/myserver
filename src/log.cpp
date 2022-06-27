#include "log.h"

namespace dserver {

LogLevel::Level LogLevel::FromString(const std::string& str) {
#define XX(level, v) if (str == #v) { return LogLevel::level; }
    XX(FATAL, FATAL);
    XX(ALERT, ALERT);
    XX(CRIT, CRIT);
    XX(ERROR, ERROR);
    XX(WARN, WARN);
    XX(NOTICE, NOTICE);
    XX(FATAL, FATAL);
    XX(INFO, INFO);
    XX(DEBUG, DEBUG);
#undef XX

    return LogLevel::NOTSET;
}

const char* LogLevel::ToString(Level level) {
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
    default:
        return "NOSET";
    }
    return "NOSET";
}

LogEvent::LogEvent(const std::string& log_name, LogLevel::Level level, const char* file, uint32_t line, 
                  uint64_t elapse, uint32_t thread_id, uint64_t fiber_id, time_t time, 
                  const std::string& thread_name)
                : level_(level)
                , file_(file)
                , line_(line)
                , elapse_(elapse)
                , thread_id_(thread_id)
                , fiber_id_(fiber_id)
                , time_(time)
                , thread_name_(thread_name)
                , logger_name_(log_name)  {
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
        os << event->GetLogName();
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
        :format_(format) {
        if(format_.empty()) {
            format_ = "%Y-%m-%d %H:%M:%S";
        }
    }

    void format(std::ostream& os, LogEvent::ptr event) override {
        struct tm tm;
        time_t time = event->GetTime();
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

LogFormatter::LogFormatter(const std::string &pattern)
    : pattern_(pattern) {
    init();
}

void LogFormatter::init() {
    std::vector<std::pair<int, std::string>> patterns;
    // 临时存储常规字符串
    std::string tmp;
    // 日期格式字符串，默认把位于%d后面的大括号对里的全部字符都当作格式字符，不校验格式是否合法
    std::string dateformat;
    bool error = false;

    // 是否正在解析常规字符，初始时为true
    bool parsing_string = true;

    uint32_t i = 0;
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
    } // end whi

    if(error) {
        error_ = true;
        return;
    }

    // 模板解析结束之后剩余的常规字符也要算进去
    if(!tmp.empty()) {
        patterns.push_back(std::make_pair(0, tmp));
        tmp.clear();
    }

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
    for (auto& item : items_) {
        item->format(ss, event);
    }
    return ss.str();
}

std::ostream& LogFormatter::format(std::ostream &os, LogEvent::ptr event) {
    for(auto& item : items_) {
        item->format(os, event);
    }
    return os;
}

LogAppender::LogAppender(LogFormatter::ptr default_formatter)
    : default_formatter_(default_formatter) {}

void LogAppender::SetFormatter(LogFormatter::ptr val) {
    std::lock_guard<std::mutex> locker(mtx_);
    formatter_ = val;
}

LogFormatter::ptr LogAppender::GetFormatter() {
    std::lock_guard<std::mutex> locker(mtx_);
    return formatter_ ? formatter_ : default_formatter_;
}

StdoutLogAppender::StdoutLogAppender() 
    : LogAppender(LogFormatter::ptr(new LogFormatter)) {
}

void StdoutLogAppender::log(LogEvent::ptr event) {
    if (formatter_) {
        formatter_->format(std::cout, event);
    } else {
        default_formatter_->format(std::cout, event);
    }
}

std::string StdoutLogAppender::toYamlString() {
    std::lock_guard<std::mutex> locker(mtx_);
    YAML::Node node;
    node["type"] = "StdoutLogAppender";
    node["pattern"] = formatter_->GetPattern();
    std::stringstream ss;
    ss << node;
    return ss.str();
}

FileLogAppender::FileLogAppender(const std::string& file)
    : LogAppender(LogFormatter::ptr(new LogFormatter)), file_name_(file) {
    reopen();
    if(reopen_error_) {
        std::cout << "reopen file " << file_name_ << " error" << std::endl;
    }
}

void FileLogAppender::log(LogEvent::ptr event) {
    uint64_t now = event->GetTime();
    if (now >= last_time_ + 3) {
        reopen();
        if(reopen_error_) {
            std::cout << "reopen file " << file_name_ << " error" << std::endl;
        }
        last_time_ = now;
    }
    if(reopen_error_) {
        return;
    }
    std::lock_guard<std::mutex> locker(mtx_);
    if (formatter_) {
        if (!formatter_->format(file_stream_, event)) {
            std::cout << "[ERROR] FileLogAppender::log() format error" << std::endl;
        }
    } else {
        if (!default_formatter_->format(file_stream_, event)) {
            std::cout << "[ERROR] FileLogAppender::log() format error" << std::endl;
        }
    }
}

std::string FileLogAppender::toYamlString() {
    std::lock_guard<std::mutex> locker(mtx_);
    YAML::Node node;
    node["type"] = "FileLogAppender";
    node["file"] = file_name_;
    node["pattern"] =  formatter_ ? formatter_->GetPattern() : default_formatter_->GetPattern();
    std::stringstream ss;
    ss << node;
    return ss.str();
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

Logger::Logger(const std::string& name) 
    : name_(name)
    , level_(LogLevel::INFO)
    , create_time_(GetElapsedMS()){
}

void Logger::add_appender(LogAppender::ptr appender) {
    std::lock_guard<std::mutex> locker(mtx_);
    appenders_.push_back(appender);
}

void Logger::del_appender(LogAppender::ptr appender) {
    std::lock_guard<std::mutex> locker(mtx_);
    auto it = appenders_.begin();
    while (it != appenders_.end()) {
        if (*it == appender) {
            appenders_.erase(it);
            break;
        }
        it++;
    }
}

void Logger::clear_appender() {
    std::lock_guard<std::mutex> locker(mtx_);
    appenders_.clear();
}

/**
 * 调用Logger的所有appenders将日志写一遍，
 * Logger至少要有一个appender，否则没有输出
 */
void Logger::log(LogEvent::ptr event) {
    if (event->GetLevel() >= level_) {
        return;
    }
    for (auto& it : appenders_) {
        it->log(event);
    }
}

std::string Logger::ToYamlString() {
    std::lock_guard<std::mutex> locker(mtx_);

    YAML::Node node;
    node["name"] = name_;
    node["level"] = LogLevel::ToString(level_);
    for(auto &i : appenders_) {
        node["appenders"].push_back(YAML::Load(i->toYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

LogEventWrap::LogEventWrap(Logger::ptr logger, LogEvent::ptr event) 
    : logger_(logger)
    , event_(event) {
}

LogEventWrap::~LogEventWrap() {
    logger_->log(event_);
}

LoggerManager::LoggerManager() {
    root_logger_.reset(new Logger("root"));
    root_logger_->add_appender(LogAppender::ptr(new StdoutLogAppender));
    loggers_[root_logger_->GetName()] = root_logger_;
    init();
}

void LoggerManager::init() {
    // TODO
}

Logger::ptr LoggerManager::GetLogger(const std::string& name) {
    std::lock_guard<std::mutex> locker(mtx_);

    auto it = loggers_.find(name);
    if (it != loggers_.end()) {
        return it->second;
    }
    Logger::ptr new_logger(new Logger(name));
    loggers_[name] = new_logger;
    return new_logger;
}

std::string LoggerManager::ToYamlString() {
    std::lock_guard<std::mutex> locker(mtx_);

    YAML::Node node;
    for(auto& i : loggers_) {
        node.push_back(YAML::Load(i.second->ToYamlString()));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
}

// 从配置文件中加载日志配置
/**
 * @brief 日志输出器配置结构体定义
 */
struct LogAppenderDefine {
    int type = 0;
    std::string pattern;
    std::string file;

    bool operator==(LogAppenderDefine& other) const {
        return type == other.type && pattern == other.pattern && file == other.file;
    }
};

/**
 * @brief 日志器配置结构体定义
 */
struct LogDefine {
    std::string name;
    LogLevel::Level level = LogLevel::NOTSET;
    std::vector<LogAppenderDefine> appenders;

    bool operator==(const LogDefine &oth) const {
        return name == oth.name && level == oth.level && appenders == appenders;
    }

    bool operator<(const LogDefine &oth) const {
        return name < oth.name;
    }

    bool isValid() const {
        return !name.empty();
    }
};

} // namespace dserver