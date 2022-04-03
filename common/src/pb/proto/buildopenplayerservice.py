import os
from os import system
import md5tool
import shutil
import threading
import _thread
import protofilearray
from multiprocessing import cpu_count

local = threading.local()

local.rpcarry = []
local.servicenames = []
local.playerservice = ''
local.service = ''
local.hfilename = ''
local.playerservice = ''
local.playerservicearray = []
local.fileservice = []

threads = []
local.pkg = ''
cpkg = 'package'
yourcodebegin = '///<<< BEGIN WRITING YOUR CODE'
yourcodeend = '///<<< END WRITING YOUR CODE'
rpcbegin = '///<<<rpc begin'
rpcend = '///<<<rpc end'
tabstr = '    '
cpprpcpart = 2
cppmaxpart = 4
controller = '(common::EntityPtr& entity'
servicedir = './md5/'
protodir = 'logic_proto/'
playerservicedir = '../../../../gateway_server/src/service'

if not os.path.exists(servicedir):
    os.makedirs(servicedir)

def parseplayerservcie(filename):
    if filename.find('client_player') < 0:
        return
    local.fileservice.append(filename.replace('.proto', ''))
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find(cpkg) >= 0:
                local.pkg = fileline.replace(cpkg, '').replace(';', '').replace(' ', '').strip('\n')
            elif fileline.find('service ') >= 0:
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')
                local.playerservicearray.append(local.pkg + '.' + local.service)
                
def genplayerservcielist(filename):
    fullfilename = servicedir + filename
    newstr =  '#include <unordered_set>\n'
    for f in local.fileservice:
        newstr += '#include "' + f + '.pb.h"\n'
    newstr += 'namespace gateway\n{\n'
    newstr += 'std::unordered_set<std::string> g_open_player_services;\n'
    
    newstr += 'void OpenPlayerServcie()\n{\n'
    for service in local.playerservicearray:
        if service.find('clientplayer') >= 0:
            newstr += tabstr + 'g_open_player_services.emplace("' + service + '");\n'
    newstr += '}\n}//namespace gateway\n'
    with open(fullfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def md5copy(filename, writedir):
        if not filename.endswith("open_service.cpp"):
            return
        gennewfilename = servicedir + filename
        filenamemd5 = gennewfilename + '.md5'
        error = None
        emptymd5 = False
        if  not os.path.exists(filenamemd5):
            emptymd5 = True
        else:
            error = md5tool.check_against_md5_file(gennewfilename, filenamemd5)              
        fullfilename = writedir + '/' + filename
        if error == None and os.path.exists(fullfilename) and emptymd5 == False:
            return
        print("copy %s ---> %s" % (gennewfilename, fullfilename))
        md5tool.generate_md5_file_for(gennewfilename, filenamemd5)
        shutil.copy(gennewfilename, fullfilename)
def md5copydir():
    for (dirpath, dirnames, filenames) in os.walk(servicedir):
        for filename in filenames:        
            md5copy(filename, '../../../../gateway_server/src/service')

genfile = []

def get_file_list(file_path):
    dir_list = os.listdir(file_path)
    if not dir_list:
        return
    else:
        dir_list = sorted(dir_list,key=lambda x: os.path.getmtime(os.path.join(file_path, x)))
        return dir_list

def inputfile():
    dir_list  = get_file_list(protodir)
    for each_filename in dir_list:
        genfile.append([protodir  + each_filename, playerservicedir])
def main():
    for file in genfile:
        parseplayerservcie(file[0])
    genplayerservcielist('open_service.cpp')
inputfile() 
main()
md5copydir()
