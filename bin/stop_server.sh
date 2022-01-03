ps -ef|grep deploy_server | awk '{print $2}' | xargs -r kill -9
ps -ef|grep region_server | awk '{print $2}' |  xargs -r kill -9
ps -ef|grep database_server | awk '{print $2}' |  xargs -r kill -9
ps -ef|grep master_server | awk '{print $2}' |  xargs -r kill -9
ps -ef|grep login_server | awk '{print $2}' |  xargs -r kill -9
ps -ef|grep game_server | awk '{print $2}' |  xargs -r kill -9
ps -ef|grep gateway_server | awk '{print $2}' |  xargs -r kill -9
