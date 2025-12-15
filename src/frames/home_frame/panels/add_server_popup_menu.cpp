#include "panels.hpp"
#include "../../../main.hpp"
#include "../../../utils/crypto/crypto.hpp"
#include "../../../utils/net/net.hpp"
#include <swift_net.h>
#include "../../../objects/objects.hpp"
#include <wx/event.h>
#include <wx/timer.h>
#include <wx/valtext.h>

using AddServerPopupMenu = frames::home_frame::panels::ServersPanel::AddServerPopupMenu;

AddServerPopupMenu::AddServerPopupMenu(wxWindow* parent, wxPoint pos) : wxDialog(parent, wxID_ANY, "Add Server", pos, wxSize(420, 320), wxSTAY_ON_TOP | wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{

    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(this, wxID_ANY, "Join a Server");
    title->SetFont(title->GetFont().Scale(1.8).Bold());
    title->SetForegroundColour(wxColour(255, 255, 255));
    mainSizer->Add(title, 0, wxALIGN_CENTER | wxTOP | wxLEFT | wxRIGHT, 25);

    auto* helpText = new wxStaticText(this, wxID_ANY, "Enter the server code and your username to join.");
    helpText->SetForegroundColour(wxColour(180, 180, 190));
    mainSizer->Add(helpText, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT | wxBOTTOM, 20);

    auto* codeLabel = new wxStaticText(this, wxID_ANY, "Server Code");
    codeLabel->SetForegroundColour(wxColour(220, 220, 220));
    codeLabel->SetFont(codeLabel->GetFont().Bold());
    mainSizer->Add(codeLabel, 0, wxLEFT | wxTOP, 20);

    auto* serverCodeInput = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(360, 40));
    serverCodeInput->SetBackgroundColour(wxColour(50, 50, 60));
    serverCodeInput->SetForegroundColour(*wxWHITE);
    serverCodeInput->SetHint("e.g. ABCDE-12345-FGHIJ");
    serverCodeInput->SetFont(serverCodeInput->GetFont().Scale(1.1));
    mainSizer->Add(serverCodeInput, 0, wxALL | wxEXPAND, 12);

    auto* userLabel = new wxStaticText(this, wxID_ANY, "Username");
    userLabel->SetForegroundColour(wxColour(220, 220, 220));
    userLabel->SetFont(userLabel->GetFont().Bold());
    mainSizer->Add(userLabel, 0, wxLEFT, 20);

    auto* usernameInput = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(360, 40));
    usernameInput->SetBackgroundColour(wxColour(50, 50, 60));
    usernameInput->SetForegroundColour(*wxWHITE);
    usernameInput->SetHint("Your display name");
    usernameInput->SetFont(usernameInput->GetFont().Scale(1.1));
    mainSizer->Add(usernameInput, 0, wxALL | wxEXPAND, 12);

    auto* buttonSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* cancelBtn = new widgets::Button(this, "Cancel", [this](wxMouseEvent& evt){
        this->EndModal(wxID_CLOSE);
    });
    cancelBtn->SetForegroundColour(*wxWHITE);
    cancelBtn->SetFont(cancelBtn->GetFont().Bold().Scale(1.1));
    cancelBtn->SetMinSize(wxSize(140, 45));

    auto* addBtn = new widgets::Button(this, "Join Server", [this, serverCodeInput, usernameInput](wxMouseEvent&) {
        wxString code = serverCodeInput->GetValue().Trim().Upper();
        wxString username = usernameInput->GetValue().Trim();

        if (code.IsEmpty()) {
            wxMessageBox("Please enter a server code.", "Missing Code", wxOK | wxICON_WARNING, this);
            return;
        }
        if (username.IsEmpty()) {
            wxMessageBox("Please enter a username.", "Missing Username", wxOK | wxICON_WARNING, this);
            return;
        }
        if (username.length() > 20) {
            wxMessageBox("Username too long (max 20 chars).", "Invalid Username", wxOK | wxICON_ERROR, this);
            return;
        }

        AddServerReturnCode result = AddServer(code, username);

        if (result == SUCCESS) {
            EndModal(wxID_OK);
        }
    });

    addBtn->SetForegroundColour(*wxWHITE);
    addBtn->SetFont(addBtn->GetFont().Bold().Scale(1.1));
    addBtn->SetMinSize(wxSize(140, 45));

    buttonSizer->Add(cancelBtn, 0);
    buttonSizer->AddStretchSpacer();
    buttonSizer->Add(addBtn, 0);

    mainSizer->Add(buttonSizer, 1, wxALL | wxEXPAND, 10);

    SetSizerAndFit(mainSizer);
    CentreOnParent();

    serverCodeInput->SetFocus();
}

AddServerPopupMenu::~AddServerPopupMenu() = default;

AddServerPopupMenu::RequestServerExistationStatus AddServerPopupMenu::RequestServerExistsConfirmation(const char* ip_address, uint16_t server_id, const in_addr address, const char* username)
{
    SwiftNetClientConnection* client = nullptr;

    // Connect to local private IP if this is our own public IP
    if (address.s_addr == utils::net::get_public_ip().s_addr) {
        client = swiftnet_create_client(inet_ntoa(utils::net::get_private_ip()), server_id, DEFAULT_TIMEOUT_CLIENT_CREATION);
    } else {
        client = swiftnet_create_client(ip_address, server_id, DEFAULT_TIMEOUT_CLIENT_CREATION);
    }

    if (!client) {
        return NO_RESPONSE;
    }

    RequestInfo request_info = { .request_type = RequestType::JOIN_SERVER };
    requests::JoinServerRequest request_data = {};
    strncpy(request_data.username, username, sizeof(request_data.username) - 1);

    auto buffer = swiftnet_client_create_packet_buffer(sizeof(request_info) + sizeof(request_data));
    swiftnet_client_append_to_packet(&request_info, sizeof(request_info), &buffer);
    swiftnet_client_append_to_packet(&request_data, sizeof(request_data), &buffer);

    SwiftNetClientPacketData* response = swiftnet_client_make_request(client, &buffer, DEFAULT_TIMEOUT_REQUEST);

    swiftnet_client_destroy_packet_buffer(&buffer);

    if (!response) {
        swiftnet_client_cleanup(client);
        return NO_RESPONSE;
    }

    auto* resp_info = (ResponseInfo*)swiftnet_client_read_packet(response, sizeof(ResponseInfo));
    auto* resp_data = (responses::JoinServerResponse*)swiftnet_client_read_packet(response, sizeof(responses::JoinServerResponse));

    if (resp_info->request_type != RequestType::JOIN_SERVER) {
        swiftnet_client_destroy_packet_data(response, client);
        swiftnet_client_cleanup(client);
        return UNKNOWN_RESPONSE;
    }

    if (resp_info->request_status == Status::SUCCESS) {
        wxGetApp().GetDatabase()->InsertJoinedServer(server_id, address);

        ServersPanel* servers_panel = wxGetApp().GetHomeFrame()->GetServersPanel();
        servers_panel->GetJoinedServers()->emplace_back(server_id, address, objects::JoinedServer::ServerStatus::ONLINE, false);
        servers_panel->DrawServers();
    } else {
    }

    swiftnet_client_destroy_packet_data(response, client);
    swiftnet_client_cleanup(client);

    return SUCCESSFULLY_CONNECTED;
}

AddServerPopupMenu::AddServerReturnCode AddServerPopupMenu::AddServer(wxString server_code_input, wxString username_input)
{
    std::vector<uint8_t> decoded = utils::crypto::base32_decode(server_code_input.ToStdString());

    if (decoded.size() < sizeof(in_addr) + sizeof(uint16_t)) {
        wxMessageBox("Invalid or corrupted server code.", "Error", wxOK | wxICON_ERROR);
        return INVALID_LENGTH;
    }

    in_addr server_ip{};
    uint16_t server_id{};

    memcpy(&server_ip, decoded.data(), sizeof(in_addr));
    memcpy(&server_id, decoded.data() + sizeof(in_addr), sizeof(uint16_t));

    const char* ip_str = inet_ntoa(server_ip);

    RequestServerExistsConfirmation(ip_str, server_id, server_ip, username_input.c_str());

    return SUCCESS;
}
