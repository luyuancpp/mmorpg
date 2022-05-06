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



