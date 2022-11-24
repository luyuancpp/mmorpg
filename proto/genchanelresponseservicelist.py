
import os
from os import system
import md5tool
import shutil
import threading
import genpublic

local = threading.local()
local.pkg = ''
local.service = []
local.servicewithpkg = []
local.servicefile = []
local.fileincludedir = []

def parsefile(filename, includedir):
    local.pkg = ''
    with open(filename, 'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find(genpublic.cpkg) >= 0:
                local.pkg = fileline.replace(genpublic.cpkg, '').replace(';', '').replace(' ', '').strip('\n')
            elif fileline.find('service ') >= 0:
                s = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')
                local.service.append(s)
                local.servicewithpkg.append(local.pkg + '::' + s)
                local.servicefile.append(filename)
                local.fileincludedir.append(includedir)
                return


def gen(filename, destpath, md5dir):
    destfilename = destpath + filename
    newheadfilename = md5dir + filename
    newstr = ''
    newstr += '#include "src/network/rpc_prototype_service.h"\n'
    
    for i in range(0, len(local.servicefile)):
        newstr += '#include "' + local.fileincludedir[i]  + local.servicefile[i].replace('.proto', '.pb.h\n')            
    newstr += 'std::unordered_map<std::string, std::unique_ptr<Service>> g_prototype_services;\n'
   
    for i in range(0, len(local.service)):
        newstr += 'class ' + local.service[i] + 'MethodServiceImpl : public '  + local.servicewithpkg[i] + '{};\n'
    newstr += 'void InitFakeProtoServiceList()\n{\n'
    for i in range(0, len(local.service)):
        newstr += genpublic.tabstr + 'g_prototype_services.emplace("' + local.service[i]  + '"'
        newstr += ', std::make_unique<' + local.service[i]  + 'MethodServiceImpl>();\n'
    newstr += '}\n'
    with open(newheadfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)
        
def inputfile():
    dir_list  = dir_list = os.listdir(genpublic.logicprotodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        parsefile(genpublic.logicprotodir + filename, 'src/pb/pbc/common_proto/')
    dir_list  = dir_list = os.listdir(genpublic.commonportodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        parsefile(genpublic.commonportodir + filename, 'src/pb/pbc/logic_proto/')
        
genpublic.makedirs()

inputfile()

gen('rpc_prototype_service.cpp', genpublic.pbcserviceinstancedir, genpublic.pbcserviceinstancemd5dir)
