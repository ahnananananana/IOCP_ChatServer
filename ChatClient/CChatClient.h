#pragma once
class CChatClient
{
	SOCKET hSocket;
	SOCKADDR_IN servAddr;
	std::mutex m_mtx;

	Client_ID m_myClientId;
	char m_myNickName[MAX_NAME_LENGTH] = "test";
	std::map<Client_ID, tUserData> m_mapUserData;

	std::jthread m_receiveThread;
	std::list<tChat> m_listNewChats;

	std::map<Request_ID, tChatRequest> m_mapRequests;

public:
	const char* GetNickName() const { return m_myNickName; }
	void ChangeNickName(const std::string& _strNewNickName);
	const char* GetNickName(Client_ID& _client_id) const;

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
	template<typename T>
	void SendPacket(T& _data)
	{
		char buf[BUF_SIZE];
		int len = _data.Serialize(buf);
		send(hSocket, buf, len, 0);
	}

	void HandleReceiving(std::stop_token _stopToken);
	void AddNewChat(tChat& _newChat);
};

