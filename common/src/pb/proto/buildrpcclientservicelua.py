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
cpprpcpart = 1
servicedir = './md5/'
protodir = 'logic_proto/'
writedir = '../../../../bin/script/client/service/'
client_player = 'client_player'
fileprev = 'c_'
process_fun_name = 'Process(request, response)\n'

if not os.path.exists(servicedir):
    os.makedirs(servicedir)

def parsefile(filename):
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

def yourcode():
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
            owncode = 1 
            for fileline in file:
                if part != cpprpcpart and fileline.find(yourcodebegin) >= 0:
                    owncode = 1
                    newstr += fileline
                    continue
                elif part != cpprpcpart and fileline.find(yourcodeend) >= 0:
                    owncode = 0
                    newstr += fileline + '\n'
                    part += 1
                    continue     
                elif part == cpprpcpart:
                    if fileline.find(rpcbegin) >= 0:
                        newstr += fileline
                        continue
                    elif serviceidx < len(local.rpcarry) and fileline.find(process_fun_name) >= 0 :
                        owncode = 0
                        newstr += gencpprpcfunbegin(serviceidx)
                        continue
                    elif fileline.find(yourcodebegin) >= 0 :
                        newstr += yourcodebegin + ' '  + '\n'
                        owncode = 1
                        continue
                    elif fileline.find(yourcodeend) >= 0 :
                        newstr += yourcodeend + ' '  + '\nend\n\n'
                        owncode = 0
                        serviceidx += 1  
                        continue
                    elif fileline.find(rpcend) >= 0:
                       break
                if owncode == 1:
                    newstr += fileline
                    continue                

    except FileNotFoundError:
            newstr += yourcode() + '\n'
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
        fullfilename = writedir  + filename
        if  not os.path.exists(filenamemd5) or not os.path.exists(gennewfilename) or not os.path.exists(fullfilename):
            emptymd5 = True
        else:
            error = md5tool.check_against_md5_file(gennewfilename, filenamemd5)              
        if error == None and os.path.exists(fullfilename) and emptymd5 == False:
            return
        print("copy %s ---> %s" % (gennewfilename, fullfilename))
        shutil.copy(gennewfilename, fullfilename)
        md5tool.generate_md5_file_for(fullfilename, filenamemd5)
def md5copydir():
    for (dirpath, dirnames, filenames) in os.walk(servicedir):
        for filename in filenames:    
            if filename.find('.lua') >= 0:
                md5copy(filename)

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
    step = filelen / cpu_count() + 1
    if cpu_count() > filelen:
        for i in range(0, filelen):
            t = myThread(genfile[i])
            t.start()
            threads.append(t)
    else :
        for i in range(0, cpu_count()):
            for j in range(i, i * step) :
                t = myThread(genfile[j][0], genfile[j][1])
                t.start()
                threads.append(t)
    for t in threads :
        t.join()

inputfile() 
main()
md5copydir()

