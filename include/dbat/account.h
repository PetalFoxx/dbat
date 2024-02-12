#pragma once
#include "structs.h"

extern std::unordered_map<vnum, std::shared_ptr<account_data>> accounts;

std::shared_ptr<account_data> findAccount(const std::string &name);

std::shared_ptr<account_data> createAccount(const std::string &name, const std::string &password);