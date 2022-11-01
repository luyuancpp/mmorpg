start deploy_server.exe
ping /n 1 127.1 >nul
start region_server.exe
ping /n 1 127.1 >nul
start database_server.exe
ping /n 1 127.1 >nul
start controller_server.exe
ping /n 1 127.1 >nul
start login_server.exe
ping /n 1 127.1 >nul
start game_server.exe
ping /n 1 127.1 >nul
start gateway_server.exe