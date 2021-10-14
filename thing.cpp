#include "pgsql_things.cpp"
const char* conninfo;
PGconn* conn;
PGresult* res;

#include <algorithm>
#include <string_view>
#include "boost.cpp"

//#include "lua_things.cpp"
#include <utility>
#include <chrono>
#include <boost/json.hpp>
//#include "libpq-fe.h"
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#include <random>
namespace json = boost::json;

std::random_device rd;
std::mt19937 gen(rd());

static int sql_get(lua_State* L) {
    std::string d = lua_tostring(L, 1);
    int row = lua_tointeger(L, 1);
    int col = lua_tointeger(L, 1);
    res = PQexec(conn, d.c_str());
    unsigned int type = PQftype(res, 0);
    char* ptr = PQgetvalue(res, row, col);
    switch (type) {
    case 701://FLOAT8
        lua_pushstring(L, "FLOAT8");
        lua_setglobal(L, "sql_get_type");
        lua_pushstring(L, ptr);
        lua_setglobal(L, "sql_get_res");
        break;
    case 25://text
        lua_pushstring(L, "TEXT");
        lua_setglobal(L, "sql_get_type");
        lua_pushstring(L, ptr);
        lua_setglobal(L, "sql_get_res");
        break;
    }
    PQclear(res);
    return 0;  /* number of results */
}
static int sql_exec(lua_State* L) {
    std::string d = lua_tostring(L, 1);  /* get argument */
    res = PQexec(conn, d.c_str());
    PQclear(res);
    return 0;  /* number of results */
}

static int send_respond(lua_State* L) {
    std::string id = lua_tostring(L, 1);
    std::string text = lua_tostring(L, 2);
    lua_getglobal(L, "vk");
    bool vk = lua_toboolean(L, 3);
    std::uniform_int_distribution<> distrib(1, INT_MAX);
    if (vk) {
        auto pos = text.find(' ');
        while (pos != text.npos) {
            text.replace(pos, 1, "%20");
            pos = text.find(' ');
        }
        make_request("api.vk.com", ("/method/messages.send?user_id=" + id + "&random_id=" + std::to_string(distrib(gen)) + "&message=" + text + "&access_token=e0000766fb1128e4aaf77181e7174325e53d3028e17fa0c09f2033c2b060ee196c6784987b6b9801be6d7&v=5.131").c_str());
    }
    else {
        make_request("api.telegram.org",std::string("/bot1721867652:AAHmsA2n8IZSFAOxW3upFL3dNbz-50-z20M/sendMessage?chat_id="+id+"&text="+text).c_str());
    }
    return 0;  /* number of results */
}
int server() {
    try
    {
        auto const address = net::ip::make_address("0.0.0.0");
        auto const port = static_cast<unsigned short>(std::atoi("8080"));
        auto const doc_root = std::make_shared<std::string>(".");

        // The io_context is required for all I/O
        net::io_context ioc{ 1 };

        // The acceptor receives incoming connections
        tcp::acceptor acceptor{ ioc, {address, port} };
        for (;;)
        {
            // This will receive the new connection
            tcp::socket socket{ ioc };

            // Block until we get a connection
            acceptor.accept(socket);

            // Launch the session, transferring ownership of the socket
            std::thread{ std::bind(
                &do_session,
                std::move(socket),
                doc_root) }.detach();
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
        std::exit(1);
    }
} 
void process_message(std::uint64_t _id, const char* message, bool vk) {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    
    luaL_loadfile(L, "main.lua");
    int s = lua_pcall(L, 0, 0, 0);
    
    if (s) {
        printf("Error: %s \n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    lua_pushcfunction(L, sql_exec);
    lua_setglobal(L, "sql_exec");
    lua_pushcfunction(L, send_respond);
    lua_setglobal(L, "send_respond");
    lua_pushcfunction(L, sql_get);
    lua_setglobal(L, "sql_get");
    lua_pushstring(L, std::to_string(_id).c_str());
    lua_setglobal(L, "id");
    lua_pushstring(L, message);
    lua_setglobal(L, "message");
    lua_pushboolean(L, vk);
    lua_setglobal(L, "vk");
    
    std::string_view view = message;
    std::string_view com;
    bool fallback = false;
    if (view[0] == '!') {
        auto pos = view.find(' ');
        if (pos == view.npos) {
            com = view.substr(1, view.size());
            lua_getglobal(L, std::string(com).c_str());
            if (lua_isnil(L, 1)) {
                fallback = true;
            }
        }
        else {
            com = view.substr(1, pos - 1);
            lua_getglobal(L, std::string(com).c_str());
            if (lua_isnil(L, 1)) {
                fallback = true;
            }
        }
    }
    if(fallback) {
        res = PQexec(conn, ("select resp from public.resp where com = '"+std::string(com)+"'").c_str());
        if (PQgetisnull(res, 0, 0)) {
            lua_getglobal(L, "main");
            lua_pcall(L, 0, 0, 0);
        }
        else {
            std::string text (PQgetvalue(res, 0, 0));
            std::uniform_int_distribution<> distrib(1, INT_MAX);
            if (vk) {
                auto pos = text.find(' ');
                while (pos != text.npos) {
                    text.replace(pos, 1, "%20");
                    pos = text.find(' ');
                }
                make_request("api.vk.com", ("/method/messages.send?user_id=" + std::to_string(_id) + "&random_id=" + std::to_string(distrib(gen)) + "&message=" + text + "&access_token=e0000766fb1128e4aaf77181e7174325e53d3028e17fa0c09f2033c2b060ee196c6784987b6b9801be6d7&v=5.131").c_str());
            }
            else {
                make_request("api.telegram.org", std::string("/bot1721867652:AAHmsA2n8IZSFAOxW3upFL3dNbz-50-z20M/sendMessage?chat_id=" + std::to_string(_id) + "&text=" + text).c_str());
            }
        }
        PQclear(res);
    }
    else {
        lua_getglobal(L, com.data());
        lua_pcall(L, 0, 0, 0);
    }
    
    lua_close(L);

    if (vk) {
        make_request("api.vk.com",("http://api.vk.com/method/messages.markAsAnsweredConversation?peer_id="+std::to_string(_id)+"&answered=1&access_token=e0000766fb1128e4aaf77181e7174325e53d3028e17fa0c09f2033c2b060ee196c6784987b6b9801be6d7&v=5.131").c_str());
        make_request("api.vk.com", ("http://api.vk.com/method/messages.deleteConversation?peer_id=" + std::to_string(_id) + "&access_token=e0000766fb1128e4aaf77181e7174325e53d3028e17fa0c09f2033c2b060ee196c6784987b6b9801be6d7&v=5.131").c_str());
    }
}
int messsage_checker() {
    std::uint64_t telegram_offset = 0;
    while (true) {
        //vk
       json::value res = json::parse(make_request("api.vk.com","/method/messages.getConversations?access_token=e0000766fb1128e4aaf77181e7174325e53d3028e17fa0c09f2033c2b060ee196c6784987b6b9801be6d7&v=5.131"));
       if (res.as_object()["response"].as_object()["count"].as_int64()) {
           if (res.as_object()["response"].as_object()["unread_count"].as_int64()) {
               auto arr = res.as_object()["response"].as_object()["items"].as_array();
               for (auto t : arr) {
                   process_message(
                       t.as_object()["last_message"].as_object()["peer_id"].as_int64(),
                       t.as_object()["last_message"].as_object()["text"].as_string().c_str(),true);
                   
               }
           }
       }
        //telegram
       res = json::parse(make_request("api.telegram.org",("/bot1721867652:AAHmsA2n8IZSFAOxW3upFL3dNbz-50-z20M/getUpdates?offset="+std::to_string(telegram_offset)).c_str()));
       auto arr = res.as_object()["result"].as_array();
       for(auto t : arr){
           telegram_offset = t.as_object()["update_id"].as_int64();
           process_message(
               t.as_object()["message"].as_object()["from"].as_object()["id"].as_int64(),
               t.as_object()["message"].as_object()["text"].as_string().c_str(),false);
       }
       if (arr.size() != 0) {
           telegram_offset++;
       }
       std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    return 0;
}

int main(int argc, char* argv[])
{
    conninfo = "host=localhost port=5432 dbname=postgres user=test_user password=test";
    conn = PQconnectdb(conninfo);
    if (PQstatus(conn) != CONNECTION_OK)
    {
        fprintf(stderr, "%s", PQerrorMessage(conn));
        exit_nicely(conn);
    }
    res = PQexec(conn,
        "SELECT pg_catalog.set_config('search_path', '', false)");
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "SET failed: %s", PQerrorMessage(conn));
        PQclear(res);
        exit_nicely(conn);
    }
    PQclear(res);
    

    std::thread server_thread(server);
    std::thread message_checker_thread(messsage_checker);
    server_thread.join();
    PQfinish(conn); 
}
