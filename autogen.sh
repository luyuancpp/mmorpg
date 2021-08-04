#!/bin/sh

python vcxproj2cmake.py

cd protopb && cmake . && make -j4
if [$? -ne 0]; then 
    exit 
fi

cd ..

cd third_party && cmake . && make -j4
if [$? -ne 0]; then 
    exit 
fi

cd ..

cd common && cmake . && make -j4
if [$? -ne 0]; then 
    exit 
fi

cd ..

cd deploy_server && cmake . && make -j4
if [$? -ne 0]; then 
    exit 
fi

