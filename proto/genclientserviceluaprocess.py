import os
from os import system
import md5tool
import shutil
import threading
import _thread
import protofilearray
from multiprocessing import cpu_count

local = threading.local()


local.playerservice = ''
local.service = ''
local.playerservice = ''

threads = []
local.pkg = ''
cpkg = 'package'
yourcodebegin = '---<<< BEGIN WRITING YOUR CODE'
yourcodeend = '---<<< END WRITING YOUR CODE'
rpcbegin = '---<<<rpc begin'
rpcend = '---<<<rpc end'
tabstr = '    '
cpprpcservicepart = 1
servicedir = './md5/'
protodir = 'logic_proto/'
writedir = '../bin/script/client/service/'
client_player = 'client_player'
fileprev = 'c_'
process_fun_name = 'Process(request, response)\n'

if not os.path.exists(servicedir):
    os.makedirs(servicedir)

def parsefile(filename):
    local.rpcarry = []
    local.pkg = ''
    local.playerservice = ''
    local.service = ''
    rpcbegin = 0 
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('rpc') >= 0 and rpcbegin == 1:
                local.rpcarry.append(fileline)
            elif fileline.find(cpkg) >= 0:
                local.pkg = fileline.replace(cpkg, '').replace(';', '').replace(' ', '').strip('\n')
            elif fileline.find('service ') >= 0:
                rpcbegin = 1
                local.service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')
                local.playerservice = local.service

def inputfiledestdir(filename):
    local.pkg = ''
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find(cpkg) >= 0:
                local.pkg = fileline.replace(cpkg, '').replace(';', '').replace(' ', '').strip('\n')
                break
def gencpprpcfunbegin(rpcindex):
    servicestr = ''
    s = local.rpcarry[rpcindex]
    s = s.strip(' ').split(' ')
    servicestr = 'function ' +  s[1] + process_fun_name
    return servicestr

def genyourcodepair():
    return yourcodebegin + '\n' + yourcodeend + '\n'

def gencppfile(filename):
    cppfilename = writedir  + fileprev + filename.replace('.proto', '.lua').replace(protodir, '')
    newcppfilename = servicedir + fileprev + filename.replace('.proto', '.lua').replace(protodir, '')
    if not os.path.exists(newcppfilename) and os.path.exists(cppfilename.replace(protodir, '')):
        shutil.copy(cppfilename.replace(protodir, ''), newcppfilename)
        return
    newstr = ''
    serviceidx = 0
    try:
        with open(cppfilename,'r+', encoding='utf-8') as file:
            part = 0
            yourcode = 1 
            for fileline in file:
                if part != cpprpcservicepart and fileline.find(yourcodebegin) >= 0:
                    yourcode = 1
                    newstr += fileline
                    continue
                elif part != cpprpcservicepart and fileline.find(yourcodeend) >= 0:
                    yourcode = 0
                    newstr += fileline + '\n'
                    part += 1
                    continue     
                elif part == cpprpcservicepart:
                    if fileline.find(rpcbegin) >= 0:
                        newstr += fileline
                        continue
                    elif serviceidx < len(local.rpcarry) and fileline.find(process_fun_name) >= 0 :
                        yourcode = 0
                        newstr += gencpprpcfunbegin(serviceidx)
                        continue
                    elif fileline.find(yourcodebegin) >= 0 :
                        newstr += yourcodebegin + ' '  + '\n'
                        yourcode = 1
                        continue
                    elif fileline.find(yourcodeend) >= 0 :
                        newstr += yourcodeend + ' '  + '\nend\n\n'
                        yourcode = 0
                        serviceidx += 1  
                        continue
                    elif fileline.find(rpcend) >= 0:
                       break
                if yourcode == 1:
                    newstr += fileline
                    continue                
    except FileNotFoundError:
            newstr += genyourcodepair() + '\n'
            newstr += rpcbegin + '\n'
    while serviceidx < len(local.rpcarry) :
        newstr += gencpprpcfunbegin(serviceidx)
        newstr += yourcodebegin +  '\n'
        newstr += yourcodeend + '\nend\n\n'
        serviceidx += 1 
    newstr += rpcend + '\n'
    with open(newcppfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def generate(filename):
    if filename.find(client_player) >= 0:
        parsefile(filename)
        gencppfile(filename)
                
def md5copy(filename):
        if filename.find('md5') >= 0 or filename.find('.lua') < 0:
            return
        gennewfilename = servicedir  + filename
        filenamemd5 = gennewfilename + '.md5'
        error = None
        emptymd5 = False
        destfilename = writedir  + filename
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

def inputfile():
    dir_list  = os.listdir(protodir)
    for filename in dir_list:
        if not (filename[-6:].lower() == '.proto'):
            continue
        if filename.find(client_player) < 0:
            continue
        inputfiledestdir(protodir + filename)
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

inputfile() 
main()
md5copydir()

