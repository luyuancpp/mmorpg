#!/bin/bash

python vcxproj2cmake.py

cat /proc/cpuinfo  | grep "processor" | wc -l
cpu=$1

./third_party.sh 


cd bin/config 
python gen.py
if test $? -ne 0; then 
   exit 
fi
cd ../../

cd  proto/generator/src 
go build -o genproto 
./genproto
if test $? -ne 0; then 
    echo "generator go failed"
   exit 
fi
cd ../../../
echo "generator go ok"


cd pkg/common/src/network && ./autogen.sh
if test $? -ne 0; then 
    echo "pkg/common/src/network failed"
    exit 
fi
cd ../../../../
echo "pkg/common/src/network ok"

cd pkg/pbc 
cmake . 
make -j$cpu
if test $? -ne 0; then 
    echo "pkg/pbc build failed"
    exit 
fi
cd ../../
echo "pkg/pbc build ok"

cd pkg/config
cmake . 
make -j$cpu
if test $? -ne 0; then 
    echo "pkg/config build failed"
    exit 
fi
cd ../../
echo "pkg/config build ok"

cd pkg/common 
cmake . 
make -j$cpu
if test $? -ne 0; then 
    echo "pkg/common build error"
    exit 
fi
cd ../../
echo "pkg/pbc build ok"

cd server/gate_server 
cmake . 
make -j$cpu
if test $? -ne 0; then 
    echo "server/gate_server build error"
    exit 
fi
cd ../../
echo "server/gate_server build ok"

cd server/database_server 
cmake . 
make -j$cpu
if test $? -ne 0; then 
    echo "server/database_server build error"
    exit 
fi
cd ../../
echo "server/database_server build ok"

cd server/login_server 
cmake . 
make -j$cpu
if test $? -ne 0; then 
    echo "server/login_server build error"
    exit 
fi
cd ../../
echo "server/login_server build ok"

cd server/controller_server 
cmake . 
make -j$cpu
if test $? -ne 0; then 
    echo "server/controller_server build error"
    exit 
fi
cd ../../
echo "server/controller_server build ok"

cd server/game_server 
cmake . 
make -j$cpu
if test $? -ne 0; then 
    echo "server/game_server build error"
    exit 
fi
cd ../../
echo "server/game_server build ok"

echo " go go go !"
