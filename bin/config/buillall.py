#!/usr/bin/env python
# coding=utf-8
import os
from os import system

if not os.path.exists("cpp"):
	os.makedirs("cpp")
if not os.path.exists("protocpp"):
	os.makedirs("protocpp")
if not os.path.exists("json"):
	os.makedirs("json")

system("xlstojson.py")
system("xlstopb.py")
system("cd proto && buildproto.py")
system("xlstocpp.py")
system("xlstocppid.py")
system("md5tool.py md5copy ./cpp/ ../../common/src/game_config/")
system("md5tool.py md5copy ./protocpp/ ../../protopb/pbc/")