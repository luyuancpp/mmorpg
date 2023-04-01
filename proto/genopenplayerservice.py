import os
from os import system
import md5tool
import shutil

md5dir = './md5/'
openfilename = 'open_service.cpp'

if not os.path.exists(md5dir):
    os.makedirs(md5dir)

def gen(readfilename, filename):
    destfilename = md5dir + filename
    newstr =  '#include <unordered_set>\n\n'
    declaration = False
   
    with open(md5dir + readfilename,'r', encoding='utf-8') as file:
        for fileline in file:
            if fileline.find('#include') >= 0:
                newstr += fileline
            elif fileline.find('C2SRequest') >= 0:
                if declaration == False:
                    newstr += '\nstd::unordered_set<uint32_t> g_player_service_method_id{\n'
                    declaration = True
                str = fileline.split('[')[1]
                msgid = str.split(']')[0]
                newstr += msgid + ',\n'
    newstr = newstr.strip('\n').strip(',')
    newstr += '\n};\n'
    with open(destfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

def md5copy(filename, destdir):
    if not filename.endswith("open_service.cpp"):
        return
    srcmd5filename = md5dir + filename
    md5suffixfilename = srcmd5filename + '.md5'
    error = None
    emptymd5 = False
    if  not os.path.exists(md5suffixfilename):
        emptymd5 = True
    else:
        error = md5tool.check_against_md5_file(srcmd5filename, md5suffixfilename)              
    destfilename = destdir + '/' + filename
    if error == None and os.path.exists(destfilename) and emptymd5 == False:
        return
    print("copy %s ---> %s" % (srcmd5filename, destfilename))
    md5tool.generate_md5_file_for(srcmd5filename, md5suffixfilename)
    shutil.copy(srcmd5filename, destfilename)

gen('logic_proto/serviceid/service_method_id.cpp', openfilename)
md5copy(openfilename, '../gate_server/src/service')