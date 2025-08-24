#!/bin/bash

for file in *.proto; do
	protoc  -I=./ -I=../../../../../third_party/protobuf/src --cpp_out=./ $file
done
