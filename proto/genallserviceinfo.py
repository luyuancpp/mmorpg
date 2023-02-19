import os
from os import system
import md5tool
import shutil
import threading
import genpublic

filelist = []

def scanfiles(dir):
    dirfiles  = os.listdir(dir)
    for filename in dirfiles:
        filelist.append(filename)
        
def genservcieimplobj():
    for filename in filelist:
        with open(filename,'r', encoding='utf-8') as file:
            for fileline in file:
                if fileline.find('service ') < 0:
                    continue
        
scanfiles(genpublic.commonportodir)
scanfiles(genpublic.logicprotodir)

#print(filelist)