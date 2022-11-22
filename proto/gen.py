#!/usr/bin/env python
# coding=utf-8
import os
from os import system
system("python genprotoc.py")
system("python genserviceprocess.py")
system("python genlogicserviceprocess.py")
system("python genserviceid.py")
system("python genprotosol.py")
system("python genplayerserviceprocess.py")
system("python genopenplayerservice.py")
system("python genserverserviceregister.py")
system("python genclientplayerservice.py")
system("python genclientserviceluaprocess.py")
system("python genclientluacppserviceprocess.py")
system("python geneventreceiver.py")
system("python genservicemethod.py")
system("python genplayerservicereplied.py")
print('注意重要事件存储db,事件优先级考虑清除!!')
