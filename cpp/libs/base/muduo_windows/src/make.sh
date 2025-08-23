#!/bin/bash

#refer to gcc configure 


ac_option=$1
ac_prev=
ac_dashdash=

#debug,release
build_model=debug

build_alias=

#exe,shared,lib

make_clean=

#xxx.vcxproj
vs_file=

build_dir=./build

proj_dir=

makefile_dir=.
makefile_name=Makefile

tomakefile_name=
tomakefile_dir=

for ac_option
do
  # If the previous option needs an argument, assign it.
  if test -n "$ac_prev"; then
    eval $ac_prev=\$ac_option
    ac_prev=
    continue
  fi

  case $ac_option in
  *=*)	ac_optarg=`expr "X$ac_option" : '[^=]*=\(.*\)'` ;;
  *)	ac_optarg=yes ;;
  esac

  # Accept the important Cygnus configure options, so we can diagnose typos.

  case $ac_dashdash$ac_option in
      --)
          ac_dashdash=yes ;;

  -bindir | --bindir | --bindi | --bind | --bin | --bi)
    ac_prev=bindir ;;
  -bindir=* | --bindir=* | --bindi=* | --bind=* | --bin=* | --bi=*)
    bindir=$ac_optarg ;;

  -proj-dir | --proj-dir)
    ac_prev=proj_dir ;;
  -proj-dir=* | --proj-dir=*)
    proj_dir=$ac_optarg ;;

  -build | --build | --buil | --bui | --bu)
    ac_prev=build_alias ;;
  -build=* | --build=* | --buil=* | --bui=* | --bu=*)
    build_alias=$ac_optarg ;;

  -build-dir | --build-dir )
    ac_prev=build_alias ;;
  -build-dir=* | --build-dir=* )
    build_dir=$ac_optarg ;;

  -build-model | --build-model )
    ac_prev=build_model ;;
  -build-model=* | --build-model=*)
    build_model=$ac_optarg ;;
  
  -make-clean | --make-clean )
    ac_prev=make_clean ;;
  -make-clean=* | --make-clean=* )
    make_clean=$ac_optarg ;;

  -vs-file | --vs-file )
    ac_prev=vs_file ;;
  -vs-file=* | --vs-file=* )
    vs_file=$ac_optarg ;;

  -makefile-dir | --makefile-dir )
    ac_prev=makefile_dir ;;
  -makefile-dir=* | --makefile-dir=* )
    makefile_dir=$ac_optarg ;;

  -makefile-name | --makefile-name )
    ac_prev=makefile_name ;;
  -makefile-name=* | --makefile-name=* )
    makefile_name=$ac_optarg ;;

  -tomakefile-dir | --tomakefile-dir )
    ac_prev=tomakefile_dir ;;
  -tomakefile-dir=* | --tomakefile-dir=* )
    tomakefile_dir=$ac_optarg ;;

  -tomakefile-name | --tomakefile-name )
    ac_prev=tomakefile_name ;;
  -tomakefile-name=* | --tomakefile-name=* )
    tomakefile_name=$ac_optarg ;

  esac
done

if [ ! $proj_dir ] ; then
    proj_dir=$build_alias
fi


set -x
function fbuildfile()
{
    cp -rf $makefile_dir/$makefile_name $proj_dir 
    cd $proj_dir
    build_model=${build_model^}
    $tomakefile_dir/$tomakefile_name $vs_file  $build_model $build_dir $build_alias > vs.mk
    if test "$make_clean" ; then
        make $make_clean  
    else
        make  -j20  
    fi
    rm -rf vs.mk
    cd ..
}

set -ex

fbuildfile
