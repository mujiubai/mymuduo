*根据施磊**剖析muduo网络库核心代码**项目所写*, [视频地址](**https://fixbug.ke.qq.com/** )

----

---



# 1. 基础

**知识储备**

- 1、TCP协议和UDP协议
- 2、TCP编程和UDP编程步骤
- 3、IO复用接口编程select、poll、epoll编程
- 4、Linux的多线程编程pthread、进程和线程模型 C++20标准加入了协程的支持

## 1.1 **阻塞、非阻塞、同步、异步**

> 典型的一次IO的两个阶段是什么？ 数据准备 和 数据读写

 在数据准备上，根据系统IO操作的就绪状态可以分为：

- 阻塞：调用IO方法的线程会进入阻塞状态。比如epoll_wait（阻塞类sockfd），调用此方法，会使得线程被阻塞
- 非阻塞：调用IO方法的线程不会进入阻塞状态，不改变线程的状态，其通过调用方法返回值进行判断。

在数据读写上，根据应用程序和内核的交互方式可以分为：

- 同步：当前线程需要等待数据读写完成才能进行下一步操作。例如，当epoll_wait返回之后使用recv去读数据，此时需要等recv读完数据才能进行下一步操作
- 异步：当前线程将数据读写工作交给其他线程（如内核），读写操作完成后再通知当前线程，其无需等待当前线程完成就能进行下一步操作（在IO异步上一般都需要操作系统提供支持，如Linux下使用aio_read）。

**陈硕大神原话：在处理 IO 的时候，阻塞和非阻塞都是同步 IO。只有使用了特殊的 API 才是异步IO。**

![image-20221023200030489](https://cdn.jsdelivr.net/gh/mujiubai/piclib@main/picgo/image-20221023200030489.png)

**总结**：无论是同步异步、阻塞非阻塞，其差异都是等待任务完成或不等任务完成而是完成后发出通知提醒（等通知，这通知是系统级别实现，而无需死等。如果应用层面则也可以是回调）。

上面说的同步异步指的是IO层级，应用级别的同步异步也一样（比如A需要处理任务，A向B传入需要处理的任务和任务结束后的回调函数，B在任务处理完后进行回调）

> **作者总结**：
>
> - 一个典型的网络IO接口调用，分为两个阶段，分别是“数据就绪”和“数据读写”，数据就绪阶段分为阻塞和非阻塞，表现得结果就是，阻塞当前线程或是直接返回。
>
> - 同步表示A向B请求调用一个网络IO接口时（或者调用某个业务逻辑API接口时），数据的读写都是由请求方A自己来完成的（不管是阻塞还是非阻塞）；异步表示A向B请求调用一个网络IO接口时（或者调用某个业务逻辑API接口时），向B传入请求的事件以及事件发生时通知的方式，A就可以处理其它逻辑了，当B监听到事件处理完成后，会用事先约定好的通知方式，通知A处理结果。

## 1.2 **Unix/Linux上的五种IO模型**

**（同步）阻塞blocking**

![image-20221024105628921](https://cdn.jsdelivr.net/gh/mujiubai/piclib@main/picgo/image-20221024105628921.png)

**同步非阻塞 non-blocking**

![image-20221024105721152](https://cdn.jsdelivr.net/gh/mujiubai/piclib@main/picgo/image-20221024105721152.png)

**IO复用（IO multiplexing）**

![image-20221024105734893](https://cdn.jsdelivr.net/gh/mujiubai/piclib@main/picgo/image-20221024105734893.png)

**信号驱动（signal-driven）**

![image-20221024105744974](https://cdn.jsdelivr.net/gh/mujiubai/piclib@main/picgo/image-20221024105744974.png)

内核在第一个阶段是异步，在第二个阶段是同步；与非阻塞IO的区别在于它提供了消息通知机制，不需要用户进程不断的轮询检查，减少了系统API的调用次数，提高了效率。

**异步不阻塞（asynchronous）**

![image-20221024105758521](https://cdn.jsdelivr.net/gh/mujiubai/piclib@main/picgo/image-20221024105758521.png)

aio_read所使用的结构体

```c++
struct aiocb {
	int aio_fildes
	off_t aio_offset
	volatile void *aio_buf
	size_t aio_nbytes
	int aio_reqprio
	struct sigevent aio_sigevent //信号量
	int aio_lio_opcode
}
```

## 1.3 **好的网络服务器设计**

> 在这个多核时代，服务端网络编程如何选择线程模型呢？ 赞同libev作者的观点：one loop per thread is usually a good model，这样多线程服务端编程的问题就转换为如何设计一个高效且易于使用的event loop，然后每个线程run一个event loop就行了（当然线程间的同步、互斥少不了，还有其它的耗时事件需要起另外的线程来做）。

event loop 是 non-blocking 网络编程的核心，在现实生活中，non-blocking 几乎总是和 IO multiplexing 一起使用，原因有两点：

- 没有人真的会用轮询 (busy-pooling) 来检查某个 non-blocking IO 操作是否完成，这样太浪费

  CPU资源了。

- IO-multiplex 一般不能和 blocking IO 用在一起，因为 blocking IO 中read() /write() /accept() /connect() 都有可能阻塞当前线程，这样线程就没办法处理其他 socket上的 IO 事件了。

所以，当我们提到 non-blocking 的时候，实际上指的是 non-blocking + IO-multiplexing，单用其中任何一个都没有办法很好的实现功能。（这里的非阻塞主要指的还是异步，是指在当前线程中除了IO复用时进行阻塞，其他操作都不能阻塞当前线程，其实就是把处理事件交给其他线程

> epoll + fork不如epoll + pthread？
>
> 强大的nginx服务器采用了epoll+fork模型作为网络模块的架构设计，实现了简单好用的负载算法，使各个fork网络进程不会忙的越忙、闲的越闲，并且通过引入一把乐观锁解决了该模型导致的**服务器惊群**现象，功能十分强大。

## 1.4 **Reactor模型**

> The reactor design pattern is an event handling pattern for handling service requests delivered concurrently to a service handler by one or more inputs. The service handler then demultiplexes the incoming requests and dispatches them synchronously to the associated request handlers.

**重要组件**：**Event事件、Reactor反应堆、Demultiplex事件分发器（应该叫事件解码器，将多个事件进行聚合，如epoll）、Evanthandler事件处理器**

![image-20221024113236711](https://cdn.jsdelivr.net/gh/mujiubai/piclib@main/picgo/image-20221024113236711.png)

muduo库的Multiple Reactors模型如下：

![image-20221024113253413](https://cdn.jsdelivr.net/gh/mujiubai/piclib@main/picgo/image-20221024113253413.png)

在这个模型中，其实是将reactor和demultiplex都结合到reactor中，mainReactor将事件进一步细分到subReactor中（例如mainReactor负责新用户的链接，然后将这些链接分发到subReactor中）

## 1.5 **epoll**

**select的缺点**

- 1、单个进程能够监视的文件描述符的数量存在最大限制，通常是1024，当然可以更改数量，但由于select采用轮询的方式扫描文件描述符，文件描述符数量越多，性能越差；(在linux内核头文件中，有这样的定义：#define __FD_SETSIZE 1024
- 2、内核 / 用户空间内存拷贝问题，select需要复制大量的句柄数据结构，产生巨大的开销
- 3、select返回的是含有整个句柄的数组，应用程序需要遍历整个数组才能发现哪些句柄发生了事件
- 4、select的触发方式是水平触发，应用程序如果没有完成对一个已经就绪的文件描述符进行IO操作，那么之后每次select调用还是会将这些文件描述符通知进程（也不能算缺点，只是不支持ET）

相比select模型，poll使用链表保存文件描述符，因此没有了监视文件数量的限制，但其他三个缺点依

然存在。

> 以select模型为例，假设我们的服务器需要支持100万的并发连接，则在__FD_SETSIZE 为1024的情况下，则我们至少需要开辟1k个进程才能实现100万的并发连接。除了进程间上下文切换的时间消耗外，从内核/用户空间大量的句柄结构内存拷贝、数组轮询等，是系统难以承受的。因此，基于select模型的服务器程序，要达到100万级别的并发访问，是一个很难完成的任务。

**epoll原理以及优势**

> **设想一下如下场景**：有100万个客户端同时与一个服务器进程保持着TCP连接。而每一时刻，通常只有几百上千个TCP连接是活跃的(事实上大部分场景都是这种情况)。如何实现这样的高并发？
>
> 在select/poll时代，服务器进程每次都把这100万个连接告诉操作系统（从用户态复制句柄数据结构到内核态），让操作系统内核去查询这些套接字上是否有事件发生，轮询完成后，再将句柄数据复制到用户态，让服务器应用程序轮询处理已发生的网络事件，这一过程资源消耗较大，因此，select/poll一般只能处理几千的并发连接。

epoll的设计和实现与select完全不同。epoll通过在Linux内核中申请一个简易的文件系统（文件系统一般用什么数据结构实现？B+树，磁盘IO消耗低，效率很高）。把原先的select/poll调用分成以下3个部分：

- 调用epoll_create()建立一个epoll对象（在epoll文件系统中为这个句柄对象分配资源）
- 调用epoll_ctl向epoll对象中添加这100万个连接的套接字
- 调用epoll_wait收集发生的事件的fd资源

如此一来，要实现上面说是的场景，只需要在进程启动时建立一个epoll对象，然后在需要的时候向这个epoll对象中添加或者删除事件。

epoll_wait的效率也非常高，因为调用epoll_wait时，并没有向操作系统复制这100万个连接的句柄数据，内核也不需要去遍历全部的连接（猜测当内核得知某sockfd有消息时，会去epoll中找到对应的sockfd进行标记，而epoll的实现使得查找非常块）。

epoll_create在内核上创建的eventpoll结构如下：

```c++
struct eventpoll{
....
/*红黑树的根节点，这颗树中存储着所有添加到epoll中的需要监控的事件*/
struct rb_root rbr;
/*双链表中则存放着将要通过epoll_wait返回给用户的满足条件的事件*/
struct list_head rdlist;
....
};
```

**LT模式**

内核数据没被读完，就会一直上报数据。

**ET模式**

内核数据只上报一次。

**muduo采用的是LT**

- 不会丢失数据或者消息
  - 应用没有读取完数据，内核是会不断上报的
- 低延迟处理
  - 每次读数据只需要一次系统调用；照顾了多个连接的公平性，不会因为某个连接上的数据量过大而影响其他连接处理消息
- 跨平台处理
  - 像select一样可以跨平台使用



# 2. muduo

muduo主要分为了以下几大类：

- **Channel**：封装了每个sockfd设置感兴趣事件和对发生事件的处理，其成员主要包括fd、events、revents、callbacks 
- **Poller**：封装系统IO复用的通用接口，其主要成员包括ownerLoop、记录注册的channels
  - **EPollPoller**：Poller类的派生类，使用epoll实现了Poller类的各种接口，其成员包括epollfd、记录发生事件的events_
- **EventLoop**：是一个Reactor，其作为Poller类和Channel类的使用者，Poller和Channel的通信都是通过EventLoop来完成，其使用Poller类来获得发生事件的Channel，然后调用Channel的相应回调。
  - 主要成员包括poller、wakeupfd（唤醒当前Eventloop）、activeChannels_(记录发生事件的channel)、pendingFuctors_（当前需要执行的回调）。
- **Thread**：封装了线程创建、线程启动等线程基础操作
- **EventLoopThread**：将EventLoop和Thread绑定，封装成一个Thread执行一个loop
  - **EventLoopThreadPool**：EventLoopThread的线程池封装，使得mainLoop更方便进行subLoop任务的分发
- **Socket**：封装socket的常用操作，如设置非阻塞、监听、noDelay等，但并不包含创建sockfd
- **Acceptor**：主要封装了listenfd相关的操作，其监听新连接用户，并分发给subLoop
- **Buffer**：封装缓冲区常见操作，其内部是一个char类型的vector
- **TcpConnection**：封装一个连接信息，其内包含本地地址、对端地址、socket、channel、以及读写buffer
- **TcpServer**：上述所有类的综合使用者，用户通过使用此类来设置各种回调，通过此类来管理subLoop个数等



**读**：

- 设置读事件回调

![image-20221110214049385](C:\Users\MyPC\AppData\Roaming\Typora\typora-user-images\image-20221110214049385.png)

- 当读事件发生

![image-20221110214042301](C:\Users\MyPC\AppData\Roaming\Typora\typora-user-images\image-20221110214042301.png)

**写**：

- 发送数据

  ![image-20221110215931048](C:\Users\MyPC\AppData\Roaming\Typora\typora-user-images\image-20221110215931048.png)

- 当写事件发生：和读事件发生流程一样，只是最后一步执行的writeCallback

**新用户到来**：

![image-20221110214730307](C:\Users\MyPC\AppData\Roaming\Typora\typora-user-images\image-20221110214730307.png)

**服务器启动**

![image-20221110215755177](C:\Users\MyPC\AppData\Roaming\Typora\typora-user-images\image-20221110215755177.png)

流程图都在飞书的processon上



## 2.1 Channel

封装了每个sockfd设置感兴趣事件函数和对发生事件的处理，注意！Channel不直接与Poller打交道，其通过EventLoop来与Poller进行通信，比如向Poller中设置感兴趣事件，Poller返回事件等。

通过设置sockfd的各种回调，当有事件发生时，Poller调用每个事件对应channel的*HandleEvent*函数进行判断处理发生了哪些事件以及调用相应回调

**成员变量**

```c++
  //这三个是作为事件标识
  static const int kNoneEvent;   // 0
  static const int kReadEvent;   // EPOLLIN | EPOLLPRI
  static const int kWriteEvent;  // EPOLLOUT;

  EventLoop *loop_;  //事件循环
  const int fd_;     //监听的sockfd
  int revents_;      //返回的事件
  int events_;       // sockfd上监听的事件
  int index_;  // 此Poller的状态，如新添加、已删除等，主要是用于Poller中判断状态

  //绑定TcpConnection对象的弱引用，可用于判断对象是否存活和得到shared指针
  //用于观察注册回调的TcpConnection对象是否存活，避免调用回调函数时出错
  std::weak_ptr<void> tie_;
  bool tied_;  //是否绑定标志

  //用于处理revents的四种回调函数
  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
```

**成员函数**

```c++
 public:
  using EventCallback = std::function<void()>;
  using ReadEventCallback = std::function<void(Timestamp)>;
  Channel(EventLoop *loop, int fd);
  ~Channel();
  // fd得到poller通知以后，处理事件的函数
  void HandleEvent(Timestamp receiveTime);

  //设置回调函数
  void setReadCallback(ReadEventCallback cb) {
    readCallback_ = std::move(cb);
  }  //这里使用move感觉没啥用，除非function中有申请的资源
  void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

  //绑定TcpConnection对象，防止当TcpConnection对象被手动remove后，channel还在执行回调操作
  void tie(const std::shared_ptr<void> &);

  int fd() const { return fd_; };
  //获取当前设置的Event
  int events() const { return events_; }
  //供poller设置发生事件
  int setRevents(int revt) { revents_ = revt; }  

  //设置或取消读 写事件:
  void enableReading() {
    events_ |= kReadEvent;
    update();
  }
  void disableReading() {
    events_ &= ~kReadEvent;
    update();
  }
  void enableWriting() {
    events_ |= kWriteEvent;
    update();
  }
  void disableWriting() {
    events_ &= ~kWriteEvent;
    update();
  }
  void disableAll() {
    events_ = kNoneEvent;
    update();
  }

  //判断当前事件状态，是否设置读或写事件或无事件
  bool isWriting() const { return events_ & kWriteEvent; }
  bool isReading() const { return events_ & kReadEvent; }
  bool isNoneEvent() const { return events_ == kNoneEvent; }

  //得到Channel状态
  int index() const { return index_; }
  //设置Channel状态
  void setIndex(int index) { index_ = index; }

  //返回此channel所属的eventloop
  EventLoop *ownerLoop() { return loop_; }
  //在所属的eventloop移除此channel
  void remove();

 private:
  void update();  //更新poller中对应的channel的事件
  //安全的处理事件，作为handleEvent函数的底层函数
  void handleEventWithGuard(Timestamp receiveTime);
```

## 2.2 Poller

### 2.2.1 Poller基类

主要封装统一IO复用的接口，以便实现Poll、Epoll等。其成员比较简单，就一个channels记录管理的channel和ownerLoop记录所属的EventLoop

**成员变量**

```c++
 protected:
  // key:sockfd value:sockfd所属的channel通道类型
  using ChannelMap = std::unordered_map<int, Channel *>;
  ChannelMap channels_;//记录管理的Channel

 private:
  EventLoop *ownerLoop_;//记录poller绑定的EventLoop
```

**成员函数**

```c++
 public:
  using ChannelList = std::vector<Channel *>;
  Poller(EventLoop *loop);
  virtual ~Poller()=default;

  //统一IO复用接口，扩展类如select、poll和Epoll都必须实现这几个接口
  virtual Timestamp poll(int timeousMs, ChannelList *activeChannels) = 0;
  virtual void updateChannel(Channel *channel) = 0;
  virtual void removeChannel(Channel *channel) = 0;

  //判断channel是否存在此poller中
  bool hasChannel(Channel *channel) const;

  // eventloop通过该接口获取默认的IO复用的具体实现
  //为避免基类引用派生类头文件，此函数是在一个单独的cc文件中进行实现
  static Poller *newDefaultPoller(EventLoop *loop);
```

**关键**

hasChannel函数中通过判断channel指针避免了由于sockfd复用而导致的错乱情况

```c++
bool Poller::hasChannel(Channel *channel) const {
    auto it=channels_.find(channel->fd());
    //it->second==channel应该是为了避免sockfd被复用时而channel不对的情况
    return it!=channels_.end()&&it->second==channel;
}
```

### 2.2.2 EPollPoller

基于Poller接口实现了Epoll，相比Poller，主要增加了epollfd和events成员以及epoll常用函数的封装

**成员变量**

```c++
  using EventList = std::vector<epoll_event>;
  int epollfd_;//epoll的fd
  EventList events_;//记录epoll返回的发生事件
  static const int kInitEventListSize = 16;  //给events的初始长度
```

**成员函数**

```c++
 public:
  EPollPoller(EventLoop *loop);
  ~EPollPoller() override;
  
  //重写Poller的抽象方法
  Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
  void updateChannel(Channel *channel) override;
  void removeChannel(Channel *channel) override;

 private:
  //将poll返回的发生事件的channel写入到activeChannels中
  void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
  //更新channel通道设置的事件
  void update(int operation, Channel *channel);
```

## 2.3 EventLoop

EventLoop相当于提供了一个Reactor，通过Poller来获取发生的事件，再调用相应的channel的回调函数

**成员变量**

```c++
  using ChannelList = std::vector<Channel *>;

  std::atomic_bool looping_;  //原子操作，通过CAS实现
  std::atomic_bool quit_;     //标志是否退出loop循环
  const pid_t threadId_;      //当前loop所在线程id
  Timestamp pollReturnTime_;  // poller返回发生事件的时间点
  std::unique_ptr<Poller> poller_;

  //唤醒当前loop所在线程的fd
  int wakeupFd_;  //当mainLoop获取新用户channel时，通过轮询算法选择一个subloop，通过该成员唤醒subLoop处理channel
  std::unique_ptr<Channel> wakeupChanel_;  // wakeupfd_的channel的指针

  ChannelList activeChannels_;  //记录发生事件的所有channel
  Channel *currentActiveChannel_;

  std::atomic_bool callingPendingFuctors_;  //当前loop是否有需要执行的回调操作
  std::vector<Functor> pendingFuctors_;  //存储loop需要执行的所有回调操作
  std::mutex mutex_;  //保护pendingFuctors_的线程安全操作
```

**成员函数**

```c++
 public:
  using Functor = std::function<void()>;

  EventLoop();
  ~EventLoop();

  //开启事件循环
  void loop();
  //退出事件循环
  void quit();

  Timestamp pollReturnTime() const { return pollReturnTime_; }

  //在当前loop中执行
  void runInLoop(Functor cb);
  //把cb放入队列中，唤醒loop所在的线程，再执行cb
  void queueInLoop(Functor cb);

  //唤醒loop所在线程
  void wakeup();

  // eventloop使用poller对channel操作的方法
  void updateChannel(Channel *channel);
  void removeChannel(Channel *channel);
  bool hasChannel(Channel *channel);

  //判断eventloop对象是否在自己线程中
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

 private:
  void handleRead();         // wakeupfd_的读回调函数
  void doPendingFunctors();  //执行需要执行的回调操作
```

## 2.4 Thread



**成员变量**

```c++
  bool started_;//是否已经启动
  bool joined_;//是否join
  //使用智能指针，避免使用thread初始化是就开始启动线程
  std::shared_ptr<std::thread> thread_;
  pid_t tid_;//记录线程id
  ThreadFunc func_;//线程执行的回调函数
  std::string name_;//线程名

  static std::atomic_int numCreated_;//现有线程创建个数
```

**成员函数**

```c++
 public:
  using ThreadFunc = std::function<void()>;

  explicit Thread(ThreadFunc, const std::string& name = std::string());
  ~Thread();

  //启动线程
  void start();
  void join();

  //获取线程是否启动
  bool started() const { return started_; }
  //获取线程id
  pid_t tid() const { return tid_; }
  //获取设置的线程名
  const std::string& name() const { return name_; }

  //获取当前通过Thread创建的线程数量
  static int numCreated() { return numCreated_; }

 private:
  void setDefaultName();
```

**关键**

```c++
//一个thread对象记录的就是一个新线程的详细信息
void Thread::start() {
  started_ = true;
  sem_t sem;  //信号量 muduo书中说多线程别用信号量？？
  sem_init(&sem, false, 0);
  //开启线程，
  thread_ = std::shared_ptr<std::thread>(new std::thread([&]() {
    //获取线程的tid值
    tid_ = CurrentThread::tid();
    sem_post(&sem);
    //开启一个新线程，专门执行该线程函数
    func_();
  }));

  //必须等待上面新创建的线程的tid值
  sem_wait(&sem);
}
```

## 2.5 EventLoopThread

将EventLoop和Thread绑定，封装成一个Thread执行一个loop

**成员变量**

```c++
  EventLoop *loop_;
  bool exiting_;      //线程是否正在退出
  Thread thread_;     //线程
  std::mutex mutex_;  //互斥访问loop_
  std::condition_variable cond_;  //条件变量，用于是否已经成功创建通信
  ThreadInitCallback callback_;  //上层设置的回调函数
```

**成员函数**

```c++
 public:
  using ThreadInitCallback = std::function<void(EventLoop *)>;
  EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                  const std::string &name = std::string());
  ~EventLoopThread();

  //创建线程，启动loop
  EventLoop *startLoop();

 private:
  //线程创建时传入的回调函数，里面进行创建loop等操作
  void threadFunc();
```

### 2.5.1 EventLoopThreadPool

封装成一个线程池，使得TcpServer能更方便管理多个subLoop。

**成员变量**

```c++
  // mainReactor，设置这个成员是考虑轮询算法寻找subLoop时，当不存在subLoop时能将mainLoop作为作为返回结果
  EventLoop *baseLoop_;
  std::string name_;
  bool started_;
  int numThreads_;
  //轮询时下一个位置指向
  int next_;
  std::vector<std::unique_ptr<EventLoopThread>> threads_;
  std::vector<EventLoop *> loops_;
```

**成员函数**

```c++
 public:
  using ThreadInitCallback = std::function<void(EventLoop *)>;

  EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
  ~EventLoopThreadPool();

  void setThreadNum(int numThreads) { numThreads_ = numThreads; }
  void start(const ThreadInitCallback &cb = ThreadInitCallback());

  //如果工作在多线程中，baseLoop以轮询方式分配channel给subloop
  EventLoop *getNextLoop();

  std::vector<EventLoop *> getAllLoops();

  bool started() const { return started_; }
  const std::string name() const { return name_; }
```

**关键**

```c++
void EventLoopThreadPool::start(const ThreadInitCallback &cb) {
  started_ = true;
  for (int i = 0; i < numThreads_; ++i) {
    char buf[name_.size() + 32];
    snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
    EventLoopThread *t = new EventLoopThread(cb, buf);
    threads_.push_back(std::unique_ptr<EventLoopThread>(t));
    //底层创建线程，绑定一个新的Eventloop，并记录该loop地址
    loops_.push_back(t->startLoop());
  }

  //没有创建其他线程，只有一个mainLoop
  if (numThreads_ == 0 && cb) {
    cb(baseLoop_);  //如果用户传了cb则需执行下回调******************
  }
}
```

## 2.6 Socket

Socket封装socket的常用操作，如设置非阻塞、监听、noDelay等，但并不包含创建sockfd。

注意与Channel的区别，Channel是管理事件和事件发生回调，而Socket则设置socket的属性、绑定监听等

**成员变量**

```c++
  const int sockfd_;
```

**成员函数**

```c++
 public:
  explicit Socket(int sockfd) : sockfd_(sockfd) {}
  ~Socket();

  int fd() const { return sockfd_; }
  void bindAddress(const InetAddress &localAddr);
  void listen();
  int accept(InetAddress *peeraddr);

  void shutdownWrite();

  //设置不等待而直接发送
  void setTcpNoDelay(bool on);
  //设置端口释放后立即就可以被再次使用
  void setReuseAddr(bool on);
  //允许许多个线程或进程，绑定在同一个端口上
  void setReusePort(bool on);
  //设置是否开启心跳
  void setKeepAlive(bool on);
```

## 2.7 Acceptor

封装了listenfd相关的操作，其监听新连接用户，并分发给subLoop

**成员变量**

```c++
  EventLoop *loop_;  // acceptor使用的用户定义的loop，也叫mainloop
  Socket acceptSocket_;//listenfd对应的socket
  Channel acceptChannel_;//listenfd对应的channel
  NewConnectionCallback newConnectionCallback_;  //当有新连接到来，需要做的回调
  bool listenning_;//是否监听标志
```

**成员函数**

```c++
 public:
  using NewConnectionCallback =
      std::function<void(int sockfd, const InetAddress &)>;

  Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
  ~Acceptor();

  void setNewConnectionCallback(const NewConnectionCallback &cb) {
    newConnectionCallback_ = cb;
  }

  bool listenning() const { return listenning_; }
  //开始监听
  void listen();

 private:
  // listenfd有事件发生即有新用户连接，调用此函数
  void handleRead();
```

**关键**

```c++
// listenfd有事件发生即有新用户连接，调用此函数
void Acceptor::handleRead() {
  InetAddress peerAddr;
  int connfd = acceptSocket_.accept(&peerAddr);
  if (connfd >= 0) {
    if (newConnectionCallback_) {
      //轮询找到subloop，将其唤醒，分发当前客户端的channel
      newConnectionCallback_(connfd, peerAddr);
    } else {
      ::close(connfd);
    }
  } else {
    LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__, __FUNCTION__, __LINE__,
              errno);
    // EMFILE表示服务器sockfd资源用完
    if (errno == EMFILE) {
      LOG_ERROR("%s:%s:%d sockfd reached limit \n", __FILE__, __FUNCTION__,
                __LINE__);
    }
  }
}
```

## 2.8 **Buffer**

封装缓冲区常见操作，其内部是一个char类型的vector

```c++
/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
```

**成员变量**

```c++
  std::vector<char> buffer_;  //缓冲区
  size_t readerIndex_;        //可读位置
  size_t writerIndex_;        //可写位置
```

**成员函数**

```c++
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

  //把data中的len长的数据添加到缓冲区中
  void append(const char* data, size_t len) {
    ensureWriteableBytes(len);
    std::copy(data, data + len, beginWrite());
    writerIndex_ += len;
  }

  //从fd上读取数据
  size_t readFd(int fd, int* saveErrno);
  //通过fd发送数据
  size_t writeFd(int fd, int* saveErrno);

 private:
  //返回buffer首元素地址，即数组的起始地址，&*不能抵消，因为*被重写过
  char* begin() { return &*buffer_.begin(); }
  const char* begin() const { return &*buffer_.begin(); }

  //通过整理buffer空间或增加buffer空间使得容量够写
  void makeSpace(size_t len) {
    //如果可写空间加上前面预留空间（预留空间可能由于依次被读完而变长）还是小于要求的长度
    if (writeableBytes() + prependableBytes() < len + kCheapPrepend) {
      buffer_.resize(writerIndex_ + len);
    } else {
      //如果数据空间够用，则将现有数据前移
      size_t readable = readableBytes();
      std::copy(begin() + readerIndex_, begin() + writeableBytes(),
                begin() + kCheapPrepend);
    }
  }

  //返回可写处的指针
  char* beginWrite() { return begin() + writerIndex_; }
  const char* beginWrite() const { return begin() + writerIndex_; }
```

**关键**

```c++
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
```

## 2.9 **TcpConnection**

封装一个连接信息，其内包含本地地址、对端地址、socket、channel、以及读写buffer

**成员变量**

```c++
EventLoop *loop_;  // subLoop地址
  const std::string name_;
  std::atomic_int state_;//TcpConnection状态
  bool reading_;  //可读标志

  std::unique_ptr<Socket> socket_;//one Tcpconnection one socket
  std::unique_ptr<Channel> channel_;//one Tcpconnection one channel

  const InetAddress localAddr_;  //本地地址
  const InetAddress peerAddr_;   //对端地址

  ConnectionCallback connectionCallback_;        //有新连接时的回调
  MessageCallback messageCallback_;              //有读写消息的回调
  WriteCompleteCallback writeCompleteCallback_;  //消息发送完成的回调
  // HighWaterMarkCallback:读写数据达到警戒线的回调。比如当发送数据过快，接收方来不及接受时，此时发送方需要进行处理比如需要停止发送，
  HighWaterMarkCallback highWaterMarkCallback_;
  CloseCallback closeCallback_;

  size_t highWaterMark_;  //水位线标志，超过此数则表示达到警戒线

  Buffer inputBuffer_;   //接受数据缓冲
  Buffer outputBuffer_;  //发送数据缓冲
```

**成员函数**

```c++
public:
  TcpConnection(EventLoop *loop, const std::string &name, int sockfd,
                const InetAddress &localAddr, const InetAddress &peerAddr);
  ~TcpConnection();

  EventLoop *getLoop() const { return loop_; }
  const std::string &name() const { return name_; }
  const InetAddress &localAddress() const { return localAddr_; }
  const InetAddress &peerAddress() const { return peerAddr_; }

  bool connected() const { return state_ == kConnected; }

  //发送数据
  void send(const std::string &buf);
  // void send(const void *message, int len);
  //关闭连接
  void shutdown();

  //设置回调函数
  void setConnectionCallback(const ConnectionCallback &cb) {
    connectionCallback_ = cb;
  }
  void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
  void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
    writeCompleteCallback_ = cb;
  }
  void setHighWaterMarkCallback(const HighWaterMarkCallback &cb) {
    highWaterMarkCallback_ = cb;
  }
  void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }

  //连接建立时的回调函数
  void connectEstablished();
  //连接销毁的回调函数
  void connectDestroyed();

 private:
  enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
  void setState(StateE state) { state_ = state; }
```

**关键**

```c++
//发送数据
//应用写得快，而内核发送数据慢，需要把待发送数据写入缓冲区，而且设置水位回调
void TcpConnection::sendInLoop(const void *message, size_t len) {
  size_t nwrote = 0;
  size_t remaining = len;
  bool faultError = false;

  //之前已经调用过shutdown，不能再进行发送了
  if (state_ == kDisconnected) {
    LOG_ERROR("disconnected, give up writing");
    return;
  }

  // channel未设置写感兴趣事件（说明无数据待从缓冲区中写入）且缓冲区没有待发送数据
  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
    nwrote = ::write(channel_->fd(), message, len);
    if (nwrote >= 0) {
      remaining = len - nwrote;
      if (remaining == 0 && writeCompleteCallback_) {
        //当一次将数据发送完成，就不用再给channel设置epollout事件了
        loop_->queueInLoop(
            std::bind(writeCompleteCallback_, shared_from_this()));
      }
    } else {
      nwrote = 0;
      if (errno != EWOULDBLOCK) {
        LOG_ERROR("TcpConnection::sendInLoop\n");
        if (errno == EPIPE || errno == ECONNREFUSED) {
          faultError = true;
        }
      }
    }
  }

  /****************************************************/
  //说明这次write没有全部发送出去，剩余数据需要保存到缓冲区中，
  //然后给channel注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知相应的sock-channel，调用WriteCallback方法
  //最终也就是调用handleWrite方法，把发送缓冲区中的数据全部发送完成
  if (!faultError && remaining > 0) {
    //目前发送缓冲区剩余的待发送数据长度
    size_t oldLen = outputBuffer_.readableBytes();
    if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ &&
        highWaterMarkCallback_) {
      loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(),
                                   oldLen + remaining));
    }
    outputBuffer_.append((char *)message + nwrote, remaining);
    if (!channel_->isWriting()) {
      //这里一定要注册channel的写事件，否则poller不会给channel通知epollout
      channel_->enableWriting();
    }
  }
}
```

```c++
//啥时候执行写回调？poller怎么通知写回调，数据不是都先到buffer中的吗
//这是因为只要注册了EPOLLOUT事件，当内核写缓冲可写时，会不断发送EPOLLOUT信号，然后调用此函数将buffer中的数据进行发送
void TcpConnection::handleWrite() {
  if (channel_->isWriting()) {
    int savedError = 0;
    ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedError);
    if (n > 0) {
      outputBuffer_.retrieve(n);
      if (outputBuffer_.readableBytes() == 0) {
        channel_->disableWriting();
        if (writeCompleteCallback_) {
          //唤醒loop_对应的thread线程，执行回调
          //其实此时就是在loop_对应的thread线程中
          loop_->queueInLoop(
              std::bind(writeCompleteCallback_, shared_from_this()));
        }
        //当用户shutdown时数据如果没发送完成，不会真的shutdown，只是将标志置为kDisconnecting，而是会等待用户把数据发送完成
        //当数据发送完成后，则再执行一次shutdownInLoop
        if (state_ == kDisconnecting) {
          shutdownInLoop();
        }
      }
    } else {
      LOG_ERROR("TcpConnection::handleWrite() error\n");
    }
  } else {
    LOG_ERROR("TcpConnection fd=%d is down, no more writing \n",
              channel_->fd());
  }
}
```

## 2.10 **TcpServer**

上述所有类的综合使用者，用户通过使用此类来设置各种回调，通过此类来管理subLoop个数等

**成员变量**

```c++
  using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

  EventLoop *loop_;  //用户定义的loop
  const std::string ipPort_;
  const std::string name_;
  std::unique_ptr<Acceptor> acceptor_;  // mainLoop的监听新连接事件
  std::shared_ptr<EventLoopThreadPool> threadPool_;  // one loop per thread

  ConnectionCallback connectionCallback_;        //有新连接时的回调
  MessageCallback messageCallback_;              //有读写消息的回调
  WriteCompleteCallback writeCompleteCallback_;  //消息发送完成的回调

  ThreadInitCallback threadInitCallback_;  //线程初始化的回调
  std::atomic_int started_;

  int nextConnId_;//TcpConnection的id，是用来加到TcpConnection名字中
  ConnectionMap connections_;  //保存所有的连接
```

**成员函数**

```c++
 public:
  using ThreadInitCallback = std::function<void(EventLoop *)>;

  enum Option {
    kNoReusePort,
    kReusePort,
  };

  TcpServer(EventLoop *loop, const InetAddress &listenAddr,
            const std::string nameArg, Option option = kNoReusePort);
  ~TcpServer();

  void setThreadInitCallback(const ThreadInitCallback &cb) {
    threadInitCallback_ = cb;
  }
  void setConnectionCallback(const ConnectionCallback &cb) {
    connectionCallback_ = cb;
  }
  void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
  void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
    writeCompleteCallback_ = cb;
  }

  //设置低层subloop的个数
  void setThreadNum(int numThreads);

  //开启服务器监听
  void start();

 private:
  //当Acceptor有新连接时会调用的回调函数
  void newConnection(int sockfd, const InetAddress &peerAddr);
  
  void removeConnection(const TcpConnectionPtr &conn);
  void removeConnectionInLoop(const TcpConnectionPtr &conn);
```

**关键**

```c++
//当有新的客户端连接，acceptor会执行这个回调
//根据轮询算法选择一个subLoop，唤醒subLoop，把当前connfd封装成channel分发给subloop
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr) {
  //根据轮询选择一个subloop来管理channel
  EventLoop *ioLoop = threadPool_->getNextLoop();
  char buf[64] = {0};
  snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
  ++nextConnId_;
  std::string connName = name_ + buf;
  LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s\n",
           name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
  //通过sockfd获取其绑定的ip地址和端口信息
  sockaddr_in local;
  bzero(&local, sizeof local);
  socklen_t addrlen = sizeof local;
  if (::getsockname(sockfd, (sockaddr *)&local, &addrlen) < 0) {
    LOG_ERROR("sockets::getLocalAddr\n");
  }
  InetAddress localAddr(local);

  //根据连接成功的sockfd，创建TcpConnection连接对象
  TcpConnectionPtr conn(
      new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
  connections_[connName] = conn;
  //下面的回调都来源于用户设置给TcpServer => TcpConnection => channel =>Poller
  //=>notify channel调用回调
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  //设置如何关闭连接的回调 用户调用shutdwon => socket关闭写端 =>
  // poller通知channel EPOLLHUP事件 => channel调用closeCallback =>
  // TcpConnection::handleClose() => TcpServer::removeConnection
  conn->setCloseCallback(
      std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

  //直接调用connectEstablished方法
  ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}
```

