#!/bin/bash

#http://unix.stackexchange.com/questions/45781/shell-script-fails-syntax-error-unexpected

vs_file=$1
build_model=$2
build_dir=$3
build_alias=$4
lib_dir=


function ToGenOutPutLibDir()
{
    lib_dir=$(sed -n -e '/<PropertyGroup Condition="...Configuration....Platform.....'$build_model'.'$1'.">/,/<OutDir>/p' $vs_file | awk -F "<" '{print $2}' | awk -F ">" '{print $2}')
}

if egrep -q "OutDir" $vs_file; then
    ToGenOutPutLibDir WIN32
    if [ -z $lib_dir ]; then
        ToGenOutPutLibDir x64 
    fi
    if [ $lib_dir ]; then
        build_alias=$lib_dir"/"$(basename $build_alias);
    fi
fi

echo 'SOURCES = \'
egrep "ClCompile Include=" $vs_file | awk -F"\"" '{print $2}' | sed 's/\\/\//g' | sed 's/^\.\//  \.\.\//g'  > tmp
cat tmp | sed 's/$/\ \\/g'
rm -rf tmp

echo 


#include file , preprocessor ToBackSlash
function ToGen()
{
    sed -n -e '/<ItemDefinitionGroup Condition="...Configuration....Platform.....'$4'.Win32.">/,/<''$2''>/p' $1 |
    egrep --max-count=1 $2|
    awk -F"%" '{print $1}' |
    awk -F">" '{print $2}' |
    sed 's/\\/\//g' |
    sed 's/^\.\//  \.\.\//g' |  
    sed 's/.$//' |
    sed 's/;/ \\\n/g' |
    sed 's/^/ '$3' /g' |
    sed 's/WIN32/__linux__/g' |
    sed 's/__WINDOWS__/__linux__/g' 
    echo
}

#include file , preprocessor ToBackSlash
function ToGenSo()
{
    sed -n -e '/<ItemDefinitionGroup Condition="...Configuration....Platform.....'$4'.Win32.">/,/<''$2''>/p' $1 |
    egrep --max-count=1 $2|
    awk -F">" '{print $2}' |
    awk -F"<" '{print $1}' |
    awk -F"%" '{print $1}' | 
    sed 's/;$//g' |
    sed 's/\\/\//g' |  
    sed 's/;/ \n/g'  | 
    sed -r 's/.*\///g'|
    sed 's/.lib//g'|
    sed 's/^/'$3'/g' |
    sed 's/$/\\/g' 
    echo
}

function ToGenLibDir()
{
    sed -n -e '/<ItemDefinitionGroup Condition="...Configuration....Platform.....'$4'.Win32.">/,/<''$2''>/p' $1 |
    egrep --max-count=1 $2|
    awk -F">" '{print $2}' |
    awk -F"<" '{print $1}' |
    sed 's/.$//' |
    sed 's/;/ \\\n/g' |
    sed 's/\\/\//g' |
    sed 's/^\.\//  \.\.\//g' |  
    sed 's/^/'$3'/g'
    echo 
}


echo 'INCLUDES = \'
ToGen $vs_file AdditionalIncludeDirectories "-I" $build_model
echo 'DEFINEDS = \'
ToGen $vs_file PreprocessorDefinitions "-D"  $build_model


echo 'DEPEND_LIBS = \'

if egrep -q "StaticLibrary" $vs_file; then
    ToGen $vs_file AdditionalDependencies "" $build_model
    echo
    echo lib=$build_alias".lib";
    echo .DEFAULT_GOAL := $build_alias".lib";
    bin_type="lib"
elif egrep -q "DynamicLibrary" $vs_file; then
    ToGenSo $vs_file AdditionalDependencies "-l" $build_model
    echo 'DEPEND_LIBS_DIR = \'
    ToGenLibDir $vs_file AdditionalLibraryDirectories "-L" $build_model
    echo
    echo shared=$build_alias".so";
    echo .DEFAULT_GOAL := $build_alias".so";
    bin_type="shared"
elif egrep -q "Application" $vs_file; then
    ToGen $vs_file AdditionalDependencies "" $build_model
    echo
    echo exe=$build_alias;
    echo .DEFAULT_GOAL := $build_alias;
    bin_type="exe"
fi

if [ $build_model = "Debug" ]; then  
    echo 'CXXFLAGS += -g -pg -D NDEBUG'
    echo
fi

echo BUILD_DIR := $build_dir

