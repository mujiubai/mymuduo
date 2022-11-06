#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

using namespace muduo;

/**
 * @brief 从fd上读取数据  低层的Poller工作在LT模式
 * buffer缓冲区有大小，但是从fd上读数据时，却不知道tcp数据最终的大小，
 * 如果不断将buffer进行扩容，效率很低;而如果预先就设为很大空间，当用户数量很多时，内存容量消耗极大
 * 因此，使用一个栈上临时变量来存储现有缓冲区不够存的数据，最后将其写入缓冲区中
 * @param fd sockfd
 * @param saveErrno 错误
 * @return size_t 成功读取大小
 */
size_t Buffer::readFd(int fd, int* saveErrno) {
  char extrabuf[65536] = {0};  //栈上空间 64k
  struct iovec vec[2];
  // buffer缓冲区剩余的可写空间大小
  const size_t writeable = writeableBytes();
  vec[0].iov_base = beginWrite();
  vec[0].iov_len = writeable;

  vec[1].iov_base = extrabuf;
  vec[1].iov_len = sizeof extrabuf;

  /**************************************************************************/
  //当缓冲区可写空间小于extrabuf时，使用buffer和extrabuf一起作为readv写入的空间
  //反之，如果可写空间大于extrabuf空间时，就没有必要再使用一个extrabuf来作为暂存了
  //因为buffer在写入数据时，如果空间不够会自动扩容为原来2倍，而即使加上一个extrabuf能存储的数据也没有扩容后的大
  //这样效率效率还没有直接使用一个buffer让其自动扩容效率高，毕竟buffer
  //resize时也会进行拷贝复制
  //使用extra就会在readv中写入一次数据，还需再将其拷贝到buffer中，而不使用的话就是只有readv将数据写入到buf中
  // extrabuf的使用原因是为了避免buffer不断扩容而导致效率低
  //这里不用担心如果空间不够存，使用的是LT模式，没读完的数据后面会不断唤醒读
  const int iovcnt = (writeable < sizeof extrabuf) ? 2 : 1;
  // readv能自动将数据写入到多个缓冲区中，缓冲区都在vec数组中
  const size_t n = ::readv(fd, vec, iovcnt);
  if (n < 0) {
    *saveErrno = errno;
  } else if (n <= writeable) {  // buffer的缓冲区够存储数据
    writerIndex_ += n;
  } else {  // extrabuf里也写入了数据
    writerIndex_ = buffer_.size();
    //将extrabuf追加到buffer中
    append(extrabuf, n - writeable);
  }
  return n;
}

size_t Buffer::writeFd(int fd, int* saveErrno) {
  ssize_t n = ::write(fd, peek(), readableBytes());
  if (n <0) {
    *saveErrno = errno;
  }
  return n;
}