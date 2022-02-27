import os

import md5tool
import shutil

funsname = []
msg = 'message'
begin = '{'
end = '}'
srcdir = './proto2sol2/'
destdir = '../pb2sol2/'

if not os.path.exists(srcdir):
    os.makedirs(srcdir)

def genluasol(filename, srcdir):
    global funsname
    msgcode = 0
    funcname = 'void Pb2sol2' + filename.replace('.proto', '') + '()'
    funsname.append(funcname)
    newstr = funcname + '\n{\n'
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
                newstr = newstr.strip(',')
                newstr += ');\n'
                msgcode = 0 
                continue             
            elif msgcode == 1 and filedbegin == 1 :
                s = fileline.split(' ')
                typename = s[0]
                fildename = s[1]
                templatename = ''
                if typename == 'bool' or typename == 'uint32' or typename == 'int32' or typename == 'uint64' or typename == 'int64' :
                    templatename = ''
                elif typename == 'string' :
                    templatename = '<const std::string&>'  
                newstr +=  'sol::property(&' + classname + '::' + fildename + ', &' + classname + '::set_' + fildename
                newstr += templatename + '),'
                continue
    newstr += '}\n'
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
    headfilename = srcdir + 'pb2sol2.h'
    cppfilename = srcdir + 'pb2sol2.cpp'    
    totalfuncitonname = 'void pb2sol2()'
    newstr = totalfuncitonname + '\n{\n'
    for fn in funsname:
        newstr += fn.replace('void', '').strip(' ') + ';\n'
    newstr += '}\n'
    with open(headfilename, 'w', encoding='utf-8')as file:
        file.write(totalfuncitonname + ';')     
    with open(cppfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)    

genluasol('gw2ms.proto', srcdir)

gentotalfile(destdir, srcdir)
md5copy(destdir, srcdir, '.h')
md5copy(destdir, srcdir, '.cpp')

