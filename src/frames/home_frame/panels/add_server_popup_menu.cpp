#include "panels.hpp"
#include "../../../main.hpp"
#include "../../../utils/crypto/crypto.hpp"
#include "../../../utils/net/net.hpp"
#include <cstring>
#include <swift_net.h>
#include "../../../objects/objects.hpp"

using AddServerPopupMenu = frames::home_frame::panels::ServersPanel::AddServerPopupMenu;

AddServerPopupMenu::AddServerPopupMenu(wxWindow* parent, wxPoint pos) : wxDialog(parent, wxID_ANY, wxT("Add Server"), pos, wxSize(400, 200), wxSTAY_ON_TOP | wxDEFAULT_DIALOG_STYLE) {
    wxTextCtrl* server_code_input = new wxTextCtrl(this, 0, "Server Code");
    wxTextCtrl* username_input = new wxTextCtrl(this, 0, "Username");

    widgets::Button* add_server_button = new widgets::Button(this, "Add Server", [this, server_code_input, username_input](wxMouseEvent& event) { 
        AddServerPopupMenu::AddServerReturnCode return_code = AddServerPopupMenu::AddServer(server_code_input->GetValue(), username_input->GetValue());

        switch (return_code) {
            case INVALID_LENGTH:
                break;
            case SUCCESS:
                this->EndModal(wxID_CANCEL);
                break;
        }
    });

    add_server_button->SetMinSize(wxSize(-1, 20));

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(server_code_input, 0, wxALIGN_CENTER | wxALL, 10);
    sizer->Add(username_input, 0, wxALIGN_CENTER | wxALL, 10);
    sizer->Add(add_server_button, 0, wxALL | wxEXPAND, 10);
    sizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxALIGN_CENTER | wxALL, 10);
    SetSizerAndFit(sizer);
}

AddServerPopupMenu::~AddServerPopupMenu() {

}

static void packet_handler(SwiftNetClientPacketData* const packet_data, void* const user) {
    printf("Received packet\n");
}

enum AddServerPopupMenu::RequestServerExistationStatus AddServerPopupMenu::RequestServerExistsConfirmation(const char* ip_address, const uint16_t server_id, const in_addr address, const char* username) {
    SwiftNetClientConnection* client = nullptr;

    if (address.s_addr == utils::net::get_public_ip().s_addr) {
        client = swiftnet_create_client(inet_ntoa(utils::net::get_private_ip()), server_id, DEFAULT_TIMEOUT_CLIENT_CREATION);
    } else {
        client = swiftnet_create_client(ip_address, server_id, DEFAULT_TIMEOUT_CLIENT_CREATION);
    }

    if (client == nullptr) {
        return AddServerPopupMenu::RequestServerExistationStatus::NO_RESPONSE;
    }

    swiftnet_client_set_message_handler(client, packet_handler, NULL);

    const RequestInfo request_info = {.request_type = RequestType::JOIN_SERVER};

    const requests::JoinServerRequest request_data = {};

    strncpy((char*)request_data.username, username, sizeof(request_data.username));

    auto buffer = swiftnet_client_create_packet_buffer(sizeof(request_info) + sizeof(request_data));

    swiftnet_client_append_to_packet(&request_info, sizeof(request_info), &buffer);
    swiftnet_client_append_to_packet(&request_data, sizeof(request_data), &buffer);

    SwiftNetClientPacketData* response = swiftnet_client_make_request(client, &buffer, DEFAULT_TIMEOUT_REQUEST);
    if (response == nullptr) {
        swiftnet_client_destroy_packet_buffer(&buffer);

        return AddServerPopupMenu::RequestServerExistationStatus::NO_RESPONSE;
    }

    swiftnet_client_destroy_packet_buffer(&buffer);

    auto response_info_received = (ResponseInfo*)swiftnet_client_read_packet(response, sizeof(ResponseInfo));
    auto response_data_received = (responses::JoinServerResponse*)swiftnet_client_read_packet(response, sizeof(responses::JoinServerResponse));

    if (response_info_received->request_type != RequestType::JOIN_SERVER) {
        return AddServerPopupMenu::RequestServerExistationStatus::UNKNOWN_RESPONSE;
    }

    in_addr parsed_ip_address;
    inet_pton(AF_INET, ip_address, &parsed_ip_address);

    if(response_info_received->request_status == Status::SUCCESS) {
        wxGetApp().GetDatabase()->InsertJoinedServer(server_id, parsed_ip_address);

        ServersPanel* const servers_panel = wxGetApp().GetHomeFrame()->GetServersPanel();

        servers_panel->GetJoinedServers()->push_back(objects::JoinedServer(server_id, parsed_ip_address, objects::JoinedServer::ServerStatus::ONLINE, false));
        
        servers_panel->DrawServers();
    } else {
        // Handle err
    }

    swiftnet_client_destroy_packet_data(response, client);

    swiftnet_client_cleanup(client);

    return AddServerPopupMenu::RequestServerExistationStatus::SUCCESSFULLY_CONNECTED;
}

AddServerPopupMenu::AddServerReturnCode AddServerPopupMenu::AddServer(wxString server_code_input, wxString username_input) {
    in_addr server_ip_address;
    uint16_t server_id;

    std::vector<uint8_t> decoded_code = utils::crypto::base32_decode(server_code_input.ToStdString());

    memcpy(&server_ip_address, decoded_code.data(), sizeof(server_ip_address));
    memcpy(&server_id, decoded_code.data() + sizeof(server_ip_address), sizeof(server_id));

    in_addr public_ip_local = utils::net::get_public_ip();

    const char* server_ip_string = inet_ntoa(server_ip_address);

    this->RequestServerExistsConfirmation(server_ip_string, server_id, server_ip_address, username_input.c_str());

    return SUCCESS;
}
