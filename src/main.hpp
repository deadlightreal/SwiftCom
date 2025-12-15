#pragma once

#include <cstdint>
#include <wx/wx.h>
#include <sqlite3.h>
#include "objects/objects.hpp"
#include "frames/frames.hpp"
#include <swift_net.h>

#define DEFAULT_TIMEOUT_CLIENT_CREATION 500
#define DEFAULT_TIMEOUT_REQUEST 200
#define LOOPBACK false

// Request Types

enum Status {
    SUCCESS,
    FAIL
};

enum RequestType {
    JOIN_SERVER,
    LOAD_SERVER_INFORMATION,
    LOAD_CHANNEL_DATA,
    SEND_MESSAGE,
    LOAD_JOINED_SERVER_DATA,
    LOAD_ADMIN_MENU_DATA,
    CREATE_NEW_CHANNEL
};

struct RequestInfo {
    enum RequestType request_type;
};

struct ResponseInfo {
    enum RequestType request_type;
    enum Status request_status;
};

// Requests

namespace requests {
    struct LoadChannelDataRequest {
        uint32_t channel_id;
    };

    struct SendMessageRequest {
        uint32_t message_len;
        uint32_t channel_id;
    };

    struct JoinServerRequest {
        char username[20];
    };

    struct LoadJoinedServerDataRequest {
    };

    struct LoadAdminMenuDataRequest {
    };

    struct CreateNewChannelRequest {
        char name[20];
    };
}

// Responses

namespace responses {
    struct JoinServerResponse {
    };

    struct LoadServerInformationResponse {
        uint32_t server_chat_channels_size;
    };

    struct LoadChannelDataResponse {
        uint32_t channel_messages_len;
    };

    struct LoadJoinedServerDataResponse {
        bool admin;
    };

    struct LoadAdminMenuDataResponse {
        uint32_t channels_size;
    };

    struct CreateNewChannelResponse {

    };
}

class Application : public wxApp
{
public:
    Application();
    ~Application();

    virtual bool OnInit();

    void AddChatRoomFrame(frames::ChatRoomFrame* frame);

    objects::Database* GetDatabase();
    frames::HomeFrame* GetHomeFrame();
    std::vector<frames::ChatRoomFrame*>* GetChatRoomFrames();
    std::vector<frames::ServerSettingsFrame*>* GetServerSettingsFrames();
    std::vector<frames::AdminMenuFrame*>* GetAdminMenuFrames();
    frames::ChatRoomFrame* GetChatRoomFrameById(const uint16_t server_id);
private:
    frames::HomeFrame* home_frame;

    objects::Database* database;

    std::vector<frames::ChatRoomFrame*> chat_room_frames;
    std::vector<frames::ServerSettingsFrame*> server_settings_frames;
    std::vector<frames::AdminMenuFrame*> admin_menu_frames;
};

wxDECLARE_APP(Application);
