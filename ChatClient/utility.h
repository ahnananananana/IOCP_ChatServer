#pragma once

struct tChat
{
	EDATA_TYPE data_type;
	bool is_mine = false;
	Client_ID sender_id;
	std::string nick_name;
	std::time_t chat_time;
	std::vector<char> data;
};

std::string ToUTF8(const std::string& _strAnsi);