import os
import threading
import genpublic
local = threading.local()

local.filemethodarray = []
local.service = ''

threads = []

tabstr = '    '
servicedir = './md5/logic_proto/'
methodsufix = 'method.h'

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
        method = s[1]
        methodname =  local.service +  method
        methodindex = methodname + 'Index'
        line = 'const uint32_t ' +  methodindex + ' = ' + str(index) + ';\n'
        line += '#define ' +  methodname + ' ::' + local.service + '_Stub'\
        + '::descriptor()->method(' + methodindex + ')\n\n'
        index += 1
        servicestr += line
    return servicestr


def genheadfile(filename):
    filename = filename.replace('.proto', methodsufix) 
    md5filename = genpublic.servicemethodmd5dir +  filename
    newstr = '#pragma once\n'
    newstr += '#include <cstdint>\n\n'
    newstr += '#include "'  + filename.replace(methodsufix, '') + '.pb.h"\n'
    newstr += genheadrpcfun()
    with open(md5filename, 'w', encoding='utf-8')as file:
        file.write(newstr)

class myThread (threading.Thread):
    def __init__(self, filename, filepath):
        threading.Thread.__init__(self)
        self.filename = str(filename)
        self.filepath = str(filepath)
    def run(self):
        parsefile(self.filepath)
        if local.service == '':
            return
        md5info = genpublic.md5fileinfo()
        md5info.filename = self.filename
        md5info.destdir = genpublic.servicemethoddir
        md5info.md5dir = genpublic.servicemethodmd5dir
        md5info.originalextension = '.proto'
        md5info.targetextension = methodsufix
        checkheadmd5,_,_,_ = genpublic.md5check(md5info)   
        if checkheadmd5 == True:
            return
        genheadfile(self.filename)
        genpublic.md5copy(md5info)

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
    global threads
    for i in range(0, len(genfile)):
        t = myThread( genfile[i][0], genfile[i][1])
        threads.append(t)
        t.start()
    for t in threads :
        t.join()

genpublic.makedirs()
scanfile() 
main()
