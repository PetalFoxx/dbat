#include "dbat/account.h"
#include "dbat/db.h"
#include "dbat/utils.h"
#include "sodium.h"
#include <stdexcept>

std::unordered_map<vnum, std::shared_ptr<account_data>> accounts;

std::shared_ptr<account_data> findAccount(const std::string &name) {
    for (auto &[aid, account] : accounts) {
        if (boost::iequals(account.name, name)) {
            return &account;
        }
    }
    return nullptr;
}

static std::optional<std::string> hashPassword(const std::string& password) {
    char hashed_password[crypto_pwhash_STRBYTES];
    if(password.empty()) return std::nullopt;
    if(crypto_pwhash_str(hashed_password, password.data(), password.size(),
                         crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        return std::nullopt;
    }
    return hashed_password;

}

bool account_data::checkPassword(const std::string &password) {
    auto result = crypto_pwhash_str_verify(passHash.data(), password.data(), password.size());
    return result == 0;
}

bool account_data::setPassword(const std::string &password) {
    auto hash = hashPassword(password);
    if(!hash) return false;
    passHash = hash.value();
    lastPasswordChanged = time(nullptr);
    return true;
}

nlohmann::json account_data::serialize() {
    nlohmann::json j;

    if(vn != NOTHING) j["vn"] = vn;
    if(!name.empty()) j["name"] = name;
    if(!passHash.empty()) j["passHash"] = passHash;
    if(!email.empty()) j["email"] = email;
    if(created) j["created"] = created;
    if(lastLogin) j["lastLogin"] = lastLogin;
    if(lastLogout) j["lastLogout"] = lastLogout;
    if(lastPasswordChanged) j["lastPasswordChanged"] = lastPasswordChanged;
    if(totalPlayTime != 0.0) j["totalPlayTime"] = totalPlayTime;
    if(!disabledReason.empty()) j["disabledReason"] = disabledReason;
    if(disabledUntil) j["disabledUntil"] = disabledUntil;
    if(rpp) j["rpp"] = rpp;
    if(slots != 3) j["slots"] = slots;
    if(adminLevel) j["adminLevel"] = adminLevel;
    for(auto c : characters) j["characters"].push_back(c);

    return j;
}

void account_data::deserialize(const nlohmann::json& j) {
    if(j.contains("vn")) vn = j["vn"];
    if(j.contains("name")) name = j["name"];
    if(j.contains("passHash")) passHash = j["passHash"];
    if(j.contains("email")) email = j["email"];
    if(j.contains("created")) created = j["created"];
    if(j.contains("lastLogin")) lastLogin = j["lastLogin"];
    if(j.contains("lastLogout")) lastLogout = j["lastLogout"];
    if(j.contains("lastPasswordChanged")) lastPasswordChanged = j["lastPasswordChanged"];
    if(j.contains("totalPlayTime")) totalPlayTime = j["totalPlayTime"];
    if(j.contains("disabledReason")) disabledReason = j["disabledReason"];
    if(j.contains("disabledUntil")) disabledUntil = j["disabledUntil"];
    if(j.contains("rpp")) rpp = j["rpp"];
    if(j.contains("slots")) slots = j["slots"];
    if(j.contains("adminLevel")) adminLevel = j["adminLevel"];
    if(j.contains("characters")) {
        for(auto c : j["characters"]) {
            characters.push_back(c);
        }
    }
}

account_data::account_data(const nlohmann::json &j) {
    deserialize(j);
}

int account_data::getNextID() {
    int id = 0;
    while(accounts.contains(id)) id++;
    return id;
}

std::shared_ptr<account_data> createAccount(const std::string &name, const std::string &password) {
    if(name.empty()) throw std::invalid_argument("Username cannot be blank.");
    if(password.empty()) throw std::invalid_argument("Password cannot be blank.");

    if(auto found = findAccount(name); found) {
        throw std::invalid_argument("Username already exists.");
    }

    auto hash = hashPassword(password);
    if(!hash) {
        throw std::invalid_argument("Password rejected by hashing algorithm, try another.");
    }

    auto nextId = account_data::getNextID();
    auto a = std::shared_ptr<account_data>();
    accounts[nextId] = a;
    a->name = name;
    a->vn = nextId;
    a->passHash = hash.value();
    a->created = time(nullptr);
    a->lastLogin = time(nullptr);

    return a;
}