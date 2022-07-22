#pragma once
#include <CImguiMgr.h>

class CChatClient;

class CUIClientMgr :
    public CImguiMgr
{
    std::list<tChat> m_listChats;
    bool m_bIsNewChatAdded = false;

public:
    CChatClient* m_pClient;

    void AddNewChats(std::list<tChat>& _listNewChats);

protected:
    void Update() override;
};

