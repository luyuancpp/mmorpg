#!/bin/bash

for file in *.proto; do
	protoc  -I=./ --cpp_out=./ $file
done
