#pragma once

#include "structs.h"

// global variables
extern std::unordered_map<int64_t, std::shared_ptr<player_data>> players;

struct char_data *findPlayer(const std::string& name);

OpResult<> validate_pc_name(const std::string& name);