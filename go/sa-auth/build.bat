goctl api go -api api/login.api -dir .
goctl model mysql ddl -src model/user.sql -dir ./model --cache true
goctl model mysql ddl -src model/user_oauth.sql -dir ./model --cache true
goctl model mysql ddl -src model/user_password.sql -dir ./model --cache true
goctl model mysql ddl -src model/user_phone.sql -dir ./model --cache true
pause