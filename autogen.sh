#!/bin/bash

python vcxproj2cmake.py

cd protopb && cmake . && make -j4
if test $? -ne 0; then 
   exit 
fi

cd ..
cd third_party && cmake . && make -j4
if test $? -ne 0; then 
    exit 
fi

cd ..
cd common && cmake . && make -j4
if test $? -ne 0; then 
    exit 
fi

cd ..
cd deploy_server && cmake . && make -j4
if test $? -ne 0; then 
    exit 
fi

cd ..
cd database_server && cmake . && make -j4
if test $? -ne 0; then 
    exit 
fi

cd ..
cd login_server && cmake . && make -j4
if test $? -ne 0; then 
    exit 
fi

cd ..
cd master_server && cmake . && make -j4
if test $? -ne 0; then 
    exit 
fi

cd ..
cd gateway_server && cmake . && make -j4
if test $? -ne 0; then 
    exit 
fi

cd ..
cd game_server && cmake . && make -j4
if test $? -ne 0; then 
    exit 
fi

echo " go go go !"
