import os

rpcarry = []
pkg = ''
cpkg = 'package'
service = ''
tabstr = '    '

def parsefile(filename):
    global rpcarry
    global pkg
    global service
    rpcarry = []
    pkg = ''
    service = ''
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('rpc') >= 0:
                rpcarry.append(fileline)
            elif fileline.find(cpkg) >= 0:
                pkg = fileline.replace(cpkg, '').replace(';', '').strip('').strip('\n')
            elif fileline.find('service') >= 0:
                service = fileline.replace('service', '').replace('{', '').replace(' ', '').strip('\n')

def genheadrpcstr():
    servicestr = ""
    for service in rpcarry:
        s = service.strip(' ').split(' ')
        line = tabstr + 'void ' + s[1] + '(::google::protobuf::RpcController* controller,\n'
        line += tabstr + tabstr + 'const ' + pkg + '::' + s[2].replace('(', '').replace(')', '') + '* request,\n'
        rsp = s[4].replace('(', '').replace(')',  '').replace(';',  '').strip('\n');
        if rsp == 'google.protobuf.Empty' :
            line += tabstr + tabstr + '::google::protobuf::Empty* response,\n'
        else :
            line += tabstr + tabstr + pkg + '::' + rsp + '* response,\n'
        line += tabstr + tabstr + '::google::protobuf::Closure* done)override;\n\n'
        servicestr += line
    return servicestr

def genhfile(filename, writedir):
    hfilename = writedir + '/' + filename.replace('.proto', '.h')
    newhfilename = writedir + '/' + filename.replace('.proto', '.h.h')
    headdefine = writedir.replace('/', '_').replace('.', '').upper().strip('_')
    newstr = '#ifndef ' + headdefine + '_H_\n'
    newstr += '#define ' + headdefine + '_H_\n'
    newstr += '#include "' + hfilename + '.pb.h"\n'
    with open(hfilename,'r+', encoding='utf-8') as file:
        part = 0
        owncode = 1 
        generated = 0
        line = 0 
        for fileline in file:
            if line < 3 :
                line += 1
                continue
            if fileline.find('///<<< BEGIN WRITING YOUR CODE') >= 0:
                generated = 0
                owncode = 1
                newstr += fileline
                continue
            elif fileline.find('///<<< END WRITING YOUR CODE') >= 0:
                owncode = 0
                newstr += fileline
                part += 1
                continue
            if owncode == 1 :
                newstr += fileline
                continue
            if owncode == 0  and generated == 0:
                generated = 1
                if part == 1 :
                    newstr += "namespace " + pkg + '{\n'
                elif part == 2:
                    newstr += "class " + service + 'Impl : public ' + service + '{\npublic:\n'
                    newstr += genheadrpcstr()                    

    newstr += "};\n}// namespace " + pkg + '\n'
    newstr += "#endif//" + headdefine + "_H_\n"
    with open(newhfilename,'w', encoding='utf-8')as file:
        file.write(newstr)

def gencppfile(filename, writedir):
    with open(filename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find("rpc") < 0:
                continue
            rpcarry.append(fileline)

def generate(filename, writedir):
    parsefile(filename)
    genhfile(filename, writedir)
    gencppfile(filename, writedir)

generate('gw2l.proto', '../../login_server/src/gateway')