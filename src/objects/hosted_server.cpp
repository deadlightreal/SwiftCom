#include "objects.hpp"
#include <arpa/inet.h>
#include <atomic>
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
        swiftnet_server_append_to_packet(message.message, message.message_length + 1, buffer);

        printf("Serializing message: %s\n", message.message);
    }
}

void HostedServer::BackgroundProcesses() {
    while (true) {
        if (atomic_load_explicit(&this->stop_background_processes, memory_order_acquire) == true) {
            break;
        }

        if (this->GetNewMessages()->size() == 0) {
            usleep(200000);
            continue;
        }

        uint32_t bytes_to_allocate = sizeof(ResponseInfo) + sizeof(responses::LoadChannelDataResponse);

        for (auto &new_message : *this->GetNewMessages()) {
            bytes_to_allocate += new_message.message_length + 1;
        }

        const ResponseInfo response_info = {
            .request_type = RequestType::LOAD_CHANNEL_DATA,
            .request_status = Status::SUCCESS
        };

        const responses::LoadChannelDataResponse response = {
            .channel_messages_len = static_cast<uint32_t>(this->GetNewMessages()->size())
        };
        
        auto buffer = swiftnet_server_create_packet_buffer(bytes_to_allocate);

        swiftnet_server_append_to_packet(&response_info, sizeof(response_info), &buffer);
        swiftnet_server_append_to_packet(&response, sizeof(response), &buffer);

        SerializeChannelMessages(&buffer, this->GetNewMessages());

        for (auto &user : *this->GetConnectedUsers()) {
            swiftnet_server_send_packet(this->GetServer(), &buffer, user.addr_data);
        }

        swiftnet_server_destroy_packet_buffer(&buffer);

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

    const auto query_result = database->SelectHostedServerUsers(server->GetServerId(), std::nullopt, nullptr, packet_data->metadata.sender.sender_address.s_addr);
    if (query_result->size() == 0) {
        return;
    }

    const auto user = query_result->at(0);

    free(query_result);

    server->AddConnectedUser((ConnectedUser){.addr_data = packet_data->metadata.sender, .user_id = user.id, .port = packet_data->metadata.port_info.source_port});

    auto channel_messages = database->SelectChannelMessages(std::nullopt, nullptr, std::nullopt, request_data->channel_id);

    uint32_t bytes_to_allocate = (sizeof(responses::LoadChannelDataResponse) + sizeof(RequestInfo));

    for (auto &message : *channel_messages) {
        bytes_to_allocate += message.message_length + 1;
        bytes_to_allocate += sizeof(message.id) + sizeof(message.message_length) + sizeof(message.sender_id);
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

    int result = wxGetApp().GetDatabase()->InsertHostedServerUser(server_id, ip_address, username);
    if (result != 0) {
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

    const ConnectedUser* connected_user = server->GetUserByIp(packet_data->metadata.sender, packet_data->metadata.port_info.source_port);
    if (connected_user == nullptr) {
        std::cout << "User not connected" << std::endl;

        swiftnet_server_destroy_packet_data(packet_data, server->GetServer());

        return;
    }

    int result = wxGetApp().GetDatabase()->InsertChannelMessage(message, request->channel_id, connected_user->user_id);

    if ((result < 0) == false) {
        char* message_clone = (char*)malloc(request->message_len);

        memcpy(message_clone, message, request->message_len);
        
        server->GetNewMessages()->push_back(objects::Database::ChannelMessageRow{
            .message = message_clone,
            .message_length = request->message_len,
            .channel_id = request->channel_id,
            .sender_id = connected_user->user_id,
            .id = (uint32_t)result
        });
    }

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

    this->connected_users.clear();

    atomic_store_explicit(&this->stop_background_processes, true, memory_order_release);

    this->background_processes_thread->join();

    delete this->background_processes_thread;

    this->background_processes_thread = nullptr;

    wxGetApp().GetHomeFrame()->GetHostingPanel()->DrawServers();
}

void HostedServer::AddConnectedUser(const ConnectedUser connected_user) {
    this->GetConnectedUsers()->push_back(connected_user);
}

ConnectedUser* HostedServer::GetUserByIp(const SwiftNetClientAddrData addr_data, const uint16_t port) {
    for (auto &user : *this->GetConnectedUsers()) {
        if (port == user.port && memcmp(&addr_data, &user.addr_data, sizeof(SwiftNetClientAddrData)) == 0) {
            return &user;
        }
    }

    return nullptr;
}

std::vector<ConnectedUser>* HostedServer::GetConnectedUsers() {
    return &this->connected_users;
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
