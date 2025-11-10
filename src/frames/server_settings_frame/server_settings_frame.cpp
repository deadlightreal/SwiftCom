#include "../frames.hpp"
#include "../../main.hpp"
#include <cstdint>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/osx/frame.h>
#include <wx/panel.h>
#include <wx/sizer.h>

using frames::ServerSettingsFrame;

ServerSettingsFrame::ServerSettingsFrame(const uint16_t server_id, const in_addr_t ip_address) : wxFrame(wxGetApp().GetHomeFrame(), wxID_ANY, "Server Settings", wxDefaultPosition, wxSize(800, 600)), server_id(server_id), ip_address(ip_address) {
    auto main_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);

    auto main_sizer = new wxBoxSizer(wxHORIZONTAL);

    this->menu_bar = new widgets::MenuBar(main_panel, {"Admin List"}, wxVERTICAL, [this](){
        this->SelectedMenuChange();
    });

    this->admin_list_panel = new ServerSettingsFrame::AdminListPanel(main_panel, server_id);

    main_sizer->Add(this->menu_bar, 0, wxEXPAND);
    main_sizer->Add(this->admin_list_panel, 1, wxEXPAND);

    this->admin_list_panel->Hide();

    this->SelectedMenuChange();

    main_panel->SetSizer(main_sizer);
}

void ServerSettingsFrame::SelectedMenuChange() {
    if (this->active_menu != nullptr) {
        this->active_menu->Hide();
    }

    static wxPanel* const menus[] = {
        this->admin_list_panel
    };

    this->active_menu = menus[this->menu_bar->GetCurrentIndex()];

    this->active_menu->Show();

    Refresh();
    Layout();
}

ServerSettingsFrame::~ServerSettingsFrame() {
    auto const server_settings_frames = wxGetApp().GetServerSettingsFrames();
    for (uint32_t i = 0; i < server_settings_frames->size(); i++) {
        if (server_settings_frames->at(i) == this) {
            server_settings_frames->erase(server_settings_frames->begin() + i);

            break;
        }
    }
}
