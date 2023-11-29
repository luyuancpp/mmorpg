#!/bin/bash

cat /proc/cpuinfo  | grep "processor" | wc -l
cat=$1

./bazel.sh --prefix=/usr
echo "bazel install ok"

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
git submodule update --init --recursive
cmake -DCMAKE_INSTALL_PREFIX=/usr/bin/mysql -DMYSQL_DATADIR=/usr/bin/mysql/data -DDOWNLOAD_BOOST=1 -DWITH_BOOST=./boost -DFORCE_INSOURCE_BUILD=ON
make -j$cpu
make install  
if test $? -ne 0; then 
   exit 
fi
echo "mysql install ok"
cd ../..