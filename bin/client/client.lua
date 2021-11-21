function LoginRequest:ReadyGo() 
	self.account = ("luhailong"..PlayerId.player_id)
	PlayerId.player_id = PlayerId.player_id + 1
end

function ReadyGo()
	request = LoginRequest.new()
	request:ReadyGo()
	request:Send()
end

function CreatePlayer()
	request = CreatePlayerRequest.new()
	request:Send()
end

function EnterGame(player_id)
	request = EnterGameRequest.new()
	request.player_id = player_id
	request:Send()
end

function LeaveGame()
	request = LeaveGameRequest.new()
	request:Send()
end