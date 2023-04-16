import os
import md5tool
import shutil

projectdir = '../'
lobby_file_prefix = 'lobby_'
md5dir = 'md5/'
pbcdir = '../common/src/pb/pbc/'
pbcserviceinstancedir = '../common/src/pb/pbc/prototypeserviceinstance/'
logicprotodir = 'logic_proto/'
commonportodir = 'common_proto/'

gslogicervicedir = '../game_server/src/service/logic_proto/'

lobbylogicservicedir = '../lobby_server/src/service/logic_proto/'
controllerlogicservicedir = '../controller_server/src/service/logic_proto/'

gatelogicservicedir = '../gate_server/src/service/logic_proto/'
servicemethoddir = '../common/src/pb/pbc/service_method/'
servicemethoddir = '../common/src/pb/pbc/service_method/'

gsservicelogicreplieddir = '../game_server/src/service/logic_proto_replied/'
controllerservicelogicreplieddir = '../controller_server/src/service/logic_proto_replied/'

controller = '(::google::protobuf::RpcController* controller'
yourcodebegin = '///<<< BEGIN WRITING YOUR CODE'
yourcodeend = '///<<< END WRITING YOUR CODE'
rpcbegin = '///<<<rpc begin'
rpcend = '///<<<rpc end'

luayourcodebegin = '---<<< BEGIN WRITING YOUR CODE'
luayourcodeend = '---<<< END WRITING YOUR CODE'
luarpcbegin = '---<<<rpc begin'
luarpcend = '---<<<rpc end'


projectdirs = ['common',
'controller_server', 
'game_server', 
'gate_server',
'login_server',
'lobby_server',
'database_server',
'deploy_server',
'client']

tabstr = '    '

def is_server_proto(filename):
    return (filename.find('client_player') < 0 and filename.find('server_player') < 0) or  filename.find(lobby_file_prefix) >= 0 

def is_gs_and_controller_server_proto(filename):
    return filename.find('client_player') < 0 and filename.find('server_player') < 0 and  filename.find(lobby_file_prefix) < 0  

def is_client_player_proto(filename):
    return filename.find('client_player') >= 0 

def is_server_player_proto(filename):
    return filename.find('server_player') >= 0 

def is_not_client_proto(filename):
    return is_client_player_proto(filename)  == False
    

def create_dirtree_without_files2md5(src, dst):
    if os.path.exists(dst):
        return
    os.makedirs(dst)
    src_prefix = len(src) + len(os.path.sep) - 1
    projectlen = len(projectdirs)
    srcprojectdirs = []
    for i in range(0, projectlen):
        srcprojectdirs.append(src + projectdirs[i])
    for root, dirs, files in os.walk(src):
        iscopydir = False
        for i in range(0, projectlen):
            if root.find(srcprojectdirs[i]) >= 0:
                iscopydir = True
                break
        if iscopydir == False:
            continue
        #print(dirs)
        for dirname in dirs:
            dirpath = os.path.join(dst, root[src_prefix:], dirname)
            #print(dirpath)
            if os.path.exists(dirpath):
                continue
            os.makedirs(dirpath)
            

create_dirtree_without_files2md5(projectdir, md5dir)

def makedirs():
    if not os.path.exists(pbcdir):
        os.makedirs(pbcdir)
    if not os.path.exists(pbcserviceinstancedir):
        os.makedirs(pbcserviceinstancedir)
    if not os.path.exists(md5dir):
        os.makedirs(md5dir)
    if not os.path.exists(servicemethoddir):
        os.makedirs(servicemethoddir)
                         
def is_service_fileline(fileline):
    return fileline.find('service ') >= 0 and (fileline.find('{') >= 0 or fileline.find('Service') >= 0)

class md5fileinfo():
    def __init__(self):
        self.filename = ''
        self.destdir = ''
        self.md5dir = ''
        self.originalextension = ''
        self.targetextension = ''
        self.extensionfitler = ['.md5']
        self.destfilename = ''
        self.genfilename = ''
        self.filenamemd5= ''
        self.basefilename = ''
        
    def addextfilters(self, fiters):
        for f in fiters:
            self.extensionfitler.append(f)

def md5check(md5info):
    filebasename = os.path.basename(md5info.filename).replace(md5info.originalextension, md5info.targetextension)
    tomd5dir = md5info.md5dir
    if tomd5dir == '':
        tomd5dir = md5info.destdir.replace(projectdir, md5dir)
    genfilename = tomd5dir + filebasename
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

def getmd5filename(destfilename):
    return destfilename.replace(projectdir, md5dir)

class cpp():
    def __init__(self):
        self.destfilename = ''
        self.includestr = ''
        self.filemethodarray = []
        self.begunfun = None
        self.controller = ''
    def getmd5filename(self):
        return self.destfilename.replace(projectdir, md5dir)

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
    with open(cppfile.getmd5filename(), 'w', encoding='utf-8')as file:
        file.write(newstr)
        
makedirs()