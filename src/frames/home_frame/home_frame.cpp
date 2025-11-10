#include <iostream>
#include <ostream>
#include <wx/event.h>
#include <wx/panel.h>
#include <wx/wx.h>
#include "../../widgets/widgets.hpp"
#include "panels/panels.hpp"
#include "../frames.hpp"

using namespace frames;

HomeFrame::HomeFrame() : wxFrame(nullptr, wxID_ANY, "Home Frame", wxDefaultPosition, wxSize(800, 600)) {
    wxPanel* mainPanel = new wxPanel(this);

    this->menu_bar = new widgets::MenuBar(mainPanel, {"Servers", "Hosting"}, wxHORIZONTAL, [this](){
        this->SelectedMenuChange();
    });

    this->hosting_panel = new home_frame::panels::HostingPanel(mainPanel);
    this->servers_panel = new home_frame::panels::ServersPanel(mainPanel);

    wxBoxSizer* vSizer = new wxBoxSizer(wxVERTICAL);
    vSizer->Add(menu_bar, 0, wxEXPAND);
    vSizer->Add(hosting_panel, 1, wxEXPAND);
    vSizer->Add(servers_panel, 1, wxEXPAND);

    this->hosting_panel->Hide();
    this->servers_panel->Hide();

    mainPanel->SetSizer(vSizer);

    this->SelectedMenuChange();
}

HomeFrame::~HomeFrame() {
    delete this->menu_bar;
    delete this->hosting_panel;
}

void HomeFrame::SelectedMenuChange() {
    if (this->active_panel != nullptr) {
        this->active_panel->Hide();
    }

    static wxPanel* const panels[] = {
        this->servers_panel,
        this->hosting_panel
    };

    this->active_panel = panels[this->menu_bar->GetCurrentIndex()];

    this->active_panel->Show();

    Layout();
    Refresh();
}

home_frame::panels::ServersPanel* HomeFrame::GetServersPanel() {
    return this->servers_panel;
}

home_frame::panels::HostingPanel* HomeFrame::GetHostingPanel() {
    return this->hosting_panel;
}
