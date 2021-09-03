#!/usr/bin/env python
# coding=utf-8
import os
from os import system

system("xlstojson.py")
system("xlstopb.py")
system("proto/build.py")
system("xlstocpp.py")
system("md5tool.py md5copy ./cpp/ ../../common/src/game_config/")