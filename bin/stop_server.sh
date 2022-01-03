ps -ef|grep java | awk '{print deploy_server}' | xarsg -r kill -9
ps -ef|grep java | awk '{print region_server}' | xarsg -r kill -9
ps -ef|grep java | awk '{print database_server}' | xarsg -r kill -9
ps -ef|grep java | awk '{print master_server}' | xarsg -r kill -9
ps -ef|grep java | awk '{print login_server}' | xarsg -r kill -9
ps -ef|grep java | awk '{print game_server}' | xarsg -r kill -9
ps -ef|grep java | awk '{print gateway_server}' | xarsg -r kill -9
