import os
from os import system

controller_file_prefix = 'controller_'
gs_file_prefix = 'gs_'
lobby_file_prefix = 'lobby_'
md5dir = './md5/'

servermd5dirs = [md5dir + 'contoller/', md5dir + 'game/', md5dir + 'gate/', md5dir + 'login/', md5dir + 'lobby/', md5dir + 'database/']
conrollermd5dirindex = 0
gamemd5dirindex = 1
gatemd5dirindex = 2
loginmd5dirindex = 3
lobbymd5dirindex = 4
databasemd5dirindex = 5

def is_server_proto(filename):
    return (filename.find('client_player') < 0 and filename.find('server_player') < 0) or  filename.find(lobby_file_prefix) >= 0 

def is_gs_and_controller_server_proto(filename):
    return filename.find('client_player') < 0 and filename.find('server_player') < 0 and  filename.find(lobby_file_prefix) < 0  

def is_client_proto(filename):
    return filename.find('client_player') >= 0 

def is_not_client_proto(filename):
    return is_client_proto(filename)  == False
    
def makedirs():
    if not os.path.exists(md5dir):
        os.makedirs(md5dir)
    for d in servermd5dirs :
        if not os.path.exists(d):
            os.makedirs(d)

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
 
def iscontrollerdir(dirpath):
    return dirpath.find(controllermd5dir()) >= 0
    
def gamemd5dir():
    return servermd5dirs[gamemd5dirindex]  
    
def isgamedir(dirpath):
    return dirpath.find(gamemd5dir()) >= 0

def lobbymd5dir():
    return servermd5dirs[lobbymd5dirindex]  

def islobbydir(dirpath):
    return dirpath.find(lobbymd5dir()) >= 0