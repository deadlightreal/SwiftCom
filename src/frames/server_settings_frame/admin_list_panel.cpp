#include "../frames.hpp"
#include <cstdint>
#include <cstdio>
#include <wx/event.h>
#include <wx/osx/stattext.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include "../../main.hpp"

using frames::ServerSettingsFrame;

ServerSettingsFrame::AdminListPanel::AdminListPanel(wxPanel* parent, const uint16_t server_id) : wxPanel(parent), server_id(server_id) {
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    this->user_list_panel = new wxPanel(this);
    this->user_list_panel->SetSizer(new wxBoxSizer(wxVERTICAL));
    this->user_list_panel->SetMaxSize(wxSize(300, -1));

    mainSizer->Add(this->user_list_panel, 0, wxTOP | wxALIGN_CENTER_HORIZONTAL, 50);

    this->SetSizer(mainSizer);

    this->RenderUserList();
}

ServerSettingsFrame::AdminListPanel::~AdminListPanel() {

}

void ServerSettingsFrame::AdminListPanel::RenderUserList() {
    this->user_list_panel->DestroyChildren();

    auto list_sizer = this->user_list_panel->GetSizer();

    auto users = wxGetApp().GetDatabase()->SelectHostedServerUsers(server_id);

    for (auto &user : *users) {
        auto user_panel = new widgets::StyledPanel(this->user_list_panel);
        user_panel->SetMinSize(wxSize(300, 40));
        user_panel->SetMaxSize(wxSize(300, 40));

        auto username_text = new wxStaticText(user_panel, wxID_ANY, user.username);

        auto make_admin_button = new widgets::Button(user_panel, "Make Admin", [](wxMouseEvent&){});
        make_admin_button->SetMinSize(wxSize(-1, 40));

        auto user_panel_sizer = new wxBoxSizer(wxHORIZONTAL);

        user_panel_sizer->Add(username_text, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);
        user_panel_sizer->AddStretchSpacer(1);
        user_panel_sizer->Add(make_admin_button, 1, wxALIGN_CENTER_VERTICAL);

        user_panel->SetSizer(user_panel_sizer);

        list_sizer->Add(user_panel, 1);
    }

    delete users;
}
