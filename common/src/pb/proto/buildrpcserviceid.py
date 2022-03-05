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

threads = []
local.pkg = ''
tabstr = '    '
cpkg = 'package'
servicedir = './service/'
writedir = '../pbc/'
perserviceidcount = 200

if not os.path.exists(servicedir):
    os.makedirs(servicedir)

def parsefile(filename, fileid):
    local.rpcarry = []
    local.pkg = ''
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

def genheadrpcfun(fileid):
    servicestr = '\n'
    local.servicenames = []
    serviceid = fileid * perserviceidcount
    for service in local.rpcarry:
        s = service.strip(' ').split(' ')
        line = 'const uint32_t ' + local.pkg.upper() + '_'
        line +=  '_' + s[1].upper()  + '_ID = ' + str(serviceid)
        line += ';\n'
        serviceid += 1
        servicestr += line
    #print(servicestr)
    return servicestr

def genheadfile(filename, fileid):
    hfullfilename = writedir + '/' + filename.replace('.proto', 'id.h')
    folder_path, local.hfilename = os.path.split(hfullfilename)    
    newheadfilename = servicedir + local.hfilename.replace('.proto', 'id.h')
    headdefine = writedir.replace('/', '_').replace('.', '').upper().strip('_') + '_' + filename.replace('.proto', '').upper() + 'SERVICE_ID'
    newstr = '#ifndef ' + headdefine + '_H_\n'
    newstr += '#define ' + headdefine + '_H_\n'
    newstr += genheadrpcfun(fileid) + '\n'
    newstr += '#endif//' + headdefine + '_H_\n'
    with open(newheadfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def md5copy(filename, fileextend):
        gennewfilename = servicedir + filename.replace('.proto', fileextend)
        filenamemd5 = gennewfilename + '.md5'
        error = None
        emptymd5 = False
        if  not os.path.exists(filenamemd5):
            emptymd5 = True
        else:
            error = md5tool.check_against_md5_file(gennewfilename, filenamemd5)              
        hfullfilename = writedir + '/' + filename.replace('.proto', fileextend)
        if error == None and os.path.exists(hfullfilename) and emptymd5 == False:
            return
        #print("copy %s ---> %s" % (gennewfilename, hfullfilename))
        md5tool.generate_md5_file_for(gennewfilename, filenamemd5)
        shutil.copy(gennewfilename, hfullfilename)

def generate(filename, fileid):
    parsefile(filename, fileid)
    genheadfile(filename, fileid)
    md5copy(filename, 'id.h')

genfile = protofilearray.genfile

class myThread (threading.Thread):
    def __init__(self, filename,  fileid):
        threading.Thread.__init__(self)
        self.filename = str(filename)
        self.fileid = fileid
    def run(self):
        generate(self.filename, self.fileid)

def main():
    filelen = len(genfile)
    global threads
    step = filelen / cpu_count() + 1
    if cpu_count() > filelen:
        for i in range(0, filelen):
            t = myThread( genfile[i][0],  i )
            t.start()
            threads.append(t)
    else :
        for i in range(0, cpu_count()):
            for j in range(i, i * step) :
                t = myThread(genfile[j][0],  j)
                t.start()
                threads.append(t)
    for t in threads :
        t.join()
main()
