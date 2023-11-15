#!/bin/bash

python vcxproj2cmake.py

cat /proc/cpuinfo  | grep "processor" | wc -l
cat=$1

cd bin/config && python gen.py
if test $? -ne 0; then 
   exit 
fi
cd ../..

cd common/src/pb/proto && python build.py
if test $? -ne 0; then 
   exit 
fi

cd ..
cd third_party && \cp -rf muduo-linux/* muduo/ && cmake . && make -j$cpu
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
cd lobby_server && cmake . && make -j$cpu
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
cd controller_server && cmake . && make -j$cpu
if test $? -ne 0; then 
    exit 
fi

cd ..
cd gate_server && cmake . && make -j$cpu
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
