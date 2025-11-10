#pragma once

#include <cstdint>
#include <wx/wx.h>
#include "../../../objects/objects.hpp"
#include <vector>

namespace frames::home_frame::panels {
    class HostingPanel : public wxPanel {
    public:
        HostingPanel(wxPanel* parent_panel);
        ~HostingPanel();

        void DrawServers();

        void InsertHostedServer(const uint16_t server_id);

        objects::HostedServer* GetServerById(const uint16_t server_id);

        std::vector<objects::HostedServer>* GetHostedServers();
    private:
        void CreateNewServer(wxMouseEvent&);

        wxPanel* hosted_servers_panel;

        std::vector<objects::HostedServer> hosted_servers;
    };

    class ServersPanel : public wxPanel {
    public:
        class AddServerPopupMenu : public wxDialog {
        public:
            enum AddServerReturnCode {
                SUCCESS,
                INVALID_LENGTH
            };

            enum RequestServerExistationStatus {
                SUCCESSFULLY_CONNECTED,
                NO_RESPONSE,
                UNKNOWN_RESPONSE
            };

            AddServerPopupMenu(wxWindow* parent, wxPoint pos);
            ~AddServerPopupMenu();

            enum RequestServerExistationStatus RequestServerExistsConfirmation(const char* ip_address, const uint16_t server_id, const in_addr address);

            AddServerReturnCode AddServer(wxString input);
        };

        ServersPanel(wxPanel* parent_panel);
        ~ServersPanel();

        void DrawServers();

        void OpenAddServerPopupMenu(wxMouseEvent& event, wxWindow* parent);

        std::vector<objects::JoinedServer>* GetJoinedServers();
    private:
        std::vector<objects::JoinedServer> joined_servers;

        wxPanel* joined_servers_panel;
    };
}
