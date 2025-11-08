#include <cstdlib>
#include <ctime>
#include <iostream>
#include <wx/wx.h>
#include "frames/frames.hpp"
#include "objects/objects.hpp"
#include "swift_net.h"
#include "main.hpp"

Application::Application() {
    this->database = new objects::Database();

    this->home_frame = new frames::HomeFrame();
}

Application::~Application() {
    const objects::Database* database = this->GetDatabase();
    
    if (database != nullptr) {
        delete database;
    }

    swiftnet_cleanup();
}

bool Application::OnInit() {
    swiftnet_initialize();

    swiftnet_add_debug_flags(DEBUG_INITIALIZATION | DEBUG_LOST_PACKETS | DEBUG_PACKETS_RECEIVING | DEBUG_PACKETS_SENDING);

    srand(time(0));

    frames::HomeFrame* home_frame = this->GetHomeFrame();

    home_frame->GetHostingPanel()->DrawServers();
    home_frame->GetServersPanel()->DrawServers();

    home_frame->Show(true);
    return true;
}

void Application::AddChatRoomFrame(frames::ChatRoomFrame* frame) {
    this->chat_room_frames.push_back(frame);
}

frames::ChatRoomFrame* Application::GetChatRoomFrameById(const uint16_t server_id) {
    for (auto server : *this->GetChatRoomFrames()) {
        std::cout << server->GetServerId() << " : " << server_id << std::endl;

        if (server->GetServerId() == server_id) {
            return server;
        }
    }

    return nullptr;
}

std::vector<frames::ChatRoomFrame*>* Application::GetChatRoomFrames() {
    return &this->chat_room_frames;
}

frames::HomeFrame* Application::GetHomeFrame() {
    return this->home_frame;
}

objects::Database* Application::GetDatabase() {
    return this->database;
}

wxIMPLEMENT_APP(Application);
