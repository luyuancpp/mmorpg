#/bin/bash

for file in ./*; do
	protoc  -I=./ -I=../../third_party/protobuf/src --cpp_out=../pbc/ $file
done
