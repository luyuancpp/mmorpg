import os

import md5tool
import shutil

funsname = []
msg = 'message'
begin = '{'
end = '}'
srcdir = './md5/'
destdir = '../pb2sol2/'
setname = '::set_'
mutablename = '::mutable_'
genprotodir = ['./logic_proto/', './component_proto/']
enum = {}
maptype = 'map'

if not os.path.exists(srcdir):
    os.makedirs(srcdir)
if not os.path.exists(destdir):
    os.makedirs(destdir)    

def tocppinttype(typestring):
    if typestring == 'uint32' or typestring == 'int32' or typestring == 'uint64' or typestring == 'int64' :
        typestring = typestring + '_t'
        return typestring,True
    return typestring,False
def iscpptype(typestring):
    if typestring == 'uint32' or typestring == 'int32' or typestring == 'uint64' or typestring == 'int64' or typestring == 'bool':
       return True
    return False

def genluasol(filename, srcdir, protodir):
    global funsname
    msgcode = 0
    enumcode = 0
    pbheadname = filename.replace('.proto', '')
    funcname = 'void Pb2sol2' + filename.replace('.proto', '').replace('.', '_').replace('/', '_') + '()'
    funsname.append(funcname)
    newstr = '#include "' + pbheadname + '.pb.h"\n'
    newstr += '#include <sol/sol.hpp>\n'

    newstr += 'extern thread_local sol::state g_lua;\n'
    newstr +=  funcname + '\n{\n'    
    newfilename = srcdir + filename.replace('.proto', '_sol2.cpp').replace(protodir, '')
    with open(filename,'r', encoding='utf-8') as file:
        filedbegin = 0
        for fileline in file:
            if fileline.find('enum') >= 0 :
                enumcode = 1     
                continue 
            if fileline.find(end) >= 0 and enumcode == 1:      
                enumcode = 0 
                continue
            elif enumcode == 1 :
                continue
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
                newstr += '"DebugString",\n'
                newstr += '&' + classname + '::DebugString,\n'
                newstr += 'sol::base_classes, sol::bases<::google::protobuf::Message>());\n\n'
                msgcode = 0 
                continue             
            elif msgcode == 1 and filedbegin == 1 :
                s = fileline.strip('\t').strip(' ').split(' ')
                if len(s) < 3:
                    continue
                typename = s[0]
                fildename = s[1]
                templatename = ''
                sn = setname
                repeatedfiled = True
                keytype = ''
                valuetype = ''
                keyinttype = False
                valueinttype = False
                if typename == 'bool' or typename == 'uint32' or typename == 'int32' or typename == 'uint64' or typename == 'int64' :
                    templatename = ''
                    repeatedfiled = False
                elif typename == 'string' :
                    templatename = '<const std::string&>'
                    repeatedfiled = False
                elif typename == 'repeated':  
                    typename = s[1]
                    fildename = s[2]
                elif typename.find(maptype) >= 0:  
                    typename = maptype
                    keytype = s[0].strip('\tmap<').split(',')[0]
                    keytype , keyinttype = tocppinttype(keytype)
                    vt = s[1].strip('\t').strip(' ').split('>')[0]
                    valuetype = vt
                    valuetype, valueinttype = tocppinttype(valuetype)
                    if iscpptype(vt) == False:
                        valuetype = valuetype + '&'
                    fildename = s[2]
                else:
                    newstr += '"' + fildename + '",\n'
                    newstr += '[](' + classname + '& pb) ->decltype(auto){ return pb.' + fildename +'();},\n'
                    newstr += '"mutable_' + fildename + '",\n'
                    newstr += '[](' + classname + '& pb) ->decltype(auto){ return pb.mutable_' + fildename +'();},\n'
                    continue
                if repeatedfiled == False :
                    newstr += '"' + fildename + '",\n'
                    newstr +=  'sol::property(&' + classname + '::' + fildename + ', &' + classname + sn + fildename
                    newstr += templatename + '),\n'
                else:
                    if typename == 'string' :
                        #add string
                        templatename = '<const std::string&>'
                        newstr += '"add_' + fildename + '",\n'
                        newstr += '[](' + classname + '& pb, const std::string& value) ->decltype(auto){ return pb.add_' + fildename +'(value);},\n'

                        #get const string
                        newstr += '"' + fildename + '",\n'
                        newstr += '[](' + classname + '& pb, int index) ->decltype(auto){ return pb.' + fildename +'(index);},\n'

                        #set string by index
                        newstr += '"set_' + fildename + '",\n'
                        newstr += '[](' + classname + '& pb, int index, const std::string& value) ->decltype(auto){ return pb.set_' + fildename +'(index, value);},\n'
                    elif typename == maptype: 
                        newstr += '"count_' + fildename + '",\n'
                        newstr += '[](' + classname + '& pb, ' + keytype + ' key) ->decltype(auto){ return pb.' + fildename +'().count(key);},\n'
                        newstr += '"insert_' + fildename + '",\n'
                        
                        newstr += '[](' + classname + '& pb, ' + keytype + ' key, ' + valuetype + ' value) ->decltype(auto){ return pb.mutable_' + fildename +'()->emplace(key, value).second;},\n'

                        newstr += '"' + fildename + '",\n'
                        newstr += '[](' + classname + '& pb, ' + keytype + ' key) ->decltype(auto){ return pb.mutable_' + fildename +'()->find(key)->second;},\n'

                    else:
                        typename, inttype = tocppinttype(typename)
                        if inttype == True :
                            newstr += '"add_' + fildename + '",\n'
                            newstr += '&' + classname + '::add_' + fildename + ',\n'

                            newstr += '"' + fildename + '",\n'
                            newstr += '[](' + classname + '& pb, int index) ->decltype(auto){ return pb.' + fildename +'(index);},\n'

                            #set int by index
                            newstr += '"set_' + fildename + '",\n'
                            newstr += '[](' + classname + '& pb, int index, ' + typename + ' value) ->decltype(auto){ return pb.set_' + fildename +'(index, value);},\n'
                        else:
                            newstr += '"add_' + fildename + '",\n'
                            newstr += '&' + classname + '::add_' + fildename + ',\n'

                            newstr += '"' + fildename + '",\n'
                            newstr += '[](' + classname + '& pb, int index) ->decltype(auto){ return pb.' + fildename +'(index);},\n'

                            #set int by index
                            newstr += '"mutable_' + fildename + '",\n'
                            newstr += '[](' + classname + '& pb, int index) ->decltype(auto){ return pb.mutable_' + fildename +'(index);},\n'
                    #fileds size 
                    newstr += '"' + fildename + '_size",\n'
                    newstr += '&' + classname + '::' + fildename + '_size,\n'

                    #clear fileds 
                    newstr += '"clear_' + fildename + '",\n'
                    newstr += '&' + classname + '::clear_' + fildename + ',\n'
                
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
    headfilename = 'pb2sol2.h'
    srcheadfilename = srcdir + 'pb2sol2.h'
    cppfilename = srcdir + 'pb2sol2.cpp'    
    totalfuncitonname = 'void pb2sol2()'
    
    with open(srcheadfilename, 'w', encoding='utf-8')as file:
        definestr = 'COMMON_SRC_PB_PB2SOL2_H_'
        headstr = '#ifndef ' + definestr + '\n'
        headstr += '#define ' + definestr + '\n'
        headstr += '#include <google/protobuf/message.h>\n'
        headstr += '#include <sol/sol.hpp>\n'
        headstr += totalfuncitonname + ';\n'
        headstr += 'extern thread_local sol::state g_lua;\n'
        headstr += '#endif//' + definestr + '\n' 
        file.write(headstr)            
    with open(cppfilename, 'w', encoding='utf-8')as file:
        cppnewstr = '#include "' + headfilename + '"\n'
        cppnewstr += 'thread_local sol::state g_lua;\n'
        for fn in funsname:
            cppnewstr += fn + ';\n'
        cppnewstr += totalfuncitonname + '\n{\n'
        cppnewstr += 'g_lua.new_usertype<::google::protobuf::Message>("Message");\n'
        for fn in funsname:
            cppnewstr += fn.replace('void', '').strip(' ') + ';\n'
        cppnewstr += '}\n'
        file.write(cppnewstr)    

genluasol('common.proto', srcdir, '')
genluasol('c2gw.proto', srcdir, '')

def get_file_list(file_path):
    dir_list = os.listdir(file_path)
    if not dir_list:
        return
    else:
        dir_list = sorted(dir_list,key=lambda x: os.path.getmtime(os.path.join(file_path, x)))
        return dir_list

def inputfile():
    for protodir in genprotodir:
        dir_list  = get_file_list(protodir)
        for filename in dir_list:
            if not (filename.find('client_player.proto') >= 0 or filename.find('comp.proto') >= 0):
                continue
            genluasol(protodir + filename, srcdir, protodir)

inputfile()
gentotalfile(destdir, srcdir)
md5copy(destdir, srcdir, 'sol2.h')
md5copy(destdir, srcdir, 'sol2.cpp')

