#pragma once

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

namespace muduo {

/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
//缓冲区类
class Buffer {
 public:
  static const size_t kCheapPrepend = 8;
  static const size_t kInitialSize = 1024;
  explicit Buffer(size_t initialSize = kInitialSize)
      : buffer_(kCheapPrepend + initialSize),
        readerIndex_(kCheapPrepend),
        writerIndex_(kCheapPrepend) {}

  //可读数据长度
  size_t readableBytes() const { return writerIndex_ - readerIndex_; }
  //可写数据长度
  size_t writeableBytes() const { return buffer_.size() - writerIndex_; }
  //预留空间长度，初始的预留空间是为消息长度而准备
  size_t prependableBytes() const { return readerIndex_; }

  //返回缓冲区中可读地址的起始地址
  const char* peek() const { return begin() + readerIndex_; }

  //读取len长度后调用此函数
  void retrieve(size_t len) {
    //如果数据没读完
    if (len < readableBytes()) {
      readerIndex_ += len;  //应用只读取了可读缓冲区的一部分，
    } else {
      retrieveAll();  //已经读完，需将缓冲区复位为初始状态
    }
  }
  void retrieveAll() { readerIndex_ = writerIndex_ = kCheapPrepend; }

  //把onMessage函数上报的buffer数据，转成string类型返回
  std::string retrieveAllAsString() {
    return retrieveAsString(readableBytes());
  }

  std::string retrieveAsString(size_t len) {
    std::string result(peek(), len);
    //上面已经把缓冲区的数据读出，需要对缓冲区进行复位操作
    retrieve(len);
    return result;
  }

  //确保len长度的数据能被写入
  void ensureWriteableBytes(size_t len) {
    if (writeableBytes() < len) {
      makeSpace(len);
    }
  }

  //从fd上读取数据
  size_t readFd(int fd, int* saveErrno);

 private:
  //返回buffer首元素地址，即数组的起始地址，&*不能抵消，因为*被重写过
  char* begin() { return &*buffer_.begin(); }

  const char* begin() const { return &*buffer_.begin(); }

  //通过整理buffer空间或增加buffer空间使得容量够写
  void makeSpace(size_t len) {
    //如果可写空间加上前面预留空间（预留空间可能由于依次没读完而边长）还是小于要求的长度
    if (writeableBytes() + prependableBytes() < len + kCheapPrepend) {
      buffer_.resize(len + len);
    } else {
      //如果数据空间够用，则将现有数据前移
      size_t readable = readableBytes();
      std::copy(begin() + readerIndex_, begin() + writeableBytes(),
                begin() + kCheapPrepend);
    }
  }

  //把data中的len长的数据添加到缓冲区中
  void append(const char* data, size_t len) {
    ensureWriteableBytes(len);
    std::copy(data, data + len, beginWrite());
    writerIndex_ += len;
  }

  //返回可写处的指针
  char* beginWrite() { return begin() + writerIndex_; }
  const char* beginWrite() const { return begin() + writerIndex_; }

  std::vector<char> buffer_;
  size_t readerIndex_;
  size_t writerIndex_;
};
}  // namespace muduo