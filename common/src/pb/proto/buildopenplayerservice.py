import os
from os import system
import md5tool
import shutil

tabstr = '    '
servicedir = './md5/'
playerservicedir = '../../../../gateway_server/src/service'
openfilename = 'open_service.cpp'

if not os.path.exists(servicedir):
    os.makedirs(servicedir)

def gen(readfilename, filename):
    fullfilename = servicedir + filename
    newstr =  '#include <unordered_set>\n'
    newstr += 'std::unordered_set<uint32_t> g_open_player_msgids{\n'
    with open(servicedir + readfilename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('};') >= 0:
                msgid = fileline.split(',')[1].replace('}', '').replace(';', '')
                newstr += tabstr  + msgid + '};\n'
                break
            elif fileline.find('C2SRequest') >= 0:
                msgid = fileline.split(',')[1].replace('}', '').replace('"', '')
                newstr += tabstr  + msgid + ',\n'
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

gen('logic_proto/msgmap.cpp', openfilename)
md5copy(openfilename, '../../../../gateway_server/src/service')