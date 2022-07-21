#include "pch.h"
#include "CUIClientMgr.h"
#include "CChatClient.h"

void CUIClientMgr::AddNewChats(std::list<tChat>& _vecNewChats)
{
	m_listChats.splice(m_listChats.end(), _vecNewChats, _vecNewChats.begin(), _vecNewChats.end());
}

void CUIClientMgr::Update()
{
    //if (ImGui::BeginMainMenuBar())
    //{
    //    if (ImGui::BeginMenu("File"))
    //    {
    //        ImGui::EndMenu();
    //    }
    //    if (ImGui::BeginMenu("Connection"))
    //    {
    //        if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
    //        if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
    //        ImGui::Separator();
    //        if (ImGui::MenuItem("Cut", "CTRL+X")) {}
    //        if (ImGui::MenuItem("Copy", "CTRL+C")) {}
    //        if (ImGui::MenuItem("Paste", "CTRL+V")) {}
    //        ImGui::EndMenu();
    //    }
    //    ImGui::EndMainMenuBar();
    //}


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

            switch (chat.data_type)
            {
                case EDATA_TYPE::STRING:
                {
                    std::string str;
                    str.resize(chat.data.size() + 1);
                    memcpy(str.data(), chat.data.data(), chat.data.size());
                    
                    ImGui::Text(std::to_string(chat.sender_id).c_str());
                    ImGui::SameLine();
                    ImGui::Text(str.c_str());

                    break;
                }
            }
        }
        ImGui::EndChild();

        static char input_buf[1024] = {};
        ImGui::SetNextItemWidth(-1);
        ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("##input", input_buf, 1024, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            m_pClient->SendChat(input_buf);
            ZeroMemory(input_buf, 1024);
        }
    }
    ImGui::End();
}
