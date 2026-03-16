ps -ef | grep scene | grep -v grep | awk '{print $2}' | xargs kill -9
ps -ef | grep gate | grep -v grep | awk '{print $2}' | xargs kill -9
