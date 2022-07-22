#include "pch.h"
#include "CChatServer.h"

CChatServer::~CChatServer()
{
	m_connectionThread.request_stop();
	for (auto& t : m_vecWorkerThreads)
	{
		t.request_stop();
	}

	for (auto d : m_setUsingIODatas)
	{
		delete d;
	}

	for (auto d : m_vecCapableIODatas)
	{
		delete d;
	}

	WSACleanup();
}

void CChatServer::Init()
{
	//Winsock init
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		throw;
	}
	
	//IOCP 생성
	hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	//Worker 쓰레드 생성
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	for (int i = 0; i < sysInfo.dwNumberOfProcessors; ++i)
	{
		m_vecWorkerThreads.emplace_back(&CChatServer::HandleIO, this);
		m_vecWorkerThreads.back().detach();
	}

	//미리 IOData 생성
	m_vecCapableIODatas.reserve(m_vecWorkerThreads.size());
	for (int i = 0; i < m_vecWorkerThreads.size(); ++i)
	{
		m_vecCapableIODatas.emplace_back(new tIOData);
	}

	//소켓 생성
	hServSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	//서버 주소 초기화
	SOCKADDR_IN servAddr;
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(50000);

	//소켓 포트 바인딩
	bind(hServSock, (SOCKADDR*)&servAddr, sizeof(servAddr));
	listen(hServSock, 5);

	//Connetion용 쓰레드 생성
	m_connectionThread = std::jthread(&CChatServer::HandleConnection, this);
	m_connectionThread.detach();
}

void CChatServer::HandleConnection()
{
	std::stop_token _token = m_connectionThread.get_stop_token();
	SOCKADDR_IN clntAddr;
	int iAddrLen = sizeof(clntAddr);

	while (!_token.stop_requested())
	{
		//소켓 Accept
		SOCKET hClntSock = accept(hServSock, (SOCKADDR*)&clntAddr, &iAddrLen);
		if(_token.stop_requested())
			break;

		//새로운 클라이언트 정보 생성
		m_client_data_mtx.lock();
		tClientData& client_data = m_mapClientData[hClntSock];
		m_client_data_mtx.unlock();
		client_data.user_data.id = hClntSock;
		client_data.socket = hClntSock;

		//소켓과 IOCP 연결
		CreateIoCompletionPort((HANDLE)hClntSock, hIOCP, (ULONG_PTR)&client_data, 0);

		//해당 소켓 수신 시작
		ReceivePacket(hClntSock, GetNewIOData());

		//새로운 클라이언트에게 채팅방에 있는 유저 정보와 ID 등 초기화에 필요한 데이터 전송
		tChatInit chat_init;
		chat_init.id = client_data.user_data.id;
		{
			std::lock_guard<std::mutex> lg(m_client_data_mtx);
			for (auto& pair : m_mapClientData)
			{
				tClientData& cd = pair.second;
				if (cd.user_data.id == client_data.user_data.id)
					continue;
				chat_init.other_user_data.emplace_back(cd.user_data);
			}
		}

		tIOData* pIOData = GetNewIOData();
		pIOData->wsaBuf.len = chat_init.Serialize(pIOData->buffer);
		SendPacket(client_data.socket, pIOData);
	}
}

void CChatServer::HandleIO()
{
	DWORD data_len;
	tClientData* pSenderData;
	tIOData* pIOData;

	while (true)
	{
		//IOCP 완료 대기
		GetQueuedCompletionStatus(hIOCP, &data_len, (PULONG_PTR)&pSenderData, (LPOVERLAPPED*)&pIOData, INFINITE);

		//전달받은 소켓
		SOCKET sock = pSenderData->socket;

		//유효한지 검사
		m_client_data_mtx.lock();
		auto iter = m_mapClientData.find(pSenderData->user_data.id);
		if (iter == m_mapClientData.end())
		{
			m_client_data_mtx.unlock();
			throw;
		}
		m_client_data_mtx.unlock();
		tClientData& client_data = iter->second;

		//데이터 수신
		if (pIOData->rwMode == READ)
		{
			//연결 해제인 경우
			if (data_len == 0)
			{
				m_client_data_mtx.lock();
				closesocket(sock);
				ReturnIOData(pIOData);

				tChatExit chat_exit;
				chat_exit.client_id = client_data.user_data.id;
				for (const auto& pair : m_mapClientData)
				{
					const tClientData& cd = pair.second;
					
					tIOData* pIOData = GetNewIOData();
					pIOData->wsaBuf.len = chat_exit.Serialize(pIOData->buffer);
					SendPacket(cd.socket, pIOData);
				}
				m_mapClientData.erase(iter);
				m_client_data_mtx.unlock();
				continue;
			}

			//현재 시간 측정
			std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			
			//패킷 종류에 따라 처리
			EPACKET_TYPE packet_type;
			char* buf = pIOData->wsaBuf.buf;
			memcpy(&packet_type, buf, sizeof(EPACKET_TYPE));

			switch (packet_type)
			{
			case EPACKET_TYPE::INIT_FINISH:
			{
				tChatInitFinish init_finish;
				init_finish.Deserialize(buf, data_len);

				memcpy(client_data.user_data.nick_name, init_finish.nick_name, MAX_NAME_LENGTH);

				//다른 유저들에게 새로운 유저 정보 전파
				tChatEnter chat_enter;
				chat_enter.user_data = client_data.user_data;
				chat_enter.enter_time = now;

				m_client_data_mtx.lock();
				for (auto pair : m_mapClientData)
				{
					tClientData& cd = pair.second;
					if (cd.user_data.id == client_data.user_data.id)
					{
						continue;
					} 

					tIOData* pIOData = GetNewIOData();
					pIOData->wsaBuf.len = chat_enter.Serialize(pIOData->wsaBuf.buf);
					SendPacket(cd.socket, pIOData);
				}
				m_client_data_mtx.unlock();
				break;
			}
				//채팅이 온 경우
			case EPACKET_TYPE::CHAT_REQUEST:
			{
				//채팅 역직렬화
				tChatRequest request;
				request.Deserialize(buf, data_len);

				//응답 패킷 전송
				{
					tChatResponse response;

					response.request_id = request.request_id;
					response.chat_time = now;

					pIOData->wsaBuf.len = response.Serialize(buf);

					SendPacket(sock, pIOData);
				}

				//다른 유저들에게 채팅 전파
				{
					tChatBroadcast broadcast;
					broadcast.data_type = request.data_type;
					broadcast.sender_id = pSenderData->user_data.id;
					broadcast.data = std::move(request.data);
					broadcast.chat_time = now;

					m_client_data_mtx.lock();
					for (auto& pair : m_mapClientData)
					{
						tClientData& cd = pair.second;

						if(cd.user_data.id == pSenderData->user_data.id)
							continue;

						tIOData* pIOData = GetNewIOData();
						
						pIOData->wsaBuf.len = broadcast.Serialize(pIOData->wsaBuf.buf);

						SendPacket(cd.socket, pIOData);
					}
					m_client_data_mtx.unlock();
				}
				break;
			}
			case EPACKET_TYPE::CHANGE_NICK_NAME:
			{
				tChangeNickName change_nick_name;
				change_nick_name.Deserialize(buf, data_len);

				{
					std::lock_guard<std::mutex> lg(m_client_data_mtx);
					auto iter = m_mapClientData.find(change_nick_name.client_id);
					if (iter == m_mapClientData.end())
					{
						throw;
					}
					tClientData& cd = m_mapClientData[change_nick_name.client_id];
					memcpy(cd.user_data.nick_name, change_nick_name.new_nick_name, MAX_NAME_LENGTH);

					for (const auto& pair : m_mapClientData)
					{
						const tClientData& cd = pair.second;
						if(cd.user_data.id == change_nick_name.client_id)
							continue;

						tIOData* pIOData = GetNewIOData();

						pIOData->wsaBuf.len = change_nick_name.Serialize(pIOData->buffer);
						SendPacket(cd.socket, pIOData);
					}
				}

				break;
			}
			default:
			{
				throw;
			}
			}

			ReceivePacket(sock, GetNewIOData());
		}
		//데이터 송신
		else
		{
			OutputDebugString(L"Send\n");

			ReturnIOData(pIOData);
		}
	}
}

tIOData* CChatServer::GetNewIOData()
{
	tIOData* pIOData = nullptr;

	{
		std::lock_guard<std::mutex> lg(m_capable_io_mtx);
		if (!m_vecCapableIODatas.empty())
		{
			pIOData = m_vecCapableIODatas.back();
			m_vecCapableIODatas.pop_back();
			{
				std::lock_guard<std::mutex> lg(m_using_io_mtx);
				m_setUsingIODatas.emplace(pIOData);
			}
		}
	}

	if (!pIOData)
	{
		pIOData = new tIOData;
		std::lock_guard<std::mutex> lg(m_using_io_mtx);
		m_setUsingIODatas.emplace(pIOData);
	}

	return pIOData;
}

void CChatServer::ReturnIOData(tIOData* _pIOData)
{
	{
		std::lock_guard<std::mutex> lg1(m_capable_io_mtx);
		std::lock_guard<std::mutex> lg2(m_using_io_mtx);
		m_vecCapableIODatas.emplace_back(_pIOData);
		m_setUsingIODatas.erase(_pIOData);
	}
}

void CChatServer::SendPacket(SOCKET _socket, tIOData* _pIOData)
{
	memset(&(_pIOData->overlapped), 0, sizeof(OVERLAPPED));
	_pIOData->rwMode = WRITE;
	WSASend(_socket, &(_pIOData->wsaBuf), 1, NULL, 0, &(_pIOData->overlapped), NULL);
}

void CChatServer::ReceivePacket(SOCKET _socket, tIOData* _pIOData)
{
	memset(&(_pIOData->overlapped), 0, sizeof(OVERLAPPED));
	_pIOData->wsaBuf.len = BUF_SIZE;
	_pIOData->rwMode = READ;
	DWORD flags = 0;
	WSARecv(_socket, &(_pIOData->wsaBuf), 1, NULL, &flags, &(_pIOData->overlapped), NULL);
}

void CChatServer::Update()
{
}
