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
    servicestr = 'public:\n'
    global controller
    local.servicenames = []
    for service in local.rpcarry:
        s = service.strip(' ').split(' ')
        line = tabstr + 'void ' + s[1] + controller + ',\n'
        local.servicenames.append(s[1])
        rq =  s[2].replace('(', '').replace(')', '')
        pkg = local.pkg
        if rq in local.packagemessage:
            pass
        else:
            pkg = ''
        line += tabstr + tabstr + 'const ' + pkg + '::' + rq + '* request,\n'
        rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
        if rsp in local.packagemessage:
            pass
        else:
            pkg = ''
        if rsp == 'google.protobuf.Empty' :
            line += tabstr + tabstr + '::google::protobuf::Empty* response,\n'
        else :
            line += tabstr + tabstr + pkg + '::' + rsp + '* response,\n'
        line += tabstr + tabstr + '::google::protobuf::Closure* done)override;\n\n'
        servicestr += line
    return servicestr

def gencpprpcfunbegin(rpcindex):
    servicestr = ''
    s = local.rpcarry[rpcindex]
    s = s.strip(' ').split(' ')
    servicestr = 'void ' + local.service + 'Impl::' + s[1] + controller + ',\n'
    rq =  s[2].replace('(', '').replace(')', '')
    pkg = local.pkg
    if rq in local.packagemessage:
        pass
    else:
        pkg = ''
    servicestr +=  tabstr + 'const ' + pkg + '::' + rq + '* request,\n'
   
    rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
    if rsp in local.packagemessage:
        pass
    else:
        pkg = ''
    if rsp == 'google.protobuf.Empty' :
        servicestr +=  tabstr + '::google::protobuf::Empty* response,\n'
    else :
        servicestr +=  tabstr + pkg + '::' + rsp + '* response,\n'
    servicestr +=  tabstr + '::google::protobuf::Closure* done)\n{\n'
    servicestr +=  tabstr + 'AutoRecycleClosure d(done);\n'
    return servicestr

def genyourcode():
    return yourcodebegin + '\n' + yourcodeend + '\n'

def classbegin():
    return 'class ' + local.service + 'Impl : public ' + local.pkg + '::' + local.service + '{\npublic:\n'  


def getprevfilename(filename, writedir):
    if filename.find(logicprotodir) >= 0:
        if writedir == gsservicedir:
            return genpublic.gs_file_prefix
        if writedir == controllerservicedir:
            return genpublic.controller_file_prefix
        if writedir == lobbyservicedir:
            return ''
    return ''

def getpbdir(filename, writedir):
    if filename.find(logicprotodir) >= 0:
        return 'src/pb/pbc/logic_proto/'
    return ''

def getfilenamewithnopath(filename, writedir):
    servertypedir = genpublic.getservertype(writedir) + '/'
    return filename.replace(logicprotodir, '').replace('common_proto/', '').replace(servertypedir,'')

def genheadfile(filename, writedir):
    local.servicenames = []
    filename = getfilenamewithnopath(filename, writedir).replace('.proto', '.h') 
    headfun = [classbegin, genheadrpcfun]
    destfilename = writedir + filename
    md5dir = genpublic.getsrcpathmd5dir(writedir, genpublic.commonproto())
    md5filename = md5dir +  filename
    newstr = '#pragma once\n'
    newstr += '#include "' + getpbdir(filename, writedir) + filename.replace('.h', '') + '.pb.h"\n'
    
    for i in range(0, 2) :
        newstr += headfun[i]()

    newstr += '};'
    with open(md5filename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def gencppfile(filename, writedir):
    filename = getfilenamewithnopath(filename, writedir).replace('.proto', '.cpp') 
    destfilename = writedir + filename
    md5filename = genpublic.getsrcpathmd5dir(writedir, genpublic.commonproto()) +  filename
    newstr = '#include "' + getprevfilename(destfilename, writedir) + filename.replace('.cpp', '.h') + '"\n'
    newstr += '#include "src/network/rpc_closure.h"\n'
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


def getmd5prevfilename(filename, writedir):
    if genpublic.is_server_proto(filename) == True :
        if writedir == gsservicedir:
            return genpublic.gs_file_prefix
        if writedir == controllerservicedir:
            return genpublic.controller_file_prefix
        if writedir == lobbyservicedir:
            return ''
    return ''

def md5copy(filename, writedir, fileextend):
        if filename.find('/') >= 0 :
            s = filename.split('/')
            filename = s[len(s) - 1]
        gennewfilename = genpublic.getsrcpathmd5dir(writedir, genpublic.commonproto()) + filename.replace('.proto', fileextend)
        destfilename = writedir +  getmd5prevfilename(filename, writedir) + filename.replace('.proto', fileextend)
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
          
        #print("copy %s ---> %s  %s" % (filename, writedir, gennewfilename))
        if error == None and emptymd5 == False:
            return
        print("copy %s ---> %s" % (gennewfilename, destfilename))
        shutil.copy(gennewfilename, destfilename)
        md5tool.generate_md5_file_for(gennewfilename, filenamemd5)

def generate(filename, writedir):
    parsefile(filename)
    genheadfile(filename, writedir)
    gencppfile(filename, writedir)
    md5copy(filename, writedir, '.h')
    md5copy(filename, writedir, '.cpp')

class myThread (threading.Thread):
    def __init__(self, filename, writedir):
        threading.Thread.__init__(self)
        self.filename = str(filename)
        self.writedir = str(writedir)
    def run(self):
        generate(self.filename, self.writedir)

def main():
    filelen = len(genfile)
    global threads
    step = int(filelen / cpu_count() + 1)
    if cpu_count() > filelen:
        for i in range(0, filelen):
            t = myThread( genfile[i][0], genfile[i][1])
            threads.append(t)
            t.start()
    else :
        for i in range(0, cpu_count()):
            for j in range(i, i * step) :
                t = myThread(genfile[j][0], genfile[j][1])
                threads.append(t)
                t.start()
    for t in threads :
        t.join()

genfile = protofilearray.genfile
genpublic.makedirs()
main()
