#!/usr/bin/env python
# coding=utf-8
import os
from os import system
system("python buildprotoc.py")
system("python buildrpcservice.py")
system("python buildrpcserviceid.py")
system("python buildprotosol.py")
system("python buildrpcplayerservice.py")
system("python buildopenplayerservice.py")
system("python buildservernormalservice.py")
system("python buildrpcclientplayerservice.py")
system("python buildrpcclientservicelua.py")
system("python buildclientserviceinitluacpp.py")
system("python buildeventreceiver.py")
print('注意重要事件存储db,事件优先级考虑清除!!')
