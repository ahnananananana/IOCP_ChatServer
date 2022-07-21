#pragma once
enum class EDATA_TYPE
{
	INVAILD,
	STRING,
	IMAGE,
	FILE,
};

enum class EPACKET_TYPE
{
	INVAILD,
	INIT,
	CHAT_REQUEST,
	CHAT_RESPONSE,
	CHAT_BROADCAST,
};

using Client_ID = uint64_t;
using Request_ID = uint64_t;

#define BUF_SIZE 1024

struct tIOData
{
	OVERLAPPED overlapped;
	WSABUF wsaBuf;
	char buffer[BUF_SIZE];
	int rwMode;

	tIOData()
	{
		wsaBuf.buf = buffer;
	}
};


struct tUserData
{
	Client_ID id;
	std::string nick_name;
};

struct tChat
{
	EDATA_TYPE data_type;
	Client_ID sender_id;
	std::time_t chat_time;
	std::vector<char> data;
};

struct tChatRequest 
{
	EPACKET_TYPE packet_type = EPACKET_TYPE::CHAT_REQUEST;
	EDATA_TYPE data_type;
	Request_ID request_id;
	std::vector<char> data;

	int Serialize(char* _pBuf)
	{
		int header_len = sizeof(tChatRequest) - sizeof(data);
		memcpy(_pBuf, this, header_len);
		memcpy(_pBuf + header_len, data.data(), data.size());
		return header_len + data.size();
	}

	void Deserialize(const char* _pBuf, int _iBufLen)
	{
		int header_len = sizeof(tChatRequest) - sizeof(tChatRequest::data);
		memcpy(this, _pBuf, header_len);
		_pBuf += header_len;

		int data_len = _iBufLen - header_len;
		data.resize(data_len);
		memcpy(data.data(), _pBuf, data_len);
	}
};

struct tChatResponse 
{
	EPACKET_TYPE packet_type = EPACKET_TYPE::CHAT_RESPONSE;
	Request_ID request_id;
	std::time_t chat_time;

	int Serialize(char* _pBuf)
	{
		memcpy(_pBuf, this, sizeof(tChatResponse));
		return sizeof(tChatResponse);
	}

	void Deserialize(const char* _pBuf, int _iBufLen)
	{
		memcpy(this, _pBuf, sizeof(tChatResponse));
	}
};

struct tChatBroadcast 
{
	EPACKET_TYPE packet_type = EPACKET_TYPE::CHAT_BROADCAST;
	EDATA_TYPE data_type;
	Client_ID sender_id;
	std::time_t chat_time;
	std::vector<char> data;

	int Serialize(char* _pBuf)
	{
		int header_len = sizeof(tChatBroadcast) - sizeof(data);
		memcpy(_pBuf, this, header_len);
		memcpy(_pBuf + header_len, data.data(), data.size());
		return header_len + data.size();
	}

	void Deserialize(const char* _pBuf, int _iBufLen)
	{
		int header_len = sizeof(tChatBroadcast) - sizeof(data);
		memcpy(this, _pBuf, header_len);
		_pBuf += header_len;

		int data_len = _iBufLen - header_len;
		data.resize(data_len);
		memcpy(data.data(), _pBuf, data_len);
	}
};

struct tChatInit
{
	EPACKET_TYPE packet_type = EPACKET_TYPE::INIT;
	Client_ID id;
	std::vector<tUserData> other_user_data;

	int Serialize(char* _pBuf)
	{
		int header_len = sizeof(tChatInit) - sizeof(other_user_data);
		memcpy(_pBuf, this, header_len);
		memcpy(_pBuf + header_len, other_user_data.data(), other_user_data.size() * sizeof(tUserData));
		return header_len + other_user_data.size() * sizeof(tUserData);
	}

	void Deserialize(const char* _pBuf, int _iBufLen)
	{
		int header_len = sizeof(tChatInit) - sizeof(other_user_data);
		memcpy(this, _pBuf, header_len);
		_pBuf += header_len;

		int data_len = _iBufLen - header_len;
		other_user_data.resize(data_len);
		memcpy(other_user_data.data(), _pBuf, data_len);
	}
};