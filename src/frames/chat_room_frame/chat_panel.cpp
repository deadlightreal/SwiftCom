#include "../frames.hpp"
#include "swift_net.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <wx/osx/textctrl.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include "../../main.hpp"

using namespace frames;

static ChatRoomFrame::ChatPanel* current_chat_panel = nullptr;

static std::vector<objects::Database::ChannelMessageRow>* DeserializeChannelMessages(SwiftNetClientPacketData* packet_data, const uint32_t channel_messages_len) {
    auto result = new std::vector<objects::Database::ChannelMessageRow>();

    for (uint32_t i = 0; i < channel_messages_len; i++) {
        const uint32_t* message_id = (uint32_t*)swiftnet_client_read_packet(packet_data, sizeof(uint32_t));
        const uint32_t* sender_id = (uint32_t*)swiftnet_client_read_packet(packet_data, sizeof(uint32_t));
        const uint32_t* message_length = (uint32_t*)swiftnet_client_read_packet(packet_data, sizeof(uint32_t));
        const char* message = (const char*)swiftnet_client_read_packet(packet_data, *message_length);

        char* message_copy = (char*)malloc(*message_length);
        memcpy(message_copy, message, *message_length);

        result->push_back((objects::Database::ChannelMessageRow){
            .message_length = *message_length,
            .sender_id = *sender_id,
            .id = *message_id,
            .message = message_copy
        });
    }

    return result;
}

void ChatRoomFrame::ChatPanel::HandleLoadChannelDataRequest(SwiftNetClientPacketData* const packet_data) {
    const RequestInfo* const request_info = (RequestInfo*)swiftnet_client_read_packet(packet_data, sizeof(RequestInfo));
    if (request_info->request_type != LOAD_CHANNEL_DATA) {
        return;
    }

    const LoadChannelDataResponse* response = (LoadChannelDataResponse*)swiftnet_client_read_packet(packet_data, sizeof(LoadChannelDataResponse));

    if (response->status != SUCCESS) {
        // Handle errors
        return;
    }

    auto channel_messages = DeserializeChannelMessages(packet_data, response->channel_messages_len);

    *(this->GetChannelMessages()) = *channel_messages;
}

ChatRoomFrame::ChatPanel::ChatPanel(const uint32_t channel_id, const uint16_t server_id, wxWindow* parent_window, const in_addr ip_address) : channel_id(channel_id), server_id(server_id), wxPanel(parent_window) {
    current_chat_panel = this;

    this->InitializeConnection(ip_address);

    this->LoadChannelData();

    // wxWidgets
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    this->messages_panel = new wxPanel(this);
    this->new_message_input = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);

    this->GetNewMessageInput()->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent& event) {
        wxString value = this->GetNewMessageInput()->GetValue();

        const char* message = value.c_str();
        const uint32_t message_len = value.length();

        if (message != nullptr) {
            this->GetNewMessageInput()->Clear();

            this->SendMessage(message, message_len);
        }
    });

    this->GetNewMessageInput()->SetMinSize(wxSize(-1, 50));
    this->GetNewMessageInput()->SetMaxSize(wxSize(-1, 50));

    main_sizer->Add(this->GetMessagesPanel(), 1, wxEXPAND);
    main_sizer->Add(this->GetNewMessageInput(), 0);

    this->SetSizer(main_sizer);
}

ChatRoomFrame::ChatPanel::~ChatPanel() {
    swiftnet_client_cleanup(this->GetClientConnection());

    current_chat_panel = nullptr;
}

void ChatRoomFrame::ChatPanel::SendMessage(const char* message, const uint32_t message_len) {
    SwiftNetClientConnection* connection = this->GetClientConnection();

    const RequestInfo request_info = {
        .request_type = SEND_MESSAGE
    };

    const SendMessageRequest request = {
        .message_len = message_len + 1,
        .channel_id = this->GetChannelId()
    };

    SwiftNetPacketBuffer buffer = swiftnet_client_create_packet_buffer(sizeof(request) + sizeof(request_info) + (message_len + 1));

    swiftnet_client_append_to_packet(&request_info, sizeof(request_info), &buffer);
    swiftnet_client_append_to_packet(&request, sizeof(request), &buffer);
    swiftnet_client_append_to_packet(message, message_len + 1, &buffer);

    swiftnet_client_send_packet(connection, &buffer);

    swiftnet_client_destroy_packet_buffer(&buffer);
}

void ChatRoomFrame::ChatPanel::InitializeConnection(const in_addr ip_address) {
    const char* ip_address_string = inet_ntoa(ip_address);
    SwiftNetClientConnection* new_connection = swiftnet_create_client(ip_address_string, this->GetServerId());
    if (new_connection == nullptr) {
        fprintf(stderr, "Failed to connect to server\n");
        return;
    }
}

void ChatRoomFrame::ChatPanel::LoadChannelData() {
    SwiftNetClientConnection* connection = this->GetClientConnection();

    const RequestInfo request_info = {
        .request_type = LOAD_CHANNEL_DATA
    };

    const LoadChannelDataRequest request = {
        .channel_id = this->GetChannelId()
    };

    SwiftNetPacketBuffer buffer = swiftnet_client_create_packet_buffer(sizeof(request) + sizeof(request_info));

    swiftnet_client_append_to_packet(&request_info, sizeof(request_info), &buffer);
    swiftnet_client_append_to_packet(&request, sizeof(request), &buffer);

    SwiftNetClientPacketData* const packet_data = swiftnet_client_make_request(connection, &buffer);
    if (packet_data == nullptr) {
        return;
    }

    this->HandleLoadChannelDataRequest(packet_data);

    swiftnet_client_destroy_packet_buffer(&buffer);
}

wxTextCtrl* ChatRoomFrame::ChatPanel::GetNewMessageInput() {
    return this->new_message_input;
}

std::vector<objects::Database::ChannelMessageRow>* ChatRoomFrame::ChatPanel::GetChannelMessages() {
    return &this->channel_messages;
}

SwiftNetClientConnection* ChatRoomFrame::ChatPanel::GetClientConnection() {
    return this->client_connection;
}

wxPanel* ChatRoomFrame::ChatPanel::GetMessagesPanel() {
    return this->messages_panel;
}

uint16_t ChatRoomFrame::ChatPanel::GetServerId() {
    return this->server_id;
}

uint32_t ChatRoomFrame::ChatPanel::GetChannelId() {
    return this->channel_id;
}
