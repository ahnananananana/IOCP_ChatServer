#include "pch.h"
#include "CChatClient.h"

#define BUF_SIZE 1024

void CChatClient::ChangeNickName(const std::string& _strNewNickName)
{
	tChangeNickName change_nick_name;
	change_nick_name.client_id = m_myClientId;
	memcpy(change_nick_name.new_nick_name, _strNewNickName.c_str(), _strNewNickName.size() + 1);
	//TODO: �� �г��� �ߺ�
	memcpy(m_mapUserData[m_myClientId].nick_name, _strNewNickName.c_str(), _strNewNickName.size() + 1);
	memcpy(m_myNickName, _strNewNickName.c_str(), _strNewNickName.size() + 1);
	
	SendPacket(change_nick_name);
}

const char* CChatClient::GetNickName(Client_ID& _client_id) const
{
	auto iter = m_mapUserData.find(_client_id);
	if (iter == m_mapUserData.end())
	{
		return nullptr;
	}
	return iter->second.nick_name;
}

CChatClient::~CChatClient()
{
	WSACleanup();
}

void CChatClient::Init()
{
	//Winsock �ʱ�ȭ
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		throw;

	//���� ����
	hSocket = socket(PF_INET, SOCK_STREAM, 0);
	if (hSocket == INVALID_SOCKET)
		throw;
}

bool CChatClient::TryConnect(const wchar_t* _pServAddr, int _iPort)
{
	//�ּ� ����
	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	InetPton(AF_INET, _pServAddr, &servAddr.sin_addr);
	servAddr.sin_port = htons(_iPort);

	//����
	if(connect(hSocket, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
		return false;

	//���ſ� ������ ����
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
	//��û id
	static Request_ID s_request_id = 0;

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

	//Response�� �����ϱ� ���� Request ����
	m_mapRequests.emplace(request.request_id, request);

	SendPacket(request);
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

		//���� �������� ���
		if (readLen == 0)
		{
			Disconnect();
			break;
		}

		EPACKET_TYPE packet_type;
		memcpy(&packet_type, message, sizeof(EPACKET_TYPE));

		switch (packet_type)
		{	
			//�ʱ�ȭ ��Ŷ�� ���
			case EPACKET_TYPE::INIT:
			{
				//�ʱ�ȭ �ڷ� ����
				tChatInit chat_init;
				chat_init.Deserialize(message, readLen);

				m_myClientId = chat_init.id;
				m_mapUserData[m_myClientId].id = m_myClientId;
				memcpy(m_mapUserData[m_myClientId].nick_name, m_myNickName, MAX_NAME_LENGTH);
				for (tUserData& user : chat_init.other_user_data)
				{
					m_mapUserData.emplace(user.id, user);
				}

				//�ʱ�ȭ �Ϸ� ��Ŷ ����
				tChatInitFinish init_finish;
				//���� �� �г��� ����
				memcpy(init_finish.nick_name, m_myNickName, MAX_NAME_LENGTH);
				SendPacket(init_finish);
				break;
			}
			case EPACKET_TYPE::CHAT_ENTER:
			{
				tChatEnter chat_enter;
				chat_enter.Deserialize(message, readLen);

				//���ο� �������� �߰�
				m_mapUserData.emplace(chat_enter.user_data.id, chat_enter.user_data);

				//���ο� ������ �����ߴ� ä�� ����
				tChat chat;
				chat.sender_id = chat_enter.user_data.id;
				chat.nick_name = chat_enter.user_data.nick_name;
				chat.chat_time = chat_enter.enter_time;
				chat.data_type = EDATA_TYPE::ENTER;

				AddNewChat(chat);
				break;
			}
			case EPACKET_TYPE::CHAT_EXIT:
			{
				tChatExit chat_exit;
				chat_exit.Deserialize(message, readLen);

				tUserData& ud = m_mapUserData[chat_exit.client_id];

				//������ �����ٴ� ä�� ����
				tChat chat;
				chat.sender_id = ud.id;
				chat.nick_name = ud.nick_name;
				chat.chat_time = chat_exit.exit_time;
				chat.data_type = EDATA_TYPE::EXIT;

				AddNewChat(chat);

				m_mapUserData.erase(chat_exit.client_id);
				break;
			}
			//��û ������ ���
			case EPACKET_TYPE::CHAT_RESPONSE:
			{
				tChatResponse response;
				response.Deserialize(message, readLen);

				//��ȿ���� �˻�
				auto iter = m_mapRequests.find(response.request_id);
				if (iter == m_mapRequests.end())
				{
					throw;
				}

				tChatRequest& request = iter->second;
				tChat chat;
				chat.is_mine = true;
				chat.chat_time = response.chat_time;
				chat.data_type = request.data_type;
				chat.sender_id = m_myClientId;
				chat.data = std::move(request.data);

				m_mapRequests.erase(iter);

				//�����س��� �� ä���� �߰�
				AddNewChat(chat);
				break;
			}
			//���ο� ä�� ����
			case EPACKET_TYPE::CHAT_BROADCAST:
			{
				tChatBroadcast broadcast;
				broadcast.Deserialize(message, readLen);

				//�����ϴ� ä�� ����
				tChat chat;
				chat.chat_time = broadcast.chat_time;
				chat.data_type = broadcast.data_type;
				chat.nick_name = m_mapUserData[broadcast.sender_id].nick_name;
				chat.sender_id = broadcast.sender_id;
				chat.data = std::move(broadcast.data);

				AddNewChat(chat);
				break;
			}
			case EPACKET_TYPE::CHANGE_NICK_NAME:
			{
				tChangeNickName change_nick_name;
				change_nick_name.Deserialize(message, readLen);

				auto iter = m_mapUserData.find(change_nick_name.client_id);
				if (iter == m_mapUserData.end())
				{
					throw;
				}
				tUserData& ud = m_mapUserData[change_nick_name.client_id];
				memcpy(ud.nick_name, change_nick_name.new_nick_name, MAX_NAME_LENGTH);
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

void CChatClient::AddNewChat(tChat& _newChat)
{
	std::lock_guard<std::mutex> lg(m_mtx);
	m_listNewChats.emplace_back(_newChat);
}

void CChatClient::Update()
{
}