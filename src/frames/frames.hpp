#pragma once

#include <cstdint>
#include <netinet/in.h>
#include <wx/event.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/wx.h>
#include "../widgets/widgets.hpp"
#include "home_frame/panels/panels.hpp"
#include <vector>
#include "../objects/objects.hpp"
#include <swift_net.h>

namespace frames {
    class HomeFrame : public wxFrame {
    public:
        HomeFrame();
        ~HomeFrame();
        
        void SelectedMenuChange();

        frames::home_frame::panels::HostingPanel* GetHostingPanel();
        frames::home_frame::panels::ServersPanel* GetServersPanel();
    private:
        frames::home_frame::panels::HostingPanel* hosting_panel;
        frames::home_frame::panels::ServersPanel* servers_panel;

        wxPanel* active_panel = nullptr;
        
        widgets::MenuBar* menu_bar;
        
        uint8_t current_menu;
    };

    class AdminMenuFrame : public wxFrame {
    public:
        AdminMenuFrame(const char* ip_address, const uint16_t server_id);
        ~AdminMenuFrame();

        void SelectedMenuChange();

        class Channels : public wxPanel {
            public:
                Channels(wxWindow* parent, SwiftNetClientConnection* client_connection);
                ~Channels();

            private:
                void OnAddChannel(wxMouseEvent& evt);
                void LoadChannels();
                void DrawChannelList();
                int CreateNewTextChannel(const char* name);

                wxScrolled<wxPanel>* scrollPanel;
                wxBoxSizer* channelsSizer;

                std::vector<objects::Database::ServerChatChannelRow> chat_channels;

                DECLARE_EVENT_TABLE()

                SwiftNetClientConnection* client_connection;
        };

        private:
            SwiftNetClientConnection* client_connection;
            widgets::MenuBar* menu_bar;
            wxPanel* active_menu = nullptr;
            AdminMenuFrame::Channels* channels_panel;
    };

    class ServerSettingsFrame : public wxFrame {
    public:
        class AdminListPanel : public wxPanel {
        public:
            AdminListPanel(wxPanel* parent, const uint16_t server_idip_address);
            ~AdminListPanel();

            void RenderAdminList();
            void RenderMemberList();
        private:
            void MakeUserAdmin(const char* username);
            void RemoveUserAdmin(const char* username);

            wxPanel* admin_list_panel;
            wxPanel* member_list_panel;

            uint16_t server_id;
        };

        ServerSettingsFrame(const uint16_t server_id, const in_addr_t ip_address);
        ~ServerSettingsFrame();

        void SelectedMenuChange();
    private:
        uint16_t server_id;
        in_addr_t ip_address;
        uint32_t menu_selected_index;

        wxPanel* active_menu = nullptr;

        wxPanel* admin_list_panel;

        widgets::MenuBar* menu_bar;
    };

    class ChatRoomFrame : public wxFrame {
    public:
        class ChatChannel {
        public:
            ChatChannel(const uint32_t id, char name[20]);
            ~ChatChannel();

            uint32_t GetId();
            const char* GetName();
        private:
            uint32_t id;
            char name[20];
        };

        class ChatPanel : public wxPanel {
        public:
            ChatPanel(const uint32_t channel_id, const uint16_t server_id, wxWindow* parent_window, const in_addr ip_address);
            ~ChatPanel();

            void InitializeConnection(const in_addr ip_address);

            void LoadChannelData();

            void SendMessage(const char* message, const uint32_t message_len);

            uint16_t GetServerId();
            uint32_t GetChannelId();
            SwiftNetClientConnection* GetClientConnection();
            std::vector<objects::Database::ChannelMessageRow>* GetChannelMessages();
            wxPanel* GetMessagesPanel();
            wxTextCtrl* GetNewMessageInput();
        private:
            void HandleLoadChannelDataRequest(SwiftNetClientPacketData* const packet_data);
            void RedrawMessages();

            SwiftNetClientConnection* client_connection;
            
            uint32_t channel_id;
            uint16_t server_id;

            wxTextCtrl* new_message_input;
            wxPanel* messages_panel;
            wxBoxSizer* messages_sizer;

            std::vector<objects::Database::ChannelMessageRow> channel_messages;
        };

        ChatRoomFrame(const in_addr ip_address, const uint16_t server_id);
        ~ChatRoomFrame();

        void DrawChannels();

        void LoadServerInformation();

        void HandleLoadServerInfoResponse(SwiftNetClientPacketData* packet_data);

        uint16_t GetServerId();
        in_addr GetServerIpAddress();
        ChatPanel* GetChatPanel();
        SwiftNetClientConnection* GetConnection();
        std::vector<ChatChannel*>* GetChatChannels();
    private:
        void UpdateMainSizer();
        
        wxPanel* channel_list_panel = nullptr;

        uint16_t server_id;
        in_addr server_ip_address;

        SwiftNetClientConnection* client_connection;
        
        std::vector<ChatChannel*> chat_channels;

        ChatPanel* chat_panel = nullptr;

        wxPanel* main_panel = nullptr;
        wxBoxSizer* main_sizer = nullptr;
    };
}
