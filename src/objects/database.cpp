#include "objects.hpp"
#include "../main.hpp"
#include <cstdio>
#include <cstring>
#include <optional>
#include <sqlite3.h>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace objects;

Database::Database() {
    this->OpenDatabase();

    this->InitializeDatabaseTables();
    
    this->PrepareStatements();
}

Database::~Database() {
    for (const auto &statement : this->statements) {
        sqlite3_finalize(statement.second);
    }

    sqlite3_close_v2(this->database_connection);
}

void Database::OpenDatabase() {
    sqlite3* database_ptr;

    int result = sqlite3_open("swift_com", &database_ptr);
    if (result != SQLITE_OK) {
        std::cerr << "Failed to open database: " << sqlite3_errmsg(database_ptr) << std::endl;
        exit(EXIT_FAILURE);
    }

    this->database_connection = database_ptr;
}

void Database::PrepareStatements() {
    const Statement statements[] = {
        (Statement){.statement_name = "insert_hosted_server_user", .query = "INSERT INTO hosted_server_users (ip_address, server_id, username) VALUES ($1, $2, $3);"},
        (Statement){.statement_name = "insert_joined_server", .query = "INSERT INTO joined_servers (ip_address, server_id) VALUES ($1, $2);"},
        (Statement){.statement_name = "insert_hosted_server", .query = "INSERT INTO hosted_servers (id) VALUES ($1);"},
        (Statement){.statement_name = "insert_server_chat_channel", .query = "INSERT INTO server_chat_channels (name, hosted_server_id) VALUES ($1, $2);"},
        (Statement){.statement_name = "insert_channel_message", .query = "INSERT INTO channel_messages (message, channel_id, sender_id) VALUES ($1, $2, $3) RETURNING channel_messages.id, (SELECT username FROM hosted_server_users WHERE id = channel_messages.sender_id) AS sender_username;"},
        (Statement){.statement_name = "update_hosted_server_users", .query = "UPDATE hosted_server_users SET username = COALESCE($1, username), user_type = COALESCE($2, user_type) WHERE ($3 IS NULL OR id = $3) AND ($4 IS NULL OR ip_address = $4) AND ($5 IS NULL OR server_id = $5) OR ($6 IS NULL OR username = $6) OR ($7 IS NULL OR user_type = $7);"},
        (Statement){.statement_name = "select_hosted_servers", .query = "SELECT id FROM hosted_servers WHERE ($1 IS NULL OR id = $1);"},
        (Statement){.statement_name = "select_joined_servers", .query = "SELECT id, ip_address, server_id FROM joined_servers WHERE ($1 IS NULL OR id = $1) AND ($2 IS NULL OR ip_address = $2) AND ($3 IS NULL OR server_id = $3);"},
        (Statement){.statement_name = "select_hosted_server_users", .query = "SELECT id, username, ip_address, user_type FROM hosted_server_users WHERE ($1 IS NULL OR server_id = $1) AND ($2 IS NULL OR user_type = $2) AND ($3 IS NULL OR username = $3) AND ($4 IS NULL OR ip_address = $4);"},
        (Statement){.statement_name = "select_server_chat_channels", .query = "SELECT id, name, hosted_server_id FROM server_chat_channels WHERE ($1 IS NULL OR $1 = id) AND ($2 IS NULL OR $2 = name) AND ($3 IS NULL OR $3 = hosted_server_id);"},
        (Statement){.statement_name = "select_channel_messages", .query = "SELECT messages.id, messages.message, length(messages.message), messages.sender_id, messages.channel_id, users.username FROM channel_messages messages JOIN hosted_server_users users ON users.id = messages.sender_id WHERE ($1 IS NULL OR messages.id = $1) AND ($2 IS NULL OR messages.message = $2) AND ($3 IS NULL OR messages.sender_id = $3) AND ($4 IS NULL OR messages.channel_id = $4);"},
    };

    for(const auto& statement : statements) {
        sqlite3_stmt* stmt;

        if(sqlite3_prepare_v2(this->GetDatabaseConnection(), statement.query, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to prepare: " << sqlite3_errmsg(this->GetDatabaseConnection()) << "\n";
            sqlite3_close(this->GetDatabaseConnection());
            exit(EXIT_FAILURE);
        }

        this->GetStatements().insert({statement.statement_name, stmt});
    }
}

void Database::InitializeDatabaseTables() {
    const char* queries[] = {
        "CREATE TABLE IF NOT EXISTS hosted_server_users (id INTEGER PRIMARY KEY AUTOINCREMENT, ip_address INTEGER UNIQUE NOT NULL, server_id INTEGER NOT NULL, username VARCHAR(20) NOT NULL, user_type INT NOT NULL DEFAULT 0);",
        "CREATE TABLE IF NOT EXISTS hosted_servers (id INTEGER PRIMARY KEY NOT NULL);",
        "CREATE TABLE IF NOT EXISTS joined_servers (id INTEGER PRIMARY KEY AUTOINCREMENT, ip_address INTEGER NOT NULL, server_id INTEGER NOT NULL);",
        "CREATE TABLE IF NOT EXISTS server_chat_channels (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, hosted_server_id INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS channel_messages (id INTEGER PRIMARY KEY AUTOINCREMENT, message TEXT NOT NULL, channel_id INTEGER NOT NULL, sender_id INTEGER NOT NULL);"
    };

    for(const auto& query : queries) {
        int result = sqlite3_exec(this->GetDatabaseConnection(), query, 0, 0, nullptr);
        if (result) {
            std::cerr << "Failed to create table" << std::endl;
            exit(EXIT_FAILURE);
        };
    }
}

int Database::InsertHostedServerUser(const uint16_t server_id, in_addr ip_address, const char* username) {
    sqlite3_stmt* stmt = this->GetStatement("insert_hosted_server_user");

    sqlite3_bind_int(stmt, 1, ip_address.s_addr);
    sqlite3_bind_int(stmt, 2, server_id);
    sqlite3_bind_text(stmt, 3, username, -1, SQLITE_TRANSIENT);

    int result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        std::cerr << "Failed to insert hosted_server_user" << std::endl;
        
        sqlite3_reset(stmt);

        return -1;
    }

    sqlite3_reset(stmt);

    return 0;
}

std::optional<Database::ChannelMessageRow> Database::InsertChannelMessage(const char* message, const uint32_t channel_id, const uint32_t sender_id) {
    sqlite3_stmt* stmt = this->GetStatement("insert_channel_message");

    sqlite3_bind_text(stmt, 1, message, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, channel_id);
    sqlite3_bind_int(stmt, 3, sender_id);

    int result = sqlite3_step(stmt);
    if (result != SQLITE_ROW) {
        std::cerr << "Failed to insert channel message" << std::endl;

        sqlite3_reset(stmt);

        return std::nullopt;
    }

    int new_message_id = sqlite3_column_int(stmt, 0);
    const char* username = (const char*)sqlite3_column_text(stmt, 1);

    printf("DB MESSAGE INSERT USERNAME: %s\n", username);

    Database::ChannelMessageRow row = {
        .channel_id = channel_id,
        .message = message,
        .message_length = static_cast<uint32_t>(strlen(message)),
        .id = static_cast<uint32_t>(new_message_id),
        .sender_id = sender_id,
    };

    memcpy(row.sender_username, username, sizeof(row.sender_username));

    sqlite3_reset(stmt);

    return row;
}

int Database::InsertJoinedServer(const uint16_t server_id, in_addr ip_address) {
    sqlite3_stmt* stmt = this->GetStatement("insert_joined_server");

    sqlite3_bind_int(stmt, 1, ip_address.s_addr);
    sqlite3_bind_int(stmt, 2, server_id);

    int result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        std::cerr << "Failed to insert insert_joined_server" << std::endl;

        sqlite3_reset(stmt);

        return -1;
    }

    sqlite3_reset(stmt);

    return 0;
}

int Database::InsertServerChatChannel(const char* name, const uint16_t server_id) {
    sqlite3_stmt* stmt = this->GetStatement("insert_server_chat_channel");

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, server_id);

    int result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        std::cerr << "Failed to insert insert_joined_server" << std::endl;

        sqlite3_reset(stmt);

        return -1;
    }

    sqlite3_reset(stmt);

    return 0;
}

int Database::InsertHostedServer(const uint16_t server_id) {
    sqlite3_stmt* stmt = this->GetStatement("insert_hosted_server");

    sqlite3_bind_int(stmt, 1, server_id);

    int result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        std::cerr << "Failed to insert insert_hosted_server" << std::endl;

        sqlite3_reset(stmt);

        return -1;
    }

    sqlite3_reset(stmt);

    return 0;
}

int Database::UpdateHostedServerUsers(const char* new_username, const std::optional<Database::UserType> new_user_type, const std::optional<uint32_t> id, const std::optional<in_addr_t> ip_address, const std::optional<uint16_t> server_id, const char* username, const std::optional<Database::UserType> user_type) {
    sqlite3_stmt* stmt = this->GetStatement("update_hosted_server_users");

    new_username != nullptr ? sqlite3_bind_text(stmt, 1, new_username, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, 1);
    new_user_type.has_value() ? sqlite3_bind_int(stmt, 2, new_user_type.value()) : sqlite3_bind_null(stmt, 2);
    id.has_value() ? sqlite3_bind_int(stmt, 3, id.value()) : sqlite3_bind_null(stmt, 3);
    ip_address.has_value() ? sqlite3_bind_int(stmt, 4, ip_address.value()) : sqlite3_bind_null(stmt, 4);
    server_id.has_value() ? sqlite3_bind_int(stmt, 5, server_id.value()) : sqlite3_bind_null(stmt, 5);
    username != nullptr ? sqlite3_bind_text(stmt, 6, username, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, 6);
    user_type.has_value() ? sqlite3_bind_int(stmt, 7, user_type.value()) : sqlite3_bind_null(stmt, 7);

    int result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        std::cerr << "Failed to update hosted_server_users" << std::endl;

        sqlite3_reset(stmt);
        
        return -1;
    }

    sqlite3_reset(stmt);

    return 0;
} 

std::vector<Database::HostedServerUser>* Database::SelectHostedServerUsers(const std::optional<uint16_t> server_id, const std::optional<Database::UserType> user_type, const char* username, const std::optional<in_addr_t> ip_address) {
    sqlite3_stmt* stmt = this->GetStatement("select_hosted_server_users");

    server_id.has_value() ? sqlite3_bind_int(stmt, 1, server_id.value()) : sqlite3_bind_null(stmt, 1);
    user_type.has_value() ? sqlite3_bind_int(stmt, 2, user_type.value()) : sqlite3_bind_null(stmt, 2);
    username != nullptr ? sqlite3_bind_text(stmt, 3, username, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, 3);
    ip_address.has_value() ? sqlite3_bind_int(stmt, 4, ip_address.value()) : sqlite3_bind_null(stmt, 4);

    std::vector<Database::HostedServerUser>* result = new std::vector<Database::HostedServerUser>();

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Database::HostedServerUser new_row = {
            .id = (uint16_t)sqlite3_column_int(stmt, 0),
            .ip_address = (in_addr_t)sqlite3_column_int(stmt, 2),
            .user_type = (Database::UserType)sqlite3_column_int(stmt, 3)
        };

        const char* username = (const char*)sqlite3_column_text(stmt, 1);

        memcpy(&new_row.username, username, strlen(username) + 1);

        result->push_back(new_row);
    }

    sqlite3_reset(stmt);

    return result;
}

std::vector<Database::JoinedServerRow>* Database::SelectJoinedServers(const std::optional<uint32_t> id, const std::optional<in_addr_t> ip_address, const std::optional<uint16_t> server_id) {
    sqlite3_stmt* stmt = this->GetStatement("select_joined_servers");

    id.has_value() ? sqlite3_bind_int(stmt, 1, id.value()) : sqlite3_bind_null(stmt, 1);
    ip_address.has_value() ? sqlite3_bind_int(stmt, 2, ip_address.value()) : sqlite3_bind_null(stmt, 2);
    server_id.has_value() ? sqlite3_bind_int(stmt, 3, server_id.value()) : sqlite3_bind_null(stmt, 3);

    std::vector<Database::JoinedServerRow>* result = new std::vector<Database::JoinedServerRow>();

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const uint32_t id = sqlite3_column_int(stmt, 0);
        const in_addr_t server_ip_address = sqlite3_column_int(stmt, 1);
        const uint16_t server_id = sqlite3_column_int(stmt, 2);

        result->push_back((Database::JoinedServerRow){.server_id = server_id, .ip_address = server_ip_address});
    }

    sqlite3_reset(stmt);

    return result;
}

std::vector<Database::ChannelMessageRow>* Database::SelectChannelMessages(const std::optional<uint32_t> id, const char* message, const std::optional<uint32_t> sender_id, const std::optional<uint32_t> channel_id) {
    sqlite3_stmt* stmt = this->GetStatement("select_channel_messages");

    id.has_value() ? sqlite3_bind_int(stmt, 1, id.value()) : sqlite3_bind_null(stmt, 1);
    message != nullptr ? sqlite3_bind_text(stmt, 2, message, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, 2);
    sender_id.has_value() ? sqlite3_bind_int(stmt, 3, sender_id.value()) : sqlite3_bind_null(stmt, 3);
    channel_id.has_value() ? sqlite3_bind_int(stmt, 4, channel_id.value()) : sqlite3_bind_null(stmt, 4);

    std::vector<Database::ChannelMessageRow>* result = new std::vector<Database::ChannelMessageRow>();

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const uint32_t id = sqlite3_column_int(stmt, 0);
        const char* const message = (const char*)sqlite3_column_text(stmt, 1);

        printf("Got message from db: %s\n", message);

        const uint32_t message_length = sqlite3_column_int(stmt, 2);
        const uint32_t sender_id = sqlite3_column_int(stmt, 3);
        const uint32_t channel_id = sqlite3_column_int(stmt, 4);
        const char* sender_username = (const char*)sqlite3_column_text(stmt, 5);

        char* message_clone = (char*)malloc(message_length + 1);

        memcpy(message_clone, message, message_length + 1);

        auto message_row = (Database::ChannelMessageRow){
            .message = message_clone,
            .message_length = message_length,
            .id = id,
            .sender_id = sender_id,
            .channel_id = channel_id
        };

        memcpy(&message_row.sender_username, sender_username, sizeof(message_row.sender_username));

        result->push_back(message_row);
    }

    sqlite3_reset(stmt);

    return result;
}

std::vector<Database::ServerChatChannelRow>* Database::SelectServerChatChannels(const std::optional<uint32_t> id, const char* name, const std::optional<uint16_t> server_id) {
    sqlite3_stmt* stmt = this->GetStatement("select_server_chat_channels");

    id.has_value() ? sqlite3_bind_int(stmt, 1, id.value()) : sqlite3_bind_null(stmt, 1);
    name != nullptr ? sqlite3_bind_text(stmt, 2, name, -1, SQLITE_TRANSIENT) : sqlite3_bind_null(stmt, 2);
    server_id.has_value() ? sqlite3_bind_int(stmt, 3, server_id.value()) : sqlite3_bind_null(stmt, 3);

    std::vector<Database::ServerChatChannelRow>* result = new std::vector<Database::ServerChatChannelRow>();

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const uint32_t got_id = sqlite3_column_int(stmt, 0);
        const uint16_t got_hosted_server_id = sqlite3_column_int(stmt, 2);

        auto row = (Database::ServerChatChannelRow){
            .id = got_id,
            .hosted_server_id = got_hosted_server_id
        };

        const char* got_name = (const char*)sqlite3_column_text(stmt, 1);

        memcpy(row.name, got_name, strlen(got_name) + 1);

        result->push_back(row);
    }

    sqlite3_reset(stmt);

    return result;
}

std::vector<Database::HostedServerRow>* Database::SelectHostedServers(const std::optional<uint16_t> server_id) {
    sqlite3_stmt* stmt = this->GetStatement("select_hosted_servers");

    std::vector<Database::HostedServerRow>* result = new std::vector<Database::HostedServerRow>();

    server_id.has_value() ? sqlite3_bind_int(stmt, 1, server_id.value()) : sqlite3_bind_null(stmt, 1);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        uint16_t id = sqlite3_column_int(stmt, 0);

        result->push_back((Database::HostedServerRow){.server_id = id});
    }

    sqlite3_reset(stmt);

    return result;
}

sqlite3* Database::GetDatabaseConnection() {
    return this->database_connection;
}

std::unordered_map<const char*, sqlite3_stmt*>& Database::GetStatements() {
    return this->statements;
}

sqlite3_stmt* Database::GetStatement(const char* statement_name) {
    return this->GetStatements().at(statement_name);
}
