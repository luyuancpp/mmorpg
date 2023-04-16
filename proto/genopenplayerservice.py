import genpublic

srcdir = '../gate_server/src/service/'
openfilename = 'open_service.cpp'

def gen(readfilename, filename):
    newstr =  '#include <unordered_set>\n\n'
    declaration = False
    with open(genpublic.md5dir + readfilename,'r', encoding='utf-8') as file:
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
    with open(genpublic.getmd5filename(srcdir + filename), 'w', encoding='utf-8')as file:
        file.write(newstr)

gen('logic_proto/serviceid/service_method_id.cpp', openfilename)

cppmd5info = genpublic.md5fileinfo()
cppmd5info.destdir = srcdir
cppmd5info.filename = openfilename
genpublic.md5copy(cppmd5info)