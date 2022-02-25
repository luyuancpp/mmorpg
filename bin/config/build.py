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

print(system("python xlstojson.py"))
print(system("python xlstopb.py"))
print(system("cd proto && python buildproto.py"))
print(system("python xlstocpp.py"))
print(system("python xlstocppid.py"))
print(system("python md5tool.py md5copy ./cpp/ ../../common/src/game_config/"))
print(system("python md5tool.py md5copy ./protocpp/ ../../common/src/pb/pbc/"))
print(system("python genxlsmd5.py"))
