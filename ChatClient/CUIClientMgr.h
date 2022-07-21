#pragma once
#include <CImguiMgr.h>

class CChatClient;

class CUIClientMgr :
    public CImguiMgr
{
    std::list<tChat> m_listChats;

public:
    CChatClient* m_pClient;

    void AddNewChats(std::list<tChat>& _vecNewChats);

protected:
    void Update() override;
};

