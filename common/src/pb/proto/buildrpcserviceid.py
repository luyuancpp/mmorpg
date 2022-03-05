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
local.hfilename = ''
local.rpcmsgnameid = []
local.msgcount = 0
local.cppfilename = 'msgmap.cpp'

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
                pbfullname = local.pkg + '.' + s[2].replace('(', '').replace(')', '')
                local.rpcmsgnameid.append([[s[1], rq, local.pkg, local.service],serviceid])
                serviceid += 1
            elif fileline.find(cpkg) >= 0:
                local.pkg = fileline.replace(cpkg, '').replace(';', '').replace(' ', '').strip('\n')
            elif fileline.find('service ') >= 0:
                rpcbegin = 1
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')

def genmsgidcpp(fullfilename):
    newstr = '#include <array>\n'
    newstr += '#include <string>\n'
    newstr += '#include <unordered_map>\n\n'
    newstr += 'std::unordered_map<std::string, uint32_t> g_msgid{\n'
    #msg 2 id
    for kv in local.rpcmsgnameid:
        newstr += '{"' + kv[0][2] + '.' + kv[0][0] + '", ' + str(kv[1]) + '},\n'
    newstr = newstr.strip('\n').strip(',')
    newstr += '};\n'
    newstr += '\nstd::array<std::string, ' + str(local.msgcount) + '> g_idservice;\n'
    newstr += 'void InitMsgId2Servcie()\n{\n'
    #id 2 service 
    for kv in local.rpcmsgnameid:
        newstr += tabstr + 'g_idservice[' + str(kv[1]) + '] = "' +  kv[0][2] + '.' + kv[0][3] +'";\n'
    newstr += '}\n'
    #id 2 service 
    newstr += '\nstd::array<std::string, ' + str(local.msgcount) + '> g_idmsg;\n'
    newstr += 'void InitMsgId2Msg()\n{\n'
    for kv in local.rpcmsgnameid:
        newstr += tabstr + 'g_idmsg[' + str(kv[1]) + '] = "' + kv[0][1] +'";\n'
    newstr += '}\n'
    with open(fullfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def md5copy(hfullfilename):
        gennewfilename = servicedir + local.cppfilename
        filenamemd5 = gennewfilename + '.md5'
        error = None
        copy = False
        if  not os.path.exists(filenamemd5) or not os.path.exists(hfullfilename):
            copy = True
        else:
            error = md5tool.check_against_md5_file(hfullfilename, filenamemd5)              
        
        if error == None and copy == False:
            return
        #print("copy %s ---> %s" % (gennewfilename, hfullfilename))
        md5tool.generate_md5_file_for(gennewfilename, filenamemd5)
        shutil.copy(gennewfilename, hfullfilename)

def generate(filename, fileid):
    parsefile(filename, fileid)

genfile = ['c2gs.proto']

def main():
    filelen = len(genfile)
    local.msgcount = filelen * perserviceidcount
    for i in range(0, filelen):
           generate(genfile[i], i)
main()
genfilename = servicedir + local.cppfilename
fullfilename = writedir + local.cppfilename
genmsgidcpp(genfilename)
md5copy(fullfilename)