#include "../src/log.h"
#include <unistd.h>

using namespace server;

Logger::ptr g_logger = LOG_ROOT(); // 默认INFO级别

int main(int argc, char const *argv[]) {

    LOG_FATAL(g_logger) << "fatal msg";
    LOG_ERROR(g_logger) << "err msg";
    LOG_INFO(g_logger) << "info msg";
    LOG_DEBUG(g_logger) << "debug msg";

    LOG_FMT_FATAL(g_logger, "fatal %s:%d", __FILE__, __LINE__);
    LOG_FMT_ERROR(g_logger, "err %s:%d", __FILE__, __LINE__);
    LOG_FMT_INFO(g_logger, "info %s:%d", __FILE__, __LINE__);
    LOG_FMT_DEBUG(g_logger, "debug %s:%d", __FILE__, __LINE__);

    sleep(1);
    SetThreadName("brand_new_thread");

    g_logger->setLevel(LogLevel::WARN);
    LOG_FATAL(g_logger) << "fatal msg";
    LOG_ERROR(g_logger) << "err msg";
    LOG_INFO(g_logger) << "info msg"; // 不打印
    LOG_DEBUG(g_logger) << "debug msg"; // 不打印

    FileLogAppender::ptr fileAppender(new server::FileLogAppender("./log.txt"));
    g_logger->addAppender(fileAppender);
    LOG_FATAL(g_logger) << "fatal msg";
    LOG_ERROR(g_logger) << "err msg";
    LOG_INFO(g_logger) << "info msg"; // 不打印
    LOG_DEBUG(g_logger) << "debug msg"; // 不打印

    server::Logger::ptr test_logger = LOG_NAME("test_logger");
    server::StdoutLogAppender::ptr appender(new server::StdoutLogAppender);
    server::LogFormatter::ptr formatter(new server::LogFormatter("%d:%rms%T%p%T%c%T%f:%l %m%n")); // 时间：启动毫秒数 级别 日志名称 文件名：行号 消息 换行
    appender->setFormatter(formatter);
    test_logger->addAppender(appender);
    test_logger->setLevel(server::LogLevel::WARN);

    LOG_ERROR(test_logger) << "err msg";
    LOG_INFO(test_logger) << "info msg"; // 不打印

    return 0;
}
