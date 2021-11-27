for /f "tokens=2,3" %%a in ('tasklist ^| find "_server.exe"') do (
	taskkill /f /pid %%a >> nul)

