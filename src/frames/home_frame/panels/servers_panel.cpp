#include "panels.hpp"
#include <arpa/inet.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <wx/utils.h>
#include <wx/wx.h>
#include "../../../utils/net/net.hpp"
#include "../../../widgets/widgets.hpp"
#include "../../../utils/crypto/crypto.hpp"
#include <swift_net.h>
#include "../../../main.hpp"

using frames::home_frame::panels::ServersPanel;

ServersPanel::ServersPanel(wxPanel* parent_panel) : wxPanel(parent_panel) {
    widgets::Button* add_server_button = new widgets::Button(this, "Add Server", [this](wxMouseEvent& event) { this->OpenAddServerPopupMenu(event, this); } );
    add_server_button->SetMinSize(wxSize(-1, 40));
    add_server_button->SetMaxSize(wxSize(-1, 40));

    this->joined_servers_panel = new wxPanel(this);

    wxBoxSizer* main_vert_sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* main_sizer_margin = new wxBoxSizer(wxHORIZONTAL);

    main_vert_sizer->Add(this->joined_servers_panel, 4, wxTOP | wxBOTTOM | wxEXPAND, 10);
    main_vert_sizer->Add(add_server_button, 4, wxTOP | wxBOTTOM | wxEXPAND, 10);

    main_sizer_margin->AddStretchSpacer(2);
    main_sizer_margin->Add(main_vert_sizer, 10, wxEXPAND);
    main_sizer_margin->AddStretchSpacer(2);

    this->SetSizer(main_sizer_margin);

    // Load joined servers
    std::vector<objects::Database::JoinedServerRow>* joined_servers = wxGetApp().GetDatabase()->SelectJoinedServers();

    auto stored_joined_servers = this->GetJoinedServers();

    for (auto &server : *joined_servers) {
        stored_joined_servers->push_back(objects::JoinedServer(server.server_id, server.ip_address));
    }

    free(joined_servers);
}

ServersPanel::~ServersPanel() {

}

void ServersPanel::DrawServers() {
    this->joined_servers_panel->DestroyChildren();

    wxBoxSizer* v_sizer = new wxBoxSizer(wxVERTICAL);

    this->joined_servers_panel->SetSizer(v_sizer);

    for (auto &server : *wxGetApp().GetHomeFrame()->GetServersPanel()->GetJoinedServers()) {
        uint16_t server_id = server.GetServerId();
        in_addr server_ip_address = server.GetServerIpAddress();

        const char* server_ip_address_parsed = inet_ntoa(server_ip_address);

        widgets::StyledPanel* server_panel = new widgets::StyledPanel(this->joined_servers_panel);
        server_panel->SetMinSize(wxSize(-1, 30));
        server_panel->SetMaxSize(wxSize(-1, 30));

        widgets::Button* start_server_button = new widgets::Button(server_panel, "Enter Server", [this, &server](wxMouseEvent& event){
            frames::ChatRoomFrame* chat_room_frame = new frames::ChatRoomFrame(server.GetServerIpAddress(), server.GetServerId());
            chat_room_frame->Show(true);

            wxGetApp().AddChatRoomFrame(chat_room_frame);
        });
        start_server_button->SetMinSize(wxSize(-1, 30));
        start_server_button->SetMaxSize(wxSize(-1, 30));

        wxBoxSizer* button_sizer = new wxBoxSizer(wxHORIZONTAL);

        std::string joined_server_text_string = std::string(server_ip_address_parsed) + "   " + std::to_string(server_id);

        wxStaticText* joined_server_text = new wxStaticText(server_panel, wxID_ANY, wxString(joined_server_text_string));

        button_sizer->Add(joined_server_text, 4, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);
        button_sizer->AddStretchSpacer(1);
        button_sizer->Add(start_server_button, 4, wxALIGN_CENTER_VERTICAL);

        server_panel->SetSizer(button_sizer);

        v_sizer->Add(server_panel, 4, wxTOP | wxBOTTOM | wxEXPAND, 5);
    }

    this->joined_servers_panel->GetParent()->Layout();
    this->joined_servers_panel->GetParent()->Refresh();
    this->joined_servers_panel->GetParent()->SendSizeEvent();
}

void ServersPanel::OpenAddServerPopupMenu(wxMouseEvent& event, wxWindow* parent) {
    AddServerPopupMenu* popup_menu = new AddServerPopupMenu(parent->MacGetTopLevelWindow(), wxDefaultPosition);

    popup_menu->CentreOnParent();
    popup_menu->ShowModal();
    popup_menu->Destroy();
}

std::vector<objects::JoinedServer>* ServersPanel::GetJoinedServers() {
    return &this->joined_servers;
}
