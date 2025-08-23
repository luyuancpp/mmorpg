#!/bin/bash

root_dir=$(pwd)

function clean()
{
    for d in $1/*;
    do [[ -d "$d" ]] && { ./make.sh -proj-dir=$d -build=../../../../build/$(basename $d) --vs-file=$(basename $d).vcxproj -tomakefile-dir=$(pwd) -tomakefile-name=vcxproj2mk.sh --make-clean=clean --make-target=lib; };
    done
}

clean ./muduo/base/tests
clean ./muduo/net/tests

rm -rf build/*

cd $root_dir
