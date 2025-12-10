#include "objects.hpp"

using JoinedServer = objects::JoinedServer;

JoinedServer::JoinedServer(const uint16_t server_id, const in_addr server_ip_address, const ServerStatus status, const bool admin) : server_id(server_id), server_ip_address(server_ip_address), status(status), admin(admin) {};

uint16_t JoinedServer::GetServerId() {
    return this->server_id;
}

in_addr JoinedServer::GetServerIpAddress() {
    return this->server_ip_address;
}

JoinedServer::ServerStatus JoinedServer::GetServerStatus() {
    return this->status;
}

bool JoinedServer::IsAdmin() {
    return this->admin;
}
