import os
import threading
import genpublic

local = threading.local()
local.destdir = '../game_server/src/event_receiver/'
local.md5dir = ''
local.eventprotoarray = []

eventricevermd5dir = './md5/event_proto/'
gsdestdir = '../game_server/src/event_receiver/'
controllerdestdir = '../controller_server/src/event_receiver/'

eventprotodir = './event_proto/'
tabstr = '    '
filelist = []
threads = []

cpprpceventpart = 1
syncfilename = 'sync_event_receiver'
syncclassfilename = 'SyncEventReceiver'
asyncfilename = 'async_event_receiver'
asyncclassfilename = 'AsyncEventReceiver'
normalcfilename = 'event_receiver'
normalcclassfilename = 'EventReceiver'
currentfilename = normalcfilename
currentclassname = normalcclassfilename

if not os.path.exists(eventricevermd5dir):
    os.makedirs(eventricevermd5dir)

def beginendstring():
    return genpublic.yourcodebegin + '\n' + genpublic.yourcodeend + '\n'

def parsefile(filename):
	local.eventprotoarray = []
	with open(filename,'r', encoding='utf-8') as file:
		for fileline in file:
			if not (fileline.find('message') >= 0):
				continue
			local.eventprotoarray.append(fileline.split(' ')[1].strip('\n'))

def getfilenamenoprefixsuffix(filename):
	return os.path.basename(filename).replace('.proto', '').replace('event', currentfilename)
def getmd5destfilename(filename):
	return eventricevermd5dir + getfilenamenoprefixsuffix(filename)

def getfileclassname(filename):
	letterarray = getfilenamenoprefixsuffix(filename).split('_')
	classname = ''
	for i in range(0, len(letterarray)): 
		classname += letterarray[i].capitalize()	
	return classname + 'Receiver'
def getprotofilenamenoprefixsuffix(filename):
	return os.path.basename(filename).replace('.proto', '')

def getrealsuffix(filename):
	filename.replace('event', currentfilename)

def generatehead(filename):
	newstr = '#pragma once\n'
	newstr += '#include "src/game_logic/thread_local/thread_local_storage.h"\n\n'
	md5headfilename = getmd5destfilename(filename) + '.h'
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

def generatecppregisterunregisterfunction(filename):
	classname = getfileclassname(filename)
	cppreceiverfunctionname = classname + '::Receive'
	newstr = 'void ' + classname + '::Register(entt::dispatcher& dispatcher)\n{\n'
	eventindex = 0
	while eventindex < len(local.eventprotoarray) :
		newstr += 'dispatcher.sink<' + local.eventprotoarray[eventindex] + '>().connect<&'
		newstr += classname + '::' + cppreceiverfunctionname + str(eventindex) + '>();\n'
		eventindex += 1
	newstr += '}\n\n'
	newstr += 'void ' + classname + '::UnRegister(entt::dispatcher& dispatcher)\n{\n'
	eventindex = 0
	while eventindex < len(local.eventprotoarray) :
		newstr += 'dispatcher.sink<' + local.eventprotoarray[eventindex] + '>().disconnect<&'
		newstr += classname + '::' + cppreceiverfunctionname + str(eventindex) + '>();\n'
		eventindex += 1
	newstr += '}\n\n'
	return newstr

def generatecpp(filename):
	md5cppfilename = getmd5destfilename(filename) + '.cpp'
	destcppfilename = local.destdir + getfilenamenoprefixsuffix(filename) + '.cpp'
	classname = getfileclassname(filename)
	headerinclude = '#include "' + getfilenamenoprefixsuffix(filename)  + '.h"\n'
	pbinclude = '#include "event_proto/' + getprotofilenamenoprefixsuffix(filename) + '.pb.h"\n'
	newstr = headerinclude + pbinclude
	eventindex = 0
	cppreceiverfunctionname = 'void '+ classname + '::Receive'
	try:
		with open(destcppfilename,'r+', encoding='utf-8') as file:
			yourcode = 0 
			part = 0
			head_line_count = 0
			for fileline in file:
				if head_line_count < 2:
					head_line_count += 1
					continue
				if part == 0:
					newstr += fileline
					if fileline.find(genpublic.yourcodeend) >= 0:
						newstr += generatecppregisterunregisterfunction(filename)
						part += 1
					continue
				if part != cpprpceventpart and fileline.find(genpublic.yourcodebegin) >= 0:
					newstr += fileline
					continue
				elif part != cpprpceventpart and fileline.find(genpublic.yourcodeend) >= 0:
					newstr += fileline 
					part += 1
					continue 
				elif part == cpprpceventpart:
					if eventindex < len(local.eventprotoarray) and fileline.find(cppreceiverfunctionname) >= 0 :
						yourcode = 0
						newstr += cppreceiverfunctionname + str(eventindex)
						newstr += '(const ' + local.eventprotoarray[eventindex] + '& event_obj)\n{\n'
						continue
					elif fileline.find(genpublic.yourcodebegin) >= 0 :
						newstr += genpublic.yourcodebegin + '\n'
						yourcode = 1
						continue
					elif fileline.find(genpublic.yourcodeend) >= 0 :
						newstr += genpublic.yourcodeend  + '\n}\n\n'
						yourcode = 0
						eventindex += 1  
						continue
				if yourcode == 1:
					newstr += fileline				     
	except FileNotFoundError:
		newstr += beginendstring() + '\n'
		newstr += generatecppregisterunregisterfunction(filename)
	while eventindex < len(local.eventprotoarray) :
		newstr += cppreceiverfunctionname + str(eventindex)
		newstr += '(const ' + local.eventprotoarray[eventindex] + '& event_obj)\n{\n'
		newstr += beginendstring()
		newstr += '}\n'
		eventindex += 1
	with open(md5cppfilename, 'w', encoding='utf-8')as file:
		file.write(newstr)  

def geneventreceiverhead():
	md5headfilename = eventricevermd5dir + currentfilename + '.h'
	newstr = '#pragma once\n'
	newstr += '#include "src/game_logic/thread_local/thread_local_storage.h"\n'	
	newstr += '\nclass ' + currentclassname + '\n{\npublic:\n'
	newstr += tabstr + 'static void Register(entt::dispatcher& dispatcher);\n'
	newstr += tabstr + 'static void UnRegister(entt::dispatcher& dispatcher);\n'
	newstr += '};\n'
	with open(md5headfilename, 'w', encoding='utf-8')as file:
		file.write(newstr)

def geneventreceivercpp():
	md5headfilename = eventricevermd5dir + currentfilename + '.cpp'
	newstr = '#pragma once\n'
	newstr += '#include "' + currentfilename + '.h"\n'
	for i in range(0, len(filelist)): 
		newstr +=  '#include "' + getfilenamenoprefixsuffix(filelist[i]) +  '.h"\n'	
	newstr += '\n'
	newstr += 'void ' + currentclassname + '::Register(entt::dispatcher& dispatcher)\n{\n'
	for i in range(0, len(filelist)): 
		classname = getfileclassname(filelist[i])
		newstr += tabstr +  classname + '::Register(dispatcher);\n'
	newstr += '}\n\n'
	newstr += 'void ' + currentclassname + '::UnRegister(entt::dispatcher& dispatcher)\n{\n'
	for i in range(0, len(filelist)): 
		classname = getfileclassname(filelist[i])
		newstr += tabstr +  classname + '::UnRegister(dispatcher);\n'
	newstr += '}\n'
	with open(md5headfilename, 'w', encoding='utf-8')as file:
		file.write(newstr)

def scanprotofile():
    dir_list = os.listdir(eventprotodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        filelist.append(eventprotodir + filename)

class myThread (threading.Thread):
    def __init__(self, filename):
        threading.Thread.__init__(self)
        self.filename = str(filename)
    def run(self):
        cppmd5info = genpublic.md5fileinfo()
        local.destdir = gsdestdir
        cppmd5info.destdir = local.destdir
        cppmd5info.md5dir =  eventricevermd5dir 
        parsefile(self.filename)
        generatehead(self.filename)
        generatecpp(self.filename)
        
        cppmd5info.filename = getfilenamenoprefixsuffix(self.filename) + '.h'
        genpublic.md5copy(cppmd5info)
        cppmd5info.filename = getfilenamenoprefixsuffix(self.filename) + '.cpp'
        genpublic.md5copy(cppmd5info)

def main():
    global threads
    for i in range(0, len(filelist)):
        t = myThread(filelist[i])
        threads.append(t)
        t.start()
    for t in threads:
        t.join()
    


genpublic.makedirs()

#sync
scanprotofile()
main()
geneventreceiverhead()

cppmd5info = genpublic.md5fileinfo()
cppmd5info.filename = 'event_receiver.h'
cppmd5info.destdir = local.destdir 
cppmd5info.md5dir = eventricevermd5dir
genpublic.md5copy(cppmd5info)
geneventreceivercpp()
cppmd5info.destdir = local.destdir  
cppmd5info.md5dir =   eventricevermd5dir 
cppmd5info.filename = 'event_receiver.cpp'
genpublic.md5copy(cppmd5info)

#currentfilename = asyncfilename
#currentclassname = asyncclassfilename
#main()
#geneventreceiverhead()
#md5copy('event', '.h')
#geneventreceivercpp()
#md5copy('event', '.cpp')