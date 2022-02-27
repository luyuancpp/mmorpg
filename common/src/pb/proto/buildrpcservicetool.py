import os

import md5tool
import shutil

rpcarry = []
servicenames = []
pkg = ''
cpkg = 'package'
yourcodebegin = '///<<< BEGIN WRITING YOUR CODE'
yourcodeend = '///<<< END WRITING YOUR CODE'
rpcbegin = '///<<<rpc begin'
rpcend = '///<<<rpc end'
service = ''
tabstr = '    '
cpprpcpart = 2
cppmaxpart = 4
controller = '(::google::protobuf::RpcController* controller'
hfilename = ''
servicedir = './service/'


if not os.path.exists(servicedir):
    os.makedirs(servicedir)

def parsefile(filename):
    global rpcarry
    global pkg
    global service
    rpcarry = []
    pkg = ''
    service = ''
    rpcbegin = 0 
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('rpc') >= 0 and rpcbegin == 1:
                rpcarry.append(fileline)
            elif fileline.find(cpkg) >= 0:
                pkg = fileline.replace(cpkg, '').replace(';', '').replace(' ', '').strip('\n')
            elif fileline.find('service ') >= 0:
                rpcbegin = 1
                service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')
def genheadrpcfun():
    servicestr = 'public:\n'
    global servicenames
    global controller
    servicenames = []
    for service in rpcarry:
        s = service.strip(' ').split(' ')
        line = tabstr + 'void ' + s[1] + controller + ',\n'
        servicenames.append(s[1])
        line += tabstr + tabstr + 'const ' + pkg + '::' + s[2].replace('(', '').replace(')', '') + '* request,\n'
        rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
        if rsp == 'google.protobuf.Empty' :
            line += tabstr + tabstr + '::google::protobuf::Empty* response,\n'
        else :
            line += tabstr + tabstr + pkg + '::' + rsp + '* response,\n'
        line += tabstr + tabstr + '::google::protobuf::Closure* done)override;\n\n'
        servicestr += line
    return servicestr

def gencpprpcfunbegin(rpcindex):
    global service
    servicestr = ''
    s = rpcarry[rpcindex]
    s = s.strip(' ').split(' ')
    servicestr = 'void ' + service + 'Impl::' + s[1] + controller + ',\n'
    servicestr +=  tabstr + 'const ' + pkg + '::' + s[2].replace('(', '').replace(')', '') + '* request,\n'
    rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
    if rsp == 'google.protobuf.Empty' :
        servicestr +=  tabstr + '::google::protobuf::Empty* response,\n'
    else :
        servicestr +=  tabstr + pkg + '::' + rsp + '* response,\n'
    servicestr +=  tabstr + '::google::protobuf::Closure* done)\n{\n'
    servicestr +=  tabstr + 'AutoRecycleClosure d(done);\n'
    return servicestr

def yourcode():
    return yourcodebegin + '\n' + yourcodeend + '\n'
def namespacebegin():
    return 'namespace ' + pkg + '{\n'
def classbegin():
    return 'class ' + service + 'Impl : public ' + service + '{\npublic:\n'  
def emptyfun():
    return ''

def genheadfile(filename, writedir):
    global hfilename
    headfun = [emptyfun, namespacebegin, classbegin, genheadrpcfun]
    hfullfilename = writedir + '/' + filename.replace('.proto', '.h')
    folder_path, hfilename = os.path.split(hfullfilename)    
    newheadfilename = servicedir + hfilename.replace('.proto', '.h')
    headdefine = writedir.replace('/', '_').replace('.', '').upper().strip('_') + '_' + filename.replace('.proto', '').upper()
    newstr = '#ifndef ' + headdefine + '_H_\n'
    newstr += '#define ' + headdefine + '_H_\n'
    newstr += '#include "' + hfilename.replace('.h', '') + '.pb.h"\n'
    try:
        with open(hfullfilename,'r+', encoding='utf-8') as file:
            part = 0
            owncode = 1 
            skipheadline = 0 
            partend = 0
            for fileline in file:
                if skipheadline < 3 :
                    skipheadline += 1
                    continue
                if fileline.find(yourcodebegin) >= 0:
                    owncode = 1
                    newstr += fileline
                    continue
                elif fileline.find(yourcodeend) >= 0:
                    owncode = 0
                    partend = 1
                    newstr += fileline
                    part += 1
                    continue
                if owncode == 1 :
                    newstr += fileline
                    continue
                if part > 0 and part < len(headfun) and owncode == 0 and partend == 1:
                    newstr += headfun[part]()
                    partend = 0
                    continue
                elif part >= len(headfun):
                    break

    except FileNotFoundError:
        for i in range(0, 4) :
            if i > 0:
                newstr += yourcode()
            newstr += headfun[i]()

    newstr += '};\n}// namespace ' + pkg + '\n'
    newstr += '#endif//' + headdefine + '_H_\n'
    with open(newheadfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def gencppfile(filename, writedir):
    global cppmaxpart
    hfullfilename = writedir + '/' + filename.replace('.proto', '.h')
    cppfilename = writedir + '/' + filename.replace('.proto', '.cpp')
    newcppfilename = servicedir + hfilename.replace('.h', '.cpp')
    newstr = '#include "' + hfilename + '"\n'
    global servicenames
    try:
        with open(cppfilename,'r+', encoding='utf-8') as file:
            part = 0
            owncode = 1 
            skipheadline = 0 
            serviceidx = 0
            curservicename = ''
            nextrpcline = 0
            for fileline in file:
                if skipheadline < 1 :
                    skipheadline += 1
                    continue
                if part != cpprpcpart and fileline.find(yourcodebegin) >= 0:
                    generated = 0
                    owncode = 1
                    newstr += fileline
                    continue
                elif part != cpprpcpart and fileline.find(yourcodeend) >= 0:
                    owncode = 0
                    newstr += fileline + '\n'
                    part += 1
                    if part == 1 :
                        newstr += namespacebegin()
                    continue     
                elif part == cpprpcpart:
                    if fileline.find(rpcbegin) >= 0:
                        newstr += fileline
                        continue
                    elif serviceidx < len(rpcarry) and fileline.find(servicenames[serviceidx] + controller) >= 0 :
                        curservicename = servicenames[serviceidx]
                        owncode = 0
                        newstr += gencpprpcfunbegin(serviceidx)
                        continue
                    elif fileline.find(yourcodebegin) >= 0 :
                        newstr += yourcodebegin + ' ' + curservicename + '\n'
                        owncode = 1
                        continue
                    elif fileline.find(yourcodeend) >= 0 :
                        newstr += yourcodeend + ' ' + curservicename + '\n}\n\n'
                        owncode = 0
                        nextrpcline = 0
                        serviceidx += 1  
                        continue
                    elif fileline.find(rpcend) >= 0:
                        owncode = 0
                        while serviceidx < len(rpcarry) :
                            newstr += gencpprpcfunbegin(serviceidx)
                            newstr += yourcodebegin + ' ' + curservicename + '\n'
                            newstr += yourcodeend + ' ' + curservicename + '\n}\n\n'
                            serviceidx += 1 
                        newstr += fileline
                        part += 1 
                        continue
                if owncode == 1:
                    newstr += fileline
                    continue                
                if part > cppmaxpart :
                    break
    except FileNotFoundError:
            newstr += yourcode() + '\n'
            newstr += namespacebegin()
            newstr += yourcode() + '\n'
            serviceidx = 0
            newstr += rpcbegin + '\n'
            while serviceidx < len(rpcarry) :
                newstr += gencpprpcfunbegin(serviceidx)
                newstr += yourcodebegin + ' ' + servicenames[serviceidx] + '\n'
                newstr += yourcodeend + ' ' + servicenames[serviceidx] + '\n}\n\n'
                serviceidx += 1 
            newstr += rpcend + '\n'
    newstr += '}// namespace ' + pkg + '\n'
    with open(newcppfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)


def md5copy(filename, writedir, fileextend):
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

def generate(filename, writedir):
    parsefile(filename)
    genheadfile(filename, writedir)
    gencppfile(filename, writedir)
    md5copy(filename, writedir, '.h')
    md5copy(filename, writedir, '.cpp')

generate('gw2l.proto', '../../../../login_server/src/service')
generate('l2db.proto', '../../../../database_server/src/service')
generate('ms2g.proto', '../../../../game_server/src/service')
generate('rg2g.proto', '../../../../game_server/src/service')
generate('node2deploy.proto', '../../../../deploy_server/src/service')
generate('ms2gw.proto', '../../../../gateway_server/src/service')
generate('gw2ms.proto', '../../../../master_server/src/service')
generate('l2ms.proto', '../../../../master_server/src/service')
generate('g2ms.proto', '../../../../master_server/src/service')