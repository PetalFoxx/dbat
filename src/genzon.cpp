/************************************************************************
 * Generic OLC Library - Zones / genzon.c			v1.0	*
 * Copyright 1996 by Harvey Gilpin					*
 * Copyright 1997-2001 by George Greer (greerga@circlemud.org)		*
 ************************************************************************/

#include <fstream>
#include "dbat/genzon.h"
#include "dbat/utils.h"

#include "dbat/genolc.h"
#include "dbat/dg_scripts.h"
#include "dbat/shop.h"
#include "dbat/guild.h"

/* real zone of room/mobile/object/shop given */
zone_rnum real_zone_by_thing(room_vnum vznum) {
    for(auto &z : zone_table) if(vznum >= z.second.bot && vznum <= z.second.top) return z.first;
    return NOWHERE;
}

zone_rnum create_new_zone(zone_vnum vzone_num, room_vnum bottom, room_vnum top, const char **error) {
    FILE *fp;
    struct zone_data *zone;
    int i;
    zone_rnum rznum;
    char buf[MAX_STRING_LENGTH];

#if CIRCLE_UNSIGNED_INDEX
    if (vzone_num == NOWHERE) {
#else
        if (vzone_num < 0) {
#endif
        *error = "You can't make negative zones.\r\n";
        return NOWHERE;
    } else if (bottom > top) {
        *error = "Bottom room cannot be greater than top room.\r\n";
        return NOWHERE;
    }

#if _CIRCLEMUD < CIRCLEMUD_VERSION(3, 0, 21)
    /*
     * New with bpl19, the OLC interface should decide whether
     * to allow overlap before calling this function. There
     * are more complicated rules for that but it's not covered
     * here.
     */
    if (vzone_num > 326) {
      *error = "326 is the highest zone allowed.\r\n";
      return NOWHERE;
    }

    /*
     * Make sure the zone does not exist.
     */
    room = vzone_num * 100; /* Old CircleMUD 100-zones. */
    for (i = 0; i <= top_of_zone_table; i++)
      if (genolc_zone_bottom(i) <= room && zone_table[i].top >= room) {
        *error = "A zone already covers that area.\r\n";
        return NOWHERE;
      }
#else
    if(zone_table.count(vzone_num)) {
            *error = "That virtual zone already exists.\r\n";
            return NOWHERE;
        }
#endif

    /*
     * Create the zone file.
     */
    snprintf(buf, sizeof(buf), "%s%d.zon", ZON_PREFIX, vzone_num);
    if (!(fp = fopen(buf, "w"))) {
        mudlog(BRF, ADMLVL_IMPL, true, "SYSERR: OLC: Can't write new zone file.");
        *error = "Could not write zone file.\r\n";
        return NOWHERE;
    }
#if _CIRCLEMUD >= CIRCLEMUD_VERSION(3, 0, 21)
    /* File format changed. */
    fprintf(fp, "#%d\nNone~\nNew Zone~\n%d %d 30 2\nS\n$\n", vzone_num, bottom, top);
#else
    fprintf(fp, "#%d\nNew Zone~\n%d 30 2\nS\n$\n", vzone_num, (vzone_num * 100) + 99);
#endif
    fclose(fp);

    /*
     * Create the room file.
     */
    snprintf(buf, sizeof(buf), "%s%d.wld", WLD_PREFIX, vzone_num);
    if (!(fp = fopen(buf, "w"))) {
        mudlog(BRF, ADMLVL_IMPL, true, "SYSERR: OLC: Can't write new world file.");
        *error = "Could not write world file.\r\n";
        return NOWHERE;
    }
    fprintf(fp, "#%d\nThe Beginning~\nNot much here.\n~\n%d 0 0\nS\n$\n", bottom, vzone_num);
    fclose(fp);

    /*
     * Create the mobile file.
     */
    snprintf(buf, sizeof(buf), "%s%d.mob", MOB_PREFIX, vzone_num);
    if (!(fp = fopen(buf, "w"))) {
        mudlog(BRF, ADMLVL_IMPL, true, "SYSERR: OLC: Can't write new mob file.");
        *error = "Could not write mobile file.\r\n";
        return NOWHERE;
    }
    fprintf(fp, "$\n");
    fclose(fp);

    /*
     * Create the object file.
     */
    snprintf(buf, sizeof(buf), "%s%d.obj", OBJ_PREFIX, vzone_num);
    if (!(fp = fopen(buf, "w"))) {
        mudlog(BRF, ADMLVL_IMPL, true, "SYSERR: OLC: Can't write new obj file.");
        *error = "Could not write object file.\r\n";
        return NOWHERE;
    }
    fprintf(fp, "$\n");
    fclose(fp);

    /*
     * Create the shop file.
     */
    snprintf(buf, sizeof(buf), "%s%d.shp", SHP_PREFIX, vzone_num);
    if (!(fp = fopen(buf, "w"))) {
        mudlog(BRF, ADMLVL_IMPL, true, "SYSERR: OLC: Can't write new shop file.");
        *error = "Could not write shop file.\r\n";
        return NOWHERE;
    }
    fprintf(fp, "$~\n");
    fclose(fp);

    /*
     * Create the trigger file.
     */
    snprintf(buf, sizeof(buf), "%s%d.trg", TRG_PREFIX, vzone_num);
    if (!(fp = fopen(buf, "w"))) {
        mudlog(BRF, ADMLVL_IMPL, true, "SYSERR: OLC: Can't write new trigger file");
        *error = "Could not write trigger file.\r\n";
        return NOWHERE;
    }
    fprintf(fp, "$~\n");
    fclose(fp);

    /*
     * Create Gld file .
     */
    snprintf(buf, sizeof(buf), "%s/%i.gld", GLD_PREFIX, vzone_num);
    if (!(fp = fopen(buf, "w"))) {
        mudlog(BRF, ADMLVL_IMPL, true, "SYSERR: OLC: Can't write new guild file");
        *error = "Could not write guild file.\r\n";
        return NOWHERE;
    }
    fprintf(fp, "$~\n");
    fclose(fp);

    /*
     * Update index files.
     */
    create_world_index(vzone_num, "zon");
    create_world_index(vzone_num, "wld");
    create_world_index(vzone_num, "mob");
    create_world_index(vzone_num, "obj");
    create_world_index(vzone_num, "shp");
    create_world_index(vzone_num, "trg");
    create_world_index(vzone_num, "gld");

    /*
     * Make a new zone in memory. This was the source of all the zedit new
     * crashes reported to the CircleMUD list. It was happily overwriting
     * the stack.  This new loop by Andrew Helm fixes that problem and is
     * more understandable at the same time.
     *
     * The variable is 'top_of_zone_table_table + 2' because we need record 0
     * through top_of_zone (top_of_zone_table + 1 items) and a new one which
     * makes it top_of_zone_table + 2 elements large.
     */
    auto &z = zone_table[vzone_num];
    z.number = vzone_num;
    
    /*
     * Ok, insert the new zone here.
     */
    z.name = strdup("New Zone");
    z.number = vzone_num;
    z.builders = strdup("None");
#if _CIRCLEMUD >= CIRCLEMUD_VERSION(3, 0, 21)
    z.bot = bottom;
    z.top = top;
#else
    zone->top = (vzone_num * 100) + 99;
#endif
    z.lifespan = 30;
    z.age = 0;
    z.reset_mode = 2;
    z.zone_flags[0] = 0;
    z.zone_flags[1] = 0;
    z.zone_flags[2] = 0;
    z.zone_flags[3] = 0;
    z.min_level = 0;
    z.max_level = ADMLVL_IMPL;
    /*
     * No zone commands, just terminate it with an 'S'
     */
    auto &c = z.cmd.emplace_back();
    c.command = 'S';

    return rznum;
}

/*-------------------------------------------------------------------*/

void create_world_index(int znum, const char *type) {
    FILE *newfile, *oldfile;
    char new_name[32], old_name[32], *prefix;
    int num, found = false;
    char buf[MAX_STRING_LENGTH];
    char buf1[MAX_STRING_LENGTH];

    switch (*type) {
        case 'z':
            prefix = ZON_PREFIX;
            break;
        case 'w':
            prefix = WLD_PREFIX;
            break;
        case 'o':
            prefix = OBJ_PREFIX;
            break;
        case 'm':
            prefix = MOB_PREFIX;
            break;
        case 's':
            prefix = SHP_PREFIX;
            break;
        case 't':
            prefix = TRG_PREFIX;
            break;
        case 'g':
            prefix = GLD_PREFIX;
            break;
        default:
            /*
             * Caller messed up
             */
            return;
    }

    snprintf(old_name, sizeof(old_name), "%s/index", prefix);
    snprintf(new_name, sizeof(new_name), "%s/newindex", prefix);

    if (!(oldfile = fopen(old_name, "r"))) {
        mudlog(BRF, ADMLVL_IMPL, true, "SYSERR: OLC: Failed to open %s.", old_name);
        return;
    } else if (!(newfile = fopen(new_name, "w"))) {
        mudlog(BRF, ADMLVL_IMPL, true, "SYSERR: OLC: Failed to open %s.", new_name);
        fclose(oldfile);
        return;
    }

    /*
     * Index contents must be in order: search through the old file for the
     * right place, insert the new file, then copy the rest over.
     */
    snprintf(buf1, sizeof(buf1), "%d.%s", znum, type);
    while (get_line(oldfile, buf)) {
        if (*buf == '$') {
            /*
             * The following used to add a blank line, thanks to Brian Taylor for the fix... (Mythran)
             */
            fprintf(newfile, "%s", (!found ? strncat(buf1, "\n$\n", sizeof(buf1) - 1) : "$\n"));
            break;
        } else if (!found) {
            sscanf(buf, "%d", &num);
            if (num == znum) {
                found = true;
            } else if (num > znum) {
                found = true;
                fprintf(newfile, "%s\n", buf1);
            }
        }
        fprintf(newfile, "%s\n", buf);
    }

    fclose(newfile);
    fclose(oldfile);
    /*
     * Out with the old, in with the new.
     */
    remove(old_name);
    rename(new_name, old_name);
}

/*-------------------------------------------------------------------*/

void remove_room_zone_commands(zone_rnum zone, room_rnum room_num) {
    auto &z = zone_table[zone];
    z.cmd.erase(std::remove_if(z.cmd.begin(), z.cmd.end(), [&](reset_com &c) {
        switch(c.command) {
            case 'M':
            case 'O':
            case 'T':
            case 'V':
                return room_num == c.arg3;
            case 'D':
            case 'R':
                return room_num == c.arg1;
            default:
                return false;
        }
    }), z.cmd.end());
}

/*-------------------------------------------------------------------*/

/*
 * Save all the zone_table for this zone to disk.  This function now
 * writes simple comments in the form of (<name>) to each record.  A
 * header for each field is also there.
 */
int save_zone(zone_rnum zone_num) {
    return true;
}

/*-------------------------------------------------------------------*/

/*
 * Some common code to count the number of comands in the list.
 */
int count_commands(struct reset_com *list) {
    int count = 0;

    while (list[count].command != 'S')
        count++;

    return count;
}

/*-------------------------------------------------------------------*/

/*
 * Adds a new reset command into a list.  Takes a pointer to the list
 * so that it may play with the memory locations.
 */

/*-------------------------------------------------------------------*/


/*-------------------------------------------------------------------*/

/*
 * Error check user input and then add new (blank) command  
 */
int new_command(struct zone_data *zone, int pos) {
    int subcmd = 0;
    struct reset_com new_com;
    new_com.command = 'N';

    /* * Error check to ensure users hasn't given too large an index  */
    while (zone->cmd[subcmd].command != 'S')
        subcmd++;

    if (pos < 0 || pos > subcmd)
        return 0;
    zone->cmd.insert(zone->cmd.begin()+pos, new_com);
    return 1;
}

/*-------------------------------------------------------------------*/

/*
 * Error check user input and then remove command  
 */
void delete_zone_command(struct zone_data *zone, int pos) {
    zone->cmd.erase(zone->cmd.begin()+pos);
}

/*-------------------------------------------------------------------*/

reset_com::reset_com(const nlohmann::json &j) {
    if(j.contains("command")) command = j["command"].get<std::string>()[0];
    if(j.contains("if_flag")) if_flag = j["if_flag"];
    if(j.contains("arg1")) arg1 = j["arg1"];
    if(j.contains("arg2")) arg2 = j["arg2"];
    if(j.contains("arg3")) arg3 = j["arg3"];
    if(j.contains("arg4")) arg4 = j["arg4"];
    if(j.contains("arg5")) arg5 = j["arg5"];
    if(j.contains("sarg1")) sarg1 = j["sarg1"];
    if(j.contains("sarg2")) sarg2 = j["sarg2"];
}

nlohmann::json reset_com::serialize() {
    nlohmann::json j;

    std::string cmd;
    cmd.push_back(command);
    j["command"] = cmd;
    if(if_flag) j["if_flag"] = if_flag;
    if(arg1) j["arg1"] = arg1;
    if(arg2) j["arg2"] = arg2;
    if(arg3) j["arg3"] = arg3;
    if(arg4) j["arg4"] = arg4;
    if(arg5) j["arg5"] = arg5;
    if(!sarg1.empty()) j["sarg1"] = sarg1;
    if(!sarg2.empty()) j["sarg2"] = sarg2;

    return j;
}

    rapidjson::Document reset_com::rserialize() {
        rapidjson::Document d;
        d.SetObject();
        auto& allocator = d.GetAllocator();

        // Adding 'command' as a string
        std::string cmd(1, command);
        if (!cmd.empty()) {
            d.AddMember("command", rapidjson::Value(cmd.c_str(), allocator).Move(), allocator);
        }

        // Adding other members if they are set
        if (if_flag) {
            d.AddMember("if_flag", if_flag, allocator);
        }
        if (arg1) {
            d.AddMember("arg1", arg1, allocator);
        }
        if (arg2) {
            d.AddMember("arg2", arg2, allocator);
        }
        if (arg3) {
            d.AddMember("arg3", arg3, allocator);
        }
        if (arg4) {
            d.AddMember("arg4", arg4, allocator);
        }
        if (arg5) {
            d.AddMember("arg5", arg5, allocator);
        }
        if (!sarg1.empty()) {
            d.AddMember("sarg1", rapidjson::Value(sarg1.c_str(), allocator).Move(), allocator);
        }
        if (!sarg2.empty()) {
            d.AddMember("sarg2", rapidjson::Value(sarg2.c_str(), allocator).Move(), allocator);
        }

        return d;
    }

zone_data::~zone_data() {
    if(name) free(name);
    if(builders) free(builders);
}

nlohmann::json zone_data::serialize() {
    nlohmann::json j;

    j["number"] = number;
    if(name && strlen(name)) j["name"] = name;
    if(builders && strlen(builders)) j["builders"] = builders;
    if(lifespan) j["lifespan"] = lifespan;
    j["bot"] = bot;
    j["top"] = top;
    if(reset_mode) j["reset_mode"] = reset_mode;
    if(min_level) j["min_level"] = min_level;
    if(max_level) j["max_level"] = max_level;
    for(auto i = 0; i < NUM_ZONE_FLAGS; i++) if(IS_SET_AR(zone_flags, i)) j["zone_flags"].push_back(i);

    for(auto &c : cmd) j["cmd"].push_back(c.serialize());

    return j;
}

    rapidjson::Document zone_data::rserialize() {
        rapidjson::Document d;
        d.SetObject();
        auto& allocator = d.GetAllocator();

        // Adding basic fields
        d.AddMember("number", number, allocator);
        if (name && strlen(name)) {
            d.AddMember("name", rapidjson::Value(name, allocator).Move(), allocator);
        }
        if (builders && strlen(builders)) {
            d.AddMember("builders", rapidjson::Value(builders, allocator).Move(), allocator);
        }
        if (lifespan) {
            d.AddMember("lifespan", lifespan, allocator);
        }
        d.AddMember("bot", bot, allocator);
        d.AddMember("top", top, allocator);
        if (reset_mode) {
            d.AddMember("reset_mode", reset_mode, allocator);
        }
        if (min_level) {
            d.AddMember("min_level", min_level, allocator);
        }
        if (max_level) {
            d.AddMember("max_level", max_level, allocator);
        }

        // Adding 'zone_flags' as an array
        rapidjson::Value flagsArray(rapidjson::kArrayType);
        for (int i = 0; i < NUM_ZONE_FLAGS; ++i) {
            if (IS_SET_AR(zone_flags, i)) {
                flagsArray.PushBack(i, allocator);
            }
        }
        if (!flagsArray.Empty()) {
            d.AddMember("zone_flags", flagsArray, allocator);
        }

        // Adding 'cmd' as an array of serialized reset_com objects
        rapidjson::Value cmdArray(rapidjson::kArrayType);
        for (auto& c : cmd) {
            rapidjson::Document cmdDoc = c.rserialize(); // Assuming reset_com has a method rserialize
            rapidjson::Value cmdValue(rapidjson::kObjectType);
            cmdValue.CopyFrom(cmdDoc, allocator);
            cmdArray.PushBack(cmdValue, allocator);
        }
        d.AddMember("cmd", cmdArray, allocator);

        return d;
    }

zone_data::zone_data(const nlohmann::json &j) : zone_data() {
    if(j.contains("number")) number = j["number"];
    if(j.contains("name")) name = strdup(j["name"].get<std::string>().c_str());
    if(j.contains("builders")) builders = strdup(j["builders"].get<std::string>().c_str());
    if(j.contains("lifespan")) lifespan = j["lifespan"];
    if(j.contains("bot")) bot = j["bot"];
    if(j.contains("top")) top = j["top"];
    if(j.contains("reset_mode")) reset_mode = j["reset_mode"];
    if(j.contains("min_level")) min_level = j["min_level"];
    if(j.contains("max_level")) max_level = j["max_level"];
    if(j.contains("zone_flags")) {
        for(auto &f : j["zone_flags"]) SET_BIT_AR(zone_flags, f.get<int>());
    }

    if(j.contains("cmd")) {
        int line = 1;
        cmd.reserve(j["cmd"].size());
        for(auto &c : j["cmd"]) {
           auto &cm = cmd.emplace_back(c);
           cm.line = line++;
        }
    }
}
