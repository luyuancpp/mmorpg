import os

import md5tool
import shutil
import genpublic

funsname = []
msg = 'message'
begin = '{'
end = '}'
srcdir = './md5/sol2/'
destdir = '../common/src/pb/pbc/pb2sol2/'
setname = '::set_'
mutablename = '::mutable_'
genprotodir = ['./logic_proto/', './component_proto/', ]
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

def genluasol(filename, srcdir):
    global funsname
    msgcode = 0
    enumcode = 0
    pbheadname = filename.replace('.proto', '')
    funcname = 'void Pb2sol2' + filename.replace('.proto', '').replace('.', '_').replace('/', '_') + '()'
    funsname.append(funcname)
    newstr = '#include "' + pbheadname + '.pb.h"\n'
    newstr += '#include <sol/sol.hpp>\n'

    newstr += '#include "src/game_logic/thread_local/thread_local_storage_lua.h"\n'
    newstr +=  funcname + '\n{\n'    
    newfilename = srcdir + os.path.basename(filename).replace('.proto', '_sol2.cpp')
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
                newstr += 'tls_lua_state.new_usertype<' + classname + '>("' + classname + '",\n'
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
                    valuetype = s[1].strip('\t').strip(' ').split('>')[0]
                   
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
                        valuetyperef = valuetype
                        iscpptypev = iscpptype(valuetype)
                        if  iscpptypev == False:
                            valuetyperef = valuetype + '&'
                        else:
                            valuetyperef, valueinttype = tocppinttype(valuetype)
                        newstr += '[](' + classname + '& pb, ' + keytype + ' key, ' + valuetyperef + ' value) ->decltype(auto){ return pb.mutable_' + fildename +'()->emplace(key, value).second;},\n'

                        newstr += '"' + fildename + '",\n'
                        newstr += '[](' + classname + '& pb, ' + keytype + ' key) ->decltype(auto){\n'
                        newstr += ' auto it =  pb.mutable_' + fildename +'()->find(key);\n'
                        if  iscpptypev == False:
                            newstr += ' if (it == pb.mutable_' + fildename +'()->end()){ static ' + valuetype + ' instance; return instance; }\n'
                        else:
                            vt,_ = tocppinttype(valuetype)
                            newstr += ' if (it == pb.mutable_' + fildename +'()->end()){ return ' + vt + '(); }\n'
                        newstr += ' return it->second;},\n'

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
    cppmd5info = genpublic.md5fileinfo()
    cppmd5info.destdir = destdir
    cppmd5info.md5dir = srcdir
    for (dirpath, dirnames, filenames) in os.walk(srcdir):
        for each_filename in filenames:
            if each_filename[-len(fileextend):].lower() != fileextend:
                continue
            cppmd5info.filename = each_filename
            genpublic.md5copy(cppmd5info)

def gentotalfile(destdir, srcdir):
    global funsname
    headfilename = 'pb2sol2.h'
    srcheadfilename = srcdir + 'pb2sol2.h'
    cppfilename = srcdir + 'pb2sol2.cpp'    
    totalfuncitonname = 'void pb2sol2()'
    
    with open(srcheadfilename, 'w', encoding='utf-8')as file:
        headstr = '#pragma once\n'
        headstr += '#include <google/protobuf/message.h>\n'
        headstr += '#include <sol/sol.hpp>\n'
        headstr += totalfuncitonname + ';\n'
        headstr += '#include "src/game_logic/thread_local/thread_local_storage_lua.h"\n'
        file.write(headstr)            
    with open(cppfilename, 'w', encoding='utf-8')as file:
        cppnewstr = '#include "' + headfilename + '"\n'
        for fn in funsname:
            cppnewstr += fn + ';\n'
        cppnewstr += totalfuncitonname + '\n{\n'
        cppnewstr += 'tls_lua_state.new_usertype<::google::protobuf::Message>("Message");\n'
        for fn in funsname:
            cppnewstr += fn.replace('void', '').strip(' ') + ';\n'
        cppnewstr += '}\n'
        file.write(cppnewstr)    

genluasol('./common_proto/common.proto', srcdir)
genluasol('./common_proto/c2gate.proto', srcdir)

def scanprotofile():
    for protodir in genprotodir:
        dir_list  = os.listdir(protodir)
        for filename in dir_list:
            if not (filename.find('client_player.proto') >= 0 or filename.find('comp.proto') >= 0):
                continue
            genluasol(protodir + filename, srcdir)

genpublic.makedirs()

scanprotofile()
gentotalfile(destdir, srcdir)
md5copy(destdir, srcdir, 'sol2.h')
md5copy(destdir, srcdir, 'sol2.cpp')

