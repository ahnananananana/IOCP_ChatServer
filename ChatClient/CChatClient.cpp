#include "pch.h"
#include "CChatClient.h"

#define BUF_SIZE 1024

CChatClient::~CChatClient()
{
	WSACleanup();
}

void CChatClient::Init()
{
	//Winsock 초기화
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		throw;

	//소켓 생성
	hSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (hSocket == INVALID_SOCKET)
		throw;
}

bool CChatClient::TryConnect(const wchar_t* _pServAddr, int _iPort)
{
	//주소 생성
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	InetPton(AF_INET, _pServAddr, &servAddr.sin_addr);
	servAddr.sin_port = htons(_iPort);

	//연결
	if(connect(hSocket, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
		return false;

	//수신용 쓰레드 생성
	m_receiveThread = std::jthread(&CChatClient::HandleReceiving, this, std::stop_token());
	m_receiveThread.detach();
		
	return true;
}

void CChatClient::Disconnect()
{
	if (hSocket)
	{
		closesocket(hSocket);
		hSocket = NULL;
	}
}

void CChatClient::SendChat(const EDATA_TYPE& _eDataType, const char* _pData, int _iDataLen)
{
	//요청 id
	static Request_ID s_request_id = 0;

	char buf[BUF_SIZE] = {};

	tChatRequest request;
	request.request_id = s_request_id++;
	request.data.resize(_iDataLen);
	memcpy(request.data.data(), _pData, _iDataLen);

	switch (_eDataType)
	{
		case EDATA_TYPE::STRING:
		{
			request.data_type = EDATA_TYPE::STRING;
			break;
		}
		default:
		{
			return;
		}
	}

	int bufLen = request.Serialize(buf);

	//Response에 대응하기 위해 Request 저장
	m_mapRequests.emplace(request.request_id, request);

	send(hSocket, buf, bufLen, 0);
}

std::list<tChat> CChatClient::GetNewChats()
{
	std::lock_guard<std::mutex> lg(m_mtx);
	return std::move(m_listNewChats);
}

void CChatClient::HandleReceiving(std::stop_token _stopToken)
{
	char message[BUF_SIZE] = {};
	int strLen = 0, readLen = 0;
	while (!_stopToken.stop_requested())
	{
		do
		{
			readLen += recv(hSocket, &message[readLen], BUF_SIZE - 1, 0);
		} while(readLen < strLen);

		//연결 끊어졌을 경우
		if (readLen == 0)
		{
			Disconnect();
			break;
		}

		EPACKET_TYPE packet_type;
		memcpy(&packet_type, message, sizeof(EPACKET_TYPE));

		switch (packet_type)
		{
			case EPACKET_TYPE::INIT:
			{
				break;
			}
			//요청 응답인 경우
			case EPACKET_TYPE::CHAT_RESPONSE:
			{
				tChatResponse response;
				response.Deserialize(message, readLen);

				//유효한지 검사
				auto iter = m_mapRequests.find(response.request_id);
				if (iter == m_mapRequests.end())
				{
					throw;
				}

				tChatRequest& request = iter->second;
				tChat chat;
				chat.chat_time = response.chat_time;
				chat.data_type = request.data_type;
				chat.sender_id = m_clientId;
				chat.data = std::move(request.data);

				m_mapRequests.erase(iter);

				//저장해놓은 내 채팅을 추가
				{
					std::lock_guard<std::mutex> lg(m_mtx);
					m_listNewChats.emplace_back(chat);
				}
				break;
			}
			//새로운 채팅 수신
			case EPACKET_TYPE::CHAT_BROADCAST:
			{
				tChatBroadcast broadcast;
				broadcast.Deserialize(message, readLen);

				//대응하는 채팅 생성
				tChat chat;
				chat.chat_time = broadcast.chat_time;
				chat.data_type = broadcast.data_type;
				chat.sender_id = broadcast.sender_id;
				chat.data = std::move(broadcast.data);

				{
					std::lock_guard<std::mutex> lg(m_mtx);
					m_listNewChats.emplace_back(chat);
				}
				break;
			}
			default:
			{
				throw;
			}
		}

		ZeroMemory(message, BUF_SIZE);
		readLen = 0;
	}
}

void CChatClient::Update()
{
}