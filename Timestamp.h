#pragma once

#include<iostream>
#include<string>

namespace muduo{
    class Timestamp
    {
    private:
        int64_t microSecondsSinceEpoch_;
    public:
        Timestamp():microSecondsSinceEpoch_(0){};
        explicit Timestamp(int64_t microSecondsSinceEpoch):microSecondsSinceEpoch_(microSecondsSinceEpoch){};
        ~Timestamp(){};
        static Timestamp now();
        std::string toString() const;
    };
    
}