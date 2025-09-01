#!/bin/bash

#./autogen.sh

root_dir=$(pwd)

cd build
for f in *;
    do { ./$f; };
    done

cd $root_dir
