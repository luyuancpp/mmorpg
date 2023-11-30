#!/bin/bash

cat /proc/cpuinfo  | grep "processor" | wc -l
cat=$1

./bazel.sh --prefix=/usr
echo "bazel install ok"

cd third_party/lua
if test $? -ne 0; then 
   exit 
fi
echo "lua install ok"
cd ../..

cd third_party/abseil-cpp/
cmake -DCMAKE_INSTALL_PREFIX=/usr/bin -DABSL_BUILD_TESTING=OFF -DABSL_USE_GOOGLETEST_HEAD=OFF -DCMAKE_CXX_STANDARD=20 .
cmake --build . --target all
if test $? -ne 0; then 
   exit 
fi
echo "abseil install ok"
cd ../..

cd third_party/protobuf/
git submodule update --init --recursive
cmake .
make -j$cpu
make install  
if test $? -ne 0; then 
   exit 
fi
echo "protobuf install ok"
cd ../..

cd third_party/mysql/
apt-get remove mariadb-server
apt-get remove mariadb-client
apt-get remove mariadb-client-core
apt-get remove mariadb-server-core
wget http://ftp.us.debian.org/debian/pool/main/o/openssl/libssl1.1_1.1.1w-0+deb11u1_amd64.deb
apt install ./libssl1.1_1.1.1w-0+deb11u1_amd64.deb
wget https://cdn.mysql.com//Downloads/MySQL-8.2/mysql-community-server_8.2.0-1debian12_amd64.deb
apt install ./mysql-community-server_8.2.0-1debian12_amd64.deb
make -j$cpu
make install  
if test $? -ne 0; then 
   exit 
fi
echo "mysql install ok"
cd ../..