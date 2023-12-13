#!/bin/bash

cpu=$(cat /proc/cpuinfo | grep processor | wc -l)
echo $cpu

########third_party########

if [ ! -d "cmakeinstall" ]; then
   mkdir cmakeinstall 
fi
cd cmakeinstall 
wget https://github.com/Kitware/CMake/archive/refs/tags/v3.28.0.zip 
unzip v3.28.0.zip
cd CMake-3.28.0
./bootstrap 
make -j$cpu 
make install
if test $? -ne 0; then 
   echo "cmake install failed"
   exit 
fi
cd ../../
echo "cmake install ok"

cd third_party/redis
make -j$cpu
if test $? -ne 0; then 
   echo "redis install failed"
   exit 
fi
cd ../../
echo "redis install ok"

cd third_party/redis/deps/hiredis
make -j$cpu
if test $? -ne 0; then 
   echo "hiredis install failed"
   exit 
fi
cp -rf libhiredis.a ../../../../lib/
cd ../../../..
echo "hiredis install ok"

cd third_party/lua/
make -j$cpu all 
if test $? -ne 0; then 
   echo "lua install failed"
   exit 
fi
cp -rf liblua.a ../../lib/
cd ../..
echo "lua install ok"


cd third_party/protobuf/
cmake .  -DABSL_PROPAGATE_CXX_STD=TRUE  -DCMAKE_CXX_STANDARD=20
make install -j$cpu
if test $? -ne 0; then 
   echo "protobuf install failed"
   exit 
fi
cd ../..
echo "protobuf install ok"

cd third_party 
rm -rf muduo 
cp -rf muduo-linux muduo 
cd muduo
cp -f ../../pkg/common/src/muduowindow/TimerId.h  muduo/net/ 
cp -f ../../pkg/common/src/muduowindow/CMakeLists.txt  ./
cp -f ../../pkg/common/src/muduowindow/muduo/net/protorpc/CMakeLists.txt  muduo/net/protorpc/
sed -i '66,70d' muduo/net/CMakeLists.txt
sed -i '56,63d' muduo/net/CMakeLists.txt
cp -f ../../pkg/common/src/muduowindow/autogen.sh muduo/net/protorpc/
cd muduo/net/protorpc/
./autogen.sh
cd ../../../
cmake . 
make -j$cpu
if test $? -ne 0; then 
   echo "muduo install failed"
   exit 
fi
cd lib 
ar rvs libmuduo.a libmuduo_base.a libmuduo_net.a
cp -rf * ../../../lib/
cd ../../../
echo "muduo install ok"

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
cd ..
echo "mysql install ok"

cd third_party/zlib
 ./configure --64 --libdir=../../lib
make -j$cpu
if test $? -ne 0; then 
   echo "zlib install failed"
   exit 
fi
make install
cd ../..
echo "zlib install ok"
end
