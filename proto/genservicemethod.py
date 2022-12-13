import os

import md5tool
import shutil
import threading
import _thread
import protofilearray
import genpublic
from multiprocessing import cpu_count

local = threading.local()

local.rpcarry = []
local.servicenames = []
local.service = ''

threads = []
local.pkg = ''
cpkg = 'package'
yourcodebegin = '///<<< BEGIN WRITING YOUR CODE'
yourcodeend = '///<<< END WRITING YOUR CODE'
rpcbegin = '///<<<rpc begin'
rpcend = '///<<<rpc end'
gsservicedir = '../game_server/src/service/logic_proto/'
lobbyservicedir = '../lobby_server/src/service/logic_proto/'
controllerservicedir = '../controller_server/src/service/logic_proto/'
logicprotodir = 'logic_proto/'
tabstr = '    '
cpprpcservicepart = 1
controller = '(::google::protobuf::RpcController* controller'
servicedir = './md5/logic_proto/'
methodsufix = 'method.h'

genfile = []
local.packagemessage = set()

def parsefile(filename):
    local.rpcarry = []
    local.packagemessage = set()
    local.pkg = ''
    local.service = ''
    rpcbegin = 0 
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('rpc') >= 0 and rpcbegin == 1:
                local.rpcarry.append(fileline)
            elif fileline.find(cpkg) >= 0:
                local.pkg = fileline.replace(cpkg, '').replace(';', '').replace(' ', '').strip('\n')
            elif fileline.find('service ') >= 0:
                rpcbegin = 1
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')
            elif fileline.find('message ') >= 0:
                local.packagemessage.add(fileline.replace('message ', '').replace('\r', '').replace('\n', ''))
def genheadrpcfun():
    servicestr = ''
    global controller
    local.servicenames = []
    index = 0
    for service in local.rpcarry:
        s = service.strip(' ').split(' ')
        method = s[1]
        methodname = local.pkg + local.service +  method + 'Method'
        methodindexname = methodname + 'Index'
        line = 'const uint32_t ' +  methodindexname + ' = ' + str(index) + ';\n'
        line += '#define ' +  methodname + 'Desc ' \
        + local.pkg + '::' + local.service + '_Stub'\
        + '::descriptor()->method(' + methodindexname + ')\n\n'
        index += 1
        servicestr += line
    return servicestr

def getfilenamewithnopath(filename, destdir):
    servertypedir = genpublic.getservertype(destdir) + '/'
    return filename.replace(logicprotodir, '').replace('common_proto/', '').replace(servertypedir,'')

def genheadfile(filename):
    local.servicenames = []
    filename = filename.replace('.proto', methodsufix) 
    md5filename = genpublic.servicemethodmd5dir +  filename
    newstr = '#pragma once\n'
    newstr += '#include <cstdint>\n\n'
    newstr += '#include "'  + filename.replace(methodsufix, '') + '.pb.h"\n'
    newstr += genheadrpcfun()
    with open(md5filename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def md5copy(filename,   fileextend):
    gennewfilename = genpublic.servicemethodmd5dir + filename.replace('.proto', fileextend)
    destfilename = genpublic.servicemethoddir  + filename.replace('.proto', fileextend)
    filenamemd5 = gennewfilename + '.md5'
    error = None
    emptymd5 = False
    if  not os.path.exists(filenamemd5):
        emptymd5 = True
    else:
        if not os.path.exists(destfilename):
            error = True
        else:
            error = md5tool.check_against_md5_file(gennewfilename, filenamemd5)   
      
    if error == None and emptymd5 == False:
        return
    print("copy %s ---> %s" % (gennewfilename, destfilename))
    shutil.copy(gennewfilename, destfilename)
    md5tool.generate_md5_file_for(gennewfilename, filenamemd5)


class myThread (threading.Thread):
    def __init__(self, filename, filepath):
        threading.Thread.__init__(self)
        self.filename = str(filename)
        self.filepath = str(filepath)
    def run(self):
        parsefile(self.filepath)
        if local.service == '':
            return
        genheadfile(self.filename)
        md5copy(self.filename,  'method.h')

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
main()
