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
    this->menu_bar->SetMinSize(wxSize(-1, 30));

    this->hosting_panel = new home_frame::panels::HostingPanel(mainPanel);
    this->servers_panel = new home_frame::panels::ServersPanel(mainPanel);

    wxBoxSizer* vSizer = new wxBoxSizer(wxVERTICAL);
    vSizer->Add(menu_bar, wxSizerFlags(0).Expand());
    vSizer->Add(hosting_panel, wxSizerFlags(1).Expand());
    vSizer->Add(servers_panel, wxSizerFlags(1).Expand());

    this->hosting_panel->Hide();
    this->servers_panel->Hide();

    mainPanel->SetSizerAndFit(vSizer);

    this->SelectedMenuChange();
}

HomeFrame::~HomeFrame() {
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
}

home_frame::panels::ServersPanel* HomeFrame::GetServersPanel() {
    return this->servers_panel;
}

home_frame::panels::HostingPanel* HomeFrame::GetHostingPanel() {
    return this->hosting_panel;
}
