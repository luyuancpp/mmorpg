import os
import threading
import genpublic

local = threading.local()

local.filemethodarray = []
local.service = ''

threads = []

tabstr = '    '
servicedir = './md5/logic_proto/'
methodsufix = 'method_lua.cpp'
gatherfile = "init_service_lua.cpp"

genfile = []

def parsefile(filename):
    local.filemethodarray = []
    local.service = ''
    rpcbegin = 0 
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('rpc') >= 0 and rpcbegin == 1:
                local.filemethodarray.append(fileline)
            elif genpublic.is_service_fileline(fileline) == True:
                rpcbegin = 1
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')
def genheadrpcfun():
    servicestr = ''
    index = 0
    for service in local.filemethodarray:
        s = service.strip(' ').split(' ')
        methodname = local.service + s[1]
        line = 'tls_lua_state["' +  methodname + '"] = []()-> const ::google::protobuf::MethodDescriptor* {\n' + 'return ' + local.service + '_Stub'\
        + '::descriptor()->method(' + str(index) + ');\n};\n\n'
        index += 1
        servicestr += line
    return servicestr


def getmothedname(filename):
    return "Init" + filename.replace('.proto', '')  + 'MethodLua'

def genheadfile(filename):
    funname = getmothedname(filename)
    filename = filename.replace('.proto', methodsufix) 
    md5filename = genpublic.getmd5filename(genpublic.servicemethoddir) +  filename
    newstr = '#pragma once\n'
    newstr += '#include <google/protobuf/descriptor.h>\n'
    newstr += '#include "src/game_logic/thread_local/thread_local_storage_lua.h"\n'    
    
    newstr += '#include "'  + filename.replace(methodsufix, '') + '.pb.h"\n\n'
    newstr += 'void '  + funname + '(){\n\n'    
    newstr += genheadrpcfun()
    newstr += "}"
    with open(md5filename, 'w', encoding='utf-8')as file:
        file.write(newstr)

class myThread (threading.Thread):
    def __init__(self, filename, filepath):
        threading.Thread.__init__(self)
        self.filename = str(filename)
        self.filepath = str(filepath)
    def run(self):
        md5info = genpublic.md5fileinfo()
        md5info.filename = self.filename
        md5info.destdir = genpublic.servicemethoddir
        md5info.originalextension = '.proto'
        md5info.targetextension = methodsufix
        checkheadmd5,_,_,_ = genpublic.md5check(md5info)   
        if checkheadmd5 == True:
            return
        parsefile(self.filepath)
        genheadfile(self.filename)
       
        genpublic.md5copy(md5info)

def gengatherfile(filename):
    funname = "InitService"   + 'MethodLua'
    md5filename = genpublic.getmd5filename(genpublic.servicemethoddir) +  filename
    newstr = ""
    for protofilename in genfile:
            funname = getmothedname(protofilename[0])
            newstr += "void " + funname + "();\n"
    newstr += '\n\nvoid InitServiceMethodLua(){\n\n'    
    for protofilename in genfile:
            funname = getmothedname(protofilename[0])
            newstr += funname + "();\n"
    newstr += "\n\n}"
    with open(md5filename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def scanfile():
    dir_list  =  os.listdir(genpublic.logicprotodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        genfile.append([filename, genpublic.logicprotodir + filename])
    dir_list  =  os.listdir(genpublic.commonportodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        genfile.append([filename, genpublic.commonportodir + filename])
        
def main():
    filelen = len(genfile)
    global threads
    for i in range(0, filelen):
        t = myThread( genfile[i][0], genfile[i][1])
        threads.append(t)
        t.start()
    for t in threads :
        t.join()

genpublic.makedirs()
scanfile() 
gengatherfile(gatherfile)
md5info = genpublic.md5fileinfo()
md5info.filename = gatherfile
md5info.destdir = genpublic.servicemethoddir
md5info.originalextension = '.proto'
md5info.targetextension = ''
genpublic.md5copy(md5info)
main()
