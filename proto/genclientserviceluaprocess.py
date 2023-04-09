import os
from os import system
import shutil
import threading
import genpublic

local = threading.local()
threads = []

genfile = []

tabstr = '    '
serviceluamd5dir = './md5/'
protodir = 'logic_proto/'
destdir = '../bin/script/lua/service/'
client_player = 'client_player'
process_fun_name = 'Process(request, response)\n'

def parsefile(filename):
    local.filemethodarray = []
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('rpc') >= 0 :
                local.filemethodarray.append(fileline)
                
def gencpprpcfunbegin(rpcindex):
    s = local.filemethodarray[rpcindex]
    s = s.strip(' ').split(' ')
    servicestr = 'function ' +  s[1] + process_fun_name
    return servicestr

def genyourcodepair():
    return genpublic.luayourcodebegin + '\n' + genpublic.luayourcodeend + '\n'

def genluafile(filename):
    cppfilename = destdir  +  os.path.basename(filename).replace('.proto', '.lua')
    newcppfilename = serviceluamd5dir +  os.path.basename(filename).replace('.proto', '.lua')
    if not os.path.exists(newcppfilename) and os.path.exists(os.path.basename(cppfilename)):
        shutil.copy(os.path.basename(cppfilename), newcppfilename)
        return
    newstr = ''
    serviceidx = 0
    try:
        with open(cppfilename,'r+', encoding='utf-8') as file:
            service_begined = 0
            isyourcode = 0
            for fileline in file:
                if service_begined == 0 and fileline.find(genpublic.luarpcbegin) >= 0:
                    newstr += fileline
                    service_begined = 1
                    continue 
                #开始处理RPC 
                if service_begined == 1:
                    if serviceidx < len(local.filemethodarray) and fileline.find(process_fun_name) >= 0 :
                        isyourcode = 0
                        newstr += gencpprpcfunbegin(serviceidx)
                        continue
                    elif fileline.find(genpublic.luayourcodebegin) >= 0 :
                        newstr += fileline
                        isyourcode = 1
                        continue
                    elif fileline.find(genpublic.luayourcodeend) >= 0 :
                        newstr += genpublic.luayourcodeend + '\nend\n'
                        isyourcode = 0
                        serviceidx += 1  
                        continue
                    elif fileline.find(genpublic.luarpcend) >= 0:
                        break
                if isyourcode == 1 or service_begined == 0:
                    newstr += fileline
                    continue                   
    except FileNotFoundError:
            newstr += genyourcodepair() + '\n'
            newstr += genpublic.luarpcbegin + '\n'
    while serviceidx < len(local.filemethodarray) :
        newstr += gencpprpcfunbegin(serviceidx)
        newstr += genpublic.luayourcodebegin +  '\n'
        newstr += genpublic.luayourcodeend + '\nend\n'
        serviceidx += 1 
    
    newstr += genpublic.luarpcend + '\n'
    with open(newcppfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)
              
def md5copydir():
    cppmd5info = genpublic.md5fileinfo()
    cppmd5info.extensionfitler = ['md5', '.lua']
    cppmd5info.destdir = destdir
    cppmd5info.md5dir = serviceluamd5dir 
    for (dirpath, dirnames, filenames) in os.walk(serviceluamd5dir):
        for filename in filenames:    
            if filename.find('.lua') >= 0:
                cppmd5info.filename = filename
                genpublic.md5copy(cppmd5info)

def scanprotofile():
    dir_list  = os.listdir(protodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        if filename.find(client_player) < 0:
            continue
        genfile.append(protodir  + filename)

class myThread (threading.Thread):
    def __init__(self, filename):
        threading.Thread.__init__(self)
        self.filename = str(filename)
    def run(self):
        parsefile(self.filename)
        genluafile(self.filename)

def main():
    global threads
    for i in range(0, len(genfile)):
        if genfile[i].find(client_player) < 0:
            continue
        t = myThread(genfile[i])
        threads.append(t)
        t.start()
    for t in threads:
        t.join()

scanprotofile() 
main()
md5copydir()

