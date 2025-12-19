#include "panels.hpp"
#include <arpa/inet.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <wx/event.h>
#include <wx/osx/core/colour.h>
#include <wx/timer.h>
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

    main_vert_sizer->Add(this->joined_servers_panel, wxSizerFlags(4).Expand().Border(wxTOP | wxBOTTOM, 10));
    main_vert_sizer->Add(add_server_button, wxSizerFlags(4).Expand().Border(wxTOP | wxBOTTOM, 10));

    main_sizer_margin->AddStretchSpacer(1);
    main_sizer_margin->Add(main_vert_sizer, wxSizerFlags(5).Expand());
    main_sizer_margin->AddStretchSpacer(1);

    this->SetSizerAndFit(main_sizer_margin);

    this->RefreshServerInfo();

    this->refresh_servers_timer = new wxTimer(this, wxID_ANY);

    this->refresh_servers_timer->Start(5000);

    Bind(wxEVT_TIMER, [this](wxTimerEvent& event){this->RefreshServerInfo(); this->DrawServers();}, wxID_ANY);
}

ServersPanel::~ServersPanel() = default;

void ServersPanel::RefreshServerInfo() {
    // Load joined servers
    std::vector<objects::Database::JoinedServerRow>* joined_servers = wxGetApp().GetDatabase()->SelectJoinedServers(std::nullopt, std::nullopt, std::nullopt);

    auto stored_joined_servers = this->GetJoinedServers();

    stored_joined_servers->clear();
    stored_joined_servers->reserve(joined_servers->size() * sizeof(objects::Database::JoinedServerRow));

    for (auto &server : *joined_servers) {
        printf("Joined server: %s %d\n", inet_ntoa(server.ip_address), server.server_id);
        SwiftNetClientConnection* const client = swiftnet_create_client(inet_ntoa(server.ip_address), server.server_id, DEFAULT_TIMEOUT_CLIENT_CREATION);
        if (client == nullptr) {
            stored_joined_servers->push_back(objects::JoinedServer(server.server_id, server.ip_address, objects::JoinedServer::ServerStatus::OFFLINE, false));

            continue;
        }

        SwiftNetPacketBuffer buffer = swiftnet_client_create_packet_buffer(sizeof(requests::LoadJoinedServerDataRequest) + sizeof(RequestInfo));

        const RequestInfo request_info = {
            .request_type = RequestType::LOAD_JOINED_SERVER_DATA
        };

        const requests::LoadJoinedServerDataRequest request = {
        };

        swiftnet_client_append_to_packet(&request_info, sizeof(request_info), &buffer);
        swiftnet_client_append_to_packet(&request, sizeof(request), &buffer);

        SwiftNetClientPacketData* response = swiftnet_client_make_request(client, &buffer, DEFAULT_TIMEOUT_REQUEST);
        if (response == nullptr) {
            swiftnet_client_destroy_packet_buffer(&buffer);

            stored_joined_servers->push_back(objects::JoinedServer(server.server_id, server.ip_address, objects::JoinedServer::ServerStatus::OFFLINE, false));

            continue;
        }

        swiftnet_client_destroy_packet_buffer(&buffer);

        ResponseInfo* const response_info = (ResponseInfo*)swiftnet_client_read_packet(response, sizeof(ResponseInfo));
        responses::LoadJoinedServerDataResponse* const server_data = (responses::LoadJoinedServerDataResponse*)swiftnet_client_read_packet(response, sizeof(responses::LoadJoinedServerDataResponse));

        std::cout << "Admin: " << server_data->admin << std::endl;

        stored_joined_servers->push_back(objects::JoinedServer(server.server_id, server.ip_address, objects::JoinedServer::ServerStatus::ONLINE, server_data->admin));

        swiftnet_client_destroy_packet_data(response, client);

        swiftnet_client_cleanup(client);
    }

    free(joined_servers);
}

void ServersPanel::DrawServers() {
    this->joined_servers_panel->DestroyChildren();

    wxBoxSizer* v_sizer = new wxBoxSizer(wxVERTICAL);

    this->joined_servers_panel->SetSizer(v_sizer);

    for (auto &server : *this->GetJoinedServers()) {
        uint16_t server_id = server.GetServerId();
        in_addr server_ip_address = server.GetServerIpAddress();

        const char* server_ip_address_parsed = inet_ntoa(server_ip_address);

        widgets::StyledPanel* server_panel = new widgets::StyledPanel(this->joined_servers_panel);
        server_panel->SetMinSize(wxSize(-1, 30));
        server_panel->SetMaxSize(wxSize(-1, 30));

        widgets::Circle* status = new widgets::Circle(server_panel, server.GetServerStatus() == objects::JoinedServer::ServerStatus::ONLINE ? wxColour(0, 255, 0) : wxColour(255, 0, 0));
        status->SetMinSize(wxSize(30, 30));

        widgets::Button* start_server_button = new widgets::Button(server_panel, "Enter Server", [this, &server](wxMouseEvent& event){
            if (server.GetServerStatus() != objects::JoinedServer::ServerStatus::ONLINE) {
                return;
            }

            frames::ChatRoomFrame* chat_room_frame = nullptr;

            in_addr ip = server.GetServerIpAddress();

            if (server.GetServerIpAddress().s_addr == utils::net::get_public_ip().s_addr) {
                chat_room_frame = new frames::ChatRoomFrame(utils::net::get_private_ip(), server.GetServerId());
            } else {
                chat_room_frame = new frames::ChatRoomFrame(server.GetServerIpAddress(), server.GetServerId());
            }
            
            chat_room_frame->Show(true);

            wxGetApp().AddChatRoomFrame(chat_room_frame);
        });

        start_server_button->SetMinSize(wxSize(-1, 30));
        start_server_button->SetMaxSize(wxSize(-1, 30));

        wxBoxSizer* button_sizer = new wxBoxSizer(wxHORIZONTAL);

        std::string joined_server_text_string = std::string(server_ip_address_parsed) + "   " + std::to_string(server_id);

        wxStaticText* joined_server_text = new wxStaticText(server_panel, wxID_ANY, wxString(joined_server_text_string));


        button_sizer->Add(status, wxSizerFlags(0).Align(wxALIGN_CENTER_VERTICAL).Border(wxLEFT, 5));
        button_sizer->Add(joined_server_text, wxSizerFlags(4).Align(wxALIGN_CENTER_VERTICAL).Border(wxLEFT, 2));
        button_sizer->AddStretchSpacer(1);

        if (server.IsAdmin()) {
            widgets::Button* admin_button = new widgets::Button(server_panel, "Admin", [this, &server](wxMouseEvent& event){
                auto admin_frame = new frames::AdminMenuFrame(inet_ntoa(server.GetServerIpAddress()), server.GetServerId());
                admin_frame->Show(true);
            });
            admin_button->SetMinSize(wxSize(-1, 30));
            admin_button->SetMaxSize(wxSize(-1, 30));

            button_sizer->Add(admin_button, wxSizerFlags(4).Align(wxALIGN_CENTER_VERTICAL));
        }

        button_sizer->Add(start_server_button, wxSizerFlags(4).Align(wxALIGN_CENTER_VERTICAL));

        server_panel->SetSizerAndFit(button_sizer);

        v_sizer->Add(server_panel, wxSizerFlags(4).Border(wxTOP | wxBOTTOM, 5).Expand());
    }

    this->joined_servers_panel->GetParent()->Layout();
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
