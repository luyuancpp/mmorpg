#!/usr/bin/env python
# coding=utf-8
import os
from os import system
system("python buildprotoc.py")
system("python buildserviceprocess.py")
system("python buildserviceid.py")
system("python buildprotosol.py")
system("python buildplayerserviceprocess.py")
system("python buildopenplayerservice.py")
system("python buildserverserviceregister.py")
system("python buildclientplayerservice.py")
system("python buildclientserviceluaprocess.py")
system("python buildclientluacppserviceprocess.py")
system("python buildeventreceiver.py")
print('注意重要事件存储db,事件优先级考虑清除!!')
