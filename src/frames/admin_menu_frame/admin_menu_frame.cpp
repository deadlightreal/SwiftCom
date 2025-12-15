#include "../frames.hpp"
#include <wx/event.h>
#include <wx/utils.h>
#include "../../main.hpp"
#include "swift_net.h"

using AdminMenuFrame = frames::AdminMenuFrame;

AdminMenuFrame::AdminMenuFrame(const char* ip_address, const uint16_t server_id) : wxFrame(wxGetApp().GetHomeFrame(), wxID_ANY, "Admin Menu", wxDefaultPosition, wxSize(800, 600)) {
    auto main_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);

    auto main_sizer = new wxBoxSizer(wxHORIZONTAL);

    this->menu_bar = new widgets::MenuBar(main_panel, {"Text Channels"}, wxVERTICAL, [this](){
        this->SelectedMenuChange();
    });

    this->client_connection = swiftnet_create_client(ip_address, server_id, DEFAULT_TIMEOUT_CLIENT_CREATION);
    if (this->client_connection == nullptr) {
        return;
    }

    this->channels_panel = new frames::AdminMenuFrame::Channels(main_panel, this->client_connection);

    main_sizer->Add(this->menu_bar, 0, wxEXPAND);
    main_sizer->Add(this->channels_panel, 1, wxEXPAND);

    this->SelectedMenuChange();

    main_panel->SetSizer(main_sizer);
}

AdminMenuFrame::~AdminMenuFrame() {

}

void AdminMenuFrame::SelectedMenuChange() {
    if (this->active_menu != nullptr) {
        this->active_menu->Hide();
    }

    static wxPanel* const menus[] = {
        this->channels_panel
    };

    this->active_menu = menus[this->menu_bar->GetCurrentIndex()];

    this->active_menu->Show();

    Refresh();
    Layout();
}
