#!/bin/bash

set -e

# 如果没有目录，创建目录
if [ ! -d `pwd`/build ]; then
    mkdir `pwd`/build
fi

rm -rf `pwd`/build/*

cd `pwd`/build && cmake .. &&make

cd ..

# 把头文件拷贝到/usr/include/mymuduo so库拷贝到/usr/lib

if [ ! -d /usr/include/mymuduo ];then 
    mkdir /usr/include/mymuduo
fi

for header in `ls ./include/*.h`
do 
    cp $header /usr/include/mymuduo
done

cp `pwd`/lib/libmymuduo.so /usr/lib

ldconfig