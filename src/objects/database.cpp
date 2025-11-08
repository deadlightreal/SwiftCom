#include "objects.hpp"
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
        (Statement){.statement_name = "insert_hosted_server_user", .query = "INSERT INTO hosted_server_users (ip_address, server_id) VALUES ($1, $2);"},
        (Statement){.statement_name = "insert_joined_server", .query = "INSERT INTO joined_servers (ip_address, server_id) VALUES ($1, $2);"},
        (Statement){.statement_name = "insert_hosted_server", .query = "INSERT INTO hosted_servers (id) VALUES ($1);"},
        (Statement){.statement_name = "insert_server_chat_channel", .query = "INSERT INTO server_chat_channels (name, hosted_server_id) VALUES ($1, $2);"},
        (Statement){.statement_name = "insert_channel_message", .query = "INSERT INTO channel_messages (message, channel_id, sender_id) VALUES ($1, $2, $3);"},
        (Statement){.statement_name = "select_hosted_servers", .query = "SELECT id FROM hosted_servers;"},
        (Statement){.statement_name = "select_joined_servers", .query = "SELECT ip_address, server_id FROM joined_servers;"},
        (Statement){.statement_name = "select_user_id", .query = "SELECT id FROM hosted_server_users WHERE ip_address = $1 AND server_id = $2;"},
        (Statement){.statement_name = "select_server_chat_channels", .query = "SELECT id, name FROM server_chat_channels WHERE hosted_server_id = $1;"},
        (Statement){.statement_name = "select_channel_messages", .query = "SELECT id, message, length(message), sender_id FROM channel_messages WHERE channel_id = $1;"},
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
        "CREATE TABLE IF NOT EXISTS hosted_server_users (id INTEGER PRIMARY KEY AUTOINCREMENT, ip_address INTEGER UNIQUE NOT NULL, server_id INTEGER NOT NULL);",
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

int Database::InsertHostedServerUser(const uint16_t server_id, in_addr ip_address) {
    sqlite3_stmt* stmt = this->GetStatement("insert_hosted_server_user");

    sqlite3_bind_int(stmt, 1, ip_address.s_addr);
    sqlite3_bind_int(stmt, 2, server_id);

    int result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        std::cerr << "Failed to insert hosted_server" << std::endl;
        return -1;
    }

    sqlite3_reset(stmt);

    return 0;
}

int Database::InsertChannelMessage(const char* message, const uint32_t channel_id, const uint32_t sender_id) {
    sqlite3_stmt* stmt = this->GetStatement("insert_channel_message");

    sqlite3_bind_text(stmt, 1, message, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, channel_id);
    sqlite3_bind_int(stmt, 3, sender_id);

    int result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        std::cerr << "Failed to insert channel message" << std::endl;
        return -1;
    }

    sqlite3_reset(stmt);

    return 0;
}

int Database::InsertJoinedServer(const uint16_t server_id, in_addr ip_address) {
    sqlite3_stmt* stmt = this->GetStatement("insert_joined_server");

    sqlite3_bind_int(stmt, 1, ip_address.s_addr);
    sqlite3_bind_int(stmt, 2, server_id);

    int result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        std::cerr << "Failed to insert insert_joined_server" << std::endl;
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
        return -1;
    }

    sqlite3_reset(stmt);

    return 0;
}

std::vector<Database::JoinedServerRow>* Database::SelectJoinedServers() {
    sqlite3_stmt* stmt = this->GetStatement("select_joined_servers");

    std::vector<Database::JoinedServerRow>* result = new std::vector<Database::JoinedServerRow>();

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        in_addr_t server_ip_address = sqlite3_column_int(stmt, 0);
        uint16_t server_id = sqlite3_column_int(stmt, 1);

        result->push_back((Database::JoinedServerRow){.server_id = server_id, .ip_address = server_ip_address});
    }

    sqlite3_reset(stmt);

    return result;
}

uint32_t Database::SelectUserId(const in_addr_t ip_address, const uint32_t server_id) {
    sqlite3_stmt* stmt = this->GetStatement("select_joined_servers");

    sqlite3_bind_int(stmt, 1, ip_address);
    sqlite3_bind_int(stmt, 2, server_id);

    std::vector<Database::ServerChatChannelRow>* result = new std::vector<Database::ServerChatChannelRow>();

    if(sqlite3_step(stmt) == SQLITE_ROW) {
        uint32_t id = sqlite3_column_int(stmt, 0);

        return id;
    }

    sqlite3_reset(stmt);

    return 0;
}

std::vector<Database::ChannelMessageRow>* Database::SelectChannelMessages(const uint32_t channel_id) {
    sqlite3_stmt* stmt = this->GetStatement("select_channel_messages");

    sqlite3_bind_int(stmt, 1, channel_id);

    std::vector<Database::ChannelMessageRow>* result = new std::vector<Database::ChannelMessageRow>();

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        uint32_t id = sqlite3_column_int(stmt, 0);
        const char* message = (const char*)sqlite3_column_text(stmt, 1);
        uint32_t message_length = sqlite3_column_int(stmt, 2);
        uint32_t sender_id = sqlite3_column_int(stmt, 3);

        result->push_back((Database::ChannelMessageRow){
            .message = message,
            .message_length = message_length,
            .id = id,
            .sender_id = sender_id,
        });
    }

    sqlite3_reset(stmt);

    return result;
}

std::vector<Database::ServerChatChannelRow>* Database::SelectServerChatChannels(const uint16_t server_id) {
    sqlite3_stmt* stmt = this->GetStatement("select_server_chat_channels");

    sqlite3_bind_int(stmt, 1, server_id);

    std::vector<Database::ServerChatChannelRow>* result = new std::vector<Database::ServerChatChannelRow>();

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        uint16_t id = sqlite3_column_int(stmt, 0);

        auto row = (Database::ServerChatChannelRow){
            .id = id,
        };

        const unsigned char* text = sqlite3_column_text(stmt, 1);

        if (text != NULL) {
            strncpy(row.name, (const char*)text, sizeof(row.name) - 1);
            row.name[sizeof(row.name) - 1] = '\0';
        } else {
            row.name[0] = '\0';
        }

        result->push_back(row);
    }

    sqlite3_reset(stmt);

    return result;
}

std::vector<Database::HostedServerRow>* Database::SelectHostedServers() {
    sqlite3_stmt* stmt = this->GetStatement("select_hosted_servers");

    std::vector<Database::HostedServerRow>* result = new std::vector<Database::HostedServerRow>();

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
