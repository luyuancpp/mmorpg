#!/bin/bash

cat /proc/cpuinfo  | grep "processor" | wc -l
cpu=$1
