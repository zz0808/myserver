#include "log.h"

namespace server {

std::string LogLevel::ToString(LogLevel::Level level) {
    switch(level) {
#define XX(level) case LogLevel::level: return #level;
    XX(INFO);
    XX(DEBUG);
    XX(WARN);
    XX(ERROR);
    XX(FATAL);
#undef XX
    default:
        return "UNKNOWN";
    }
    return "UNKNOWN";
}

LogLevel::Level LogLevel::FromString(const std::string& str) {
#define XX(level, v) if (str == #v) return LogLevel::level;
    XX(INFO, INFO);
    XX(DEBUG, DEBUG);
    XX(WARN, WARN);
    XX(ERROR, ERROR);
    XX(FATAL, FATAL);

    XX(INFO, info);
    XX(DEBUG, debug);
    XX(WARN, warn);
    XX(ERROR, error);
    XX(FATAL, fatal);
#undef XX
    return LogLevel::UNKNOWN;
}

LogEvent::LogEvent(const std::string& filename, LogLevel::Level level, uint32_t line, uint32_t thread_id, 
    uint64_t fiber_id, uint64_t elapse, time_t time, const std::string& log_file, const std::string thread_name)
    : filename_(filename), level_(level), line_(line), thread_id_(thread_id), fiber_id_(fiber_id)
    , elapse_(elapse), time_(time), log_file_(log_file), thread_name_(thread_name) {
}

LogFormatter::LogFormatter(const std::string& pattern)
    : pattern_(pattern), is_error_(false) {
    init();
}

void LogFormatter::init() {
    std::vector<std::pair<int, std::string>> patterns;
    std::string temp;
    std::string date_format;
    bool parse_format = false;
    uint32_t i = 0;

    while (i < pattern_.size()) {
        char c = pattern_[i];
        if (c == '%') {
            if (parse_format) { // parse %
                parse_format = false;
                patterns.push_back(std::make_pair(1, std::string(1, c)));
            } else {
                parse_format = true;
                if (!temp.empty()) {
                    patterns.push_back(std::make_pair(0, temp));
                    temp.clear();
                }
            }
            i++;
        } else {
            if (parse_format) {
                patterns.push_back(std::make_pair(1, std::string(1, c)));
                parse_format = false;
                i++;
                if (c != 'd') {
                    continue;
                }
                if (i >= pattern_.size() || pattern_[i] != '{') {
                    is_error_ = true;
                    break;
                }
                while (i < pattern_.size() && pattern_[i] != '}') {
                    date_format.push_back(pattern_[i]);
                    i++;
                }
                if (pattern_[i] != '}') {
                    is_error_ = true;
                    break;
                }
            } else {
                temp.push_back(c);
                i++;
            }
        }
    }
}

} // namespace server