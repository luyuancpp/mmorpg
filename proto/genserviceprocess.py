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

genfile = []

def parsefile(filename):
    local.rpcarry = []
    local.service = ''
    rpcbegin = 0 
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('rpc') >= 0 and rpcbegin == 1:
                local.rpcarry.append(fileline)
            elif genpublic.is_service_fileline(fileline) == True:
                rpcbegin = 1
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')

def genheadrpcfun():
    servicestr = 'class ' + local.service + 'Impl : public ' + '::' + local.service + '{\npublic:\n'  
    servicestr += 'public:\n'
    global controller
    local.servicenames = []
    for service in local.rpcarry:
        s = service.strip(' ').split(' ')
        line = tabstr + 'void ' + s[1] + controller + ',\n'
        local.servicenames.append(s[1])
        rq =  s[2].replace('(', '').replace(')', '')
        line += tabstr + tabstr + 'const ' +  '::' + rq + '* request,\n'
        rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
        if rsp == 'google.protobuf.Empty' :
            line += tabstr + tabstr + '::google::protobuf::Empty* response,\n'
        else :
            line += tabstr + tabstr +  '::' + rsp + '* response,\n'
        line += tabstr + tabstr + '::google::protobuf::Closure* done)override;\n\n'
        servicestr += line
    servicestr += '};'
    return servicestr

def gencpprpcfunbegin(rpcindex):
    servicestr = ''
    s = local.rpcarry[rpcindex]
    s = s.strip(' ').split(' ')
    servicestr = 'void ' + local.service + 'Impl::' + s[1] + controller + ',\n'
    rq =  s[2].replace('(', '').replace(')', '')
    servicestr +=  tabstr + 'const ' + '::' + rq + '* request,\n'
    rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
    if rsp == 'google.protobuf.Empty' :
        servicestr +=  tabstr + '::google::protobuf::Empty* response,\n'
    else :
        servicestr +=  tabstr + '::' + rsp + '* response,\n'
    servicestr +=  tabstr + '::google::protobuf::Closure* done)\n{\n'

    return servicestr

def genyourcode():
    return yourcodebegin + '\n' + yourcodeend + '\n'

def classbegin():
    return 


def getprevfilename(filename, destdir):
    if filename.find(logicprotodir) >= 0:
        if destdir == gsservicedir:
            return genpublic.gs_file_prefix
        if destdir == controllerservicedir:
            return genpublic.controller_file_prefix
        if destdir == lobbyservicedir:
            return ''
    return ''

def getpbdir(filename, destdir):
    if filename.find(logicprotodir) >= 0:
        return 'src/pb/pbc/logic_proto/'
    return ''

def getfilenamewithnopath(filename, destdir):
    return filename.replace('common_proto/', '')

def genheadfile(filename,  destdir,  md5dir):
    local.servicenames = []
    filename = getfilenamewithnopath(filename, destdir).replace('.proto', '.h') 
    destfilename = destdir + filename
    md5filename = md5dir +  filename
    newstr = '#pragma once\n'
    newstr += '#include "' + getpbdir(filename, destdir) + filename.replace('.h', '') + '.pb.h"\n'
    newstr += genheadrpcfun()
    with open(md5filename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def gencppfile(filename, destdir, md5dir):
    filename = getfilenamewithnopath(filename, destdir).replace('.proto', '.cpp') 
    destfilename = destdir + filename
    md5filename = md5dir +  filename
    newstr = '#include "' + getprevfilename(destfilename, destdir) + filename.replace('.cpp', '.h') + '"\n'
    newstr += '#include "src/network/rpc_msg_route.h"\n'
    serviceidx = 0
    try:
        with open(destfilename,'r+', encoding='utf-8') as file:
            part = 0
            isyourcode = 1 
            skipheadline = 0 
            for fileline in file:
                if skipheadline < 2 :
                    skipheadline += 1
                    continue
                if part != cpprpcservicepart and fileline.find(yourcodebegin) >= 0:
                    isyourcode = 1
                    newstr += fileline
                    continue
                elif part != cpprpcservicepart and fileline.find(yourcodeend) >= 0:
                    isyourcode = 0
                    newstr += fileline + '\n'
                    part += 1
                    continue     
                elif part == cpprpcservicepart:
                    if fileline.find(rpcbegin) >= 0:
                        newstr += fileline
                        continue
                    elif serviceidx < len(local.rpcarry) and fileline.find(local.servicenames[serviceidx] + controller) >= 0 :
                        isyourcode = 0
                        newstr += gencpprpcfunbegin(serviceidx)
                        continue
                    elif fileline.find(yourcodebegin) >= 0 :
                        newstr += yourcodebegin + ' ' +  '\n'
                        isyourcode = 1
                        continue
                    elif fileline.find(yourcodeend) >= 0 :
                        newstr += yourcodeend + ' ' +  '\n}\n\n'
                        isyourcode = 0
                        serviceidx += 1  
                        continue
                    elif fileline.find(rpcend) >= 0:
                        break
                if isyourcode == 1:
                    newstr += fileline
                    continue                
    except FileNotFoundError:
        newstr += rpcbegin + '\n'
    while serviceidx < len(local.rpcarry) :
        newstr += gencpprpcfunbegin(serviceidx)
        newstr += yourcodebegin +  '\n'
        newstr += yourcodeend +  '\n}\n\n'
        serviceidx += 1 
    newstr += rpcend + '\n'
    with open(md5filename, 'w', encoding='utf-8')as file:
        file.write(newstr)


def md5copy(filename, destdir, md5dir, fileextend):
        if filename.find('/') >= 0 :
            s = filename.split('/')
            filename = s[len(s) - 1]
        gennewfilename = md5dir + filename.replace('.proto', fileextend)
        filenamemd5 = gennewfilename + '.md5'
        error = None
        emptymd5 = False
        if  not os.path.exists(filenamemd5):
            emptymd5 = True
        else:
            error = md5tool.check_against_md5_file(gennewfilename, filenamemd5)           
        destfilename =  destdir + filename.replace('.proto', fileextend)
        if error == None and os.path.exists(destfilename) and emptymd5 == False:
            return
        
        print("copy %s ---> %s" % (gennewfilename, destfilename))
        md5tool.generate_md5_file_for(gennewfilename, filenamemd5)
        shutil.copy(gennewfilename, destfilename)

def generate(filename, destdir, md5dir):
    parsefile(filename)
    genheadfile(filename, destdir, md5dir)
    gencppfile(filename, destdir, md5dir)
    md5copy(filename, destdir, md5dir, '.h')
    md5copy(filename, destdir, md5dir, '.cpp')

class myThread (threading.Thread):
    def __init__(self, filename, destdir, md5dir):
        threading.Thread.__init__(self)
        self.filename = str(filename)
        self.md5dir = str(md5dir)
        self.destdir = str(destdir)
    def run(self):
        generate(self.filename, self.destdir, self.md5dir)

def main():
    filelen = len(genfile)
    global threads
    step = int(filelen / cpu_count() + 1)
    if cpu_count() > filelen:
        for i in range(0, filelen):
            t = myThread( genfile[i][0], genfile[i][1], genfile[i][2])
            threads.append(t)
            t.start()
    else :
        for i in range(0, cpu_count()):
            for j in range(i, i * step) :
                t = myThread(genfile[j][0], genfile[j][1], genfile[i][2])
                threads.append(t)
                t.start()
    for t in threads :
        t.join()

genfile = protofilearray.genfile
genpublic.makedirs()
main()
