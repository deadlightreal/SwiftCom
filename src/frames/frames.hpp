#pragma once

#include <cstdint>
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
        
        void Frame(wxTimerEvent& event);

        frames::home_frame::panels::HostingPanel* GetHostingPanel();
        frames::home_frame::panels::ServersPanel* GetServersPanel();
    private:
        frames::home_frame::panels::HostingPanel* hosting_panel;
        frames::home_frame::panels::ServersPanel* servers_panel;
        
        widgets::MenuBar* menu_bar;
        
        uint8_t current_menu;
        
        wxTimer timer;
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

            SwiftNetClientConnection* client_connection;
            
            uint32_t channel_id;
            uint16_t server_id;

            wxTextCtrl* new_message_input;
            wxPanel* messages_panel;

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
        wxPanel* channel_list_panel = nullptr;

        uint16_t server_id;
        in_addr server_ip_address;

        SwiftNetClientConnection* client_connection;
        
        std::vector<ChatChannel*> chat_channels;

        ChatPanel* chat_panel = nullptr;
    };
}
