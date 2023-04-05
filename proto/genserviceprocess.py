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
logicprotodir = 'logic_proto/'
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
    
def getprevfilename(filename, destdir):
    if filename.find(logicprotodir) >= 0:
        if destdir == gsservicedir:
            return genpublic.gs_file_prefix
        if destdir == controllerservicedir:
            return genpublic.controller_file_prefix
        if destdir == lobbyservicedir:
            return ''
    return ''

def getpbdir(writedir):
    if writedir.find(logicprotodir) >= 0:
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

def genheadfile(filename,  destdir,  md5dir):
    filename = os.path.basename(filename).replace('.proto', '.h') 
    md5filename = md5dir +   filename
    newstr = '#pragma once\n'
    newstr += '#include "' + getpbdir( destdir) + filename.replace('.h', '') + '.pb.h"\n'
    newstr += genheadrpcfun()
    with open(md5filename, 'w', encoding='utf-8')as file:
        file.write(newstr)
        


class myThread (threading.Thread):
    def __init__(self, filename, destdir, md5dir):
        threading.Thread.__init__(self)
        self.filename = str(filename)
        self.md5dir = str(md5dir)
        self.destdir = str(destdir)
    def run(self):
        checkheadmd5,_,_,_ = genpublic.md5check(self.filename, self.destdir, self.md5dir, '.proto', '.h')    
        checkcppmd5,_,_,_  = genpublic.md5check(self.filename, self.destdir, self.md5dir, '.proto', '.cpp' )    
        if checkheadmd5 == True and checkcppmd5 == True:
            return
        parsefile(self.filename)
        if checkheadmd5 == False:
            genheadfile(self.filename, self.destdir, self.md5dir)
            genpublic.md5copy(self.filename, self.destdir, self.md5dir, '.proto', '.h')
        if checkcppmd5 == False:
            destext = '.cpp'
            filename = os.path.basename(self.filename).replace('.proto', destext) 
            cppfile = genpublic.cpp()
            cppfile.destfilename = self.destdir + filename
            skillinclude = '#include "' + getprevfilename(cppfile.destfilename, self.destdir) + filename.replace(destext, '.h') + '"\n'
            skillinclude += '#include "src/network/rpc_msg_route.h"\n'
            cppfile.md5filename = self.md5dir + filename
            cppfile.includestr = skillinclude
            cppfile.filemethodarray = local.filemethodarray
            cppfile.begunfun = gencpprpcfunbegin
            cppfile.controller = controller
            genpublic.gencppfile(cppfile)
            genpublic.md5copy(self.filename, self.destdir, self.md5dir, '.proto', destext)

def main():
    filelen = len(genfile)
    for i in range(0, filelen):
        t = myThread( genfile[i][0], genfile[i][1], genfile[i][2])
        threads.append(t)
        t.start()
    for t in threads :
        t.join()

def scanprotofile():
    dir_list  = os.listdir(logicprotodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        if genpublic.is_gs_and_controller_server_proto(filename) == True :
            genfile.append([logicprotodir + filename, genpublic.controllerlogicservicedir, genpublic.servermd5dirs[genpublic.conrollermd5dirindex] + logicprotodir ])
            genfile.append([logicprotodir + filename, genpublic.gslogicervicedir, genpublic.servermd5dirs[genpublic.gamemd5dirindex] + logicprotodir])
        elif filename.find(genpublic.lobby_file_prefix) >= 0:
            genfile.append([logicprotodir + filename, genpublic.lobbylogicservicedir, genpublic.servermd5dirs[genpublic.lobbymd5dirindex] + logicprotodir])
            
genpublic.makedirs()
scanprotofile()
main()
