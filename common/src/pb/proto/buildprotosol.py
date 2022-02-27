import os

import md5tool
import shutil

rpcarry = []
servicenames = []
msg = 'message'
begin = '{'
end = '}'
writedir = './protolua/'

if not os.path.exists(writedir):
    os.makedirs(writedir)

def genluasol(filename, writedir):
    msgcode = 0
    newstr = ''
    newfilename = writedir + filename.replace('.proto', '_lua.cpp')
    with open(filename,'r', encoding='utf-8') as file:
        filedbegin = 0
        for fileline in file:
            if fileline.find(msg) >= 0 :
                msgcode = 1
                classname = fileline.split(' ')[1].strip('\n')
                newstr += 'g_lua.new_usertype<' + classname + '>"' + classname + '",\n'
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
              
    with open(newfilename, 'w', encoding='utf-8')as file:
        file.write(newstr)                

def md5copy(destdir, srcdir, fileextend):
    for (dirpath, dirnames, filenames) in os.walk(srcdir):
        for each_filename in filenames:
            if not (each_filename[-4:].lower() == fileextend):
                continue
            filenamemd5 = srcdir + "/" + each_filename + '.md5'
            error = None
            first = False
            if not os.path.exists(filenamemd5):
                first = True
            else:
                error = md5tool.check_against_md5_file(each_filename, filenamemd5)
            destfilename = destdir + each_filename 
            if error == None and os.path.exists(destfilename) :
                continue
            print("copy %s %s" % (each_filename, destfilename))
            md5tool.generate_md5_file_for(each_filename, filenamemd5)
            shutil.copy(destfilename, each_filename)

genluasol('gw2ms.proto', writedir)

md5copy(writedir, '../pb2sol/', '.cpp')

