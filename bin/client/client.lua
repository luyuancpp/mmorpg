function LoginRequest:ReadyGo() 
	self.account = ("luhailong"..PlayerId.player_id)
	PlayerId.player_id = PlayerId.player_id + 1
end
request = LoginRequest.new()
request:ReadyGo()
request:Send()

