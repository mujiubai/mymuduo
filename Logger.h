#pragma once

#include "noncopyable.h"

#include <string>

namespace muduo
{
    //LOG_INFO("%s %d",arg1,arg2),##__VA__ARGS__为获取可变参列表宏
#define LOG_INFO(logmsgFormat, ...)                        \
    do                                                     \
    {                                                      \
        Logger &logger = Logger::instance();               \
        logger.setLogLevel(INFO);                          \
        char buf[1024] = {0};                              \
        snprintf(buf, 1024, logmsgFormat, ##__VA__ARGS__); \
        logger.log(buf);                                   \
    } while (0)

#define LOG_ERROR(logmsgFormat, ...)                       \
    do                                                     \
    {                                                      \
        Logger &logger = Logger::instance();               \
        logger.setLogLevel(ERROR);                         \
        char buf[1024] = {0};                              \
        snprintf(buf, 1024, logmsgFormat, ##__VA__ARGS__); \
        logger.log(buf);                                   \
    } while (0)

#define LOG_FATAL(logmsgFormat, ...)                       \
    do                                                     \
    {                                                      \
        Logger &logger = Logger::instance();               \
        logger.setLogLevel(FATAL);                         \
        char buf[1024] = {0};                              \
        snprintf(buf, 1024, logmsgFormat, ##__VA__ARGS__); \
        logger.log(buf);                                   \
    } while (0)
#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...)                       \
    do                                                     \
    {                                                      \
        Logger &logger = Logger::instance();               \
        logger.setLogLevel(DEBUG);                         \
        char buf[1024] = {0};                              \
        snprintf(buf, 1024, logmsgFormat, ##__VA__ARGS__); \
        logger.log(buf);                                   \
    } while (0)
#else
#define LOG_DEBUG(logmsgFormat, ...)
#endif

    //定义日志级别
    enum
    {
        INFO,  //普通信息
        ERROR, //错误信息
        FATAL, //会导致程序崩溃的错误信息
        DEBUG  //debug信息
    };
    //日志类
    class Logger : noncopyable
    {
    public:
        void setLogLevel(int level);      //设置日志级别
        void log(const std::string &msg); //输出信息
        static Logger &instance();        //获取日志单例类
    private:
        int logLevel_;
        Logger(){};
    };
}