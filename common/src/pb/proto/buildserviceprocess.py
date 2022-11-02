import os

import md5tool
import shutil
import threading
import _thread
import protofilearray
import buildpublic
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
gsservicedir = '../../../../game_server/src/service/logic/'
rgservicedir = '../../../../region_server/src/service/logic/'
controllerservicedir = '../../../../controller_server/src/service/logic/'
logicprotodir = './logic_proto/'
tabstr = '    '
cpprpcservicepart = 1
controller = '(::google::protobuf::RpcController* controller'
servicedir = './md5/logic_proto/'


def getwritedir(serverstr):
    writedir = ''
    if serverstr == buildpublic.gs_file_prefix:
        writedir = gsservicedir
    elif serverstr == buildpublic.controller_file_prefix:
        writedir = controllerservicedir
    elif serverstr == buildpublic.rg_file_prefix:
        writedir = rgservicedir
    return writedir

def is_server_proto(filename):
    return (filename.find('client_player') <= 0 and filename.find('server_player') <= 0) or  filename.find(buildpublic.rg_file_prefix) >= 0


if not os.path.exists(servicedir):
    os.makedirs(servicedir)

def parsefile(filename):
    local.rpcarry = []
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
def genheadrpcfun():
    servicestr = 'public:\n'
    global controller
    local.servicenames = []
    for service in local.rpcarry:
        s = service.strip(' ').split(' ')
        line = tabstr + 'void ' + s[1] + controller + ',\n'
        local.servicenames.append(s[1])
        line += tabstr + tabstr + 'const ' + local.pkg + '::' + s[2].replace('(', '').replace(')', '') + '* request,\n'
        rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
        if rsp == 'google.protobuf.Empty' :
            line += tabstr + tabstr + '::google::protobuf::Empty* response,\n'
        else :
            line += tabstr + tabstr + local.pkg + '::' + rsp + '* response,\n'
        line += tabstr + tabstr + '::google::protobuf::Closure* done)override;\n\n'
        servicestr += line
    return servicestr

def gencpprpcfunbegin(rpcindex):
    servicestr = ''
    s = local.rpcarry[rpcindex]
    s = s.strip(' ').split(' ')
    servicestr = 'void ' + local.service + 'Impl::' + s[1] + controller + ',\n'
    servicestr +=  tabstr + 'const ' + local.pkg + '::' + s[2].replace('(', '').replace(')', '') + '* request,\n'
    rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
    if rsp == 'google.protobuf.Empty' :
        servicestr +=  tabstr + '::google::protobuf::Empty* response,\n'
    else :
        servicestr +=  tabstr + local.pkg + '::' + rsp + '* response,\n'
    servicestr +=  tabstr + '::google::protobuf::Closure* done)\n{\n'
    servicestr +=  tabstr + 'AutoRecycleClosure d(done);\n'
    return servicestr

def genyourcode():
    return yourcodebegin + '\n' + yourcodeend + '\n'

def classbegin():
    return 'class ' + local.service + 'Impl : public ' + local.pkg + '::' + local.service + '{\npublic:\n'  
def emptyfun():
    return ''

def getprevfilename(filename, writedir):
    if filename.find(logicprotodir) >= 0:
        if writedir == gsservicedir:
            return buildpublic.gs_file_prefix
        if writedir == controllerservicedir:
            return buildpublic.controller_file_prefix
        if writedir == rgservicedir:
            return buildpublic.rg_file_prefix
    return ''

def getpbdir(filename, writedir):
    if filename.find(logicprotodir) >= 0:
        return 'src/pb/pbc/logic_proto/'
    return ''

def genheadfile(destfilename, writedir):
    local.servicenames = []
    filename = destfilename.replace(logicprotodir, '').replace('.proto', '.h') 
    headfun = [classbegin, genheadrpcfun]
    hdestfilename = writedir +  getprevfilename(destfilename, writedir) + filename
    newheadfilename = servicedir +  getprevfilename(destfilename, writedir) +  filename
    newstr = '#pragma once\n'
    newstr += '#include "' + getpbdir(destfilename, writedir) + filename.replace('.h', '') + '.pb.h"\n'
    try:
        with open(hdestfilename,'r+', encoding='utf-8') as file:
            part = 0
            isyourcode = 1 
            skipheadline = 0 
            partend = 0
            for fileline in file:
                if skipheadline < 2 :
                    skipheadline += 1
                    continue
                if fileline.find(yourcodebegin) >= 0:
                    isyourcode = 1
                    newstr += fileline
                    continue
                elif fileline.find(yourcodeend) >= 0:
                    isyourcode = 0
                    partend = 1
                    newstr += fileline
                    continue
                if isyourcode == 1 :
                    newstr += fileline
                    continue
                if  part < len(headfun) and partend == 1:
                    newstr += headfun[part]()
                    part += 1
                    partend = 0
                    continue
                elif part >= len(headfun):
                    break

    except FileNotFoundError:
        for i in range(0, 2) :
            if i > 0:
                newstr += yourcode()
            newstr += headfun[i]()

    newstr += '};'
    with open(newheadfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def gencppfile(destfilename, writedir):
    filename = destfilename.replace(logicprotodir, '').replace('.proto', '.cpp')
    cppfilename = writedir  + getprevfilename(destfilename, writedir) + filename
    newcppfilename = servicedir + getprevfilename(destfilename, writedir) + filename
    newstr = '#include "' + getprevfilename(destfilename, writedir) + filename.replace('.cpp', '.h') + '"\n'
    newstr += '#include "src/network/rpc_closure.h"\n'
    serviceidx = 0
    try:
        with open(cppfilename,'r+', encoding='utf-8') as file:
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
        newstr += genyourcode() + '\n'
        newstr += rpcbegin + '\n'
    while serviceidx < len(local.rpcarry) :
        newstr += gencpprpcfunbegin(serviceidx)
        newstr += yourcodebegin +  '\n'
        newstr += yourcodeend +  '\n}\n\n'
        serviceidx += 1 
    newstr += rpcend + '\n'
    with open(newcppfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)


def getmd5prevfilename(filename, writedir):
    if is_server_proto(filename) == True :
        if writedir == gsservicedir:
            return buildpublic.gs_file_prefix
        if writedir == controllerservicedir:
            return buildpublic.controller_file_prefix
        if writedir == rgservicedir:
            return buildpublic.rg_file_prefix
    return ''

def md5copy(filename, writedir, fileextend):
        if filename.find('/') >= 0 :
            s = filename.split('/')
            filename = s[len(s) - 1]
        gennewfilename = servicedir + getmd5prevfilename(filename, writedir) + filename.replace('.proto', fileextend)
        filenamemd5 = gennewfilename + '.md5'
        error = None
        emptymd5 = False
        if  not os.path.exists(filenamemd5):
            emptymd5 = True
        else:
            error = md5tool.check_against_md5_file(gennewfilename, filenamemd5)              
        hdestfilename = writedir +  getmd5prevfilename(filename, writedir) + filename.replace('.proto', fileextend)
        #print("copy %s ---> %s  %s" % (filename, writedir, gennewfilename))
        if error == None and os.path.exists(hdestfilename) and emptymd5 == False:
            return
        print("copy %s ---> %s" % (gennewfilename, hdestfilename))
        md5tool.generate_md5_file_for(gennewfilename, filenamemd5)
        shutil.copy(gennewfilename, hdestfilename)

def generate(filename, writedir):
    parsefile(filename)
    genheadfile(filename, writedir)
    gencppfile(filename, writedir)
    md5copy(filename, writedir, '.h')
    md5copy(filename, writedir, '.cpp')

genfile = protofilearray.genfile

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
    step = filelen / cpu_count() + 1
    if cpu_count() > filelen:
        for i in range(0, filelen):
            t = myThread( genfile[i][0], genfile[i][1])
            t.start()
            threads.append(t)
    else :
        for i in range(0, cpu_count()):
            for j in range(i, i * step) :
                t = myThread(genfile[j][0], genfile[j][1])
                t.start()
                threads.append(t)
    for t in threads :
        t.join()

def get_file_list(file_path):
    dir_list = os.listdir(file_path)
    if not dir_list:
        return
    else:
        dir_list = sorted(dir_list,key=lambda x: os.path.getmtime(os.path.join(file_path, x)))
        return dir_list

def inputfile():
    dir_list  = get_file_list(logicprotodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        if buildpublic.is_gs_and_controller_server_proto(filename) == True :
            genfile.append([logicprotodir + filename, getwritedir(buildpublic.controller_file_prefix)])
            genfile.append([logicprotodir + filename, getwritedir(buildpublic.gs_file_prefix)])
        elif filename.find(buildpublic.rg_file_prefix) >= 0:
            genfile.append([logicprotodir +  filename, getwritedir(buildpublic.rg_file_prefix)])
            
inputfile()
main()
