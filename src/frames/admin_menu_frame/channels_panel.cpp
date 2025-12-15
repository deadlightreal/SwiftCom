#include "../frames.hpp"
#include <cstdint>
#include <cstring>
#include <wx/dialog.h>
#include <wx/event.h>
#include "../../main.hpp"
#include <wx/layout.h>
#include <wx/osx/dialog.h>
#include <wx/textctrl.h>
#include <wx/sizer.h>

#include "../../widgets/widgets.hpp"
#include "swift_net.h"

using Channels = frames::AdminMenuFrame::Channels;

BEGIN_EVENT_TABLE(Channels, wxPanel)
END_EVENT_TABLE()

Channels::Channels(wxWindow* parent, SwiftNetClientConnection* client_connection)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNO_BORDER)
{
    if (client_connection == nullptr) {
        return;
    }

    this->client_connection = client_connection;

    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(this, wxID_ANY, "Channels");
    title->SetFont(title->GetFont().Scale(1.8).Bold());
    title->SetForegroundColour(wxColour(255, 255, 255));
    mainSizer->Add(title, 0, wxLEFT | wxTOP | wxRIGHT, 20);

    scrollPanel = new wxScrolled<wxPanel>(this);
    scrollPanel->SetScrollRate(0, 20);
    scrollPanel->SetBackgroundColour(wxColour(30, 30, 36));

    channelsSizer = new wxBoxSizer(wxVERTICAL);
    scrollPanel->SetSizer(channelsSizer);

    this->LoadChannels();
    this->DrawChannelList();

    mainSizer->Add(scrollPanel, 1, wxEXPAND | wxALL, 15);

    SetSizer(mainSizer);

    auto* addButton = new widgets::Button(this, "+", [this](wxMouseEvent& evt) {
        OnAddChannel(evt);
    });

    addButton->SetMinSize(wxSize(60, 60));
    addButton->SetBackgroundColour(wxColour(0, 120, 255));
    addButton->SetForegroundColour(*wxWHITE);
    addButton->SetFont(addButton->GetFont().Scale(2.0).Bold());

    auto* floatingSizer = new wxBoxSizer(wxVERTICAL);
    floatingSizer->AddStretchSpacer();
    floatingSizer->Add(addButton, 0, wxALL, 20);

    auto* horizontalSizer = new wxBoxSizer(wxHORIZONTAL);
    horizontalSizer->AddStretchSpacer();
    horizontalSizer->Add(floatingSizer, 0, wxALIGN_BOTTOM);

    mainSizer->Add(horizontalSizer, 0, wxEXPAND);
}

Channels::~Channels() = default;

void Channels::LoadChannels() 
{
    this->chat_channels.clear();

    const RequestInfo request_info = {
        .request_type = RequestType::LOAD_ADMIN_MENU_DATA
    };

    const requests::LoadAdminMenuDataRequest request = {
    };

    auto buffer = swiftnet_client_create_packet_buffer(sizeof(request_info) + sizeof(request));

    swiftnet_client_append_to_packet(&request_info, sizeof(request_info), &buffer);
    swiftnet_client_append_to_packet(&request, sizeof(request), &buffer);

    auto response_packet_data = swiftnet_client_make_request(this->client_connection, &buffer, DEFAULT_TIMEOUT_REQUEST);

    swiftnet_client_destroy_packet_buffer(&buffer);

    if (response_packet_data == nullptr) {
        return;
    }

    auto response_info = (ResponseInfo*)swiftnet_client_read_packet(response_packet_data, sizeof(ResponseInfo));
    auto response = (responses::LoadAdminMenuDataResponse*)swiftnet_client_read_packet(response_packet_data, sizeof(responses::LoadAdminMenuDataResponse));

    for (uint32_t i = 0; i < response->channels_size; i++) {
        auto row_ptr = (objects::Database::ServerChatChannelRow*)swiftnet_client_read_packet(response_packet_data, sizeof(objects::Database::ServerChatChannelRow));

        this->chat_channels.push_back(*row_ptr);
    }

    swiftnet_client_destroy_packet_data(response_packet_data, this->client_connection);
}

void Channels::DrawChannelList()
{
    channelsSizer->Clear(true);

    for (auto& chat_channel : this->chat_channels) {
        auto channelPanel = new widgets::StyledPanel(scrollPanel);
        channelPanel->SetMinSize(wxSize(-1, 60));

        auto hSizer = new wxBoxSizer(wxHORIZONTAL);

        auto label = new wxStaticText(channelPanel, wxID_ANY, wxString("# ") + wxString(chat_channel.name));
        label->SetForegroundColour(wxColour(220, 220, 220));
        label->SetFont(label->GetFont().Scale(1.2));
        hSizer->Add(label, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 15);

        channelPanel->SetSizer(hSizer);

        channelsSizer->Add(channelPanel, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
        channelsSizer->AddSpacer(8);
    }

    scrollPanel->FitInside();
}

int Channels::CreateNewTextChannel(const char* name) {
    const RequestInfo request_info = {
        .request_type = RequestType::CREATE_NEW_CHANNEL
    };

    const requests::CreateNewChannelRequest request = {
    };

    strncpy((char*)request.name, name, sizeof(request.name));

    auto buffer = swiftnet_client_create_packet_buffer(sizeof(request) + sizeof(request_info));

    swiftnet_client_append_to_packet(&request_info, sizeof(request_info), &buffer);
    swiftnet_client_append_to_packet(&request, sizeof(request), &buffer);

    auto response_packet_data = swiftnet_client_make_request(this->client_connection, &buffer, DEFAULT_TIMEOUT_REQUEST);

    swiftnet_client_destroy_packet_buffer(&buffer);
    
    if (response_packet_data == nullptr) {
        return -1; 
    }

    auto response_info = (ResponseInfo*)swiftnet_client_read_packet(response_packet_data, sizeof(ResponseInfo));
    auto response = (responses::CreateNewChannelResponse*)swiftnet_client_read_packet(response_packet_data, sizeof(responses::CreateNewChannelResponse));

    if (response_info->request_status != Status::SUCCESS) {
        swiftnet_client_destroy_packet_data(response_packet_data, this->client_connection);
        
        return -1;
    }

    swiftnet_client_destroy_packet_data(response_packet_data, this->client_connection);

    return 0;
}

void Channels::OnAddChannel(wxMouseEvent&)
{
    auto dialog = new wxDialog(this, wxID_ANY, "Create New Channel", wxDefaultPosition, wxSize(400, 200));

    auto* sizer = new wxBoxSizer(wxVERTICAL);

    auto* nameLabel = new wxStaticText(dialog, wxID_ANY, "Channel Name");
    nameLabel->SetForegroundColour(*wxWHITE);
    sizer->Add(nameLabel, 0, wxALL, 15);

    auto* nameInput = new wxTextCtrl(dialog, wxID_ANY, "", wxDefaultPosition, wxSize(360, 40));
    nameInput->SetBackgroundColour(wxColour(50, 50, 60));
    nameInput->SetForegroundColour(*wxWHITE);
    nameInput->SetHint("New Channel Name");
    nameInput->SetFont(nameInput->GetFont().Scale(1.1));

    sizer->Add(nameInput, 0, wxALL + wxEXPAND, 12);

    auto buttonSizer = new wxBoxSizer(wxHORIZONTAL);

    auto cancelBtn = new widgets::Button(dialog, "Cancel", [dialog](wxMouseEvent&){
        dialog->EndModal(wxID_CLOSE);
    });
    cancelBtn->SetMinSize(wxSize(140, 45));

    auto* createBtn = new widgets::Button(dialog, "Create", [dialog](wxMouseEvent&){
        dialog->EndModal(wxID_OK);
    });
    createBtn->SetMinSize(wxSize(140, 45));

    buttonSizer->Add(cancelBtn, 0);
    buttonSizer->AddStretchSpacer();
    buttonSizer->Add(createBtn, 0);

    sizer->Add(buttonSizer, 1, wxALL | wxEXPAND, 12);

    dialog->SetSizer(sizer);
    dialog->Centre();

    if (dialog->ShowModal() == wxID_OK) {
        wxString newName = nameInput->GetValue().Trim().Trim(false);

        if (newName.size() > 20) {
            return;
        }

        if (!newName.IsEmpty()) {
            int result = this->CreateNewTextChannel(newName.c_str());
            if (result != 0) {
                return;
            }
            
            this->LoadChannels();
            this->DrawChannelList();
        }
    }
}
