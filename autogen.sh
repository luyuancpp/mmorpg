#!/bin/bash

git submodule update --init --recursive

./third_party.sh

python vcxproj2cmake.py

cpu=$(cat /proc/cpuinfo | grep processor | wc -l)
echo $cpu

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

buildcmakeproject(){
    echo "$1 build begin"
    cd $1
    cmake . 
    make -j$cpu
    if test $? -ne 0; then 
        echo "$1 build failed"
        exit 
    fi
    cd $2
    echo "$1  build ok"
}

buildcmakeproject pkg/pbc ../../

buildcmakeproject pkg/config ../../

buildcmakeproject pkg/common ../../

buildcmakeproject third_party ../

buildcmakeproject server/gate_server ../../

buildcmakeproject server/database_server ../../

buildcmakeproject server/login_server ../../

buildcmakeproject server/controller_server ../../

buildcmakeproject server/game_server ../../

echo " go go go !"
