#!/bin/bash

python vcxproj2cmake.py

cat /proc/cpuinfo  | grep "processor" | wc -l
cat=$1

if [ ! -d "protopb/pbc" ];then
  mkdir protopb/pbc
fi

cd bin/config && python build.py
if test $? -ne 0; then 
   exit 
fi
cd ../..

cd protopb/proto && python build.py
if test $? -ne 0; then 
   exit 
fi
cd ../..

cd protopb && cmake . && make -j$cpu
if test $? -ne 0; then 
   exit 
fi

cd ..
cd third_party && cp -rf muduo-linux/muduo/* muduo/ && cmake . && make -j$cpu
if test $? -ne 0; then 
    exit 
fi

cd ..
cd common && cmake . && make -j$cpu
if test $? -ne 0; then 
    exit 
fi

cd ..
cd deploy_server && cmake . && make -j$cpu
if test $? -ne 0; then 
    exit 
fi

cd ..
cd region_server && cmake . && make -j$cpu
if test $? -ne 0; then 
    exit 
fi

cd ..
cd database_server && cmake . && make -j$cpu
if test $? -ne 0; then 
    exit 
fi

cd ..
cd login_server && cmake . && make -j$cpu
if test $? -ne 0; then 
    exit 
fi

cd ..
cd master_server && cmake . && make -j$cpu
if test $? -ne 0; then 
    exit 
fi

cd ..
cd gateway_server && cmake . && make -j$cpu
if test $? -ne 0; then 
    exit 
fi

cd ..
cd game_server && cmake . && make -j$cpu
if test $? -ne 0; then 
    exit 
fi

cd ..
cd client && cmake . && make -j$cpu
if test $? -ne 0; then 
    exit 
fi

echo " go go go !"
