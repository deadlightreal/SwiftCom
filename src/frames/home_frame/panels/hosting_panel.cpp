#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <unistd.h>
#include <vector>
#include <wx/event.h>
#include <wx/sizer.h>
#include <wx/wx.h>
#include "../../../main.hpp"
#include "../../../utils/net/net.hpp"
#include "../../../utils/crypto/crypto.hpp"
#include "panels.hpp"
#include "../../../widgets/widgets.hpp"

using namespace frames::home_frame::panels;

HostingPanel::HostingPanel(wxPanel* parent_panel) : wxPanel(parent_panel) {
    widgets::Button* create_new_server_button = new widgets::Button(this, "Create New Server", [this](wxMouseEvent& event) { this->CreateNewServer(event); });
    create_new_server_button->SetMinSize(wxSize(-1, 40));
    create_new_server_button->SetMaxSize(wxSize(-1, 40));

    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* main_sizer_margin = new wxBoxSizer(wxHORIZONTAL);

    this->hosted_servers_panel = new wxPanel(this);

    wxBoxSizer* manage_all_servers_sizer = new wxBoxSizer(wxHORIZONTAL);

    widgets::Button* start_all_servers_button = new widgets::Button(this, "Start All", [this](wxMouseEvent& event) {
        for (auto &server : *this->GetHostedServers()) {
            if (server.GetServerStatus() != objects::STOPPED) {
                continue;
            }

            server.StartServer();
        }
    });
    
    start_all_servers_button->SetMinSize(wxSize(-1, 40));
    start_all_servers_button->SetMaxSize(wxSize(-1, 40));

    widgets::Button* stop_all_servers_button = new widgets::Button(this, "Stop All", [this](wxMouseEvent& event) {
        for (auto &server : *wxGetApp().GetHomeFrame()->GetHostingPanel()->GetHostedServers()) {
            if (server.GetServerStatus() != objects::RUNNING) {
                continue;
            }

            server.StopServer();
        }
    });
    
    stop_all_servers_button->SetMinSize(wxSize(-1, 40));
    stop_all_servers_button->SetMaxSize(wxSize(-1, 40));

    manage_all_servers_sizer->Add(start_all_servers_button, 1, wxEXPAND);
    manage_all_servers_sizer->Add(stop_all_servers_button, 1, wxEXPAND);

    main_sizer->Add(this->hosted_servers_panel, 0, wxTOP | wxBOTTOM | wxEXPAND, 10);
    main_sizer->Add(manage_all_servers_sizer, 0, wxBOTTOM | wxEXPAND, 10);
    main_sizer->Add(create_new_server_button, 0, wxTOP | wxBOTTOM | wxEXPAND, 10);

    main_sizer_margin->AddStretchSpacer(2);
    main_sizer_margin->Add(main_sizer, 10, wxEXPAND);
    main_sizer_margin->AddStretchSpacer(2);

    this->SetSizer(main_sizer_margin);

    // Load hosted servers
    std::vector<objects::Database::HostedServerRow>* hosted_servers = wxGetApp().GetDatabase()->SelectHostedServers(std::nullopt);

    auto stored_hosted_servers = this->GetHostedServers();

    for (auto &server : *hosted_servers) {
        stored_hosted_servers->push_back(objects::HostedServer(server.server_id));
    }

    free(hosted_servers);
}

HostingPanel::~HostingPanel() {

}

void HostingPanel::DrawServers() {
    this->hosted_servers_panel->DestroyChildren();

    wxBoxSizer* v_sizer = new wxBoxSizer(wxVERTICAL);

    this->hosted_servers_panel->SetSizer(v_sizer);

    in_addr public_ip_address = utils::net::get_public_ip();

    for (auto &server : *this->GetHostedServers()) {
        uint16_t server_id = server.GetServerId();

        std::vector<uint8_t> invitation_data;
        invitation_data.resize(sizeof(public_ip_address) + sizeof(server_id));

        memcpy(invitation_data.data(), &public_ip_address, sizeof(public_ip_address));
        memcpy(invitation_data.data() + sizeof(public_ip_address), &server_id, sizeof(server_id));

        std::string invitation_code = utils::crypto::base32_encode(invitation_data);

        std::string server_button_string = std::to_string(server_id) + "   " + invitation_code;

        widgets::StyledPanel* server_panel = new widgets::StyledPanel(this->hosted_servers_panel);
        server_panel->SetMinSize(wxSize(-1, 30));
        server_panel->SetMaxSize(wxSize(-1, 30));

        widgets::Button* settings_server_button = new widgets::Button(server_panel, "Settings", [this, &server, server_id, public_ip_address](wxMouseEvent& event){ 
            auto server_settings_frame = new frames::ServerSettingsFrame(server_id, public_ip_address.s_addr);

            server_settings_frame->Show(true);
        });
        settings_server_button->SetMinSize(wxSize(-1, 30));
        settings_server_button->SetMaxSize(wxSize(-1, 30));

        widgets::Button* start_server_button = new widgets::Button(server_panel, server.GetServerStatus() == objects::RUNNING ? "Stop" : "Start", [this, &server](wxMouseEvent& event){ server.GetServerStatus() == objects::RUNNING ? server.StopServer() : server.StartServer(); });
        start_server_button->SetMinSize(wxSize(-1, 30));
        start_server_button->SetMaxSize(wxSize(-1, 30));

        wxBoxSizer* button_sizer = new wxBoxSizer(wxHORIZONTAL);

        wxStaticText* button_invitation_code_text = new wxStaticText(server_panel, wxID_ANY, invitation_code);

        button_sizer->Add(button_invitation_code_text, 4, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);
        button_sizer->AddStretchSpacer(1);
        button_sizer->Add(settings_server_button, 4, wxALIGN_CENTER_VERTICAL);
        button_sizer->Add(start_server_button, 4, wxALIGN_CENTER_VERTICAL);

        server_panel->SetSizer(button_sizer);

        v_sizer->Add(server_panel, 4, wxEXPAND | wxTOP | wxBOTTOM, 5);
    }

    this->hosted_servers_panel->GetParent()->Layout();
    this->hosted_servers_panel->GetParent()->Refresh();
}

void HostingPanel::CreateNewServer(wxMouseEvent&) {
    uint16_t random_generated_id = rand();

    wxGetApp().GetDatabase()->InsertHostedServer(random_generated_id);

    this->InsertHostedServer(random_generated_id);

    this->DrawServers();
}

void HostingPanel::InsertHostedServer(const uint16_t server_id) {
    this->GetHostedServers()->push_back(objects::HostedServer(server_id));
}

objects::HostedServer* HostingPanel::GetServerById(const uint16_t server_id) {
    for (auto &server : *this->GetHostedServers()) {
        if (server.GetServerId() == server_id) {
            return &server;
        }
    }

    return nullptr;
}

std::vector<objects::HostedServer>* HostingPanel::GetHostedServers() {
    return &this->hosted_servers;
}
