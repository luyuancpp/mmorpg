#!/bin/bash

# Run this script to generate the configure script and other files that will
# be included in the distribution.  These files are not checked in because they
# are automatically generated.

c_dir=$(pwd)

set -e

if [ ! -z "$@" ]; then
  for argument in "$@"; do
    case $argument in
	  # make curl silent
      "-s")
        curlopts="-s"
        ;;
    esac
  done
fi


# Check that we're being run from the right directory.
if test ! -f muduo/base/Types.h; then
  cat >&2 << __EOF__
Could not find source code.  Make sure you are running this script from the
root of the distribution tree.
__EOF__
  exit 1
fi

if test ! -e external; then
    mkdir external
fi

if test ! -e build; then
    mkdir build 
fi

if test ! -e lib; then
    mkdir lib 
fi

# Check that boost  is present.  Usually it is already there since the
# directory is set up as an SVN external.
if test ! -e external/boost; then
  echo "external/boost not present.  Fetching boost from the web..."
  cd external
  curl $curlopts -L -O https://dl.bintray.com/boostorg/release/1.65.0/source/boost_1_65_0.zip
  unzip -q boost_1_65_0.zip
  rm boost_1_65_0.zip
  mv boost_1_65_0 boost
  cd $c_dir
fi

if test ! -e external/googletest; then
  echo "external/googletest not present.  Fetching googletest from the web..."
  cd external
  git clone --recursive https://github.com/google/googletest.git 
  g++ -isystem ./googletest/googletest/include -I./googletest/googletest -lpthread -c ./googletest/googletest/src/gtest-all.cc
  ar -rv ../lib/gtest.lib gtest-all.o
  rm gtest-all.o
  cp ../lib/gtest.lib ../lib/gtestd.lib
  cd $c_dir
fi

if test ! -e external/zlib; then
  echo "external/zlib not present.  Fetching zlib from the web..."
  cd external
  curl $curlopts -L -O http://zlib.net/zlib-1.2.11.tar.gz
  tar -xf zlib-1.2.11.tar.gz
  mv zlib-1.2.11 zlib
  cd zlib 
  ./configure
  make
  mv libz.a zlib.lib
  cp zlib.lib zlibd.lib
  cp -rf zlib.lib ../../lib/
  cp -rf zlibd.lib ../../lib/
  cd $c_dir
fi

function gen()
{
    for d in $1/*;
    do 
        if  test -d "$d" ;then  
            ./make.sh -proj-dir=$d -build=../../../../build/$(basename $d) --vs-file=$(basename $d).vcxproj -tomakefile-dir=$(pwd) -tomakefile-name=vcxproj2mk.sh; 
        fi
        done

}

gen ./muduo/base/tests
gen ./muduo/net/tests

set -ex

exit 0
