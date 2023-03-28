import os
from os import system
import md5tool
import shutil

luadir = '../bin/script/lua/service/'
clientservicedir = '../client/src/service/logic_proto/'
includedir = 'script/lua/service/'
md5dir = './md5/'
cppfilename = 'service_lua.cpp'
md5dirfilename = md5dir + cppfilename
tabstr = '	'
genfile = []

def scanprotofile():
    dir_list  = os.listdir(luadir)
    for filename in dir_list:
        if not (filename[-4:].lower() == '.lua'):
            continue
        genfile.append(includedir  + filename)

def gencpp():
	newstr =  '#include <sol/sol.hpp>\n'
	newstr += '#include "muduo/base/Logging.h"\n'
	newstr += '#include "src/util/file2string.h"\n'
	newstr += '#include "src/game_logic/thread_local/thread_local_storage_lua.h"\n'	
	newstr += 'void InitServiceLua()\n{\n'
	newstr += tabstr + 'std::string contents;\n'
	for filename in genfile:
		newstr += tabstr + 'contents = common::File2String("' + filename + '");\n'
		newstr += tabstr + '{\n'
		newstr += tabstr + tabstr + 'auto r = tls_lua_state.script(contents);\n'
		newstr += tabstr + tabstr + 'if (!r.valid())\n'
		newstr += tabstr + tabstr + '{\n'
		newstr += tabstr + tabstr + tabstr + 'sol::error err = r;\n'
		newstr += tabstr + tabstr + tabstr + 'LOG_FATAL << err.what();\n'
		newstr += tabstr + tabstr +	'}\n'
		newstr += tabstr + '}\n'
	newstr += '\n}\n'
	with open(md5dirfilename, 'w', encoding='utf-8')as file:
		file.write(newstr)

def md5copy(destfilename):
    filenamemd5 = md5dirfilename + '.md5'
    error = None
    emptymd5 = False
    if  not os.path.exists(filenamemd5):
        emptymd5 = True
    else:
        error = md5tool.check_against_md5_file(md5dirfilename, filenamemd5) 
    if error == None and os.path.exists(destfilename) and emptymd5 == False:
        return
    print("copy %s ---> %s" % (md5dirfilename, destfilename))
    md5tool.generate_md5_file_for(md5dirfilename, filenamemd5)
    shutil.copy(md5dirfilename, destfilename)

scanprotofile()
gencpp()
md5copy(clientservicedir + cppfilename)
