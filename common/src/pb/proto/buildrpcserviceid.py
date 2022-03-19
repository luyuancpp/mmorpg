import os

import md5tool
import shutil
import threading
import _thread
import protofilearray
from multiprocessing import cpu_count

local = threading.local()

local.rpcarry = []
local.servicenames = []
local.servicebeginid = []
local.service = ''
local.rpcmsgnameid = []
local.msgcount = 0
local.cppfilename = 'msgmap.cpp'
local.hfilename = 'msgmap.h'

threads = []
local.pkg = ''
tabstr = '    '
cpkg = 'package'
servicedir = './service/'
writedir = '../pbc/'
perserviceidcount = 150

if not os.path.exists(servicedir):
    os.makedirs(servicedir)

def parsefile(filename, fileid):
    local.rpcarry = []
    local.pkg = ''
    local.service = ''
    rpcbegin = 0 
    serviceid = fileid * perserviceidcount
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('rpc') >= 0 and rpcbegin == 1:
                s = fileline.strip(' ').split(' ')
                idstr = local.pkg.upper() + '_' + s[1].upper()  + '_ID'
                line = 'const uint32_t ' + idstr + ' = ' + str(serviceid)
                line += ';\n'
                rq = s[2].replace('(', '').replace(')', '')
                rp = s[4].replace('(', '').replace(')', '').replace(';', '').replace('\n', '')
                pbfullname = local.pkg + '.' + s[2].replace('(', '').replace(')', '')
                local.rpcmsgnameid.append([[s[1], rq, rp, local.pkg, local.service],serviceid])
                serviceid += 1
            elif fileline.find(cpkg) >= 0:
                local.pkg = fileline.replace(cpkg, '').replace(';', '').replace(' ', '').strip('\n')
            elif fileline.find('service ') >= 0:
                rpcbegin = 1
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')

def genmsgidcpp(fullfilename):
    newstr = '#include "msgmap.h"\n'
    newstr += 'std::unordered_map<std::string, uint32_t> g_msgid{\n'
    #msg 2 id
    for kv in local.rpcmsgnameid:
        newstr += '{"' + kv[0][3] + '.' + kv[0][0] + '", ' + str(kv[1]) + '},\n'
    newstr = newstr.strip('\n').strip(',')
    newstr += '};\n'
    newstr += '\nstd::array<RpcService, ' + str(local.msgcount) + '> g_idservice;\n'
    newstr += 'void InitMsgService()\n{\n'
    for kv in local.rpcmsgnameid:
        curpkg = kv[0][3]
        newstr += tabstr + 'g_idservice[' + str(kv[1]) + '].service = "' + curpkg + '.' + kv[0][4] +'";\n'
        newstr += tabstr + 'g_idservice[' + str(kv[1]) + '].method = "'  + kv[0][0] +'";\n'
        newstr += tabstr + 'g_idservice[' + str(kv[1]) + '].request = "' + curpkg + '.' + kv[0][1] +'";\n'
        newstr += tabstr + 'g_idservice[' + str(kv[1]) + '].response = "' + curpkg + '.' + kv[0][2] +'";\n\n'
    newstr += '}\n'
    with open(fullfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def genmsgidhead(fullfilename):
    HEAD_FILE = 'COMMON_SRC_PB_PBC_' + local.cppfilename.replace('.', '_').upper()
    newstr = '#ifndef  ' + HEAD_FILE + '\n'
    newstr += '#define  ' + HEAD_FILE + '\n'
    newstr += '#include <array>\n'
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
    newstr += 'extern std::array<RpcService, ' + str(local.msgcount) + '> g_idservice;\n'
    newstr += 'void InitMsgService();\n'
    newstr += '#endif//  ' + HEAD_FILE + '\n'
    with open(fullfilename, 'w', encoding='utf-8')as file:
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
    print(filenamemd5)
    if error == None and copy == False:
        return
    #print("copy %s ---> %s" % (gennewfilename, destfilename))
    md5tool.generate_md5_file_for(gennewfilename, filenamemd5)
    shutil.copy(gennewfilename, destfilename)

def generate(filename, fileid):
    parsefile(filename, fileid)

genfile = ['c2gs.proto']

def main():
    filelen = len(genfile)
    local.msgcount = filelen * perserviceidcount
    for i in range(0, filelen):
           generate(genfile[i], i)
main()
cppsrcfilename = servicedir + local.cppfilename
cppdestfilename = writedir + local.cppfilename
hsrcfilename = servicedir + local.hfilename
hdestfilename = writedir + local.hfilename

genmsgidcpp(cppsrcfilename)
genmsgidhead(hsrcfilename)
md5copy(cppdestfilename, local.cppfilename)
md5copy(hdestfilename, local.hfilename)