
controller_file_prefix = 'controller_'
gs_file_prefix = 'gs_'
rg_file_prefix = 'lobby_'

def is_server_proto(filename):
    return (filename.find('client_player') < 0 and filename.find('server_player') < 0) or  filename.find(rg_file_prefix) >= 0 

def is_gs_and_controller_server_proto(filename):
    return filename.find('client_player') < 0 and filename.find('server_player') < 0 and  filename.find(rg_file_prefix) < 0  

def is_client_proto(filename):
    return filename.find('client_player') >= 0 

def is_not_client_proto(filename):
    return is_client_proto(filename)  == False