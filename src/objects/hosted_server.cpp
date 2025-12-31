#include "objects.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <pthread.h>
#include <stdatomic.h>
#include <swift_net.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include "../main.hpp"

using namespace objects;

static void SerializeChannelMessages(SwiftNetPacketBuffer* buffer, std::vector<Database::ChannelMessageRow>* messages) {
    for (auto &message : *messages) {
        const uint32_t new_message_len = message.message_length + 1;
        
        swiftnet_server_append_to_packet(&message.id, sizeof(message.id), buffer);
        swiftnet_server_append_to_packet(&message.sender_id, sizeof(message.sender_id), buffer);
        swiftnet_server_append_to_packet(&new_message_len, sizeof(message.message_length), buffer);
        swiftnet_server_append_to_packet(message.message, new_message_len, buffer);
        swiftnet_server_append_to_packet(message.sender_username, sizeof(message.sender_username), buffer);

        printf("Serializing message: %s\n", message.message);
    }
}

struct BackgroundProcessNewMessagesChannel {
    uint32_t bytes_to_allocate;
    SwiftNetPacketBuffer buffer;
    std::vector<objects::Database::ChannelMessageRow> messages;
};

void HostedServer::BackgroundProcesses() {
    while (true) {
        if (atomic_load_explicit(&this->stop_background_processes, memory_order_acquire) == true) {
            break;
        }

        if (this->GetNewMessages()->size() == 0) {
            usleep(200000);
            continue;
        }

        auto channel_new_messages = std::unordered_map<uint32_t, BackgroundProcessNewMessagesChannel>();

        for (auto &new_message : *this->GetNewMessages()) {
            auto it = channel_new_messages.find(new_message.channel_id);
            if (it == channel_new_messages.end()) {
                channel_new_messages.emplace(new_message.channel_id, (BackgroundProcessNewMessagesChannel){.bytes_to_allocate = sizeof(ResponseInfo) + sizeof(responses::PeriodicChatUpdateResponse), .messages = std::vector<objects::Database::ChannelMessageRow>()});
                channel_new_messages.at(new_message.channel_id).messages.push_back(new_message);
                channel_new_messages.at(new_message.channel_id).bytes_to_allocate += new_message.message_length + 1 + sizeof(new_message.message_length) + sizeof(new_message.id) + sizeof(new_message.sender_id) + sizeof(new_message.sender_username);

                continue;
            } else {
                it->second.bytes_to_allocate += new_message.message_length + 1 + sizeof(new_message.message_length) + sizeof(new_message.id) + sizeof(new_message.sender_id) + sizeof(new_message.sender_username);
                it->second.messages.push_back(new_message);

                continue;
            }
        }

        for (auto& [channel_id, channel_new_message] : channel_new_messages) {
            const ResponseInfo response_info = {
                .request_type = RequestType::PERIODIC_CHAT_UPDATE,
                .request_status = Status::SUCCESS
            };

            const responses::PeriodicChatUpdateResponse response = {
                .channel_messages_len = static_cast<uint32_t>(channel_new_message.messages.size())
            };
            
            channel_new_message.buffer = swiftnet_server_create_packet_buffer(channel_new_message.bytes_to_allocate);

            swiftnet_server_append_to_packet(&response_info, sizeof(response_info), &channel_new_message.buffer);
            swiftnet_server_append_to_packet(&response, sizeof(response), &channel_new_message.buffer);

            SerializeChannelMessages(&channel_new_message.buffer, &channel_new_message.messages);
        }

        for (auto &user : *this->GetServerUsers()) {
            if (user.status == ServerUserStatus::OFFLINE) {
                continue;
            }

            auto now = std::chrono::steady_clock::now();
            auto elapsed = now - user.time_since_last_request;

            if (elapsed > std::chrono::seconds(60)) {
                const RequestInfo online_check_req_info = {
                    .request_type = RequestType::CLIENT_ONLINE_CHECK
                };

                auto online_check_buffer = swiftnet_server_create_packet_buffer(sizeof(online_check_req_info));

                swiftnet_server_append_to_packet(&online_check_req_info, sizeof(online_check_req_info), &online_check_buffer);

                auto online_check_response = swiftnet_server_make_request(server, &online_check_buffer, user.addr_data, DEFAULT_TIMEOUT_REQUEST);

                swiftnet_server_destroy_packet_buffer(&online_check_buffer);

                if (online_check_response == nullptr) {
                    user.status = ServerUserStatus::OFFLINE,
                    memset(&user.addr_data, 0x00, sizeof(user.addr_data));

                    continue;
                }

                this->MarkUserOnline(&user);

                swiftnet_server_destroy_packet_data(online_check_response, server);
            }

            auto it = channel_new_messages.find(user.active_channel_id);
            if (it == channel_new_messages.end()) {
                continue;
            }

            swiftnet_server_send_packet(this->GetServer(), &it->second.buffer, user.addr_data);
        }

        for (auto& [channel_id, channel_new_message] : channel_new_messages) {
            swiftnet_server_destroy_packet_buffer(&channel_new_message.buffer);
        }

        for (auto &new_message : *this->GetNewMessages()) {
            free((void*)new_message.message);
        }

        this->GetNewMessages()->clear();

        usleep(200000);
    }
}

static void HandleLoadChannelDataRequest(HostedServer* server, SwiftNetServerPacketData* packet_data) {
    requests::LoadChannelDataRequest* request_data = (requests::LoadChannelDataRequest*)swiftnet_server_read_packet(packet_data, sizeof(requests::LoadChannelDataRequest));

    Database* database = wxGetApp().GetDatabase();

    ServerUser* user = server->GetUserByAddrData(packet_data->metadata.sender);
    if (user == nullptr) {
        printf("User is not registered as member of this server\n");
        swiftnet_server_destroy_packet_data(packet_data, server->GetServer());
        return;
    }

    if (user->status == ServerUserStatus::ONLINE) {
        user->active_channel_id = request_data->channel_id;
    } else {
        user->addr_data = packet_data->metadata.sender;
        printf("Setting addr data\n");
        server->MarkUserOnline(user);
        user->active_channel_id = request_data->channel_id;
    }

    auto channel_messages = database->SelectChannelMessages(std::nullopt, nullptr, std::nullopt, request_data->channel_id);

    uint32_t bytes_to_allocate = (sizeof(responses::LoadChannelDataResponse) + sizeof(ResponseInfo));

    for (auto &message : *channel_messages) {
        bytes_to_allocate += sizeof(message.id) + sizeof(message.sender_id) + sizeof(message.message_length) + sizeof(message.sender_username) + message.message_length + 1;
    }

    const ResponseInfo response_info = {
        .request_type = RequestType::LOAD_CHANNEL_DATA,
        .request_status = Status::SUCCESS
    };

    const responses::LoadChannelDataResponse response_request_data = {
        .channel_messages_len = (uint32_t)channel_messages->size()
    };

    SwiftNetPacketBuffer buffer = swiftnet_server_create_packet_buffer(bytes_to_allocate);

    swiftnet_server_append_to_packet(&response_info, sizeof(response_info), &buffer);
    swiftnet_server_append_to_packet(&response_request_data, sizeof(response_request_data), &buffer);

    SerializeChannelMessages(&buffer, channel_messages);

    swiftnet_server_make_response(server->GetServer(), packet_data, &buffer);

    swiftnet_server_destroy_packet_buffer(&buffer);
    swiftnet_server_destroy_packet_data(packet_data, server->GetServer());

    for (auto& message : *channel_messages) {
        free((void*)message.message);
    }

    delete channel_messages;
}

static void HandleJoinServerRequest(HostedServer* server, SwiftNetServerPacketData* packet_data) {
    const in_addr ip_address = packet_data->metadata.sender.sender_address;
    const uint16_t server_id = server->GetServerId();

    const char* username = (const char*)swiftnet_server_read_packet(packet_data, 20);

    printf("Inserting user: %s %d\n", username, ip_address.s_addr);

    auto result = wxGetApp().GetDatabase()->InsertHostedServerUser(server_id, ip_address, username);
    if (!result.has_value()) {
        const ResponseInfo response_info = {
            .request_type = RequestType::JOIN_SERVER,
            .request_status = Status::FAIL
        };

        const responses::JoinServerResponse response = {
        };

        SwiftNetPacketBuffer buffer = swiftnet_server_create_packet_buffer(sizeof(response) + sizeof(response_info));

        swiftnet_server_append_to_packet(&response_info, sizeof(response_info), &buffer);
        swiftnet_server_append_to_packet(&response, sizeof(response), &buffer);

        swiftnet_server_make_response(server->GetServer(), packet_data, &buffer);

        swiftnet_server_destroy_packet_buffer(&buffer);
        swiftnet_server_destroy_packet_data(packet_data, server->GetServer());
    }

    SwiftNetPacketBuffer buffer = swiftnet_server_create_packet_buffer(sizeof(responses::JoinServerResponse) + sizeof(ResponseInfo));

    const ResponseInfo response_info = {
        .request_type = RequestType::JOIN_SERVER,
        .request_status = Status::SUCCESS
    };

    const responses::JoinServerResponse response = {
    };

    swiftnet_server_append_to_packet(&response_info, sizeof(response_info), &buffer);
    swiftnet_server_append_to_packet(&response, sizeof(response), &buffer);

    swiftnet_server_make_response(server->GetServer(), packet_data, &buffer);

    server->GetServerUsers()->push_back((ServerUser){.data = result.value(), .status = ServerUserStatus::OFFLINE, .addr_data = packet_data->metadata.sender});

    swiftnet_server_destroy_packet_buffer(&buffer);
    swiftnet_server_destroy_packet_data(packet_data, server->GetServer());
}

static void HandleLoadAdminMenuDataRequest(HostedServer* server, SwiftNetServerPacketData* packet_data) {
    auto const request = (requests::LoadAdminMenuDataRequest*)swiftnet_server_read_packet(packet_data, sizeof(requests::LoadAdminMenuDataRequest));

    auto channels = wxGetApp().GetDatabase()->SelectServerChatChannels(std::nullopt, nullptr, server->GetServerId());

    ResponseInfo response_info = {
        .request_status = Status::SUCCESS,
        .request_type = RequestType::LOAD_ADMIN_MENU_DATA
    };

    responses::LoadAdminMenuDataResponse response = {
        .channels_size = static_cast<uint32_t>(channels->size())
    };
    
    SwiftNetPacketBuffer buffer = swiftnet_server_create_packet_buffer(sizeof(responses::LoadAdminMenuDataResponse) + sizeof(ResponseInfo) + (channels->size() * (sizeof(objects::Database::ServerChatChannelRow))));

    swiftnet_server_append_to_packet(&response_info, sizeof(response_info), &buffer);
    swiftnet_server_append_to_packet(&response, sizeof(response), &buffer);

    for (auto &channel : *channels) {
        swiftnet_server_append_to_packet(&channel, sizeof(channel), &buffer);
    }

    swiftnet_server_make_response(server->GetServer(), packet_data, &buffer);

    swiftnet_server_destroy_packet_data(packet_data, server->GetServer());
    swiftnet_server_destroy_packet_buffer(&buffer);

    delete channels;
}

static void HandleLoadServerInformationRequest(HostedServer* server, SwiftNetServerPacketData* packet_data) {
    auto server_chat_channels = wxGetApp().GetDatabase()->SelectServerChatChannels(std::nullopt, nullptr, server->GetServerId());

    SwiftNetServer* server_swiftnet = server->GetServer();

    const uint32_t size = server_chat_channels->size();

    const uint32_t bytes_to_alloc = sizeof(responses::LoadServerInformationResponse) + (size * sizeof(Database::ServerChatChannelRow));
    
    const ResponseInfo response_info = {
        .request_type = LOAD_SERVER_INFORMATION,
    };

    responses::LoadServerInformationResponse response_data = {
        .server_chat_channels_size = size
    };

    SwiftNetPacketBuffer buffer = swiftnet_server_create_packet_buffer(bytes_to_alloc + sizeof(response_info));

    swiftnet_server_append_to_packet(&response_info, sizeof(response_info), &buffer);
    swiftnet_server_append_to_packet(&response_data, sizeof(response_data), &buffer);

    if (size > 0) {
        swiftnet_server_append_to_packet(server_chat_channels->data(), size * sizeof(Database::ServerChatChannelRow), &buffer);

        for (auto &channel : *server_chat_channels) {
            printf("%s, %d, %d\n", channel.name, channel.id, channel.hosted_server_id);
        }
    }

    swiftnet_server_make_response(server_swiftnet, packet_data, &buffer);

    swiftnet_server_destroy_packet_buffer(&buffer);
    swiftnet_server_destroy_packet_data(packet_data, server->GetServer());

    delete server_chat_channels;
}

static void HandleLoadJoinedServerDataRequest(HostedServer* server, SwiftNetServerPacketData* packet_data) {
    const in_addr sender = packet_data->metadata.sender.sender_address;

    auto query_result = wxGetApp().GetDatabase()->SelectHostedServerUsers(server->GetServerId(), std::nullopt, nullptr, sender.s_addr);
    if (query_result->size() == 0) {
        return;
    }

    auto member = query_result->at(0);

    free(query_result);

    const ResponseInfo response_info = {
        .request_status = Status::SUCCESS,
        .request_type = RequestType::LOAD_JOINED_SERVER_DATA
    };

    std::cout << "User type: " << member.user_type << std::endl;
    std::cout << "Admin User type: " << Database::UserType::Admin << std::endl;
    std::cout << "Is admin: " << (member.user_type == Database::UserType::Admin) << std::endl;;

    const responses::LoadJoinedServerDataResponse response = {
        .admin = member.user_type == Database::UserType::Admin
    };

    SwiftNetPacketBuffer buffer = swiftnet_server_create_packet_buffer(sizeof(response_info) + sizeof(response));

    swiftnet_server_append_to_packet(&response_info, sizeof(response_info), &buffer);
    swiftnet_server_append_to_packet(&response, sizeof(response), &buffer);

    swiftnet_server_make_response(server->GetServer(), packet_data, &buffer);

    swiftnet_server_destroy_packet_buffer(&buffer);
    swiftnet_server_destroy_packet_data(packet_data, server->GetServer());

    return;
}

static void HandleSendMessageRequest(HostedServer* server, SwiftNetServerPacketData* packet_data) {
    requests::SendMessageRequest* request = (requests::SendMessageRequest*)swiftnet_server_read_packet(packet_data, sizeof(requests::SendMessageRequest));

    const char* message = (const char*)swiftnet_server_read_packet(packet_data, request->message_len);

    ServerUser* user = server->GetUserByAddrData(packet_data->metadata.sender);
    if (user == nullptr || user->status == ServerUserStatus::OFFLINE) {
        std::cout << "User not connected" << std::endl;

        swiftnet_server_destroy_packet_data(packet_data, server->GetServer());

        return;
    }

    char* message_clone = (char*)malloc(request->message_len);

    memcpy(message_clone, message, request->message_len);

    auto result = wxGetApp().GetDatabase()->InsertChannelMessage(message_clone, request->channel_id, user->data.id);

    if (result.has_value()) {
        printf("new message username: %s\n", result.value().sender_username);
        server->GetNewMessages()->push_back(result.value());
    } else {
        free(message_clone);
    }

    server->MarkUserOnline(user);

    swiftnet_server_destroy_packet_data(packet_data, server->GetServer());
}

static void HandleCreateNewChannelRequest(HostedServer* server, SwiftNetServerPacketData* packet_data) {
    auto request = (requests::CreateNewChannelRequest*)swiftnet_server_read_packet(packet_data, sizeof(requests::CreateNewChannelRequest));

    int result = wxGetApp().GetDatabase()->InsertServerChatChannel(request->name, server->GetServerId());

    const ResponseInfo response_info = {
        .request_status = result == 0 ? Status::SUCCESS : Status::FAIL,
        .request_type = RequestType::CREATE_NEW_CHANNEL
    };

    const responses::CreateNewChannelResponse response = {};

    auto buffer = swiftnet_server_create_packet_buffer(sizeof(response_info) + sizeof(response));

    swiftnet_server_append_to_packet(&response_info, sizeof(response_info), &buffer);
    swiftnet_server_append_to_packet(&response, sizeof(response), &buffer);

    swiftnet_server_make_response(server->GetServer(), packet_data, &buffer);

    swiftnet_server_destroy_packet_buffer(&buffer);
    swiftnet_server_destroy_packet_data(packet_data, server->GetServer());
}

static void PacketCallback(SwiftNetServerPacketData* packet_data, void* const user) {
    const uint16_t server_id = packet_data->metadata.port_info.destination_port;

    HostedServer* server = wxGetApp().GetHomeFrame()->GetHostingPanel()->GetServerById(server_id);
    if (server == nullptr) {
        printf("null server\n");
        return;
    }

    RequestInfo* request_info = (RequestInfo*)swiftnet_server_read_packet(packet_data, sizeof(RequestInfo));

    switch (request_info->request_type) {
        case JOIN_SERVER: HandleJoinServerRequest(server, packet_data); break;
        case LOAD_SERVER_INFORMATION: HandleLoadServerInformationRequest(server, packet_data); break;
        case LOAD_CHANNEL_DATA: HandleLoadChannelDataRequest(server, packet_data); break;
        case SEND_MESSAGE: HandleSendMessageRequest(server, packet_data); break;
        case LOAD_JOINED_SERVER_DATA: HandleLoadJoinedServerDataRequest(server, packet_data); break;
        case LOAD_ADMIN_MENU_DATA: HandleLoadAdminMenuDataRequest(server, packet_data); break;
        case CREATE_NEW_CHANNEL: HandleCreateNewChannelRequest(server, packet_data); break;
        default: break;
    }
}

HostedServer::HostedServer(uint16_t id) : id(id) {

};

HostedServer::~HostedServer() = default;

void HostedServer::StartServer() {
    this->status = HostedServerStatus::RUNNING;

    SwiftNetServer* const new_server = swiftnet_create_server(this->GetServerId(), LOOPBACK);
    if (new_server == nullptr) {
        printf("Failed to create new server\n");

        exit(EXIT_FAILURE);
    }

    swiftnet_server_set_message_handler(new_server, PacketCallback, nullptr);

    this->server = new_server;

    auto users = wxGetApp().GetDatabase()->SelectHostedServerUsers(this->GetServerId(), std::nullopt, nullptr, std::nullopt);
    for (auto &user : *users) {
        this->GetServerUsers()->push_back((ServerUser){
            .data = user,
            .status = ServerUserStatus::OFFLINE
        });
    }

    atomic_store_explicit(&this->stop_background_processes, false, memory_order_release);

    this->background_processes_thread = new std::thread([this]() {
        this->BackgroundProcesses();
    });

    wxGetApp().GetHomeFrame()->GetHostingPanel()->DrawServers();
}

void HostedServer::StopServer() {
    this->status = HostedServerStatus::STOPPED;

    swiftnet_server_cleanup(this->GetServer());

    this->server = nullptr;

    this->server_users.clear();

    atomic_store_explicit(&this->stop_background_processes, true, memory_order_release);

    this->background_processes_thread->join();

    delete this->background_processes_thread;

    this->background_processes_thread = nullptr;

    wxGetApp().GetHomeFrame()->GetHostingPanel()->DrawServers();
}

ServerUser* HostedServer::GetUserByAddrData(const SwiftNetClientAddrData addr_data) {
    for (auto &user : *this->GetServerUsers()) {
        if (user.data.ip_address.s_addr == addr_data.sender_address.s_addr) {
            return &user;
        }
    }

    return nullptr;
}

void HostedServer::MarkUserOnline(ServerUser* const user) {
    user->status = ServerUserStatus::ONLINE;
    user->time_since_last_request = std::chrono::steady_clock::now();
}

std::vector<ServerUser>* HostedServer::GetServerUsers() {
    return &this->server_users;
}

SwiftNetServer* HostedServer::GetServer() {
    return this->server;
}

uint16_t HostedServer::GetServerId() {
    return this->id;
}

std::vector<objects::Database::ChannelMessageRow>* HostedServer::GetNewMessages() {
    return &this->new_messages;
}

HostedServerStatus HostedServer::GetServerStatus() {
    return this->status;
}
