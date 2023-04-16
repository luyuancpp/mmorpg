import threading
import protofilearray
import os
import genpublic

local = threading.local()

local.filemethodarray = []
local.service = ''
threads = []
gsservicedir = '../game_server/src/service/logic_proto/'
lobbyservicedir = '../lobby_server/src/service/logic_proto/'
controllerservicedir = '../controller_server/src/service/logic_proto/'
tabstr = '    '
controller = '(::google::protobuf::RpcController* controller'

genfile = protofilearray.genfile


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

def getpbdir(writedir):
    if writedir.find(genpublic.logicprotodir) >= 0:
        return 'src/pb/pbc/logic_proto/'
    return ''

def genheadrpcfun():
    servicestr = 'class ' + local.service + 'Impl : public ' +  '::' + local.service + '{\npublic:\n'
    servicestr += 'public:\n'
    global controller
    for service in local.filemethodarray:
        s = service.strip(' ').split(' ')
        methodname = s[1]
        requestname = s[2]
        responsespb = s[4]
        line = tabstr + 'void ' + methodname + controller + ',\n'
        line += tabstr + tabstr + 'const ' + '::' + requestname.replace('(', '').replace(')', '') + '* request,\n'
        rsp = responsespb.replace('(', '').replace(')',  '').replace(';',  '').strip('\n')
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
    s = local.filemethodarray[rpcindex]
    s = s.strip(' ').split(' ')
    requestname = s[2]
    servicestr = 'void ' + local.service + 'Impl::' + s[1] + controller + ',\n'
    servicestr +=  tabstr + 'const ' +  '::' + requestname.replace('(', '').replace(')', '') + '* request,\n'
    responsespb = s[4]
    rsp = responsespb.replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
    if rsp == 'google.protobuf.Empty' :
        servicestr +=  tabstr + '::google::protobuf::Empty* response,\n'
    else :
        servicestr +=  tabstr +  '::' + rsp + '* response,\n'
    servicestr +=  tabstr + '::google::protobuf::Closure* done)\n{\n'
    return servicestr

def genyourcode():
    return genpublic.yourcodebegin + '\n' + genpublic.yourcodeend + '\n'

def genheadfile(filename,  destdir):
    filename = os.path.basename(filename).replace('.proto', '.h') 
    md5filename = genpublic.getmd5filename(destdir) +   filename
    newstr = '#pragma once\n'
    newstr += '#include "' + getpbdir( destdir) + filename.replace('.h', '') + '.pb.h"\n'
    newstr += genheadrpcfun()
    with open(md5filename, 'w', encoding='utf-8')as file:
        file.write(newstr)
        


class myThread (threading.Thread):
    def __init__(self, filename, destdir):
        threading.Thread.__init__(self)
        self.filename = str(filename)
        self.destdir = str(destdir)
    def run(self):
        
        hmd5info = genpublic.md5fileinfo()
        hmd5info.filename = self.filename
        hmd5info.destdir = self.destdir
        hmd5info.originalextension = '.proto'
        hmd5info.targetextension = '.h'
        checkheadmd5,_,_,_ = genpublic.md5check(hmd5info)  
        
        cppmd5info = genpublic.md5fileinfo()
        cppmd5info.filename = self.filename
        cppmd5info.destdir = self.destdir
        cppmd5info.originalextension = '.proto'
        cppmd5info.targetextension = '.cpp'  
        checkcppmd5,_,_,_  = genpublic.md5check(cppmd5info)    
        if checkheadmd5 == True and checkcppmd5 == True:
            return
        parsefile(self.filename)
        if checkheadmd5 == False:
            genheadfile(self.filename, self.destdir)
            genpublic.md5copy(hmd5info)
        if checkcppmd5 == False:
            destext = '.cpp'
            filename = os.path.basename(self.filename).replace('.proto', destext) 
            cppfile = genpublic.cpp()
            cppfile.destfilename = self.destdir + filename
            skillinclude = '#include "' +  filename.replace(destext, '.h') + '"\n'
            skillinclude += '#include "src/network/rpc_msg_route.h"\n'
            cppfile.includestr = skillinclude
            cppfile.filemethodarray = local.filemethodarray
            cppfile.begunfun = gencpprpcfunbegin
            cppfile.controller = controller
            genpublic.gencppfile(cppfile)
            genpublic.md5copy(cppmd5info)

def main():
    filelen = len(genfile)
    for i in range(0, filelen):
        t = myThread( genfile[i][0], genfile[i][1])
        threads.append(t)
        t.start()
    for t in threads :
        t.join()

def scanprotofile():
    dir_list  = os.listdir(genpublic.logicprotodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        if genpublic.is_gs_and_controller_server_proto(filename) == True :
            genfile.append([genpublic.logicprotodir + filename, genpublic.controllerlogicservicedir])
            genfile.append([genpublic.logicprotodir + filename, genpublic.gslogicervicedir])
        elif filename.find(genpublic.lobby_file_prefix) >= 0:
            genfile.append([genpublic.logicprotodir + filename, genpublic.lobbylogicservicedir])
            
genpublic.makedirs()
scanprotofile()
main()
