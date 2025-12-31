#include "../frames.hpp"
#include <swift_net.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <wx/event.h>
#include <wx/osx/frame.h>
#include <wx/osx/stattext.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/versioninfo.h>
#include "../../main.hpp"

using frames::ChatRoomFrame;

static void packet_handler(SwiftNetClientPacketData* const packet_data, void* const user) {

}

ChatRoomFrame::ChatRoomFrame(const in_addr ip_address, const uint16_t server_id) : wxFrame(wxGetApp().GetHomeFrame(), wxID_ANY, "Chat Room", wxDefaultPosition, wxSize(800, 600)), server_id(server_id), server_ip_address(ip_address)  {
    wxPanel* main_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);

    wxBoxSizer* main_sizer = new wxBoxSizer(wxHORIZONTAL);

    this->main_panel = main_panel;
    this->main_sizer = main_sizer;

    this->channel_list_panel = new wxPanel(main_panel);
    this->channel_list_panel->SetMinSize(wxSize(200, -1));

    this->users_panel = new wxScrolled<wxPanel>(main_panel);
    this->users_panel->SetScrollRate(0, 20);
    this->users_panel->SetMinSize(wxSize(200, -1));

    this->UpdateMainSizer();

    main_panel->SetSizerAndFit(main_sizer);

    // Handle loading server
    const char* string_ip = inet_ntoa(ip_address);

    SwiftNetClientConnection* const connection = swiftnet_create_client(string_ip, server_id, DEFAULT_TIMEOUT_CLIENT_CREATION);
    if (connection == NULL) {
        // Handle server not started, or any error
        return;
    }

    this->client_connection = connection;

    swiftnet_client_set_message_handler(connection, packet_handler, nullptr);

    this->LoadServerInformation();
}

ChatRoomFrame::~ChatRoomFrame() {
    auto const chat_room_frames = wxGetApp().GetChatRoomFrames();

    for(uint32_t i = 0; i < chat_room_frames->size(); i++) {
        if (this == chat_room_frames->at(i)) {
            chat_room_frames->erase(chat_room_frames->begin() + i);
            break;
        }
    }

    for (auto &chat_chanel : this->chat_channels) {
        delete chat_chanel;
    }

    swiftnet_client_cleanup(this->client_connection);
}

void ChatRoomFrame::UpdateMainSizer() {
    main_sizer->Clear(false);

    main_sizer->Add(this->channel_list_panel, wxSizerFlags(0).Expand());
    if (this->chat_panel != nullptr) {
        main_sizer->Add(this->chat_panel, wxSizerFlags(1).Expand());
    } else {
        main_sizer->AddStretchSpacer(1);
    }

    main_sizer->Add(this->users_panel, wxSizerFlags(0).Expand());

    main_panel->Layout();
    Layout();
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

            this->chat_panel = new ChatPanel(channel->GetId(), this->GetServerId(), this->main_panel, this->GetServerIpAddress());

            this->UpdateMainSizer();
        });
        channel_button->SetMinSize(wxSize(-1, 30));
        channel_button->SetMaxSize(wxSize(-1, 30));

        list_sizer->Add(channel_button, wxSizerFlags(0).Expand());
    }

    this->channel_list_panel->SetMinSize(wxSize(200, -1));
    this->channel_list_panel->SetSizer(list_sizer);
    this->channel_list_panel->Layout();
}

void ChatRoomFrame::HandleLoadServerInfoResponse(SwiftNetClientPacketData* const packet_data) {
    auto request_info = (ResponseInfo*)swiftnet_client_read_packet(packet_data, sizeof(ResponseInfo*));
    auto request_data = (responses::LoadServerInformationResponse*)swiftnet_client_read_packet(packet_data, sizeof(responses::LoadServerInformationResponse));

    if (request_info->request_type != RequestType::LOAD_SERVER_INFORMATION) {
        return;
    }

    if (request_info->request_status != Status::SUCCESS) {
        return;
    }

    objects::Database::ServerChatChannelRow* rows = (objects::Database::ServerChatChannelRow*)swiftnet_client_read_packet(packet_data, sizeof(objects::Database::ServerChatChannelRow) * request_data->server_chat_channels_size);

    for (uint32_t i = 0; i < request_data->server_chat_channels_size; i++) {
        auto channel = rows[i];

        printf("Channel: %s, %d, %d\n", channel.name, channel.id, channel.hosted_server_id);

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

    SwiftNetClientPacketData* const packet_data = swiftnet_client_make_request(connection, &buffer, DEFAULT_TIMEOUT_REQUEST);
    if (packet_data == NULL) {
        swiftnet_client_destroy_packet_buffer(&buffer);

        return;
    }

    this->HandleLoadServerInfoResponse(packet_data);

    swiftnet_client_destroy_packet_buffer(&buffer);

    swiftnet_client_destroy_packet_data(packet_data, connection);
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
