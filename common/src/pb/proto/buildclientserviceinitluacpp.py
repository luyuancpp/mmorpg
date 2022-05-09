import os
from os import system
import md5tool
import shutil

luadir = '../../../../bin/script/client/service/'
clientservicedir = '../../../../client/src/service/logic/'
includedir = 'script/client/service/'
md5dir = './md5/'
cppfilename = 'service_lua.cpp'
writedfilename = md5dir + cppfilename
tabstr = '	'
genfile = []

def get_file_list(file_path):
    dir_list = os.listdir(file_path)
    if not dir_list:
        return
    else:
        dir_list = sorted(dir_list,key=lambda x: os.path.getmtime(os.path.join(file_path, x)))
        return dir_list

def inputfile():
    dir_list  = get_file_list(luadir)
    for filename in dir_list:
        if not (filename[-4:].lower() == '.lua'):
            continue
        genfile.append(includedir  + filename)

def gencpp():
	newstr =  '#include <sol/sol.hpp>\n'
	newstr += '#include "muduo/base/Logging.h"\n'
	newstr += '#include "src/util/file2string.h"\n'
	newstr += 'extern thread_local sol::state g_lua;\n'	
	newstr += 'void InitServiceLua()\n{\n'
	newstr += tabstr + 'std::string contents;\n'
	for filename in genfile:
		newstr += tabstr + 'contents = common::File2String("' + filename + '");\n'
		newstr += tabstr + '{\n'
		newstr += tabstr + tabstr + 'auto r = g_lua.script(contents);\n'
		newstr += tabstr + tabstr + 'if (!r.valid())\n'
		newstr += tabstr + tabstr + '{\n'
		newstr += tabstr + tabstr + tabstr + 'sol::error err = r;\n'
		newstr += tabstr + tabstr + tabstr + 'LOG_FATAL << err.what();\n'
		newstr += tabstr + tabstr +	'}\n'
		newstr += tabstr + '}\n'
	newstr += '\n}\n'
	with open(writedfilename, 'w', encoding='utf-8')as file:
		file.write(newstr)

def md5copy(fullfilename):
    filenamemd5 = writedfilename + '.md5'
    error = None
    emptymd5 = False
    if  not os.path.exists(filenamemd5):
        emptymd5 = True
    else:
        error = md5tool.check_against_md5_file(writedfilename, filenamemd5) 
    if error == None and os.path.exists(fullfilename) and emptymd5 == False:
        return
    print("copy %s ---> %s" % (writedfilename, fullfilename))
    md5tool.generate_md5_file_for(writedfilename, filenamemd5)
    shutil.copy(writedfilename, fullfilename)

inputfile()
gencpp()
md5copy(clientservicedir + cppfilename)
