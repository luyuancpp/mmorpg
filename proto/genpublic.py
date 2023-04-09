import os
import md5tool
import shutil

controller_file_prefix = 'controller_'
gs_file_prefix = 'game_'
lobby_file_prefix = 'lobby_'
md5dir = 'md5/'
pbcdir = '../common/src/pb/pbc/'
pbcserviceinstancedir = '../common/src/pb/pbc/prototypeserviceinstance/'
pbcserviceinstancemd5dir = md5dir + 'rototypeserviceinstance/'
logicprotodir = 'logic_proto/'
commonportodir = 'common_proto/'
eventprotodir = 'event_proto/'

gslogicervicedir = '../game_server/src/service/logic_proto/'
gsservicelogicreplieddir = '../game_server/src/service/logic_proto_replied/'
lobbylogicservicedir = '../lobby_server/src/service/logic_proto/'
controllerlogicservicedir = '../controller_server/src/service/logic_proto/'
controllerservicelogicreplieddir = '../controller_server/src/service/logic_proto_replied/'
gatelogicservicedir = '../gate_server/src/service/logic_proto/'
servicemethoddir = '../common/src/pb/pbc/service_method/'
servicemethodmd5dir = md5dir + 'service_method/'

controller = '(::google::protobuf::RpcController* controller'
yourcodebegin = '///<<< BEGIN WRITING YOUR CODE'
yourcodeend = '///<<< END WRITING YOUR CODE'
rpcbegin = '///<<<rpc begin'
rpcend = '///<<<rpc end'

luayourcodebegin = '---<<< BEGIN WRITING YOUR CODE'
luayourcodeend = '---<<< END WRITING YOUR CODE'
luarpcbegin = '---<<<rpc begin'
luarpcend = '---<<<rpc end'

logicmd5dir = './md5/logic_proto/'
md5dirs = [md5dir + 'controller_server/', 
md5dir + 'game_server/', 
md5dir + 'gate_server/',
md5dir + 'login_server/',
md5dir + 'lobby_server/',
md5dir + 'database_server/',
md5dir + 'deploy_server/']

logicrepliedmd5dirs = \
[md5dir + 'controller_server/', 
md5dir + 'game_server/logic_proto_replied/', 
md5dir + 'gate_server/logic_proto_replied/',
md5dir + 'login_server/logic_proto_replied/',
md5dir + 'lobby_server/logic_proto_replied/',
md5dir + 'database_server/logic_proto_replied/',
md5dir + 'deploy_server/logic_proto_replied/']

commonprotomd5dirs = \
[md5dir + 'controller_server/', 
md5dir + 'game_server/common_proto/', 
md5dir + 'gate_server/common_proto/',
md5dir + 'login_server/common_proto/',
md5dir + 'lobby_server/common_proto/',
md5dir + 'database_server/common_proto/',
md5dir + 'deploy_server/common_proto/']

eventprotomd5dirs = \
[md5dir + 'controller_server/', 
md5dir + 'game_server/event_proto/', 
md5dir + 'gate_server/event_proto/',
md5dir + 'login_server/event_proto/',
md5dir + 'lobby_server/event_proto/',
md5dir + 'database_server/event_proto/',
md5dir + 'deploy_server/event_proto/']

logicprotomd5dirs = \
[md5dir + 'controller_server/', 
md5dir + 'game_server/logic_proto/', 
md5dir + 'gate_server/logic_proto/',
md5dir + 'login_server/logic_proto/',
md5dir + 'lobby_server/logic_proto/',
md5dir + 'database_server/logic_proto/',
md5dir + 'deploy_server/logic_proto/']

conrollermd5dirindex = 0
gamemd5dirindex = 1
gatemd5dirindex = 2
loginmd5dirindex = 3
lobbymd5dirindex = 4
databasemd5dirindex = 5
deploymd5dirindex = 6 

tabstr = '    '

def is_server_proto(filename):
    return (filename.find('client_player') < 0 and filename.find('server_player') < 0) or  filename.find(lobby_file_prefix) >= 0 

def is_gs_and_controller_server_proto(filename):
    return filename.find('client_player') < 0 and filename.find('server_player') < 0 and  filename.find(lobby_file_prefix) < 0  

def is_client_player_proto(filename):
    return filename.find('client_player') >= 0 

def is_server_player_proto(filename):
    return filename.find('server_player') >= 0 

def is_player_proto(filename):
    return is_client_player_proto(filename) or is_server_player_proto(filename)

def is_not_client_proto(filename):
    return is_client_player_proto(filename)  == False
    
def makedirs():
    if not os.path.exists(pbcdir):
        os.makedirs(pbcdir)
    if not os.path.exists(pbcserviceinstancedir):
        os.makedirs(pbcserviceinstancedir)
    if not os.path.exists(md5dir):
        os.makedirs(md5dir)
    if not os.path.exists(servicemethoddir):
        os.makedirs(servicemethoddir)
    if not os.path.exists(servicemethodmd5dir):
        os.makedirs(servicemethodmd5dir)
    if not os.path.exists(logicmd5dir):
        os.makedirs(logicmd5dir)    
    if not os.path.exists(pbcserviceinstancemd5dir):
        os.makedirs(pbcserviceinstancemd5dir)
    for d in md5dirs :
        if not os.path.exists(d):
            os.makedirs(d)
    for d in commonprotomd5dirs :
        if not os.path.exists(d):
            os.makedirs(d)
    for d in eventprotomd5dirs :
        if not os.path.exists(d):
            os.makedirs(d)
    for d in logicprotomd5dirs :
        if not os.path.exists(d):
            os.makedirs(d)
    
            
    for d in logicrepliedmd5dirs :
        if not os.path.exists(d):
            os.makedirs(d)
        logic_proto_dir = d + '/logic_proto/' 
        if not os.path.exists(logic_proto_dir):
            os.makedirs(logic_proto_dir)
        logic_proto_replied_dir = logic_proto_dir.replace('/logic_proto/', '/logic_proto_replied/')
        if not os.path.exists(logic_proto_replied_dir):
            os.makedirs(logic_proto_replied_dir)
        common_proto_dir = d + '/common_proto/' 
        if not os.path.exists(common_proto_dir):
            os.makedirs(common_proto_dir)
    
    
    
def makedirsbypath(dirname):
    dirlist = []
    for d in md5dirs :
        fulldir = d + dirname
        dirlist.append(fulldir)
        if not os.path.exists(fulldir):            
            os.makedirs(fulldir)
    return dirlist
 
def controllermd5dir():
    return md5dirs[conrollermd5dirindex]  
    
def controller():
    return 'controller' 
 
def iscontrollerdir(dirpath):
    return dirpath.find('controller_server') >= 0 or dirpath.find('controller') >= 0
    
def gamemd5dir():
    return md5dirs[gamemd5dirindex]  
 
def game():
    return 'game'  
 
def isgamedir(dirpath):
    return dirpath.find('game_server') >= 0 or dirpath.find('game') >= 0

def lobbymd5dir():
    return md5dirs[lobbymd5dirindex]  

def lobby():
    return 'lobby'  

def islobbydir(dirpath):
    return dirpath.find('lobby_server') >= 0 or dirpath.find('lobby') >= 0
    
def loginmd5dir():
    return md5dirs[loginmd5dirindex]  

def islogindir(dirpath):
    return dirpath.find('login_server') >= 0 or dirpath.find('login') >= 0

def databasemd5dir():
    return md5dirs[databasemd5dirindex]  

def isdatabasedir(dirpath):
    return dirpath.find('database_server') >= 0 or dirpath.find('database') >= 0
    
def gatemd5dir():
    return md5dirs[gatemd5dirindex]  

def isgatedir(dirpath):
    return dirpath.find('gate_server') >= 0 or dirpath.find('gate') >= 0
   
def deploymd5dir():
    return md5dirs[deploymd5dirindex]   

def isdeploydir(dirpath):
    return dirpath.find('deploy_server') >= 0 or dirpath.find('deploy') >= 0 
  
def commonproto():
    return commonportodir
    
def iscommonproto(dirpath):
    return dirpath.find(commonproto()) >= 0
       
def getservertype(dirpath):
    if isgamedir(dirpath):  
        return 'game'
    elif iscontrollerdir(dirpath):
        return 'controller'
    elif islobbydir(dirpath):
        return 'lobby'
    elif isgatedir(dirpath):
        return 'gate'
    elif islogindir(dirpath):
        return 'login'  
    elif isdatabasedir(dirpath):
        return 'database'  
    elif isdeploydir(dirpath):
        return 'deploy'  
    return ''
  
def getsrcpathmd5dir(dirpath, subdir):
    srcdir = ''
    if iscommonproto(dirpath):
        subdir = commonproto()
    if isgamedir(dirpath):
        srcdir = md5dirs[gamemd5dirindex]
    elif iscontrollerdir(dirpath):
        srcdir = md5dirs[conrollermd5dirindex]
    elif islobbydir(dirpath):
        srcdir = md5dirs[lobbymd5dirindex]
    elif isgatedir(dirpath):
        srcdir = md5dirs[gatemd5dirindex]
    elif islogindir(dirpath):
        srcdir = md5dirs[loginmd5dirindex]  
    elif isdatabasedir(dirpath):
        srcdir = md5dirs[databasemd5dirindex]    
    elif isdeploydir(dirpath):
        srcdir = md5dirs[deploymd5dirindex]   
    return srcdir + subdir
    
def getdestdir(dirpath):
    srcdir = ''
    if isgamedir(dirpath):
        srcdir = gslogicervicedir
    elif iscontrollerdir(dirpath):
        srcdir = controllerlogicservicedir
    elif islobbydir(dirpath):
        srcdir = lobbylogicservicedir
    elif isgatedir(dirpath):
        srcdir = gatelogicservicedir
    elif islogindir(dirpath):
        srcdir = md5dirs[loginmd5dirindex]  
    elif isdatabasedir(dirpath):
        srcdir = md5dirs[databasemd5dirindex]    
    elif isdeploydir(dirpath):
        srcdir = md5dirs[deploymd5dirindex]   
    return srcdir 

def is_service_fileline(fileline):
    return fileline.find('service ') >= 0 and (fileline.find('{') >= 0 or fileline.find('Service') >= 0)


class md5fileinfo():
    def __init__(self):
        self.filename = ''
        self.destdir = ''
        self.md5dir = ''
        self.originalextension = ''
        self.targetextension = ''
        self.extensionfitler = []
        self.destfilename = ''
        self.genfilename = ''
        self.filenamemd5= ''

def md5check(md5info):
    filebasename = os.path.basename(md5info.filename).replace(md5info.originalextension, md5info.targetextension)
    genfilename = md5info.md5dir + filebasename
    filenamemd5 = genfilename + '.md5'
    destfilename = md5info.destdir + filebasename
    genfilemd5error = None
    destfilemd5error = None
    fileempty = False
    if  not os.path.exists(filenamemd5):
        fileempty = True
    elif not os.path.exists(destfilename):
        fileempty = True
    else:
        genfilemd5error = md5tool.check_against_md5_file(genfilename, filenamemd5)     
        destfilemd5error = md5tool.check_against_md5_file(destfilename, filenamemd5)        
    
    if genfilemd5error == None and destfilemd5error == None and  fileempty == False:
        return True, destfilename,  genfilename,  filenamemd5
    return False, destfilename,  genfilename,  filenamemd5

def md5copy(md5info):
    for filterext in md5info.extensionfitler:
        if md5info.filename.find(filterext) >= 0:
            return 
    checkmd5, destfilename, genfilename , genfilenamemd5 = md5check(md5info)    
    if checkmd5 == True:
        return
    print("copy %s ---> %s" % (genfilename, destfilename))
    md5tool.generate_md5_file_for(genfilename, genfilenamemd5)
    shutil.copy(genfilename, destfilename)
    
class cpp():
    def __init__(self):
        self.destfilename = ''
        self.md5filename = ''
        self.includestr = ''
        self.filemethodarray = []
        self.begunfun = None
        self.controller = ''

def gencppfile(cppfile):
    newstr = cppfile.includestr
    serviceidx = 0
    skipline = cppfile.includestr.count('\n')
    try:
        with open(cppfile.destfilename,'r+', encoding='utf-8') as file:
            service_begined = 0
            isyourcode = 1 
            skipheadline = 0 
            for fileline in file:
                if skipheadline < skipline :
                    skipheadline += 1
                    continue
                #处理开始自定义文件
                if service_begined == 0 and fileline.find(rpcbegin) >= 0:
                    newstr += fileline
                    service_begined = 1
                    continue 
                #开始处理RPC 
                if service_begined == 1:
                    if serviceidx < len(cppfile.filemethodarray) and fileline.find(cppfile.controller) >= 0 :
                        isyourcode = 0
                        newstr += cppfile.begunfun(serviceidx)
                        continue
                    elif fileline.find(yourcodebegin) >= 0 :
                        newstr += fileline
                        isyourcode = 1
                        continue
                    elif fileline.find(yourcodeend) >= 0 :
                        newstr += yourcodeend + '\n}\n\n'
                        isyourcode = 0
                        serviceidx += 1  
                        continue
                    elif fileline.find(rpcend) >= 0:
                        break
                if isyourcode == 1 or service_begined == 0:
                    newstr += fileline
                    continue                
    except FileNotFoundError:
        newstr += rpcbegin + '\n'
    while serviceidx < len(cppfile.filemethodarray) :
        newstr += cppfile.begunfun(serviceidx)
        newstr += yourcodebegin +  '\n'
        newstr += yourcodeend +  '\n}\n\n'
        serviceidx += 1 
    newstr += rpcend + '\n'
    with open(cppfile.md5filename, 'w', encoding='utf-8')as file:
        file.write(newstr)
        
makedirs()