#include "pch.h"
#include "CUIClientMgr.h"
#include "CChatClient.h"

void CUIClientMgr::AddNewChats(std::list<tChat>& _listNewChats)
{
    m_bIsNewChatAdded = !_listNewChats.empty();
	m_listChats.splice(m_listChats.end(), _listNewChats, _listNewChats.begin(), _listNewChats.end());
}

void CUIClientMgr::Update()
{
    static bool bOpenModal = false;

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Menu"))
        {
            if (ImGui::MenuItem("Change NickName"))
            {
                bOpenModal = true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    static bool bNickNameChanged = false;
    if (bOpenModal)
    {
        ImGui::OpenPopup("Change NickName?");
        bOpenModal = false;
        bNickNameChanged = false;
    }

    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Change NickName?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Change your nick name!\n\n");
        ImGui::Separator();

        static char buf[MAX_NAME_LENGTH] = {};
        if(!bNickNameChanged)
            memcpy(buf, m_pClient->GetNickName(), MAX_NAME_LENGTH);

        if (ImGui::InputText("##nickname_input", buf, MAX_NAME_LENGTH))
        {
            bNickNameChanged = true;
        }

        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            m_pClient->ChangeNickName(buf);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) 
        { 
            ImGui::CloseCurrentPopup(); 
        }
        ImGui::EndPopup();
    }


    static bool bWasScrollBottom = true;
    static bool p_open = true;
    static ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    if (ImGui::Begin("##full_screen", &p_open, flags))
    {
        ImGui::BeginChild("##chat_view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
        for (auto iter = m_listChats.begin(); iter != m_listChats.end(); ++iter)
        {
            tChat& chat = *iter;

            const char* nick_name = m_pClient->GetNickName(chat.sender_id);
            if (nick_name)
            {
                chat.nick_name = nick_name;
            }

            switch (chat.data_type)
            {
                case EDATA_TYPE::STRING:
                {

                    std::string str;
                    str.resize(chat.data.size() + 1);
                    memcpy(str.data(), chat.data.data(), chat.data.size());
                    
                    if (chat.is_mine)
                    {
                        float fWidth = ImGui::GetWindowContentRegionWidth();
                        float fTextWidth = ImGui::CalcTextSize(str.c_str()).x;
                        float fNickNameWidth = ImGui::CalcTextSize(chat.nick_name.c_str()).x;
                        ImGui::SetCursorPosX(fWidth - fTextWidth - 8 - fNickNameWidth);
                        ImGui::Text(str.c_str());
                        ImGui::SameLine();
                        ImGui::Text(chat.nick_name.c_str());
                    }
                    else
                    {
                        ImGui::Text(chat.nick_name.c_str());
                        ImGui::SameLine();
                        ImGui::Text(str.c_str());
                    }
                    break;
                }
                case EDATA_TYPE::ENTER:
                {
                    std::string str = chat.nick_name + ToUTF8("님이 채팅에 참가하였습니다");
                    ImGui::Text(str.c_str());
                    break;
                }
                case EDATA_TYPE::EXIT:
                {
                    std::string str = chat.nick_name + ToUTF8("님이 채팅에서 나갔습니다");
                    ImGui::Text(str.c_str());
                    break;
                }
            }
        }

        // 스크롤이 바닥인 상태에서 새로운 채팅이 추가되면 자동으로 스크롤을 바닥으로
        {
            bWasScrollBottom = (ImGui::GetScrollY() / ImGui::GetScrollMaxY()) == 1.f;
            if (m_bIsNewChatAdded && bWasScrollBottom)
            {
                ImGui::SetScrollHereY(1.f);
            }

            m_bIsNewChatAdded = false;
        }

        ImGui::EndChild();

        static char input_buf[1024] = {};
        ImGui::SetNextItemWidth(-1);

        static bool bSendChat = true;
        if (bSendChat)
        {
            ImGui::SetKeyboardFocusHere();
            bSendChat = false;
        }

        if (ImGui::InputText("##input", input_buf, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            bSendChat = true;
            m_pClient->SendChat(input_buf);
            ZeroMemory(input_buf, 1024);
        }
    }
    ImGui::End();

}