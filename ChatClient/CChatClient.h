#pragma once
class CChatClient
{
	SOCKET hSocket;
	SOCKADDR_IN servAddr;
	std::mutex m_mtx;

	Client_ID m_clientId;

	std::jthread m_receiveThread;
	std::list<tChat> m_listNewChats;
	
	std::map<Request_ID, tChatRequest> m_mapRequests;

public:
	~CChatClient();

public:
	void Init();
	void Update();
	bool TryConnect(const wchar_t* _pServAddr, int _iPort);
	void Disconnect();
	void SendChat(const EDATA_TYPE& _eDataType, const char* _pData, int _iDataLen);
	void SendChat(const std::string& _str) { SendChat(EDATA_TYPE::STRING, _str.data(), _str.size()); }
	std::list<tChat> GetNewChats();

private:
	void HandleReceiving(std::stop_token _stopToken);
};

