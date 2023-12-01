#!/bin/bash

cat /proc/cpuinfo  | grep "processor" | wc -l
cat=$1

cd third_party/redis
make
if test $? -ne 0; then 
   exit 
fi
cp -rf libhiredis.a ../../
cd ../../../..
echo "redis install ok"

cd third_party/redis/deps/hiredis
make
if test $? -ne 0; then 
   exit 
fi
cp -rf libhiredis.a ../../../../lib/
cd ../../../..
echo "hiredis install ok"

cd third_party/lua/
make all test
if test $? -ne 0; then 
   exit 
fi
cp -rf liblua.a ../../lib/
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

cd ../../../
cd third_party && rm -rf muduo 
cp -rf muduo-linux muduo 
cd muduo
cp -f ../../common/src/muduowindow/TimerId.h  muduo/net/ 
cp -f ../../common/src/muduowindow/CMakeLists.txt  ./
sed -i '109,116d' CMakeLists.txt
sed -i '56,70d' muduo/net/CMakeLists.txt
cmake . 
make -j20
cp -rf ./lib/* ../../lib/
cd ../../

mkdir deb
cd deb
apt-get remove mysql-common
apt-get remove mysql-client
apt-get remove mysql-community-server-core
apt-get remove mariadb-server
apt-get remove mariadb-client
apt-get remove mariadb-client-core
apt-get remove mariadb-server-core
wget http://ftp.us.debian.org/debian/pool/main/o/openssl/libssl1.1_1.1.1w-0+deb11u1_amd64.deb
apt install ./libssl1.1_1.1.1w-0+deb11u1_amd64.deb
wget https://cdn.mysql.com//Downloads/MySQL-8.2/mysql-server_8.2.0-1debian12_amd64.deb-bundle.tar
tar -xvf mysql-server_8.2.0-1debian12_amd64.deb-bundle.tar

apt install ./mysql-common_8.2.0-1debian12_amd64.deb
apt install ./mysql-community-client-plugins_8.2.0-1debian12_amd64.deb
apt install ./libmysqlclient22_8.2.0-1debian12_amd64.deb
apt install ./libmysqlclient-dev_8.2.0-1debian12_amd64.deb
apt install ./mysql-community-client-core_8.2.0-1debian12_amd64.deb
apt install ./mysql-community-client_8.2.0-1debian12_amd64.deb
apt install ./mysql-client_8.2.0-1debian12_amd64.deb
apt install ./mysql-community-server-core_8.2.0-1debian12_amd64.deb
apt install ./mysql-community-server_8.2.0-1debian12_amd64.deb
apt install ./mysql-community-server-debug_8.2.0-1debian12_amd64.deb
apt install ./mysql-community-test_8.2.0-1debian12_amd64.deb
apt install ./mysql-community-test-debug_8.2.0-1debian12_amd64.deb
apt install ./mysql-server_8.2.0-1debian12_amd64.deb
if test $? -ne 0; then 
   exit 
fi
echo "mysql install ok"
cd ..