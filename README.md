# 简单复现muduo库

完全去除boost依赖，使用C++11替代boost



## 1. 安装

```bash
git clone https://github.com/mujiubai/mymuduo.git
cd mymuduo
sudo bash autobuild.sh
```



## 2. demo

```bash
cd example
make
./testserver
#open a client
telnet 127.0.0.1 8000
```

