#pragma once

#include <cstdint>
#include <vector>
#include <wx/wx.h>
#include "frames/frames.hpp"
#include "objects/objects.hpp"
#include <sqlite3.h>
#include "objects/objects.hpp"

#define DEFAULT_TIMEOUT_CLIENT_CREATION 5000
#define DEFAULT_TIMEOUT_REQUEST 5000

// Request Types

typedef enum {
    SUCCESS,
    FAIL
} RequestStatus;

typedef enum {
    JOIN_SERVER,
    LOAD_SERVER_INFORMATION,
    LOAD_CHANNEL_DATA,
    SEND_MESSAGE
} RequestType;

typedef struct {
    RequestType request_type;
} RequestInfo;

// Requests
typedef struct {
    uint32_t channel_id;
} LoadChannelDataRequest;

typedef struct {
    uint32_t message_len;
    uint32_t channel_id;
} SendMessageRequest;

typedef struct {
    char username[20];
} JoinServerRequest;

// Responses
typedef struct {
    RequestStatus status;
} JoinServerResponse;

typedef struct {
    RequestStatus status;
    uint32_t server_chat_channels_size;
    objects::Database::ServerChatChannelRow server_chat_channels[];
} LoadServerInformationResponse;

typedef struct {
    RequestStatus status;
    uint32_t channel_messages_len;
} LoadChannelDataResponse;

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
    frames::ChatRoomFrame* GetChatRoomFrameById(const uint16_t server_id);
private:
    frames::HomeFrame* home_frame;

    objects::Database* database;

    std::vector<frames::ChatRoomFrame*> chat_room_frames;
    std::vector<frames::ServerSettingsFrame*> server_settings_frames;
};

wxDECLARE_APP(Application);
