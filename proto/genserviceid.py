import os

import md5tool
import shutil
import threading
import _thread
import genpublic
from multiprocessing import cpu_count

local = threading.local()

local.service = ''
local.rpcservicemethod = []
local.cppfilename = 'msgmap.cpp'
local.hfilename = 'msgmap.h'

threads = []
tabstr = '    '
servicedir = './md5/logic_proto/'
writedir = '../common/src/pb/pbc/'
protodir = './logic_proto/'
msg_index = 0

clientmsgdict = set()

if not os.path.exists(servicedir):
    os.makedirs(servicedir)

def parsefile(filename):
    local.service = ''
    rpcbegin = 0 
    local.msgdict = dict()
    global clientmsgdict
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('rpc') >= 0 and rpcbegin == 1:
                rpc = fileline.strip(' ').split(' ')
                rq = rpc[2].replace('(', '').replace(')', '')
                rp = rpc[4].replace('(', '').replace(')', '').replace(';', '').replace('\n', '')
                method = rpc[1]
                service_method_id = local.service + '_Id_' + method 
                local.rpcservicemethod.append([local.service, method, rq, rp, service_method_id])
                if service_method_id in method:
                    print('error : service method repeated', local.service, method,  ' filename:', filename)
                    continue
                clientmsgdict.add(service_method_id)
            elif genpublic.is_service_fileline(fileline) == True:
                rpcbegin = 1
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')

def genmsgidcpp(filename):
    global msg_index
    newstr = '#include "msgmap.h"\n'
    for kv in local.rpcservicemethod:
        service_method_id = kv[4]
        newstr += 'const uint32_t ' + service_method_id + ' =  ' + str(msg_index) + ';\n'
        msg_index += 1

    newstr += '\nstd::unordered_map<uint32_t, RpcService> g_serviceinfo;\n'
    newstr += 'void InitMsgService()\n{\n'
    for kv in local.rpcservicemethod:
        service_method_id = kv[4]
        newstr += tabstr + 'g_serviceinfo[' + service_method_id + '].service = "' + kv[0] +'";\n'
        newstr += tabstr + 'g_serviceinfo[' + service_method_id + '].method = "'  + kv[1] +'";\n'
        newstr += tabstr + 'g_serviceinfo[' + service_method_id + '].request = "' + kv[2] +'";\n'
        newstr += tabstr + 'g_serviceinfo[' + service_method_id + '].response = "' + kv[3] +'";\n\n'
    newstr += '}\n'
    with open(filename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def genmsgidhead(filename):
    newstr = '#pragma once\n'
    newstr += '#include <string>\n'
    newstr += '#include <unordered_map>\n\n'
    newstr += 'struct RpcService\n'
    newstr += '{\n'
    newstr +=  tabstr + 'const char* service{nullptr};\n'
    newstr +=  tabstr + 'const char* method{nullptr};\n'
    newstr +=  tabstr + 'const char* request{nullptr};\n'
    newstr +=  tabstr + 'const char* response{nullptr};\n'
    newstr += '};\n'
    newstr += 'extern std::unordered_map<std::string, uint32_t> g_msgid;\n'
    newstr += 'extern std::unordered_map<uint32_t, RpcService> g_serviceinfo;\n'
    newstr += 'void InitMsgService();\n'
    with open(filename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def md5copy(destfilename, filename):    
    gennewfilename = servicedir + filename
    filenamemd5 = gennewfilename + '.md5'
    error = None
    copy = False
    if  not os.path.exists(filenamemd5) or not os.path.exists(destfilename):
        copy = True
    else:
        error = md5tool.check_against_md5_file(gennewfilename, filenamemd5)              
    if error == None and copy == False:
        return
    md5tool.generate_md5_file_for(gennewfilename, filenamemd5)
    shutil.copy(gennewfilename, destfilename)

genfile = ['common_proto/game_service.proto', 
'common_proto/login_service.proto']

def inputfile():
    for filename in os.listdir(protodir):
        if not (filename[-6:].lower() == '.proto'):
            continue
        genfile.append(protodir + filename)

def main():
    for file in genfile:
        parsefile(file)
        
inputfile()
main()
cppsrcfilename = servicedir + local.cppfilename
cppdestfilename = writedir + local.cppfilename
hsrcfilename = servicedir + local.hfilename
hdestfilename = writedir + local.hfilename

genmsgidcpp(cppsrcfilename)
genmsgidhead(hsrcfilename)
md5copy(cppdestfilename, local.cppfilename)
md5copy(hdestfilename, local.hfilename)