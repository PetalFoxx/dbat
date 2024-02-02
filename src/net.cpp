#include "dbat/net.h"
#include "dbat/utils.h"
#include <regex>
#include "dbat/screen.h"
#include "dbat/players.h"
#include "dbat/login.h"
#include "dbat/config.h"

#define COLOR_ON(ch) (COLOR_LEV(ch) > 0)

namespace net {

    std::mutex connectionMutex;
    std::unordered_map<std::string, std::shared_ptr<Connection>> connections;

    void ConnectionParser::close() {

    }

    void ConnectionParser::sendText(const std::string &txt) {
        conn->sendText(txt);
    }

    void ConnectionParser::start() {

    }

    void ConnectionParser::handleGMCP(const std::string &txt, const nlohmann::json &j) {

    }

    void Connection::sendGMCP(const std::string &cmd, const nlohmann::json &j) {
        if(cmd.empty()) return;
        nlohmann::json j2;
        j2["cmd"] = cmd;
        j2["data"] = j;
        sendEvent("Game.GMCP", j2);
    }

    void Connection::sendText(const std::string &text) {
        if(text.empty()) return;
        nlohmann::json j;
        j["data"] = text;
        sendEvent("Game.Text", j);
    }

    void Connection::sendEvent(const std::string &name, const nlohmann::json &data) {
        outQueue.emplace_back(name, jdump(data));
    }

    void Connection::queueMessage(const std::string& event, const std::string& data) {
        inQueue.emplace_back(event, data);
    }


    void Connection::onWelcome() {
        account = nullptr;
        desc = nullptr;
        setParser(new LoginParser(shared_from_this()));
    }

    void Connection::close() {
        state = net::ConnectionState::Dead;
    }

    void Connection::onNetworkDisconnected() {
        state = net::ConnectionState::Dead;
    }


    Connection::Connection(const std::string& connId)
    : connId(connId) {

    }

    void Connection::cleanup() {
        if (account) {
            // Remove ourselves from the account's connections list.
            account->connections.erase(this);
            account = nullptr;
        }
    }


    void Connection::setParser(ConnectionParser *p) {
        if(parser) parser->close();
        parser.reset(p);
        p->start();
    }

    void Connection::handleEvent(const std::string& event, const nlohmann::json& data) {
        if(event == "Game.Command") {
            executeCommand(data["data"].get<std::string>());
        } else if(event == "Game.GMCP") {
            executeGMCP(data["cmd"].get<std::string>(), data["data"]);
        }
    }


    void Connection::onHeartbeat(double deltaTime) {
        if(!inQueue.empty())
            lastActivity = std::chrono::steady_clock::now();

        for(auto &[name, jdata] : inQueue) {
            nlohmann::json j;
            try {
                j = jparse(jdata);
            }
            catch (const nlohmann::json::parse_error &e) {
                basic_mud_log("Error parsing JSON for event %s: %s", name, e.what());
                continue;
            }

            handleEvent(name, j);
        }
    }

    void Connection::executeGMCP(const std::string &cmd, const nlohmann::json &j) {
        if(parser) parser->handleGMCP(cmd, j);
    }

    void Connection::executeCommand(const std::string &cmd) {
        if(parser) parser->parse(cmd);
    }

    std::shared_ptr<Connection> newConnection(const std::string& connID) {
        auto conn = std::make_shared<Connection>(connID);
        conn->state = ConnectionState::Pending;
        connections[connID] = conn;
        return conn;
    }


}