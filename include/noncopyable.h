#pragma once

namespace muduo
{
    //继承此类后，则派生类不能被拷贝构造和赋值构造，能正常构造和析构
    class noncopyable
    {
    public:
        noncopyable &operator=(const noncopyable &) = delete;
        noncopyable(const noncopyable &) = delete;

    protected:
        noncopyable() = default;
        ~noncopyable() = default;
    };
}
