#pragma once

#define READ 3
#define WRITE 5

struct tClientData
{
	SOCKET socket;
	tUserData user_data;
};

class CChatServer
{
	SOCKET hServSock;
	HANDLE hIOCP;

	std::mutex m_client_data_mtx;
	std::map<Client_ID, tClientData> m_mapClientData;
	
	std::mutex m_using_io_mtx;
	std::unordered_set<tIOData*> m_setUsingIODatas;
	std::mutex m_capable_io_mtx;
	std::vector<tIOData*> m_vecCapableIODatas;

	//TODO: 메모리 누수 발생
	std::jthread m_connectionThread;
	std::vector<std::jthread> m_vecWorkerThreads;


public:
	~CChatServer();

public:
	void Init();
	void Update();

private:
	void HandleConnection(std::stop_token _token);
	void HandleIO(std::stop_token _token);
	tIOData* GetNewIOData();
	void ReturnIOData(tIOData* _pIOData);


	void SendPacket(SOCKET _socket, tIOData* _pIOData);
	void ReceivePacket(SOCKET _socket, tIOData* _pIOData);
};

