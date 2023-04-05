
import os
from os import system
import md5tool
import shutil
import threading
import genpublic

local = threading.local()
local.service = []
local.servicefile = []
local.fileincludedir = []

def parsefile(filename, includedir):
    with open(filename, 'r', encoding='utf-8') as file:
        for fileline in file:
            if genpublic.is_service_fileline(fileline) == True:
                s = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')
                local.service.append(s)
                local.servicefile.append(filename)
                local.fileincludedir.append(includedir)

def gen(filename,  md5dir):
    newheadfilename = md5dir + filename
    newstr = '#include <unordered_map>\n\n'
    for i in range(0, len(local.servicefile)):
        pbcfile = local.servicefile[i].replace(genpublic.logicprotodir, '').replace(genpublic.commonportodir, '').replace('.proto', '.pb.h"\n') 
        newstr += '#include "' + local.fileincludedir[i] + pbcfile        
    newstr += '\nstd::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_services;\n\n'
   
    for i in range(0, len(local.service)):
        newstr += 'class ' + local.service[i] + 'MethodServiceImpl : public '  + local.service[i] + '{};\n'
    newstr += '\nvoid InitFakeProtoServiceList()\n{\n'
    for i in range(0, len(local.service)):
        newstr += genpublic.tabstr + 'g_services.emplace("' + local.service[i]  + '"'
        newstr += ', std::make_unique<' + local.service[i]  + 'MethodServiceImpl>());\n'
    newstr += '}\n'
    with open(newheadfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)
        
def scanprotofile():
    dir_list  =  os.listdir(genpublic.logicprotodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        parsefile(genpublic.logicprotodir + filename, 'src/pb/pbc/logic_proto/')
    dir_list  =  os.listdir(genpublic.commonportodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        parsefile(genpublic.commonportodir + filename, 'src/pb/pbc/common_proto/')
        

def md5copy(filename, destdir, md5dir):
        if filename.find('/') >= 0 :
            s = filename.split('/')
            filename = s[len(s) - 1]
        gennewfilename = md5dir + filename
        filenamemd5 = gennewfilename + '.md5'
        error = None
        emptymd5 = False
        if  not os.path.exists(filenamemd5):
            emptymd5 = True
        else:
            error = md5tool.check_against_md5_file(gennewfilename, filenamemd5)           
        destfilename =  destdir + filename
        if error == None and os.path.exists(destfilename) and emptymd5 == False:
            return
        
        print("copy %s ---> %s" % (gennewfilename, destfilename))
        md5tool.generate_md5_file_for(gennewfilename, filenamemd5)
        shutil.copy(gennewfilename, destfilename)
    
genpublic.makedirs()

scanprotofile()

gen('rpc_prototype_service.cpp', genpublic.pbcserviceinstancemd5dir)
md5copy('rpc_prototype_service.cpp', genpublic.pbcserviceinstancedir, genpublic.pbcserviceinstancemd5dir)
