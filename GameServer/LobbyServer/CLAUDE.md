### LobbyServer
Entry: `LobbyServer/lobbyServerMain.cpp`

Matchmaking server. Manages `GameSession` objects per connected player. Starts IOCP worker thread pool (`coreCnt` threads each looping on `dispatch()`). Initializes `SocketUtils`, `Memory`, `IdPool`.