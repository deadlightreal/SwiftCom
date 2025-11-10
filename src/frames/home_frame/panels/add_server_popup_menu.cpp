#include "panels.hpp"
#include "../../../main.hpp"
#include "../../../utils/crypto/crypto.hpp"
#include "../../../utils/net/net.hpp"
#include "swift_net.h"
#include "../../../objects/objects.hpp"

using AddServerPopupMenu = frames::home_frame::panels::ServersPanel::AddServerPopupMenu;

AddServerPopupMenu::AddServerPopupMenu(wxWindow* parent, wxPoint pos) : wxDialog(parent, wxID_ANY, wxT("Add Server"), pos, wxSize(400, 200), wxSTAY_ON_TOP | wxDEFAULT_DIALOG_STYLE) {
    wxTextCtrl* server_code_input = new wxTextCtrl(this, 0, "Server Code");

    widgets::Button* add_server_button = new widgets::Button(this, "Add Server", [this, server_code_input](wxMouseEvent& event) { 
        AddServerPopupMenu::AddServerReturnCode return_code = AddServerPopupMenu::AddServer(server_code_input->GetValue());

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
    sizer->Add(add_server_button, 0, wxALL | wxEXPAND, 10);
    sizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxALIGN_CENTER | wxALL, 10);
    SetSizerAndFit(sizer);
}

AddServerPopupMenu::~AddServerPopupMenu() {

}

enum AddServerPopupMenu::RequestServerExistationStatus AddServerPopupMenu::RequestServerExistsConfirmation(const char* ip_address, const uint16_t server_id, const in_addr address) {
    SwiftNetClientConnection* client = nullptr;

    if (address.s_addr == utils::net::get_public_ip().s_addr) {
        client = swiftnet_create_client(inet_ntoa(utils::net::get_private_ip()), server_id, DEFAULT_TIMEOUT_CLIENT_CREATION);
    } else {
        client = swiftnet_create_client(ip_address, server_id, DEFAULT_TIMEOUT_CLIENT_CREATION);
    }

    if (client == nullptr) {
        return AddServerPopupMenu::RequestServerExistationStatus::NO_RESPONSE;
    }

    const RequestInfo request_info = {.request_type = JOIN_SERVER};

    const JoinServerRequest request = {
    };

    SwiftNetPacketBuffer buffer = swiftnet_client_create_packet_buffer(sizeof(request_info) + sizeof(request));
    
    swiftnet_client_append_to_packet(&request_info, sizeof(request_info), &buffer);
    swiftnet_client_append_to_packet(&request, sizeof(request), &buffer);

    SwiftNetClientPacketData* const packet_data = swiftnet_client_make_request(client, &buffer, DEFAULT_TIMEOUT_REQUEST);
    if (packet_data == nullptr) {
        return AddServerPopupMenu::RequestServerExistationStatus::NO_RESPONSE;
    }

    const RequestInfo* const request_info_received = (RequestInfo*)swiftnet_client_read_packet(packet_data, sizeof(RequestInfo));
    if (request_info_received->request_type != RequestType::JOIN_SERVER) {
        return AddServerPopupMenu::RequestServerExistationStatus::UNKNOWN_RESPONSE;
    }
    
    const JoinServerResponse* const response = (JoinServerResponse*)swiftnet_client_read_packet(packet_data, sizeof(JoinServerResponse));

    in_addr parsed_ip_address;
    inet_pton(AF_INET, ip_address, &parsed_ip_address);

    if(response->status == RequestStatus::SUCCESS) {
        wxGetApp().GetDatabase()->InsertJoinedServer(server_id, parsed_ip_address);

        ServersPanel* const servers_panel = wxGetApp().GetHomeFrame()->GetServersPanel();

        servers_panel->GetJoinedServers()->push_back(objects::JoinedServer(server_id, parsed_ip_address));
        
        servers_panel->DrawServers();
    } else {
    }

    swiftnet_client_destroy_packet_buffer(&buffer);

    swiftnet_client_cleanup(client);

    return AddServerPopupMenu::RequestServerExistationStatus::SUCCESSFULLY_CONNECTED;
}

AddServerPopupMenu::AddServerReturnCode AddServerPopupMenu::AddServer(wxString input) {
    in_addr server_ip_address;
    uint16_t server_id;

    std::vector<uint8_t> decoded_code = utils::crypto::base32_decode(input.ToStdString());

    memcpy(&server_ip_address, decoded_code.data(), sizeof(server_ip_address));
    memcpy(&server_id, decoded_code.data() + sizeof(server_ip_address), sizeof(server_id));

    in_addr public_ip_local = utils::net::get_public_ip();

    const char* server_ip_string = inet_ntoa(server_ip_address);

    this->RequestServerExistsConfirmation(server_ip_string, server_id, server_ip_address);

    return SUCCESS;
}
