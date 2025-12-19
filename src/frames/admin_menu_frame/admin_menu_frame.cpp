#include "../frames.hpp"
#include <wx/event.h>
#include <wx/utils.h>
#include "../../main.hpp"
#include "swift_net.h"

using AdminMenuFrame = frames::AdminMenuFrame;

AdminMenuFrame::AdminMenuFrame(const char* ip_address, uint16_t server_id) : wxFrame(wxGetApp().GetHomeFrame(), wxID_ANY, "Admin Menu", wxDefaultPosition, wxSize(800, 600)) {
    auto* main_panel = new wxPanel(this, wxID_ANY);
    main_panel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_FRAMEBK));

    auto* main_sizer = new wxBoxSizer(wxHORIZONTAL);

    menu_bar = new widgets::MenuBar(main_panel, {"Text Channels"}, wxVERTICAL, [this]() {
        SelectedMenuChange();
    });
    menu_bar->SetMinSize(wxSize(90, -1));

    client_connection = swiftnet_create_client(ip_address, server_id, DEFAULT_TIMEOUT_CLIENT_CREATION);
    if (!client_connection) {
        wxMessageBox("Failed to connect to server.", "Connection Error", wxOK | wxICON_ERROR);
        return;
    }

    channels_panel = new Channels(main_panel, client_connection);

    main_sizer->Add(menu_bar, wxSizerFlags(0).Expand());
    main_sizer->Add(channels_panel, wxSizerFlags(1).Expand());

    main_panel->SetSizerAndFit(main_sizer);

    SelectedMenuChange();
}

AdminMenuFrame::~AdminMenuFrame() = default;

void AdminMenuFrame::SelectedMenuChange()
{
    if (active_menu) {
        active_menu->Hide();
    }

    static wxPanel* menus[] = {
        channels_panel
    };

    const size_t index = menu_bar->GetCurrentIndex();
    if (index >= std::size(menus)) {
        return;
    }

    active_menu = menus[index];
    active_menu->Show();

    active_menu->Layout();
    Layout();
}
