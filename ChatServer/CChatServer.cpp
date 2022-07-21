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
	
	//IOCP ����
	hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	//Worker ������ ����
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	for (int i = 0; i < sysInfo.dwNumberOfProcessors; ++i)
	{
		m_vecWorkerThreads.emplace_back(&CChatServer::HandleIO, this, std::stop_token());
		m_vecWorkerThreads.back().detach();
	}

	//�̸� IOData ����
	m_vecCapableIODatas.reserve(m_vecWorkerThreads.size());
	for (int i = 0; i < m_vecWorkerThreads.size(); ++i)
	{
		m_vecCapableIODatas.emplace_back(new tIOData);
	}

	//���� ����
	hServSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	//���� �ּ� �ʱ�ȭ
	SOCKADDR_IN servAddr;
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(50000);

	//���� ��Ʈ ���ε�
	bind(hServSock, (SOCKADDR*)&servAddr, sizeof(servAddr));
	listen(hServSock, 5);

	//Connetion�� ������ ����
	m_connectionThread = std::jthread(&CChatServer::HandleConnection, this, std::stop_token());
	m_connectionThread.detach();
}

void CChatServer::HandleConnection(std::stop_token _token)
{
	SOCKADDR_IN clntAddr;
	int iAddrLen = sizeof(clntAddr);

	while (!_token.stop_requested())
	{
		//���� Accept
		SOCKET hClntSock = accept(hServSock, (SOCKADDR*)&clntAddr, &iAddrLen);

		//���ο� Ŭ���̾�Ʈ ���� ����
		tClientData& client_data = m_mapClientData[hClntSock];
		client_data.user_data.id = hClntSock;
		client_data.socket = hClntSock;

		//���ϰ� IOCP ����
		CreateIoCompletionPort((HANDLE)hClntSock, hIOCP, (ULONG_PTR)&client_data, 0);

		//���ο� Ŭ���̾�Ʈ���� ä�ù濡 �ִ� ���� ���� �� �ʱ�ȭ�� �ʿ��� ������ ����
		/*tChatInit chat_init;
		chat_init.id = client_data.user_data.id;
		for (auto& pair : m_mapClientData)
		{
			tClientData& cd = pair.second;
			chat_init.other_user_data.emplace_back(cd.user_data);
		}

		int header_len = sizeof(tChatInit) - sizeof(tChatInit::other_user_data);
		int data_size = chat_init.other_user_data.size() * sizeof(tUserData);*/

		//chat_init.Serialize(ioInfo);

		//memmove(ioInfo->wsaBuf.buf, &chat_init, header_len);
		//memmove(ioInfo->wsaBuf.buf + header_len, chat_init.other_user_data.data(), data_size);

		//WSASend(handleInfo->hClntSock, &(ioInfo->wsaBuf), 1, NULL, 0, &(ioInfo->overlapped), NULL);
		
		//�ش� Ŭ���̾�Ʈ���� ���� ������ ���� ���
		/*tIOData* ioData = GetNewIOData();
		ioData->wsaBuf.len = BUF_SIZE;
		ioData->rwMode = READ;

		DWORD recvBytes, flags = 0;
		WSARecv(hClntSock, &(ioData->wsaBuf), 1, &recvBytes, &flags, &(ioData->overlapped), NULL);*/

		ReceivePacket(hClntSock, GetNewIOData());
	}
}

void CChatServer::HandleIO(std::stop_token _token)
{
	DWORD bytesTrans;
	tClientData* pSenderData;
	tIOData* pIOData;

	while (!_token.stop_requested())
	{
		//IOCP �Ϸ� ���
		GetQueuedCompletionStatus(hIOCP, &bytesTrans, (PULONG_PTR)&pSenderData, (LPOVERLAPPED*)&pIOData, INFINITE);

		//���޹��� ����
		SOCKET sock = pSenderData->socket;

		//��ȿ���� �˻�
		auto iter = m_mapClientData.find(pSenderData->user_data.id);
		if (iter == m_mapClientData.end())
		{
			throw;
		}

		//������ ����
		if (pIOData->rwMode == READ)
		{
			//���� ������ ���
			if (bytesTrans == 0)
			{
				m_mapClientData.erase(pSenderData->user_data.id);
				closesocket(sock);
				ReturnIOData(pIOData);
				continue;
			}

			//���� �ð� ����
			std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			
			//��Ŷ ������ ���� ó��
			EPACKET_TYPE packet_type;
			char* buf = pIOData->wsaBuf.buf;
			memcpy(&packet_type, buf, sizeof(EPACKET_TYPE));

			switch (packet_type)
			{
				//ä���� �� ���
			case EPACKET_TYPE::CHAT_REQUEST:
			{
				//ä�� ������ȭ
				tChatRequest request;
				request.Deserialize(buf, bytesTrans);

				//���� ��Ŷ ����
				{
					tChatResponse response;

					response.request_id = request.request_id;
					response.chat_time = now;

					pIOData->wsaBuf.len = response.Serialize(buf);

					SendPacket(sock, pIOData);
				}

				//�ٸ� �����鿡�� ä�� ����
				{
					tChatBroadcast broadcast;
					broadcast.data_type = request.data_type;
					broadcast.sender_id = pSenderData->user_data.id;
					broadcast.data = std::move(request.data);
					broadcast.chat_time = now;

					for (auto& pair : m_mapClientData)
					{
						tClientData& cd = pair.second;

						if(cd.user_data.id == pSenderData->user_data.id)
							continue;

						tIOData* pIOData = GetNewIOData();
						
						pIOData->wsaBuf.len = broadcast.Serialize(pIOData->wsaBuf.buf);

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
		//������ �۽�
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
