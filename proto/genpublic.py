import os
from os import system

controller_file_prefix = 'controller_'
gs_file_prefix = 'game_'
lobby_file_prefix = 'lobby_'
md5dir = 'md5/'
pbcdir = '../common/src/pb/pbc/'
logicprotodir = 'logic_proto/'

gslogicervicedir = '../game_server/src/service/logic_proto/'
gslogicrepliedservicedir = '../game_server/src/service/logic_proto_replied/'
lobbylogicservicedir = '../lobby_server/src/service/logic_proto/'
controllerlogicservicedir = '../controller_server/src/service/logic_proto/'
controllerlogicrepliedservicedir = '../controller_server/src/service/logic_proto_replied/'
gatelogicservicedir = '../gate_server/src/service/logic_proto/'
servicemethoddir = '../common/src/pb/pbc/service_method/'
servicemethodmd5dir = md5dir + 'service_method/'

servermd5dirs = [md5dir + 'controller_server/', 
md5dir + 'game_server/', 
md5dir + 'gate_server/',
md5dir + 'login_server/',
md5dir + 'lobby_server/',
md5dir + 'database_server/',
md5dir + 'deploy_server/']
conrollermd5dirindex = 0
gamemd5dirindex = 1
gatemd5dirindex = 2
loginmd5dirindex = 3
lobbymd5dirindex = 4
databasemd5dirindex = 5
deploymd5dirindex = 6 

def is_server_proto(filename):
    return (filename.find('client_player') < 0 and filename.find('server_player') < 0) or  filename.find(lobby_file_prefix) >= 0 

def is_gs_and_controller_server_proto(filename):
    return filename.find('client_player') < 0 and filename.find('server_player') < 0 and  filename.find(lobby_file_prefix) < 0  

def is_client_proto(filename):
    return filename.find('client_player') >= 0 

def is_not_client_proto(filename):
    return is_client_proto(filename)  == False
    
def makedirs():
    if not os.path.exists(pbcdir):
        os.makedirs(pbcdir)
    if not os.path.exists(md5dir):
        os.makedirs(md5dir)
    if not os.path.exists(servicemethoddir):
        os.makedirs(servicemethoddir)
    if not os.path.exists(servicemethodmd5dir):
        os.makedirs(servicemethodmd5dir)
    for d in servermd5dirs :
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
    for d in servermd5dirs :
        fulldir = d + dirname
        dirlist.append(fulldir)
        if not os.path.exists(fulldir):            
            os.makedirs(fulldir)
    return dirlist
 
def controllermd5dir():
    return servermd5dirs[conrollermd5dirindex]  
    
def controller():
    return 'controller' 
 
def iscontrollerdir(dirpath):
    return dirpath.find('controller_server') >= 0 or dirpath.find('controller') >= 0
    
def gamemd5dir():
    return servermd5dirs[gamemd5dirindex]  
 
def game():
    return 'game'  
 
def isgamedir(dirpath):
    return dirpath.find('game_server') >= 0 or dirpath.find('game') >= 0

def lobbymd5dir():
    return servermd5dirs[lobbymd5dirindex]  

def lobby():
    return 'lobby'  

def islobbydir(dirpath):
    return dirpath.find('lobby_server') >= 0 or dirpath.find('lobby') >= 0
    
def loginmd5dir():
    return servermd5dirs[loginmd5dirindex]  

def islogindir(dirpath):
    return dirpath.find('login_server') >= 0 or dirpath.find('login') >= 0

def databasemd5dir():
    return servermd5dirs[databasemd5dirindex]  

def isdatabasedir(dirpath):
    return dirpath.find('database_server') >= 0 or dirpath.find('database') >= 0
    
def gatemd5dir():
    return servermd5dirs[gatemd5dirindex]  

def isgatedir(dirpath):
    return dirpath.find('gate_server') >= 0 or dirpath.find('gate') >= 0
   
def deploymd5dir():
    return servermd5dirs[deploymd5dirindex]   

def isdeploydir(dirpath):
    return dirpath.find('deploy_server') >= 0 or dirpath.find('deploy') >= 0 
  
def commonproto():
    return 'common_proto/'
    
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
        srcdir = servermd5dirs[gamemd5dirindex]
    elif iscontrollerdir(dirpath):
        srcdir = servermd5dirs[conrollermd5dirindex]
    elif islobbydir(dirpath):
        srcdir = servermd5dirs[lobbymd5dirindex]
    elif isgatedir(dirpath):
        srcdir = servermd5dirs[gatemd5dirindex]
    elif islogindir(dirpath):
        srcdir = servermd5dirs[loginmd5dirindex]  
    elif isdatabasedir(dirpath):
        srcdir = servermd5dirs[databasemd5dirindex]    
    elif isdeploydir(dirpath):
        srcdir = servermd5dirs[deploymd5dirindex]   
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
        srcdir = servermd5dirs[loginmd5dirindex]  
    elif isdatabasedir(dirpath):
        srcdir = servermd5dirs[databasemd5dirindex]    
    elif isdeploydir(dirpath):
        srcdir = servermd5dirs[deploymd5dirindex]   
    return srcdir 