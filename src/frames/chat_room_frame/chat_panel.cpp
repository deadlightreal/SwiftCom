#include "../frames.hpp"
#include <swift_net.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <wx/event.h>
#include <wx/osx/stattext.h>
#include <wx/osx/textctrl.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include <wx/timer.h>
#include <wx/wx.h>
#include "../../main.hpp"

using ChatPanel = frames::ChatRoomFrame::ChatPanel;

static std::vector<objects::Database::ChannelMessageRow>* DeserializeChannelMessages(SwiftNetClientPacketData* packet_data, const uint32_t channel_messages_len) {
    auto result = new std::vector<objects::Database::ChannelMessageRow>();

    for (uint32_t i = 0; i < channel_messages_len; i++) {
        const uint32_t* const message_id = (uint32_t*)swiftnet_client_read_packet(packet_data, sizeof(uint32_t));
        const uint32_t* const sender_id = (uint32_t*)swiftnet_client_read_packet(packet_data, sizeof(uint32_t));
        const uint32_t* const message_length = (uint32_t*)swiftnet_client_read_packet(packet_data, sizeof(uint32_t));
        const char* const message = (const char*)swiftnet_client_read_packet(packet_data, *message_length);

        char* const message_copy = (char*)malloc(*message_length);
        strncpy(message_copy, message, *message_length);

        printf("Received message: %s\n", message_copy);

        auto message_row = (objects::Database::ChannelMessageRow){
            .message_length = *message_length,
            .sender_id = *sender_id,
            .id = *message_id,
            .message = message_copy
        };

        const char* const sender_username = (const char*)swiftnet_client_read_packet(packet_data, sizeof(message_row.sender_username));

        memcpy(message_row.sender_username, sender_username, sizeof(message_row.sender_username));

        result->push_back(message_row);
    }

    return result;
}

static void packet_handler(struct SwiftNetClientPacketData* const packet_data, void* const chat_panel_void) {
    ChatPanel* const chat_panel = static_cast<ChatPanel*>(chat_panel_void);

    auto const response_info = (ResponseInfo*)swiftnet_client_read_packet(packet_data, sizeof(ResponseInfo));

    switch (response_info->request_type) {
        case RequestType::PERIODIC_CHAT_UPDATE: chat_panel->HandlePeriodicChatUpdate(packet_data); break;
        default: break;
    }
};

ChatPanel::ChatPanel(const uint32_t channel_id, const uint16_t server_id, wxWindow* parent_window, const in_addr ip_address) : channel_id(channel_id), server_id(server_id), wxPanel(parent_window) {
    this->InitializeConnection(ip_address);

    swiftnet_client_set_message_handler(this->GetClientConnection(), packet_handler, this);

    // wxWidgets
    wxBoxSizer* main_sizer = new wxBoxSizer(wxVERTICAL);

    this->messages_panel = new wxScrolled<wxPanel>(this);
    this->messages_panel->SetScrollRate(0, 20);
    this->messages_panel->Bind(wxEVT_SCROLLWIN_LINEUP, [this](wxScrollWinEvent& evt) {
        this->OnScrollChange(evt);
    });
    this->messages_panel->Bind(wxEVT_SCROLLWIN_LINEDOWN, [this](wxScrollWinEvent& evt) {
        this->OnScrollChange(evt);
    });
    this->messages_panel->Bind(wxEVT_SCROLLWIN_THUMBRELEASE, [this](wxScrollWinEvent& evt) {
        this->OnScrollChange(evt);
    });

    this->messages_sizer = new wxBoxSizer(wxVERTICAL);

    this->messages_panel->SetSizer(this->messages_sizer);

    auto* new_msg_input = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(360, 40), wxTE_PROCESS_ENTER);
    new_msg_input->SetBackgroundColour(wxColour(50, 50, 60));
    new_msg_input->SetForegroundColour(*wxWHITE);
    new_msg_input->SetHint("Message Text");
    new_msg_input->SetFont(new_msg_input->GetFont().Scale(1.1));

    this->new_message_input = new_msg_input;

    this->GetNewMessageInput()->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent& event) {
        wxString value = this->GetNewMessageInput()->GetValue();

        const char* message = value.c_str();
        const uint32_t message_len = value.length() + 1;

        if (message != nullptr) {
            this->GetNewMessageInput()->Clear();

            this->SendMessage(message, message_len);
        }
    });

    this->GetNewMessageInput()->SetMinSize(wxSize(-1, 50));
    this->GetNewMessageInput()->SetMaxSize(wxSize(-1, 50));

    main_sizer->Add(this->GetMessagesPanel(), wxSizerFlags(1).Expand());
    main_sizer->Add(this->GetNewMessageInput(), wxSizerFlags(0).Expand());

    this->SetSizerAndFit(main_sizer);

    this->Bind(wxEVT_CHAT_UPDATE, &ChatPanel::OnChatUpdate, this);

    this->LoadChannelData();
    this->RedrawMessages();
}

ChatPanel::~ChatPanel() {
    swiftnet_client_cleanup(this->GetClientConnection());
}

void ChatPanel::RedrawMessages() {
    this->messages_panel->Freeze();

    this->messages_sizer->Clear(true);

    for (const auto& msg : *this->GetChannelMessages()) {
        wxBoxSizer* row_sizer = new wxBoxSizer(wxHORIZONTAL);

        wxStaticText* username_text = new wxStaticText(this->messages_panel, wxID_ANY, wxString::Format("%s: ", msg.sender_username));
        username_text->SetFont(username_text->GetFont().Scale(1.1f).Bold());
        username_text->SetForegroundColour(wxColour(180, 180, 255));

        wxStaticText* message_text = new wxStaticText(this->messages_panel, wxID_ANY, msg.message);
        message_text->SetFont(message_text->GetFont().Scale(1.6f));
        message_text->SetForegroundColour(*wxWHITE);
        message_text->Wrap(600);

        row_sizer->Add(username_text, wxSizerFlags(0).Border(wxRIGHT, 8).Align(wxALIGN_CENTER_VERTICAL));
        row_sizer->Add(message_text, wxSizerFlags(1).Align(wxALIGN_CENTER_VERTICAL));

        this->messages_sizer->Add(row_sizer, wxSizerFlags(0).Expand().Border(wxTOP | wxBOTTOM, 6));
    }

    this->messages_sizer->Layout();
    this->messages_panel->FitInside();

    if (this->messages_panel_bottom) {
        printf("Scrolling auto\n");

        int maxPos = this->messages_panel->GetScrollRange(wxVERTICAL) - this->messages_panel->GetScrollThumb(wxVERTICAL);

        this->messages_panel->Scroll(-1, maxPos);
    }

    this->messages_panel->Thaw();

    this->Layout();
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

    printf("Channel messages got: %d\n", response->channel_messages_len);

    auto channel_messages = DeserializeChannelMessages(packet_data, response->channel_messages_len);

    *(this->GetChannelMessages()) = *channel_messages;
}

void ChatPanel::OnScrollChange(wxScrollWinEvent& evt) {
    int currentPos = this->messages_panel->GetScrollPos(wxVERTICAL);

    int maxPos = this->messages_panel->GetScrollRange(wxVERTICAL) - this->messages_panel->GetScrollThumb(wxVERTICAL);

    printf("Current Pos: %d\nMax Pos: %d\n", currentPos, maxPos);

    bool bottom = (currentPos + 1) >= maxPos;

    this->messages_panel_bottom = bottom;

    evt.Skip();
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

void ChatPanel::HandlePeriodicChatUpdate(struct SwiftNetClientPacketData* const packet_data) {
    auto response = (responses::PeriodicChatUpdateResponse*)swiftnet_client_read_packet(packet_data, sizeof(responses::PeriodicChatUpdateResponse));

    printf("Periodic update\nNew messages: %d\n", response->channel_messages_len);
    
    auto new_messages = DeserializeChannelMessages(packet_data, response->channel_messages_len);

    this->channel_messages.insert(this->GetChannelMessages()->end(), new_messages->data(), new_messages->data() + new_messages->size());

    auto* evt = new wxCommandEvent(wxEVT_CHAT_UPDATE);
    wxQueueEvent(this, evt);

    swiftnet_client_destroy_packet_data(packet_data, this->GetClientConnection());
}

void ChatPanel::OnChatUpdate(wxCommandEvent& event) {
    this->RedrawMessages();
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
