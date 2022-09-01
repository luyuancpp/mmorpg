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
headdestfilesuffix = '_receiver.h'
cppdestfilesuffix = '_receiver.cpp'
cpprpceventpart = 1

yourcodebegin = '///<<< BEGIN WRITING YOUR CODE'
yourcodeend = '///<<< END WRITING YOUR CODE'

if not os.path.exists(md5dir):
    os.makedirs(md5dir)

def genyourcodepair():
    return yourcodebegin + '\n' + yourcodeend + '\n'

##输入pb事件
def parsefile(filename):
	local.eventprotoarray = []
	with open(filename,'r', encoding='utf-8') as file:
		for fileline in file:
			if not (fileline.find('message') >= 0):
				continue
			local.eventprotoarray.append(fileline.split(' ')[1].strip('\n'))

def getfilenamenoprefixsuffix(filename):
	return filename.replace(eventprotodir, '').replace('.proto', '')
def getmd5fullfilename(filename):
	return md5dir + getfilenamenoprefixsuffix(filename)
def getdestfullfilename(filename):
	return destdirpath + getfilenamenoprefixsuffix(filename)
def getfileclassname(filename):
	letterarray = getfilenamenoprefixsuffix(filename).split('_')
	classname = ''
	for i in range(0, len(letterarray)): 
		classname += letterarray[i].capitalize()	
	return classname + 'Receiver'

def generatehead(filename):
	newstr = '#pragma once\n'
	newstr += '#include "src/game_logic/game_registry.h"\n\n'
	md5headfilename = filename.replace(eventprotodir, md5dir).replace('.proto', '') + headdestfilesuffix
	classname = getfileclassname(filename)
	for i in range(0, len(local.eventprotoarray)): 
		newstr += 'class ' + local.eventprotoarray[i] + ';\n'
	newstr += '\nclass ' + classname + '\n{\npublic:\n'
	newstr += tabstr + 'static void Register(entt::dispatcher& dispatcher);\n'
	newstr += tabstr + 'static void UnRegister(entt::dispatcher& dispatcher);\n\n'
	for i in range(0, len(local.eventprotoarray)): 
		newstr += tabstr + 'static void Receive' + str(i) + '(const ' + local.eventprotoarray[i] + '& event_obj);\n'
	newstr += '};\n'
	with open(md5headfilename, 'w', encoding='utf-8')as file:
		file.write(newstr)

def generatecpp(filename):
	md5cppfilename = filename.replace(eventprotodir, md5dir).replace('.proto', '') + cppdestfilesuffix
	destcppfullfilename = getdestfullfilename(filename) + cppdestfilesuffix
	classname = getfileclassname(filename)
	headerinclude = '#include "' + getfilenamenoprefixsuffix(filename) + headdestfilesuffix + '"\n'
	newstr = headerinclude
	eventindex = 0
	cppfunctionname = 'void '+ classname + '::Receive'
	try:
		with open(destcppfullfilename,'r+', encoding='utf-8') as file:
			yourcode = 1 
			part = 0
			for fileline in file:
				if part != cpprpceventpart and fileline == headerinclude:
					continue
				if part != cpprpceventpart and fileline.find(yourcodebegin) >= 0:
				    yourcode = 1
				    newstr += fileline
				    continue
				elif part != cpprpceventpart and fileline.find(yourcodeend) >= 0:
				    yourcode = 0
				    newstr += fileline + '\n'
				    part += 1
				    continue 
				elif part == cpprpceventpart:
					if eventindex < len(local.eventprotoarray) and fileline.find(cppfunctionname) >= 0 :
						yourcode = 0
						newstr += cppfunctionname + str(eventindex)
						newstr += '(const ' + local.eventprotoarray[eventindex] + '& event_obj)\n{\n'
						continue
					elif fileline.find(yourcodebegin) >= 0 :
						newstr += yourcodebegin + ' '  + '\n'
						yourcode = 1
						continue
					elif fileline.find(yourcodeend) >= 0 :
						newstr += yourcodeend + ' '  + '\n}\n\n'
						yourcode = 0
						eventindex += 1  
						continue
				if yourcode == 1:
					newstr += fileline				     
	except FileNotFoundError:
		newstr += genyourcodepair() + '\n'
	while eventindex < len(local.eventprotoarray) :
		newstr += cppfunctionname + str(eventindex)
		newstr += '(const ' + local.eventprotoarray[eventindex] + '& event_obj)\n{\n'
		newstr += genyourcodepair()
		newstr += '}\n'
		eventindex += 1
	with open(md5cppfilename, 'w', encoding='utf-8')as file:
		file.write(newstr)  

def md5copy(filename, destfilesuffix):
    gennewfilename = getmd5fullfilename(filename) + destfilesuffix
    filenamemd5 = gennewfilename + '.md5'
    error = None
    need_copy = False
    fullfilename = getdestfullfilename(filename) + destfilesuffix
    if  not os.path.exists(filenamemd5) or not os.path.exists(gennewfilename) or not os.path.exists(fullfilename):
        need_copy = True
    else:
        error = md5tool.check_against_md5_file(gennewfilename, filenamemd5)              
    if error == None and os.path.exists(fullfilename) and need_copy == False:
        return
    print("copy %s ---> %s" % (gennewfilename, fullfilename))
    shutil.copy(gennewfilename, fullfilename)
    md5tool.generate_md5_file_for(fullfilename, filenamemd5)

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
        md5copy(self.filename, headdestfilesuffix)
        md5copy(self.filename, cppdestfilesuffix)

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
