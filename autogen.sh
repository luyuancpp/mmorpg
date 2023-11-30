#!/bin/bash

python vcxproj2cmake.py

cat /proc/cpuinfo  | grep "processor" | wc -l
cat=$1

./third_party.sh 

cd bin/config && python gen.py
if test $? -ne 0; then 
   exit 
fi
cd ../../

cd  proto/generator/src && go build -o genproto && ./genproto
if test $? -ne 0; then 
   exit 
fi

cd ../../../
cd third_party && rm -rf muduo 
mv muduo muduo-windows
cp -rf muduo-linux muduo 
cd muduo
cp -f ../../common/src/muduowindow/TimerId.h  muduo/net/ 
cp -f ../../common/src/muduowindow/CMakeLists.txt  ./
sed '56,70' muduo/net/CMakeLists.txt
cmake . 
cd ../../

cd common/src/network && ./autogen.sh
if test $? -ne 0; then 
    exit 
fi

cd ../../../
cmake .
cd pbc && cmake . && make -j$cpu
if test $? -ne 0; then 
    echo "pbc build error"
    exit 
fi

cd ../
cmake .
cd config && cmake . && make -j$cpu
if test $? -ne 0; then 
    echo "config build error"
    exit 
fi

cd ../
cd common && cmake . && make -j$cpu
if test $? -ne 0; then 
    echo "common build error"
    exit 
fi

cd ..
cd gate_server && cmake . && make -j$cpu
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
