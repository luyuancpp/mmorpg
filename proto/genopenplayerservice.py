import os
import genpublic

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
                istr = fileline.split('[')[1]
                msgid = istr.split(']')[0]
                newstr += msgid + ',\n'
    newstr = newstr.strip('\n').strip(',')
    newstr += '\n};\n'
    with open(destfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)

gen('logic_proto/serviceid/service_method_id.cpp', openfilename)

cppmd5info = genpublic.md5fileinfo()
cppmd5info.destdir = '../gate_server/src/service/'
cppmd5info.md5dir = md5dir 
cppmd5info.filename = openfilename
genpublic.md5copy(cppmd5info)