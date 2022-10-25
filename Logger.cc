#include "Logger.h"
#include "Timestamp.h"

#include <iostream>

namespace muduo
{
    //设置日志级别
    void Logger::setLogLevel(int level)
    {
        logLevel_ = level;
    }
    //输出信息
    void Logger::log(const std::string &msg)
    {
        switch (logLevel_)
        {
        case INFO:
        {
            std::cout << "[INFO}";
            break;
        }
        case ERROR:
        {
            std::cout << "[ERROR]";
            break;
        }
        case FATAL:
        {
            std::cout << "[FATAL]";
            break;
        }
        case DEBUG:
        {
            std::cout << "[DEBUG]";
            break;
        }
        default:
            break;
        }

        std::cout <<Timestamp::now().toString()<< ": " << msg << std::endl;
    }
    //获取日志单例类
    Logger& Logger::instance()
    {
        static Logger logger;
        return logger;
    }
}