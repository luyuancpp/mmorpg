import os
from os import system
import md5tool
import shutil

tabstr = '    '
md5dir = './md5/'
openfilename = 'open_service.cpp'

if not os.path.exists(md5dir):
    os.makedirs(md5dir)

def gen(readfilename, filename):
    destfilename = md5dir + filename
    newstr =  '#include <unordered_set>\n'
    newstr += 'std::unordered_set<uint32_t> g_open_player_msgids{\n'
    with open(md5dir + readfilename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('};') >= 0:
                msgid = fileline.split(',')[1].replace('}', '').replace(';', '')
                newstr += tabstr  + msgid + '};\n'
                break
            elif fileline.find('C2SRequest') >= 0:
                msgid = fileline.split(',')[1].replace('}', '').replace('"', '')
                newstr += tabstr  + msgid + ',\n'
    with open(destfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def md5copy(filename, destdir):
    if not filename.endswith("open_service.cpp"):
        return
    gennewfilename = md5dir + filename
    filenamemd5 = gennewfilename + '.md5'
    error = None
    emptymd5 = False
    if  not os.path.exists(filenamemd5):
        emptymd5 = True
    else:
        error = md5tool.check_against_md5_file(gennewfilename, filenamemd5)              
    destfilename = destdir + '/' + filename
    if error == None and os.path.exists(destfilename) and emptymd5 == False:
        return
    print("copy %s ---> %s" % (gennewfilename, destfilename))
    md5tool.generate_md5_file_for(gennewfilename, filenamemd5)
    shutil.copy(gennewfilename, destfilename)

gen('logic_proto/msgmap.cpp', openfilename)
md5copy(openfilename, '../../../../gateway_server/src/service')