#pragma once

#include <sqlite3.h>
#include <arpa/inet.h>
#include <cstdint>
#include <fstream>
#include <netinet/in.h>
#include <unordered_map>
#include <vector>
#include <swift_net.h>

namespace objects {
    typedef enum {
        STOPPED,
        RUNNING
    } HostedServerStatus;

    typedef struct {
        SwiftNetClientAddrData addr_data;
        uint16_t port;
        uint32_t user_id;
    } ConnectedUser;

    class HostedServer {
    public:
        HostedServer(uint16_t id);
        ~HostedServer();

        void StartServer();
        void StopServer();

        void AddConnectedUser(const ConnectedUser connected_user);

        ConnectedUser* GetUserByIp(const SwiftNetClientAddrData addr_data, const uint16_t port);

        std::vector<ConnectedUser>* GetConnectedUsers();
        SwiftNetServer* GetServer();
        uint16_t GetServerId();
        HostedServerStatus GetServerStatus();
    private:
        uint16_t id;

        HostedServerStatus status = STOPPED;

        SwiftNetServer* server = nullptr;

        std::vector<ConnectedUser> connected_users;
    };

    class JoinedServer {
    public:
        JoinedServer(uint16_t server_id, in_addr server_ip_address);

        uint16_t GetServerId();
        in_addr GetServerIpAddress();
    private:
        uint16_t server_id;
        in_addr server_ip_address;
    };

    class Database {
    public:
        typedef struct {
            uint16_t server_id;
        } HostedServerRow;

        typedef struct {
            uint16_t server_id;
            in_addr ip_address;
        } JoinedServerRow;

        typedef struct {
            uint32_t id;
            const char* message;
            uint32_t message_length;
            uint32_t sender_id;
        } ChannelMessageRow;

        typedef struct {
            uint16_t id;
            char name[20];
        } ServerChatChannelRow;

        typedef struct {
            uint32_t id;
            char username[20];
            in_addr ip_address;
        } HostedServerUser;

        typedef struct {
            const char* statement_name;
            const char* query;
        } Statement;

        Database();
        ~Database();

        void OpenDatabase();
        void InitializeDatabaseTables();
        void PrepareStatements();

        std::vector<HostedServerRow>* SelectHostedServers();
        std::vector<JoinedServerRow>* SelectJoinedServers();
        std::vector<ServerChatChannelRow>* SelectServerChatChannels(const uint16_t server_id);
        std::vector<ChannelMessageRow>* SelectChannelMessages(const uint32_t channel_id);
        std::vector<HostedServerUser>* SelectHostedServerUsers(const uint16_t server_id);

        uint32_t SelectUserId(const in_addr_t ip_address, const uint32_t server_id);

        int InsertHostedServer(const uint16_t server_id);
        int InsertHostedServerUser(const uint16_t server_id, in_addr ip_address, const char* username);
        int InsertJoinedServer(const uint16_t server_id, in_addr ip_address);
        int InsertServerChatChannel(const char* name, const uint16_t server_id);
        int InsertChannelMessage(const char* message, const uint32_t channel_id, const uint32_t sender_id);

        sqlite3_stmt* GetStatement(const char* statement_name);

        std::unordered_map<const char*, sqlite3_stmt*>& GetStatements();
        sqlite3* GetDatabaseConnection();
    private:
        sqlite3* database_connection;
        std::unordered_map<const char*, sqlite3_stmt*> statements;
    };
}
