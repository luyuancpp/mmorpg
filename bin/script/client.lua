function LoginRequest:ReadyGo() 
	self.account = ("luhailong"..PlayerId.guid)
	PlayerId.guid = PlayerId.guid + 1
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

function EnterGame(guid)
	request = EnterGameRequest.new()
	request.guid = guid
	request:Send()
end

function LeaveGame()
	request = LeaveGameRequest.new()
	request:Send()
end