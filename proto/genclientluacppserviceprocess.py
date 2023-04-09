import os
from os import system
import genpublic

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

scanprotofile()
gencpp()

cppmd5info = genpublic.md5fileinfo()
cppmd5info.filename = cppfilename
cppmd5info.destdir = clientservicedir
cppmd5info.md5dir = md5dir
genpublic.md5copy(cppmd5info)
