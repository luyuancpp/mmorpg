import os

import md5tool
import shutil
import threading
import _thread
from multiprocessing import cpu_count

local = threading.local()

md5dir = './md5/event_proto/'
destdirpath = '../../../../game_server/src/event_receiver/'
eventprotodir = './event_proto/'
tabstr = '    '
filelist = []
threads = []
local.eventprotoarray = []

yourcodebegin = '---<<< BEGIN WRITING YOUR CODE'
yourcodeend = '---<<< END WRITING YOUR CODE'

if not os.path.exists(md5dir):
    os.makedirs(md5dir)

##输入pb事件
def parsefile(filename):
	local.eventprotoarray = []
	with open(filename,'r', encoding='utf-8') as file:
		for fileline in file:
			if not (fileline.find('message') >= 0):
				continue
			local.eventprotoarray.append(fileline.split(' ')[1].strip('\n'))

def generatehead(filename):
	newstr = '#pragma once\n'
	newstr += '#include "src/game_logic/game_registry.h"\n\n'
	newcppfilename = filename.replace(eventprotodir, md5dir).replace('.proto', '') + '_receiver.h'
	filenamenoprefixsuffix = filename.replace(eventprotodir, '').replace('.proto', '')
	letterarray = filenamenoprefixsuffix.split('_')
	for i in range(0, len(local.eventprotoarray)): 
		newstr += 'class ' + local.eventprotoarray[i] + ';\n'
	classname = ''
	for i in range(0, len(letterarray)): 
		classname += letterarray[i].capitalize()
	
	newstr += '\nclass ' + classname + '\n{\npublic:\n'
	newstr += tabstr + 'void static void Register(entt::dispatcher& dispatcher);\n'
	newstr += tabstr + 'void static void UnRegister(entt::dispatcher& dispatcher);\n\n'
	for i in range(0, len(local.eventprotoarray)): 
		newstr += tabstr + 'void static void Receive1(const ' + local.eventprotoarray[i] + '& event_obj);\n'
	newstr += '};\n'
	with open(newcppfilename, 'w', encoding='utf-8')as file:
		file.write(newstr)

def generatecpp(filename):
	eventcount = 0

def generate(filename):
	generatehead(filename)
	generatecpp(filename)

def get_file_list(file_path):
    dir_list = os.listdir(file_path)
    if not dir_list:
        return
    else:
        dir_list = sorted(dir_list,key=lambda x: os.path.getmtime(os.path.join(file_path, x)))
        return dir_list

def inputfile():
    dir_list = get_file_list(eventprotodir)
    for filename in dir_list:
    	if not (filename[-6:].lower() == '.proto'):
            continue
    	filelist.append(eventprotodir + filename)

class myThread (threading.Thread):
    def __init__(self, filename):
        threading.Thread.__init__(self)
        self.filename = str(filename)
    def run(self):
        parsefile(self.filename)
        generate(self.filename)

def main():
    filelen = len(filelist)
    global threads
    step = filelen / cpu_count() + 1
    if cpu_count() > filelen:
        for i in range(0, filelen):
            t = myThread(filelist[i])
            t.start()
            threads.append(t)
    else :
        for i in range(0, cpu_count()):
            for j in range(i, i * step) :
                t = myThread(filelist[j][0], filelist[j][1])
                t.start()
                threads.append(t)
    for t in threads :
        t.join()
    

def md5copydir():
	pass

inputfile()
main()
md5copydir()
