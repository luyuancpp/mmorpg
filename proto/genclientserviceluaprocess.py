import os
from os import system
import md5tool
import shutil
import threading
import genpublic
from multiprocessing import cpu_count

local = threading.local()
threads = []

tabstr = '    '
servicedir = './md5/'
protodir = 'logic_proto/'
destdir = '../bin/script/lua/service/'
client_player = 'client_player'
process_fun_name = 'Process(request, response)\n'

if not os.path.exists(servicedir):
    os.makedirs(servicedir)

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
    newcppfilename = servicedir +  os.path.basename(filename).replace('.proto', '.lua')
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

def generate(filename):
    if filename.find(client_player) < 0:
        return
    parsefile(filename)
    genluafile(filename)
                
def md5copy(filename):
        if filename.find('md5') >= 0 or filename.find('.lua') < 0:
            return
        gennewfilename = servicedir  + filename
        filenamemd5 = gennewfilename + '.md5'
        error = None
        emptymd5 = False
        destfilename = destdir  + filename
        if  not os.path.exists(filenamemd5) or not os.path.exists(gennewfilename) or not os.path.exists(destfilename):
            emptymd5 = True
        else:
            error = md5tool.check_against_md5_file(gennewfilename, filenamemd5)              
        if error == None and os.path.exists(destfilename) and emptymd5 == False:
            return
        print("copy %s ---> %s" % (gennewfilename, destfilename))
        shutil.copy(gennewfilename, destfilename)
        md5tool.generate_md5_file_for(destfilename, filenamemd5)
        
def md5copydir():
    for (dirpath, dirnames, filenames) in os.walk(servicedir):
        for filename in filenames:    
            if filename.find('.lua') >= 0:
                md5copy(filename)

genfile = []

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
        generate(self.filename)

def main():
    filelen = len(genfile)
    global threads
    step = int(filelen / cpu_count() + 1)
    if cpu_count() > filelen:
        for i in range(0, filelen):
            t = myThread(genfile[i])
            threads.append(t)
            t.start()
    else :
        for i in range(0, cpu_count()):
            for j in range(i, i * step) :
                t = myThread(genfile[j][0], genfile[j][1])
                threads.append(t)
                t.start()
    for t in threads :
        t.join()

scanprotofile() 
main()
md5copydir()

