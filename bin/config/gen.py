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
if not os.path.exists("../../pkg/pbc/src/game_config/"):
	os.makedirs("../../pkg/pbc/src/game_config/")
if not os.path.exists("../../pkg/config/src/"):
	os.makedirs("../../pkg/config/src/")

print(system("python xlstojson.py"))
print(system("python xlstopb.py"))
print(system("cd proto && python buildproto.py"))
print(system("python xlstocpp.py"))
print(system("python xlstocppid.py"))
print(system("python md5tool.py md5copy ./cpp/ ../../pkg/config/src/"))
print(system("python md5tool.py md5copy ./protocpp/ ../../pkg/pbc/src/game_config/ "))
print(system("python genxlsmd5.py"))
