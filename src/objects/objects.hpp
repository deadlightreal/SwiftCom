#pragma once

#include <optional>
#include <sqlite3.h>
#include <arpa/inet.h>
#include <cstdint>
#include <netinet/in.h>
#include <thread>
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
        uint32_t channel_id;
    } ConnectedUser;

    class JoinedServer {
    public:
        enum ServerStatus {
            OFFLINE,
            ONLINE
        };

        JoinedServer(const uint16_t server_id, const in_addr server_ip_address, const ServerStatus, const bool admin);

        uint16_t GetServerId();
        in_addr GetServerIpAddress();
        ServerStatus GetServerStatus();
        bool IsAdmin();
    private:
        ServerStatus status;
        uint16_t server_id;
        in_addr server_ip_address;
        bool admin;
    };

    class Database {
    public:
        enum UserType {
            Member,
            Admin
        };

        typedef struct {
            uint16_t server_id;
        } HostedServerRow;

        typedef struct {
            uint16_t server_id;
            in_addr ip_address;
            uint32_t id;
        } JoinedServerRow;

        typedef struct {
            uint32_t id;
            const char* message;
            uint32_t message_length;
            uint32_t sender_id;
            uint32_t channel_id;
            char sender_username[20];
        } ChannelMessageRow;

        typedef struct {
            uint32_t id;
            char name[20];
            uint16_t hosted_server_id;
        } ServerChatChannelRow;

        typedef struct {
            uint32_t id;
            char username[20];
            in_addr ip_address;
            Database::UserType user_type;
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

        std::vector<HostedServerRow>* SelectHostedServers(const std::optional<uint16_t> server_id);
        std::vector<JoinedServerRow>* SelectJoinedServers(const std::optional<uint32_t> id, const std::optional<in_addr_t> ip_address, const std::optional<uint16_t> server_id);
        std::vector<ServerChatChannelRow>* SelectServerChatChannels(const std::optional<uint32_t> id, const char* name, const std::optional<uint16_t> server_id);
        std::vector<ChannelMessageRow>* SelectChannelMessages(const std::optional<uint32_t> id, const char* message, const std::optional<uint32_t> sender_id, const std::optional<uint32_t> channel_id);
        std::vector<HostedServerUser>* SelectHostedServerUsers(const std::optional<uint16_t> server_id, const std::optional<Database::UserType> user_type, const char* username, const std::optional<in_addr_t> ip_address);

        int InsertHostedServer(const uint16_t server_id);
        int InsertHostedServerUser(const uint16_t server_id, in_addr ip_address, const char* username);
        int InsertJoinedServer(const uint16_t server_id, in_addr ip_address);
        int InsertServerChatChannel(const char* name, const uint16_t server_id);
        std::optional<ChannelMessageRow> InsertChannelMessage(const char* message, const uint32_t channel_id, const uint32_t sender_id);

        int UpdateHostedServerUsers(const char* new_username, const std::optional<Database::UserType> new_user_type, const std::optional<uint32_t> id, const std::optional<in_addr_t> ip_address, const std::optional<uint16_t> server_id, const char* username, const std::optional<Database::UserType> user_type);

        sqlite3_stmt* GetStatement(const char* statement_name);

        std::unordered_map<const char*, sqlite3_stmt*>& GetStatements();
        sqlite3* GetDatabaseConnection();
    private:
        sqlite3* database_connection;
        std::unordered_map<const char*, sqlite3_stmt*> statements;
    };

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
        std::vector<Database::ChannelMessageRow>* GetNewMessages();
    private:
        uint16_t id;

        void BackgroundProcesses();
        HostedServerStatus status = STOPPED;

        _Atomic bool stop_background_processes;

        std::thread* background_processes_thread = nullptr;

        std::vector<Database::ChannelMessageRow> new_messages = {};

        SwiftNetServer* server = nullptr;

        std::vector<ConnectedUser> connected_users = {};
    };
}
