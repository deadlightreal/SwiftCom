#include "../frames.hpp"
#include <swift_net.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <vector>
#include <wx/osx/textctrl.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include "../../utils/net/net.hpp"
#include <wx/string.h>
#include "../../main.hpp"

using ChatPanel = frames::ChatRoomFrame::ChatPanel;

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

ChatPanel::ChatPanel(const uint32_t channel_id, const uint16_t server_id, wxWindow* parent_window, const in_addr ip_address) : channel_id(channel_id), server_id(server_id), wxPanel(parent_window) {
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

ChatPanel::~ChatPanel() {
    swiftnet_client_cleanup(this->GetClientConnection());
}

void ChatPanel::HandleLoadChannelDataRequest(SwiftNetClientPacketData* const packet_data) {
    const ResponseInfo* const response_info = (ResponseInfo*)swiftnet_client_read_packet(packet_data, sizeof(ResponseInfo));
    if (response_info == nullptr || response_info->request_type != RequestType::LOAD_CHANNEL_DATA) {
        return;
    }

    const responses::LoadChannelDataResponse* response = (responses::LoadChannelDataResponse*)swiftnet_client_read_packet(packet_data, sizeof(responses::LoadChannelDataResponse));
    if (response == nullptr) {
        return;
    }

    if (response_info->request_status != Status::SUCCESS) {
        // Handle errors
        return;
    }

    auto channel_messages = DeserializeChannelMessages(packet_data, response->channel_messages_len);

    *(this->GetChannelMessages()) = *channel_messages;
}

void ChatPanel::SendMessage(const char* message, const uint32_t message_len) {
    SwiftNetClientConnection* connection = this->GetClientConnection();

    const RequestInfo request_info = {
        .request_type = SEND_MESSAGE
    };

    const requests::SendMessageRequest request_data = {
        .message_len = message_len,
        .channel_id = this->GetChannelId()
    };

    auto buffer = swiftnet_client_create_packet_buffer(sizeof(request_info) + sizeof(request_data) + message_len);

    swiftnet_client_append_to_packet(&request_info, sizeof(request_info), &buffer);
    swiftnet_client_append_to_packet(&request_data, sizeof(request_data), &buffer);
    swiftnet_client_append_to_packet(message, message_len, &buffer);

    swiftnet_client_send_packet(connection, &buffer);

    swiftnet_client_destroy_packet_buffer(&buffer);
}

void ChatPanel::InitializeConnection(const in_addr ip_address) {
    const char* ip_address_string = inet_ntoa(ip_address);
    SwiftNetClientConnection* new_connection = swiftnet_create_client(ip_address_string, this->GetServerId(), DEFAULT_TIMEOUT_CLIENT_CREATION);
    if (new_connection == nullptr) {
        fprintf(stderr, "Failed to connect to server\n");
        return;
    }

    this->client_connection = new_connection;
}

void ChatPanel::LoadChannelData() {
    SwiftNetClientConnection* connection = this->GetClientConnection();

    const RequestInfo request_info = {
        .request_type = LOAD_CHANNEL_DATA
    };

    const requests::LoadChannelDataRequest request_data = {
        .channel_id = this->GetChannelId()
    };

    auto buffer = swiftnet_client_create_packet_buffer(sizeof(request_info) + sizeof(request_data));

    swiftnet_client_append_to_packet(&request_info, sizeof(request_info), &buffer);
    swiftnet_client_append_to_packet(&request_data, sizeof(request_data), &buffer);

    SwiftNetClientPacketData* const response = swiftnet_client_make_request(connection, &buffer, DEFAULT_TIMEOUT_REQUEST);
    if (response == nullptr) {
        swiftnet_client_destroy_packet_buffer(&buffer);

        return;
    }

    swiftnet_client_destroy_packet_buffer(&buffer);

    this->HandleLoadChannelDataRequest(response);

    swiftnet_client_destroy_packet_data(response, connection);
}

wxTextCtrl* ChatPanel::GetNewMessageInput() {
    return this->new_message_input;
}

std::vector<objects::Database::ChannelMessageRow>* ChatPanel::GetChannelMessages() {
    return &this->channel_messages;
}

SwiftNetClientConnection* ChatPanel::GetClientConnection() {
    return this->client_connection;
}

wxPanel* ChatPanel::GetMessagesPanel() {
    return this->messages_panel;
}

uint16_t ChatPanel::GetServerId() {
    return this->server_id;
}

uint32_t ChatPanel::GetChannelId() {
    return this->channel_id;
}
