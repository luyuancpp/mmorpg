import os

import md5tool
import shutil

funsname = []
msg = 'message'
begin = '{'
end = '}'
srcdir = './pb2sol2/'
destdir = '../pb2sol2/'
namespacestr = 'namespace common'
setname = '::set_'
mutablename = '::mutable_'

if not os.path.exists(srcdir):
    os.makedirs(srcdir)

def genluasol(filename, srcdir):
    global funsname
    msgcode = 0
    pbnamespace = filename.replace('.proto', '')
    funcname = 'void Pb2sol2' + filename.replace('.proto', '') + '()'
    funsname.append(funcname)
    newstr = '#include "' + pbnamespace + '.pb.h"\n'
    newstr += '#include <sol/sol.hpp>\n'
    newstr += 'using namespace ' + pbnamespace + ';\n'
    newstr += namespacestr + '\n{\n' 
    newstr += 'extern thread_local sol::state g_lua;\n'
    newstr +=  funcname + '\n{\n'    
    newfilename = srcdir + filename.replace('.proto', '_sol2.cpp')
    with open(filename,'r', encoding='utf-8') as file:
        filedbegin = 0
        for fileline in file:
            if fileline.find(msg) >= 0 :
                msgcode = 1
                classname = fileline.split(' ')[1].strip('\n')
                newstr += 'g_lua.new_usertype<' + classname + '>("' + classname + '",\n'
                continue
            if fileline.find(begin) >= 0 and msgcode == 1 and filedbegin == 0:
                filedbegin = 1
                continue
            elif fileline.find(end) >= 0 and msgcode == 1:
                filedbegin = 0
                newstr = newstr.strip(',\n')
                newstr += ');\n\n'
                msgcode = 0 
                continue             
            elif msgcode == 1 and filedbegin == 1 :
                s = fileline.split(' ')
                typename = s[2]
                fildename = s[3]
                templatename = ''
                sn = setname
                if typename == 'bool' or typename == 'uint32' or typename == 'int32' or typename == 'uint64' or typename == 'int64' :
                    templatename = ''
                elif typename == 'string' :
                    templatename = '<const std::string&>'
                else :  
                    continue
                newstr += '"' + fildename + '",\n'
                newstr +=  'sol::property(&' + classname + '::' + fildename + ', &' + classname + sn + fildename
                newstr += templatename + '),\n'
                continue
    newstr += '}\n'
    newstr += '}//' + namespacestr + '\n'
    with open(newfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)                

def md5copy(destdir, srcdir, fileextend):
    for (dirpath, dirnames, filenames) in os.walk(srcdir):
        for each_filename in filenames:
            relative_path_filename = srcdir + each_filename
            if each_filename[-len(fileextend):].lower() != fileextend:
                continue
            filenamemd5 = relative_path_filename + '.md5'
            error = None
            first = False
            if not os.path.exists(filenamemd5):
                first = True
            else:
                error = md5tool.check_against_md5_file(relative_path_filename, filenamemd5)
            destfilename = destdir + each_filename 
            if error == None and os.path.exists(destfilename) and first != True:
                continue
            print("copy %s %s" % (relative_path_filename, destfilename))
            md5tool.generate_md5_file_for(relative_path_filename, filenamemd5)
            shutil.copy(relative_path_filename , destfilename)

def gentotalfile(destdir, srcdir):
    global funsname
    headfilename = 'pb2sol2.h'
    srcheadfilename = srcdir + 'pb2sol2.h'
    cppfilename = srcdir + 'pb2sol2.cpp'    
    totalfuncitonname = 'void pb2sol2()'
    
    with open(srcheadfilename, 'w', encoding='utf-8')as file:
        definestr = 'COMMON_SRC_PB_PB2SOL2_H_'
        headstr = '#ifndef ' + definestr + '\n'
        headstr += '#define ' + definestr + '\n'
        headstr += '#include <sol/sol.hpp>\n'
        headstr += namespacestr + '\n{\n'
        headstr += totalfuncitonname + ';\n'
        headstr += 'extern thread_local sol::state g_lua;\n'
        headstr += '}//' + namespacestr + '\n'
        headstr += '#endif//' + definestr + '\n' 
        file.write(headstr)            
    with open(cppfilename, 'w', encoding='utf-8')as file:
        cppnewstr = '#include "' + headfilename + '"\n'
        cppnewstr += namespacestr + '\n{\n'
        cppnewstr += 'thread_local sol::state g_lua;\n'
        for fn in funsname:
            cppnewstr += fn + ';\n'
        cppnewstr += totalfuncitonname + '\n{\n'
        for fn in funsname:
            cppnewstr += fn.replace('void', '').strip(' ') + ';\n'
        cppnewstr += '}\n'
        cppnewstr += '}//' + namespacestr + '\n'
        file.write(cppnewstr)    

genluasol('gw2ms.proto', srcdir)

gentotalfile(destdir, srcdir)
md5copy(destdir, srcdir, '.h')
md5copy(destdir, srcdir, '.cpp')

