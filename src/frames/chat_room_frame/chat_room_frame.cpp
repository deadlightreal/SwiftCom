#include "../frames.hpp"
#include "swift_net.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <wx/event.h>
#include <wx/osx/frame.h>
#include <wx/osx/stattext.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/versioninfo.h>
#include "../../main.hpp"
#include "../../utils/net/net.hpp"

using namespace frames;

ChatRoomFrame::ChatRoomFrame(const in_addr ip_address, const uint16_t server_id) : wxFrame(wxGetApp().GetHomeFrame(), wxID_ANY, "Chat Room", wxDefaultPosition, wxSize(800, 600)), server_id(server_id), server_ip_address(ip_address)  {
    wxPanel* main_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);

    wxBoxSizer* main_sizer = new wxBoxSizer(wxHORIZONTAL);

    this->channel_list_panel = new wxPanel(main_panel);
    this->channel_list_panel->SetMinSize(wxSize(200, -1));

    main_sizer->Add(this->channel_list_panel, 0, wxEXPAND);

    main_panel->SetSizer(main_sizer);

    // Handle loading server
    in_addr public_ip = utils::net::get_public_ip();

    const char* string_ip = inet_ntoa(ip_address);

    SwiftNetClientConnection* const connection = swiftnet_create_client(string_ip, server_id);
    if (connection == NULL) {
        // Handle server not started, or any error
        return;
    }

    this->client_connection = connection;

    this->LoadServerInformation();
}

ChatRoomFrame::~ChatRoomFrame() {

}

void ChatRoomFrame::DrawChannels() {
    this->channel_list_panel->DestroyChildren();

    wxBoxSizer* list_sizer = new wxBoxSizer(wxVERTICAL);

    for (auto &channel : *this->GetChatChannels()) {
        widgets::Button* channel_button = new widgets::Button(this->channel_list_panel, channel->GetName(), [this, channel](wxMouseEvent& event){
            if (this->GetChatPanel() != nullptr) {
                delete this->GetChatPanel();

                this->chat_panel = nullptr;
            }

            this->chat_panel = new ChatPanel(channel->GetId(), this->GetServerId(), this, this->GetServerIpAddress());
        });
        channel_button->SetMinSize(wxSize(-1, 30));
        channel_button->SetMaxSize(wxSize(-1, 30));

        list_sizer->Add(channel_button, 0, wxEXPAND);
    }

    this->channel_list_panel->SetSizer(list_sizer);

    this->channel_list_panel->Layout();
    this->channel_list_panel->Refresh();
    this->channel_list_panel->SendSizeEvent();
}

void ChatRoomFrame::HandleLoadServerInfoResponse(SwiftNetClientPacketData* packet_data) {
    RequestInfo* request_info = (RequestInfo*)swiftnet_client_read_packet(packet_data, sizeof(RequestType));
    if (request_info->request_type != LOAD_SERVER_INFORMATION) {
        return;
    }

    LoadServerInformationResponse* response = (LoadServerInformationResponse*)swiftnet_client_read_packet(packet_data, packet_data->metadata.data_length - sizeof(RequestType));

    printf("loading\n");

    if (response->status != SUCCESS) {
        return;
    }

    printf("loading channels\n");

    for (uint32_t i = 0; i < response->server_chat_channels_size; i++) {
        printf("loading channel\n");

        auto channel = response->server_chat_channels[i];

        this->GetChatChannels()->push_back(new ChatChannel(channel.id, channel.name));
    }

    this->DrawChannels();
}

std::vector<ChatRoomFrame::ChatChannel*>* ChatRoomFrame::GetChatChannels() {
    return &this->chat_channels;
}

void ChatRoomFrame::LoadServerInformation() {
    SwiftNetClientConnection* const connection = this->GetConnection();

    const RequestInfo request_info = {
        .request_type = LOAD_SERVER_INFORMATION
    };

    SwiftNetPacketBuffer buffer = swiftnet_client_create_packet_buffer(sizeof(request_info));

    swiftnet_client_append_to_packet(&request_info, sizeof(request_info), &buffer);

    SwiftNetClientPacketData* const packet_data = swiftnet_client_make_request(connection, &buffer);
    if (packet_data == NULL) {
        return;
    }

    this->HandleLoadServerInfoResponse(packet_data);

    swiftnet_client_destroy_packet_buffer(&buffer);
}

ChatRoomFrame::ChatPanel* ChatRoomFrame::GetChatPanel() {
    return this->chat_panel;
}

in_addr ChatRoomFrame::GetServerIpAddress() {
    return this->server_ip_address;
}

uint16_t ChatRoomFrame::GetServerId() {
    return this->server_id;
}


SwiftNetClientConnection* ChatRoomFrame::GetConnection() {
    return this->client_connection;
}
