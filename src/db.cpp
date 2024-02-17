/*************************************************************************
*   File: db.c                                          Part of CircleMUD *
*  Usage: Loading/saving chars, booting/resetting world, internal funcs   *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <fstream>
#include <regex>
#include <thread>

#include "dbat/db.h"
#include "dbat/utils.h"
#include "dbat/feats.h"
#include "dbat/config.h"
#include "dbat/players.h"
#include "dbat/spec_assign.h"
#include "dbat/act.informative.h"
#include "dbat/act.other.h"
#include "dbat/act.social.h"
#include "dbat/assemblies.h"
#include "dbat/reset.h"
#include "dbat/class.h"
#include "dbat/comm.h"
#include "dbat/dg_scripts.h"
#include "dbat/interpreter.h"
#include "dbat/genolc.h"
#include "dbat/shop.h"
#include "dbat/guild.h"
#include "dbat/handler.h"
#include "dbat/mail.h"
#include "dbat/clan.h"
#include "dbat/boards.h"
#include "dbat/constants.h"
#include "dbat/spells.h"
#include "dbat/races.h"
#include "dbat/genobj.h"
#include "dbat/account.h"
#include "dbat/maputils.h"

/**************************************************************************
*  declarations of most of the 'global' variables                         *
**************************************************************************/

bool gameIsLoading = true;
bool saveAll = false;
bool isMigrating = false;

struct config_data config_info; /* Game configuration list.    */

std::unordered_map<room_vnum, room_data> world;    /* array of rooms		 */
std::unordered_map<vnum, area_data> areas;    /* area information		 */

struct char_data *character_list = nullptr; /* global linked list of chars	 */
struct char_data *affect_list = nullptr; /* global linked list of chars with affects */
struct char_data *affectv_list = nullptr; /* global linked list of chars with round-based affects */
std::unordered_map<mob_vnum, struct index_data> mob_index;    /* index table for mobile file	 */
std::unordered_map<mob_vnum, struct char_data> mob_proto;    /* prototypes for mobs		 */

VnumIndex<obj_data> objectVnumIndex;
VnumIndex<char_data> characterVnumIndex;

struct obj_data *object_list = nullptr;    /* global linked list of objs	 */
std::unordered_map<obj_vnum, struct index_data> obj_index;    /* index table for object file	 */
std::unordered_map<obj_vnum, struct obj_data> obj_proto;    /* prototypes for objs		 */

std::unordered_map<int64_t, std::pair<time_t, struct char_data*>> uniqueCharacters;
/* hash tree for fast obj lookup */
std::unordered_map<int64_t, std::pair<time_t, struct obj_data*>> uniqueObjects;

std::unordered_map<zone_vnum, struct zone_data> zone_table;    /* zone table			 */

std::unordered_map<trig_vnum, std::shared_ptr<trig_proto>> trig_index; /* index table for triggers      */

int no_mail = 0;        /* mail disabled?		 */
int mini_mud = 0;        /* mini-mud mode?		 */
int no_rent_check = 0;        /* skip rent check on boot?	 */
time_t boot_time = 0;        /* time of mud boot		 */
int circle_restrict = 0;    /* level of game restriction	 */
int dballtime = 0;              /* used by dragonball load system*/
int SHENRON = false;            /* Shenron has been summoned     */
int DRAGONR = 0;                /* Room Shenron has been summoned to */
int DRAGONZ = 0;                /* Zone Shenron has been summoned to */
int WISH[2] = {0, 0};           /* Keeps track of wishes granted */
int DRAGONC = 0;                /* Keeps count of Shenron's remaining time */
struct char_data *EDRAGON = nullptr;      /* This is Shenron when he is loaded */
room_rnum r_mortal_start_room;    /* rnum of mortal start room	 */
room_rnum r_immort_start_room;    /* rnum of immort start room	 */
room_rnum r_frozen_start_room;    /* rnum of frozen start room	 */
int converting = false;

char *credits = nullptr;        /* game credits			 */
char *news = nullptr;        /* mud news			 */
char *motd = nullptr;        /* message of the day - mortals  */
char *imotd = nullptr;        /* message of the day - immorts  */
char *GREETINGS = nullptr;        /* opening credits screen	 */
char *GREETANSI = nullptr;        /* ansi opening credits screen	 */
char *help = nullptr;        /* help screen			 */
char *info = nullptr;        /* info page			 */
char *wizlist = nullptr;        /* list of higher gods		 */
char *immlist = nullptr;        /* list of peon gods		 */
char *background = nullptr;    /* background story		 */
char *handbook = nullptr;        /* handbook for new immortals	 */
char *policies = nullptr;        /* policies page		 */
char *ihelp = nullptr;        /* help screen (immortals)	 */

struct help_index_element *help_table = nullptr;    /* the help table	 */
int top_of_helpt = 0;

struct social_messg *soc_mess_list = nullptr;      /* list of socials */
int top_of_socialt = -1;                        /* number of socials */

struct time_info_data old_time_info;/* the infomation about the time    */
struct time_info_data time_info;/* the infomation about the time    */
struct weather_data weather_info;    /* the infomation about the weather */
std::set<zone_vnum> zone_reset_queue;

std::vector<obj_vnum> dbVnums = {20, 21, 22, 23, 24, 25, 26};

/* local functions */
static void dragon_level(struct char_data *ch);

static int file_to_string(const char *name, char *buf);

static int file_to_string_alloc(const char *name, char **buf);

static int count_alias_records(FILE *fl);

static void get_one_line(FILE *fl, char *buf);

static void check_start_rooms();

static void log_zone_error(zone_rnum zone, int cmd_no, const char *message);

static void reset_time();


void mag_assign_spells();

void create_command_list();

void sort_spells();

void load_banned();

void Read_Invalid_List();

int hsort(const void *a, const void *b);

void memorize_add(struct char_data *ch, int spellnum, int timer);

void free_feats();

void free_assemblies();

/* external vars */

static void dragon_level(struct char_data *ch) {
    struct descriptor_data *d;
    int level = 0, count = 0;

    for (d = descriptor_list; d; d = d->next) {
        if (IS_PLAYING(d) && GET_ADMLEVEL(d->character) < 1) {
            level += GET_LEVEL(d->character);
            count += 1;
        }
    }

    if (level > 0 && count > 0) {
        level = level / count;
    } else {
        level = rand_number(60, 110);
    }

    if (level < 50) {
        level = rand_number(40, 60);
    }

    ch->set(CharNum::Level, level + rand_number(5, 20));
}

/*************************************************************************
*  routines for booting the system                                       *
*************************************************************************/

/* this is necessary for the autowiz system */
void reboot_wizlists() {
    file_to_string_alloc(WIZLIST_FILE, &wizlist);
    file_to_string_alloc(IMMLIST_FILE, &immlist);
}

/* Wipe out all the loaded text files, for shutting down. */
void free_text_files() {
    char **textfiles[] = {
            &wizlist, &immlist, &news, &credits, &motd, &imotd, &help, &info,
            &policies, &handbook, &background, &GREETINGS, &GREETANSI, &ihelp, nullptr
    };
    int rf;

    for (rf = 0; textfiles[rf]; rf++)
        if (*textfiles[rf]) {
            free(*textfiles[rf]);
            *textfiles[rf] = nullptr;
        }
}


/*
 * Too bad it doesn't check the return values to let the user
 * know about -1 values.  This will result in an 'Okay.' to a
 * 'reload' command even when the string was not replaced.
 * To fix later, if desired. -gg 6/24/99
 */
ACMD(do_reboot) {
    send_to_char(ch, "Not a thing anymore.\r\n");
}

static nlohmann::json load_from_file(const std::filesystem::path& loc, const std::string& name) {
    auto path = loc / name;
    if(!std::filesystem::exists(path)) {
        basic_mud_log("File %s does not exist", path.c_str());
        return {};
    }
    std::ifstream file(path);
    if(!file.is_open()) {
        basic_mud_log("Error opening file %s", path.c_str());
        return {};
    }
    nlohmann::json j;
    file >> j;
    return j;
}

static void db_load_accounts(const std::filesystem::path& loc) {
    auto data = load_from_file(loc, "accounts.json");
    accounts.reserve(data.size());
    for(auto acc : data) {
        auto vn = acc["vn"].get<int64_t>();
        auto a = std::make_shared<account_data>(acc);
        accounts[vn] = a;
    }

}

static void db_load_players(const std::filesystem::path& loc) {
    auto data = load_from_file(loc, "players.json");
    players.reserve(data.size());
    for(auto j : data) {
        auto id = j["id"].get<int64_t>();
        auto p = std::make_shared<player_data>(j);
        players[id] = p;
    }
}

static void db_load_characters_initial(const std::filesystem::path& loc) {
    auto data = load_from_file(loc, "characters.json");
    uniqueCharacters.reserve(data.size());
    for(auto j : data) {
        auto id = j["id"].get<int64_t>();
        auto generation = j["generation"].get<int>();
        auto data = j["data"];
        auto c = new char_data();
        c->script = std::make_shared<script_data>(c);
        if(auto isPlayer = players.find(id); isPlayer != players.end()) {
            c->deserializePlayer(data, false);
            isPlayer->second->character = c;
        } else {
            c->deserializeInstance(data, true);
        }
        uniqueCharacters[id] = std::make_pair(generation, c);
    }
}

static void db_load_characters_finish(const std::filesystem::path& loc) {
    for(auto j : load_from_file(loc, "characters.json")) {
        auto id = j["id"].get<int64_t>();
        auto generation = j["generation"].get<int>();
        if(auto cf = uniqueCharacters.find(id); cf != uniqueCharacters.end()) {
            if(auto c = cf->second.second) {
                c->deserializeRelations(j["relations"]);
                c->deserializeLocation(j["location"]);
            }
        }
    }
}

static void db_load_items_initial(const std::filesystem::path& loc) {
    auto data = load_from_file(loc, "items.json");
    uniqueObjects.reserve(data.size());
    for(auto j : data) {
        auto id = j["id"].get<int64_t>();
        auto generation = j["generation"].get<int>();
        auto data = j["data"];
        auto i = new obj_data();
        i->script = std::make_shared<script_data>(i);
        i->deserializeInstance(data, false);
        uniqueObjects[id] = std::make_pair(generation, i);
    }
}

static void db_load_items_finish(const std::filesystem::path& loc) {
    for(auto j : load_from_file(loc, "items.json")) {
        auto id = j["id"].get<int64_t>();
        auto generation = j["generation"].get<int>();
        if(auto cf = uniqueObjects.find(id); cf != uniqueObjects.end()) {
            if(auto i = cf->second.second) {
                i->deserializeRelations(j["relations"]);
                i->deserializeLocation(j["location"], j["slot"].get<int>());
            }
        }
    }
}

static void db_load_activate_entities() {
    // activate all items which ended up "in the world".
    for(auto &[id, r] : world) {
        if(r.script) r.script->activate();
        assign_triggers(&r, WLD_TRIGGER);
        r.activateContents();
        for(auto c = r.people; c; c = c->next_in_room) {
            if(IS_NPC(c)) {
                c->activate();
            }
        }
    }
}

static void db_load_globaldata(const std::filesystem::path& loc) {
    auto j = load_from_file(loc, "globaldata.json");

    if(j.contains("time")) {
        time_info.deserialize(j["time"]);
    }
    if(j.contains("weather")) {
        weather_info.deserialize(j["weather"]);
    }
}


static void db_load_zones(const std::filesystem::path& loc) {
    auto data = load_from_file(loc, "zones.json");
    zone_table.reserve(data.size());
    for(auto j : data) {
        auto id = j["number"].get<int64_t>();
        zone_table.emplace(id, j);
    }
}

static void db_load_dgscript_prototypes(const std::filesystem::path& loc) {
    auto data = load_from_file(loc, "dgScriptPrototypes.json");
    trig_index.reserve(data.size());
    for(auto j : load_from_file(loc, "dgScriptPrototypes.json")) {
        auto id = j["vn"].get<int64_t>();
        auto proto = std::make_shared<trig_proto>(j);
        trig_index[id] = proto;
    }
}

static void db_load_rooms(const std::filesystem::path& loc) {
    auto data = load_from_file(loc, "rooms.json");
    world.reserve(data.size());
    for(auto j : data) {
        auto id = j["vn"].get<int64_t>();
        auto r = world.emplace(id, j);
        r.first->second.script = std::make_shared<script_data>(&r.first->second);
        r.first->second.zone = real_zone_by_thing(id);
    }
}

static void db_load_exits(const std::filesystem::path& loc) {
    for(auto j : load_from_file(loc, "exits.json")) {
        auto id = j["room"].get<int64_t>();
        auto dir = j["direction"].get<int64_t>();
        auto &r = world[id];
        r.dir_option[dir] = new room_direction_data(j["data"]);
    }
}

static void db_load_shops(const std::filesystem::path& loc) {
    auto data = load_from_file(loc, "shops.json");
    shop_index.reserve(data.size());
    for(auto j : data) {
        auto id = j["vnum"].get<int64_t>();
        shop_index.emplace(id, j);
    }
}

static void db_load_guilds(const std::filesystem::path& loc) {
    auto data = load_from_file(loc, "guilds.json");
    guild_index.reserve(data.size());
    for(auto j : data) {
        auto id = j["vnum"].get<int64_t>();
        guild_index.emplace(id, j);
    }
}

static void db_load_item_prototypes(const std::filesystem::path& loc) {
    auto data = load_from_file(loc, "itemPrototypes.json");
    obj_proto.reserve(data.size());
    obj_index.reserve(data.size());
    for(auto j : data) {
        auto id = j["vn"].get<int64_t>();
        auto p = obj_proto.emplace(id, j);
        p.first->second.zone = real_zone_by_thing(id);
        auto &i = obj_index[id];
        i.vn = id;
    }
}

static void db_load_npc_prototypes(const std::filesystem::path& loc) {
    auto data = load_from_file(loc, "npcPrototypes.json");
    mob_proto.reserve(data.size());
    mob_index.reserve(data.size());
    for(auto j : load_from_file(loc, "npcPrototypes.json")) {
        auto id = j["vn"].get<int64_t>();
        auto p = mob_proto.emplace(id, j);
        p.first->second.zone = real_zone_by_thing(id);
        auto &i = mob_index[id];
        i.vn = id;
    }
}

static void db_load_areas(const std::filesystem::path& loc) {
    for(auto j : load_from_file(loc, "areas.json")) {
        auto id = j["vn"].get<int64_t>();
        auto a = areas.emplace(id, j);

        for(auto &r : a.first->second.rooms) {
            auto room = world.find(r);
            if(room != world.end()) {
                room->second.area = id;
            }
        }
    }

    for(auto &[vn, a] : areas) {
        if(a.parent) {
            auto parent = areas.find(a.parent.value());
            if(parent != areas.end()) {
                parent->second.children.insert(vn);
            }
        }
    }
}

static std::vector<std::filesystem::path> getDumpFiles() {
    std::filesystem::path dir = "dumps"; // Change to your directory
    std::vector<std::filesystem::path> directories;

    auto pattern = "dump-";
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_directory() && entry.path().filename().string().starts_with(pattern)) {
            directories.push_back(entry.path());
        }
    }

    std::sort(directories.begin(), directories.end(), std::greater<>());
    return directories;
}

void boot_db_world() {

    auto dumps = getDumpFiles();
    if (!dumps.empty()) {
        std::cout << "Newest state dump: " << dumps.front() << '\n';
    } else {
        std::cout << "No matching state dumps found.\n";
        return;
    }

    auto latest = dumps.front();

    std::vector<std::thread> threads;

    basic_mud_log("Loading Zones...");
    threads.emplace_back([&latest] {
        db_load_zones(latest);
    });

    basic_mud_log("Loading DgScripts and generating index.");
    threads.emplace_back([&latest] {
        db_load_dgscript_prototypes(latest);
    });

    basic_mud_log("Loading mobs and generating index.");
    threads.emplace_back([&latest] {
        db_load_npc_prototypes(latest);
    });

    basic_mud_log("Loading objs and generating index.");
    threads.emplace_back([&latest] {
        db_load_item_prototypes(latest);
    });

    basic_mud_log("Loading rooms.");
    threads.emplace_back([&latest] {
        db_load_rooms(latest);
    });

    basic_mud_log("Loading shops.");
    threads.emplace_back([&latest] {
        db_load_shops(latest);
    });

    basic_mud_log("Loading guild masters.");
    threads.emplace_back([&latest] {
        db_load_guilds(latest);
    });

    for(auto &t : threads) {
        t.join();
    }
    threads.clear();

    basic_mud_log("Loading exits.");
    threads.emplace_back([&latest] {
        db_load_exits(latest);
    });

    basic_mud_log("Loading areas.");
    threads.emplace_back([&latest] {
        db_load_areas(latest);
    });

    basic_mud_log("Loading global data...");
    threads.emplace_back([&latest] {
        db_load_globaldata(latest);
    });

    basic_mud_log("Loading accounts.");
    threads.emplace_back([&latest] {
        db_load_accounts(latest);
    });

    for(auto &t : threads) {
        t.join();
    }
    threads.clear();

    basic_mud_log("Loading players.");
    db_load_players(latest);

    basic_mud_log("Loading characters initial...");
    threads.emplace_back([&latest] {
        db_load_characters_initial(latest);
    });

    basic_mud_log("Loading items initial...");
    threads.emplace_back([&latest] {
        db_load_items_initial(latest);
    });

    for(auto &t : threads) {
        t.join();
    }
    threads.clear();

    // Now that all of the game entities have been spawned, we can finish loading
    // relations between them.

    basic_mud_log("Loading characters finish...");
    db_load_characters_finish(latest);

    basic_mud_log("Loading items finish...");
    db_load_items_finish(latest);

    basic_mud_log("Running activation of entities...");
    db_load_activate_entities();

    basic_mud_log("Checking start rooms.");
    check_start_rooms();

    basic_mud_log("Loading disabled commands list...");
    load_disabled();

    boot_db_shadow();
}

void boot_db_shadow() {
    if (SELFISHMETER >= 10) {
        basic_mud_log("Loading Shadow Dragons.");
        load_shadow_dragons();
    }
}



void free_extra_descriptions(struct extra_descr_data *edesc) {
    struct extra_descr_data *enext;

    for (; edesc; edesc = enext) {
        enext = edesc->next;

        free(edesc->keyword);
        free(edesc->description);
        free(edesc);
    }
}


/* Free the world, in a memory allocation sense. */
void destroy_db() {
    ssize_t cnt, itr;
    struct char_data *chtmp;
    struct obj_data *objtmp;

    /* Active Mobiles & Players */
    while (character_list) {
        chtmp = character_list;
        character_list = character_list->next;
        if (chtmp->master)
            stop_follower(chtmp);
        free_char(chtmp);
    }

    /* Active Objects */
    while (object_list) {
        objtmp = object_list;
        object_list = object_list->next;
        free_obj(objtmp);
    }

    uniqueObjects.clear();

    /* Objects */
    obj_proto.clear();
    obj_index.clear();
    
    /* Mobiles */
    mob_proto.clear();
    mob_index.clear();
    /* Shops */
    shop_index.clear();

    /* Guilds */
    guild_index.clear();

    /* Zones */
    /* zone table reset queue */
    zone_reset_queue.clear();

    zone_table.clear();

    /* context sensitive help system */
    free_context_help();

    free_feats();

    basic_mud_log("Freeing Assemblies.");
    free_assemblies();

}


/* You can define this to anything you want; 1 would work but it would
   be very inefficient. I would recommend that it actually be close to
   your total number of in-game objects if not double or triple it just
   to minimize collisions. The only O(n) [n=NUM_OBJ_UNIQUE_POOLS]
   operation is initialization of the hash table, all other operations
   that have to traverse are O(n) [n=num elements in pool], so more
   pools are better.
     - Elie Rosenblum Dec. 12 2003 */
#define NUM_OBJ_UNIQUE_POOLS 5000

void boot_db_textfiles() {
    basic_mud_log("Reading news, credits, help, ihelp, bground, info & motds.");
    file_to_string_alloc(NEWS_FILE, &news);
    file_to_string_alloc(CREDITS_FILE, &credits);
    file_to_string_alloc(MOTD_FILE, &motd);
    file_to_string_alloc(IMOTD_FILE, &imotd);
    file_to_string_alloc(HELP_PAGE_FILE, &help);
    file_to_string_alloc(INFO_FILE, &info);
    file_to_string_alloc(WIZLIST_FILE, &wizlist);
    file_to_string_alloc(IMMLIST_FILE, &immlist);
    file_to_string_alloc(POLICIES_FILE, &policies);
    file_to_string_alloc(HANDBOOK_FILE, &handbook);
    file_to_string_alloc(BACKGROUND_FILE, &background);
    file_to_string_alloc(IHELP_PAGE_FILE, &ihelp);
    file_to_string_alloc(GREETINGS_FILE, &GREETINGS);
    file_to_string_alloc(GREETANSI_FILE, &GREETANSI);
}

void boot_db_time() {
    basic_mud_log("Resetting the game time:");
    reset_time();
}

void boot_db_spellfeats() {
    basic_mud_log("Loading spell definitions.");
    mag_assign_spells();

    basic_mud_log("Loading feats.");
    assign_feats();
}

void boot_db_help() {
    basic_mud_log("Loading help entries.");
    index_boot_help();

    basic_mud_log("Setting up context sensitive help system for OLC");
    boot_context_help();
}

void boot_db_mail() {
    basic_mud_log("Booting mail system.");
    if (!scan_file()) {
        basic_mud_log("    Mail boot failed -- Mail system disabled");
    }
}

void boot_db_socials() {
    basic_mud_log("Loading social messages.");
    boot_social_messages();
}

void boot_db_clans() {
    basic_mud_log("Loading Clans.");
    clanBoot();
}

void boot_db_commands() {
    basic_mud_log("Building command list.");
    create_command_list(); /* aedit patch -- M. Scott */
}

void boot_db_specials() {
    basic_mud_log("Assigning function pointers:");
    basic_mud_log("   Mobiles.");
    assign_mobiles();
    basic_mud_log("   Shopkeepers.");
    assign_the_shopkeepers();
    basic_mud_log("   Objects.");
    assign_objects();
    basic_mud_log("   Rooms.");
    assign_rooms();
    basic_mud_log("   Guildmasters.");
    assign_the_guilds();
}

void boot_db_assemblies() {
    basic_mud_log("Booting assembled objects.");
    assemblyBootAssemblies();
}

void boot_db_sort() {
    basic_mud_log("Sorting command list and spells.");
    sort_commands();
    sort_spells();
    sort_feats();
}

void boot_db_boards() {
    basic_mud_log("Booting boards system.");
    init_boards();
}

void boot_db_banned() {
    basic_mud_log("Reading banned site and invalid-name list.");
    load_banned();
    Read_Invalid_List();
}


void boot_db_spacemap() {
    FILE *mapfile = fopen("../lib/surface.map", "r");
    int rowcounter, colcounter;
    int vnum_read;
    for (rowcounter = 0; rowcounter <= MAP_ROWS; rowcounter++) {
        for (colcounter = 0; colcounter <= MAP_COLS; colcounter++) {
            fscanf(mapfile, "%d", &vnum_read);
            mapnums[rowcounter][colcounter] = real_room(vnum_read);
        }
    }
    fclose(mapfile);
}


/* body of the booting system */


void boot_db_new() {
    boot_db_time();
    boot_db_textfiles();
    boot_db_spellfeats();
    boot_db_world();
    boot_db_mail();
    boot_db_socials();
    boot_db_clans();
    boot_db_commands();
    boot_db_specials();
    boot_db_assemblies();
    boot_db_sort();
    boot_db_boards();
    boot_db_banned();
    boot_db_spacemap();
    topLoad();
}


/* save the auction file */
void auc_save() {
    FILE *fl;

    if ((fl = fopen(AUCTION_FILE, "w")) == nullptr)
        basic_mud_log("SYSERR: Can't write to '%s' auction file.", AUCTION_FILE);
    else {
        struct obj_data *obj, *next_obj;

        for (obj = world[real_room(80)].contents; obj; obj = next_obj) {
            next_obj = obj->next_content;
            if (obj) {
                fprintf(fl, "%" I64T " %s %d %d %d %d %ld\n", obj->id, GET_AUCTERN(obj), GET_AUCTER(obj),
                        GET_CURBID(obj), GET_STARTBID(obj), GET_BID(obj), GET_AUCTIME(obj));
            }
        }
        fprintf(fl, "~END~\n");
        fclose(fl);
    }
}

/* load from auction file */
void auc_load(struct obj_data *obj) {
    char line[500], filler[50];
    int64_t oID;
    time_t timer;
    int aID, bID, cost, startc;
    FILE *fl;

    if ((fl = fopen(AUCTION_FILE, "r")) == nullptr)
        basic_mud_log("SYSERR: Can't read from '%s' auction file.", AUCTION_FILE);
    else {
        while (!feof(fl)) {
            get_line(fl, line);
            sscanf(line, "%" I64T " %s %d %d %d %d %ld\n", &oID, filler, &aID, &bID, &startc, &cost, &timer);
            if (obj->id == oID) {
                GET_AUCTERN(obj) = strdup(filler);
                GET_AUCTER(obj) = aID;
                GET_CURBID(obj) = bID;
                GET_STARTBID(obj) = startc;
                GET_BID(obj) = cost;
                GET_AUCTIME(obj) = timer;
            }
        }
        fclose(fl);
    }
}

time_t old_beginning_of_time;

/* reset the time in the game from file */
static void reset_time() {
    time_t beginning_of_time = 0;
    FILE *bgtime;

    if ((bgtime = fopen(TIME_FILE, "r")) == nullptr)
        basic_mud_log("SYSERR: Can't read from '%s' time file.", TIME_FILE);
    else {
        fscanf(bgtime, "%ld\n", &beginning_of_time);
        fscanf(bgtime, "%ld\n", &NEWSUPDATE);
        fscanf(bgtime, "%ld\n", &BOARDNEWMORT);
        fscanf(bgtime, "%ld\n", &BOARDNEWDUO);
        fscanf(bgtime, "%ld\n", &BOARDNEWCOD);
        fscanf(bgtime, "%ld\n", &BOARDNEWBUI);
        fscanf(bgtime, "%ld\n", &BOARDNEWIMM);
        fscanf(bgtime, "%ld\n", &INTERESTTIME);
        fscanf(bgtime, "%ld\n", &LASTINTEREST);
        fscanf(bgtime, "%d\n", &HIGHPCOUNT);
        fscanf(bgtime, "%ld\n", &PCOUNTDATE);
        fscanf(bgtime, "%d\n", &WISHTIME);
        fscanf(bgtime, "%d\n", &PCOUNT);
        fscanf(bgtime, "%ld\n", &LASTPAYOUT);
        fscanf(bgtime, "%d\n", &LASTPAYTYPE);
        fscanf(bgtime, "%d\n", &LASTNEWS);
        fscanf(bgtime, "%d\n", &dballtime);
        fscanf(bgtime, "%d\n", &SELFISHMETER);
        fscanf(bgtime, "%d\n", &SHADOW_DRAGON1);
        fscanf(bgtime, "%d\n", &SHADOW_DRAGON2);
        fscanf(bgtime, "%d\n", &SHADOW_DRAGON3);
        fscanf(bgtime, "%d\n", &SHADOW_DRAGON4);
        fscanf(bgtime, "%d\n", &SHADOW_DRAGON5);
        fscanf(bgtime, "%d\n", &SHADOW_DRAGON6);
        fscanf(bgtime, "%d\n", &SHADOW_DRAGON7);
        fscanf(bgtime, "%d\n", &ERAPLAYERS);
        fclose(bgtime);
    }

    if (dballtime == 0)
        dballtime = 604800;

    if (beginning_of_time == 0)
        beginning_of_time = 650336715;
    old_beginning_of_time = beginning_of_time;

}


/* Write the time in 'when' to the MUD-time file. */
void save_mud_time(struct time_info_data *when) {
    FILE *bgtime;

    if ((bgtime = fopen(TIME_FILE, "w")) == nullptr)
        basic_mud_log("SYSERR: Can't write to '%s' time file.", TIME_FILE);
    else {
        fprintf(bgtime, "%ld\n", mud_time_to_secs(when));
        fprintf(bgtime, "%ld\n", NEWSUPDATE);
        fprintf(bgtime, "%ld\n", BOARDNEWMORT);
        fprintf(bgtime, "%ld\n", BOARDNEWDUO);
        fprintf(bgtime, "%ld\n", BOARDNEWCOD);
        fprintf(bgtime, "%ld\n", BOARDNEWBUI);
        fprintf(bgtime, "%ld\n", BOARDNEWIMM);
        fprintf(bgtime, "%ld\n", INTERESTTIME);
        fprintf(bgtime, "%ld\n", LASTINTEREST);
        fprintf(bgtime, "%d\n", HIGHPCOUNT);
        fprintf(bgtime, "%ld\n", PCOUNTDATE);
        fprintf(bgtime, "%d\n", WISHTIME);
        fprintf(bgtime, "%d\n", PCOUNT);
        fprintf(bgtime, "%ld\n", LASTPAYOUT);
        fprintf(bgtime, "%d\n", LASTPAYTYPE);
        fprintf(bgtime, "%d\n", LASTNEWS);
        fprintf(bgtime, "%d\n", dballtime);
        fprintf(bgtime, "%d\n", SELFISHMETER);
        fprintf(bgtime, "%d\n", SHADOW_DRAGON1);
        fprintf(bgtime, "%d\n", SHADOW_DRAGON2);
        fprintf(bgtime, "%d\n", SHADOW_DRAGON3);
        fprintf(bgtime, "%d\n", SHADOW_DRAGON4);
        fprintf(bgtime, "%d\n", SHADOW_DRAGON5);
        fprintf(bgtime, "%d\n", SHADOW_DRAGON6);
        fprintf(bgtime, "%d\n", SHADOW_DRAGON7);
        fprintf(bgtime, "%d\n", ERAPLAYERS);
        fclose(bgtime);
    }
}


/*
 * Thanks to Andrey (andrey@alex-ua.com) for this bit of code, although I
 * did add the 'goto' and changed some "while()" into "do { } while()".
 *	-gg 6/24/98 (technically 6/25/98, but I care not.)
 */
static int count_alias_records(FILE *fl) {
    char key[READ_SIZE], next_key[READ_SIZE];
    char line[READ_SIZE], *scan;
    int total_keywords = 0;

    /* get the first keyword line */
    get_one_line(fl, key);

    while (*key != '$') {
        /* skip the text */
        do {
            get_one_line(fl, line);
            if (feof(fl))
                goto ackeof;
        } while (*line != '#');

        /* now count keywords */
        scan = key;
        do {
            scan = one_word(scan, next_key);
            if (*next_key)
                ++total_keywords;
        } while (*next_key);

        /* get next keyword line (or $) */
        get_one_line(fl, key);

        if (feof(fl))
            goto ackeof;
    }

    return (total_keywords);

    /* No, they are not evil. -gg 6/24/98 */
    ackeof:
    basic_mud_log("SYSERR: Unexpected end of help file.");
    exit(1);    /* Some day we hope to handle these things better... */
}

void index_boot_help() {
    const char *index_filename, *prefix = nullptr;    /* nullptr or egcs 1.1 complains */
    FILE *db_index, *db_file;
    int rec_count = 0, size[2];
    char buf2[PATH_MAX], buf1[MAX_STRING_LENGTH];
    int mode = DB_BOOT_HLP;

    switch (mode) {
        case DB_BOOT_HLP:
            prefix = HLP_PREFIX;
            break;
        default:
            basic_mud_log("SYSERR: Unknown subcommand %d to index_boot!", mode);
            exit(1);
    }

    if (mini_mud)
        index_filename = MINDEX_FILE;
    else
        index_filename = INDEX_FILE;

    snprintf(buf2, sizeof(buf2), "%s%s", prefix, index_filename);
    if (!(db_index = fopen(buf2, "r"))) {
        basic_mud_log("SYSERR: opening index file '%s': %s", buf2, strerror(errno));
        exit(1);
    }

    /* first, count the number of records in the file so we can malloc */
    fscanf(db_index, "%s\n", buf1);
    while (*buf1 != '$') {
        snprintf(buf2, sizeof(buf2), "%s%s", prefix, buf1);
        if (!(db_file = fopen(buf2, "r"))) {
            basic_mud_log("SYSERR: File '%s' listed in '%s%s': %s", buf2, prefix,
                index_filename, strerror(errno));
            fscanf(db_index, "%s\n", buf1);
            continue;
        } else {
            rec_count += count_alias_records(db_file);
        }

        fclose(db_file);
        fscanf(db_index, "%s\n", buf1);
    }

    /* Exit if 0 records, unless this is shops */
    if (!rec_count) {
        return;
    }

    /*
   * NOTE: "bytes" does _not_ include strings or other later malloc'd things.
   */
    switch (mode) {
        case DB_BOOT_HLP:
            CREATE(help_table, struct help_index_element, rec_count);
            size[0] = sizeof(struct help_index_element) * rec_count;
            basic_mud_log("   %d entries, %d bytes.", rec_count, size[0]);
            break;
    }

    rewind(db_index);
    fscanf(db_index, "%s\n", buf1);
    while (*buf1 != '$') {
        snprintf(buf2, sizeof(buf2), "%s%s", prefix, buf1);
        if (!(db_file = fopen(buf2, "r"))) {
            basic_mud_log("SYSERR: %s: %s", buf2, strerror(errno));
            exit(1);
        }
        switch (mode) {
            case DB_BOOT_HLP:
                load_help(db_file, buf2);
                break;
        }

        fclose(db_file);
        fscanf(db_index, "%s\n", buf1);
    }
    fclose(db_index);

    /* Sort the help index. */
    if (mode == DB_BOOT_HLP) {
        qsort(help_table, top_of_helpt, sizeof(struct help_index_element), hsort);
        top_of_helpt--;
    }
}

bitvector_t asciiflag_conv(char *flag) {
    bitvector_t flags = 0;
    int is_num = true;
    char *p;

    for (p = flag; *p; p++) {
        if (islower(*p))
            flags |= 1 << (*p - 'a');
        else if (isupper(*p))
            flags |= 1 << (26 + (*p - 'A'));

        if (!(isdigit(*p) || (*p == '-')))
            is_num = false;
    }

    if (is_num)
        flags = atol(flag);

    return (flags);
}

/* make sure the start rooms exist & resolve their vnums to rnums */
static void check_start_rooms() {
    if ((r_mortal_start_room = real_room(CONFIG_MORTAL_START)) == NOWHERE) {
        basic_mud_log("SYSERR:  Mortal start room does not exist.  Change mortal_start_room in lib/etc/config.");
        exit(1);
    }
    if ((r_immort_start_room = real_room(CONFIG_IMMORTAL_START)) == NOWHERE) {
        if (!mini_mud)
            basic_mud_log("SYSERR:  Warning: Immort start room does not exist.  Change immort_start_room in /lib/etc/config.");
        r_immort_start_room = r_mortal_start_room;
    }
    if ((r_frozen_start_room = real_room(CONFIG_FROZEN_START)) == NOWHERE) {
        if (!mini_mud)
            basic_mud_log("SYSERR:  Warning: Frozen start room does not exist.  Change frozen_start_room in /lib/etc/config.");
        r_frozen_start_room = r_mortal_start_room;
    }
}


static void get_one_line(FILE *fl, char *buf) {
    if (fgets(buf, READ_SIZE, fl) == nullptr) {
        basic_mud_log("SYSERR: error reading help file: not terminated with $?");
        exit(1);
    }

    buf[strlen(buf) - 1] = '\0'; /* take off the trailing \n */
}

void free_help(struct help_index_element *cmhelp) {
    if (cmhelp->keywords)
        free(cmhelp->keywords);
    if (cmhelp->entry && !cmhelp->duplicate)
        free(cmhelp->entry);

    free(cmhelp);
}

void free_help_table() {
    if (help_table) {
        int hp;
        for (hp = 0; hp < top_of_helpt; hp++) {
            if (help_table[hp].keywords)
                free(help_table[hp].keywords);
            if (help_table[hp].entry && !help_table[hp].duplicate)
                free(help_table[hp].entry);
        }
        free(help_table);
        help_table = nullptr;
    }
    top_of_helpt = 0;
}

void load_help(FILE *fl, char *name) {
    char key[READ_SIZE + 1], next_key[READ_SIZE + 1], entry[32384];
    size_t entrylen;
    char line[READ_SIZE + 1], hname[READ_SIZE + 1], *scan;
    struct help_index_element el;

    strlcpy(hname, name, sizeof(hname));

    get_one_line(fl, key);
    while (*key != '$') {
        strcat(key, "\r\n"); /* strcat: OK (READ_SIZE - "\n"  "\r\n" == READ_SIZE  1) */
        entrylen = strlcpy(entry, key, sizeof(entry));

        /* Read in the corresponding help entry. */
        get_one_line(fl, line);
        while (*line != '#' && entrylen < sizeof(entry) - 1) {
            entrylen += strlcpy(entry + entrylen, line, sizeof(entry) - entrylen);

            if (entrylen + 2 < sizeof(entry) - 1) {
                strcpy(entry + entrylen, "\r\n"); /* strcpy: OK (size checked above) */
                entrylen += 2;
            }
            get_one_line(fl, line);
        }

        if (entrylen >= sizeof(entry) - 1) {
            int keysize;
            const char *truncmsg = "\r\n*TRUNCATED*\r\n";

            strcpy(entry + sizeof(entry) - strlen(truncmsg) - 1,
                   truncmsg); /* strcpy: OK (assuming sane 'entry' size) */

            keysize = strlen(key) - 2;
            basic_mud_log("SYSERR: Help entry exceeded buffer space: %.*s", keysize, key);

            /* If we ran out of buffer space, eat the rest of the entry. */
            while (*line != '#')
                get_one_line(fl, line);
        }

        if (*line == '#') {
            if (sscanf(line, "#%d", &el.min_level) != 1) {
                basic_mud_log("SYSERR: Help entry does not have a min level. %s", key);
                el.min_level = 0;
            }
        }

        el.duplicate = 0;
        el.entry = strdup(entry);
        scan = one_word(key, next_key);

        while (*next_key) {
            el.keywords = strdup(next_key);
            help_table[top_of_helpt++] = el;
            el.duplicate++;
            scan = one_word(scan, next_key);
        }
        get_one_line(fl, key);
    }
}

int hsort(const void *a, const void *b) {
    const struct help_index_element *a1, *b1;

    a1 = (const struct help_index_element *) a;
    b1 = (const struct help_index_element *) b;

    return (strcasecmp(a1->keywords, b1->keywords));
}

/*************************************************************************
*  procedures for resetting, both play-time and boot-time	 	 *
*************************************************************************/


int vnum_mobile(char *searchname, struct char_data *ch) {
    int found = 0;

    for (auto &m : mob_proto)
        if (isname(searchname, m.second.name))
            send_to_char(ch, "%3d. [%5d] %-40s %s\r\n",
                         ++found, m.first, m.second.getShortDesc(),
                         !m.second.proto_script.empty() ? m.second.scriptString().c_str() : "");

    return (found);
}


int vnum_object(char *searchname, struct char_data *ch) {
    int found = 0;

    for (auto &o : obj_proto)
        if (isname(searchname, o.second.name))
            send_to_char(ch, "%3d. [%5d] %-40s %s\r\n",
                         ++found, o.first, o.second.getShortDesc(),
                         !o.second.proto_script.empty() ? o.second.scriptString().c_str() : "");

    return (found);
}


int vnum_material(char *searchname, struct char_data *ch) {
    int found = 0;

    for (auto &o : obj_proto)
        if (isname(searchname, material_names[o.second.value[VAL_ALL_MATERIAL]])) {
            send_to_char(ch, "%3d. [%5d] %-40s %s\r\n",
                         ++found, o.first, o.second.getShortDesc(),
                         !o.second.proto_script.empty() ? o.second.scriptString().c_str() : "");
        }

    return (found);
}


int vnum_weapontype(char *searchname, struct char_data *ch) {
    int found = 0;

    for (auto &o : obj_proto)
        if (o.second.type_flag == ITEM_WEAPON) {
            if (isname(searchname, weapon_type[o.second.value[VAL_WEAPON_SKILL]])) {
                send_to_char(ch, "%3d. [%5d] %-40s %s\r\n",
                             ++found, o.first, o.second.getShortDesc(),
                             !o.second.proto_script.empty() ? o.second.scriptString().c_str() : "");
            }
        }

    return (found);
}


int vnum_armortype(char *searchname, struct char_data *ch) {
    int found = 0;

    for (auto &o : obj_proto)
        if (o.second.type_flag == ITEM_ARMOR) {
            if (isname(searchname, armor_type[o.second.value[VAL_ARMOR_SKILL]])) {
                send_to_char(ch, "%3d. [%5d] %-40s %s\r\n",
                             ++found, o.first, o.second.getShortDesc(),
                             !o.second.proto_script.empty() ? o.second.scriptString().c_str() : "");
            }
        }

    return (found);
}

/* create a character, and add it to the char list */
struct char_data *create_char(bool activate) {
    auto ch = new char_data();
    ch->script = std::make_shared<script_data>(ch);

    if(activate) {
        ch->id = nextCharID();
        ch->generation = time(nullptr);
        check_unique_id(ch);
        add_unique_id(ch);
        ch->activate();
    }

    return ch;
}


/* create a new mobile from a prototype */
struct char_data *read_mobile(mob_vnum nr, int type) /* and mob_rnum */
{
    auto proto = mob_proto.find(nr);

    if(proto == mob_proto.end()) {
        basic_mud_log("WARNING: Mobile vnum %d does not exist in database.", nr);
        return (nullptr);
    }

    // Weird hack to ensure script_data is not overwritten by prototype...
    auto mob = new char_data();

    *mob = proto->second;
    mob->script = std::make_shared<script_data>(mob);

    mob->id = nextCharID();
    mob->generation = time(nullptr);
    check_unique_id(mob);
    add_unique_id(mob);
    mob->activate();

    std::map<CharAppearance, int> setNumsTo;

    if (!(IS_HOSHIJIN(mob) && GET_SEX(mob) == SEX_MALE)) {
        setNumsTo[CharAppearance::HairLength] = rand_number(0, 4);
        setNumsTo[CharAppearance::HairColor] = rand_number(1, 13);
        setNumsTo[CharAppearance::HairStyle] = rand_number(1, 11);
    }

    setNumsTo[CharAppearance::EyeColor] = rand_number(0, 11);

    GET_ABSORBS(mob) = 0;
    ABSORBING(mob) = nullptr;
    ABSORBBY(mob) = nullptr;
    SITS(mob) = nullptr;
    BLOCKED(mob) = nullptr;
    BLOCKS(mob) = nullptr;

    if (!IS_HUMAN(mob) && !IS_SAIYAN(mob) && !IS_HALFBREED(mob) && !IS_NAMEK(mob)) {
        setNumsTo[CharAppearance::SkinColor] = rand_number(0, 11);
    }
    if (IS_NAMEK(mob)) {
        setNumsTo[CharAppearance::SkinColor] = 2;
    }
    if (IS_HUMAN(mob) || IS_SAIYAN(mob) || IS_HALFBREED(mob)) {
        if (rand_number(1, 5) <= 2) {
            setNumsTo[CharAppearance::SkinColor] = rand_number(0, 1);
        } else if (rand_number(1, 5) <= 4) {
            setNumsTo[CharAppearance::SkinColor] = rand_number(4, 5);
        } else if (rand_number(1, 5) <= 5) {
            setNumsTo[CharAppearance::SkinColor] = rand_number(9, 10);
        }
    }
    if (IS_SAIYAN(mob)) {
        setNumsTo[CharAppearance::HairColor] = rand_number(1, 2);
        setNumsTo[CharAppearance::EyeColor] = 1;
    }

    for(auto &[cint, val] : setNumsTo) {
        mob->set(cint, val);
    }

    if (GET_MOB_VNUM(mob) >= 81 && GET_MOB_VNUM(mob) <= 87) {
        dragon_level(mob);
    }

    int64_t mult = 0;

    switch (GET_LEVEL(mob)) {
        case 1:
            mult = rand_number(50, 80);
            break;
        case 2:
            mult = rand_number(90, 120);
            break;
        case 3:
            mult = rand_number(100, 140);
            break;
        case 4:
            mult = rand_number(120, 180);
            break;
        case 5:
            mult = rand_number(200, 250);
            break;
        case 6:
            mult = rand_number(240, 300);
            break;
        case 7:
            mult = rand_number(280, 350);
            break;
        case 8:
            mult = rand_number(320, 400);
            break;
        case 9:
            mult = rand_number(380, 480);
            break;
        case 10:
            mult = rand_number(500, 600);
            break;
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
            mult = rand_number(1200, 1600);
            break;
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
            mult = rand_number(2400, 3000);
            break;
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
            mult = rand_number(5500, 8000);
            break;
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
            mult = rand_number(10000, 14000);
            break;
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
            mult = rand_number(16000, 20000);
            break;
        case 36:
        case 37:
        case 38:
        case 39:
        case 40:
            mult = rand_number(22000, 30000);
            break;
        case 41:
        case 42:
        case 43:
        case 44:
        case 45:
            mult = rand_number(50000, 70000);
            break;
        case 46:
        case 47:
        case 48:
        case 49:
        case 50:
            mult = rand_number(95000, 140000);
            break;
        case 51:
        case 52:
        case 53:
        case 54:
        case 55:
            mult = rand_number(180000, 250000);
            break;
        case 56:
        case 57:
        case 58:
        case 59:
        case 60:
            mult = rand_number(400000, 480000);
            break;
        case 61:
        case 62:
        case 63:
        case 64:
        case 65:
            mult = rand_number(700000, 900000);
            break;
        case 66:
        case 67:
        case 68:
        case 69:
        case 70:
            mult = rand_number(1400000, 1600000);
            break;
        case 71:
        case 72:
        case 73:
        case 74:
        case 75:
            mult = rand_number(2200000, 2500000);
            break;
        case 76:
        case 77:
        case 78:
        case 79:
        case 80:
            mult = rand_number(3000000, 3500000);
            break;
        case 81:
        case 82:
        case 83:
        case 84:
        case 85:
            mult = rand_number(4250000, 4750000);
            break;
        case 86:
        case 87:
        case 88:
        case 89:
        case 90:
            mult = rand_number(6500000, 8500000);
            break;
        case 91:
        case 92:
        case 93:
        case 94:
        case 95:
            mult = rand_number(15000000, 18000000);
            break;
        case 96:
        case 97:
        case 98:
        case 99:
        case 100:
            mult = rand_number(22000000, 30000000);
            break;
        case 101:
            mult = rand_number(32000000, 40000000);
            break;
        case 102:
            mult = rand_number(42000000, 55000000);
            break;
        case 103:
            mult = rand_number(80000000, 95000000);
            break;
        case 104:
            mult = rand_number(150000000, 200000000);
            break;
        case 105:
            mult = rand_number(220000000, 250000000);
            break;
        case 106:
        case 107:
        case 108:
        case 109:
        case 110:
            mult = rand_number(500000000, 750000000);
            break;
        case 111:
        case 112:
        case 113:
        case 114:
        case 115:
        case 116:
        case 117:
        case 118:
        case 119:
        case 120:
            mult = rand_number(800000000, 900000000);
            break;
        default:
            if (GET_LEVEL(mob) >= 150) {
                mult = rand_number(1500000000, 2000000000);
            } else {
                mult = rand_number(1250000000, 1500000000);
            }
            break;
    }

    GET_LPLAY(mob) = time(nullptr);
    bool autoset = mob->get(CharStat::PowerLevel) <= 1;
    if(autoset) {
        for(auto c : {CharStat::PowerLevel, CharStat::Ki, CharStat::Stamina}) {
            stat_t base = GET_LEVEL(mob) * mult;
            if (GET_LEVEL(mob) > 140) {
                base *= 8;
            } else if (GET_LEVEL(mob) > 130) {
                base *= 6;
            } else if (GET_LEVEL(mob) > 120) {
                base *= 3;
            } else if (GET_LEVEL(mob) > 110) {
                base *= 2;
            }
            mob->set(c, base);
        }
    }

    if (GET_MOB_VNUM(mob) == 2245) {
        for(auto c : {CharStat::PowerLevel, CharStat::Ki, CharStat::Stamina}) {
            mob->set(c, rand_number(1, 4));
        }
    }

    int base = 0;
    switch (GET_LEVEL(mob)) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            base = rand_number(80, 120);
            break;
        case 6:
            base = rand_number(200, 280);
            break;
        case 7:
            base = rand_number(250, 350);
            break;
        case 8:
            base = rand_number(275, 375);
            break;
        case 9:
            base = rand_number(300, 400);
            break;
        case 10:
            base = rand_number(325, 450);
            break;
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
            base = rand_number(500, 700);
            break;
        case 16:
        case 17:
        case 18:
        case 19:
        case 20:
            base = rand_number(700, 1000);
            break;
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
            base = rand_number(1000, 1200);
            break;
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
            base = rand_number(1200, 1400);
            break;
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
            base = rand_number(1400, 1600);
            break;
        case 36:
        case 37:
        case 38:
        case 39:
        case 40:
            base = rand_number(1600, 1800);
            break;
        case 41:
        case 42:
        case 43:
        case 44:
        case 45:
            base = rand_number(1800, 2000);
            break;
        case 46:
        case 47:
        case 48:
        case 49:
        case 50:
            base = rand_number(2000, 2200);
            break;
        case 51:
        case 52:
        case 53:
        case 54:
        case 55:
            base = rand_number(2200, 2500);
            break;
        case 56:
        case 57:
        case 58:
        case 59:
        case 60:
            base = rand_number(2500, 2800);
            break;
        case 61:
        case 62:
        case 63:
        case 64:
        case 65:
            base = rand_number(2800, 3000);
            break;
        case 66:
        case 67:
        case 68:
        case 69:
        case 70:
            base = rand_number(3000, 3200);
            break;
        case 71:
        case 72:
        case 73:
        case 74:
        case 75:
            base = rand_number(3200, 3500);
            break;
        case 76:
        case 77:
        case 78:
        case 79:
            base = rand_number(3500, 3800);
            break;
        case 80:
        case 81:
        case 82:
        case 83:
        case 84:
        case 85:
            base = rand_number(4000, 4500);
            break;
        case 86:
        case 87:
        case 88:
        case 89:
        case 90:
            base = rand_number(4500, 5500);
            break;
        case 91:
        case 92:
        case 93:
        case 94:
        case 95:
            base = rand_number(5500, 7000);
            break;
        case 96:
        case 97:
        case 98:
        case 99:
            base = rand_number(8000, 10000);
            break;
        case 100:
            base = rand_number(10000, 15000);
            break;
        case 101:
            base = rand_number(15000, 25000);
            break;
        case 102:
            base = rand_number(35000, 40000);
            break;
        case 103:
            base = rand_number(40000, 50000);
            break;
        case 104:
            base = rand_number(60000, 80000);
            break;
        case 105:
            base = rand_number(80000, 100000);
            break;
        default:
            base = rand_number(130000, 180000);
            break;
    }
    MOB_COOLDOWN(mob) = 0;
    auto money = GET_GOLD(mob);
    if (money <= 0 && !MOB_FLAGGED(mob, MOB_DUMMY)) {
        if (GET_LEVEL(mob) < 4) {
            money = GET_LEVEL(mob) * rand_number(1, 2);
        } else if (GET_LEVEL(mob) < 10) {
            money = (GET_LEVEL(mob) * rand_number(1, 2)) - 1;
        } else if (GET_LEVEL(mob) < 20) {
            money = (GET_LEVEL(mob) * rand_number(1, 3)) - 2;
        } else if (GET_LEVEL(mob) < 30) {
            money = (GET_LEVEL(mob) * rand_number(1, 3)) - 4;
        } else if (GET_LEVEL(mob) < 40) {
            money = (GET_LEVEL(mob) * rand_number(1, 3)) - 6;
        } else if (GET_LEVEL(mob) < 50) {
            money = (GET_LEVEL(mob) * rand_number(2, 3)) - 25;
        } else if (GET_LEVEL(mob) < 60) {
            money = (GET_LEVEL(mob) * rand_number(2, 3)) - 40;
        } else if (GET_LEVEL(mob) < 70) {
            money = (GET_LEVEL(mob) * rand_number(2, 3)) - 50;
        } else if (GET_LEVEL(mob) < 80) {
            money = (GET_LEVEL(mob) * rand_number(2, 4)) - 60;
        } else if (GET_LEVEL(mob) < 90) {
            money = (GET_LEVEL(mob) * rand_number(2, 4)) - 70;
        } else {
            money = (GET_LEVEL(mob) * rand_number(3, 4)) - 85;
        }
        if (!IS_HUMANOID(mob)) {
            money = GET_GOLD(mob) * 0.5;
            if (GET_GOLD(mob) <= 0)
                money = 1;
        }
        mob->set(CharMoney::Carried, money);
    }

    
    if (GET_EXP(mob) <= 0 && !MOB_FLAGGED(mob, MOB_DUMMY)) {
        int64_t mexp = GET_LEVEL(mob) * base;
        mexp = mexp * .9;
        mexp += GET_LEVEL(mob) / 2;
        mexp += GET_LEVEL(mob) / 3;
        if (IS_DRAGON(mob)) {
            mexp *= 1.4;
        } else if (IS_ANDROID(mob)) {
            mexp *= 1.25;
        } else if (IS_SAIYAN(mob)) {
            mexp *= 1.1;
        } else if (IS_BIO(mob)) {
            mexp *= 1.2;
        } else if (IS_MAJIN(mob)) {
            mexp *= 1.25;
        } else if (IS_DEMON(mob)) {
            mexp *= 1.1;
        }
        if (GET_CLASS(mob) == SenseiID::Commoner && IS_HUMANOID(mob) && !IS_DRAGON(mob)) {
            if (!IS_ANDROID(mob) && !IS_SAIYAN(mob) && !IS_BIO(mob) && !IS_MAJIN(mob)) {
                mexp *= 0.75;
            }
        }

        if (GET_LEVEL(mob) > 90) {
            mexp = mexp * .7;
        } else if (GET_LEVEL(mob) > 80) {
            mexp = mexp * .75;
        } else if (GET_LEVEL(mob) > 70) {
            mexp = mexp * .8;
        } else if (GET_LEVEL(mob) > 60) {
            mexp = mexp * .85;
        } else if (GET_LEVEL(mob) > 40) {
            mexp = mexp * .9;
        } else if (GET_LEVEL(mob) > 30) {
            mexp = mexp * .95;
        }

        if (GET_EXP(mob) > 20000000) {
            mexp = 20000000;
        }
        mob->setExperience(mexp);
    }

    mob->setAge(birth_age(mob));
    mob->time.created = mob->time.logon = time(nullptr); /* why not */
    mob->time.played = 0.0;
    mob->time.logon = time(nullptr);
    MOB_LOADROOM(mob) = NOWHERE;

    if (IS_HUMANOID(mob)) {
        for(auto f : {MOB_RARM, MOB_LARM, MOB_RLEG, MOB_LLEG}) mob->mobFlags.set(f);
    }

    copy_proto_script(&proto->second, mob, MOB_TRIGGER);
    assign_triggers(mob, MOB_TRIGGER);
    racial_body_parts(mob);

    if (GET_MOB_VNUM(mob) >= 800 && GET_MOB_VNUM(mob) <= 805) {
        number_of_assassins += 1;
    }

    return mob;
}

void add_unique_id(struct obj_data *obj) {
    if(obj->id == -1) {
        obj->id = nextObjID();
        obj->generation = time(nullptr);
        basic_mud_log("Object Found with ID -1. Automatically fixed to ID {}", obj->id);
    }

    auto &o = uniqueObjects[obj->id];
    o.first = obj->generation;
    o.second = obj;
}

void remove_unique_id(struct obj_data *obj) {
    uniqueObjects.erase(obj->id);
}

void log_dupe_objects(struct obj_data *obj1, struct obj_data *obj2) {
    mudlog(BRF, ADMLVL_GOD, true, "DUPE: Dupe object found: %s [%d] [%" TMT ":%" I64T "]",
           withPlaceholder(obj1->getShortDesc(), "<No name>").c_str(),
           GET_OBJ_VNUM(obj1), obj1->generation, obj1->id);
    mudlog(BRF, ADMLVL_GOD, true, "DUPE: First: In room: %d (%s), "
                                  "In object: %s, Carried by: %s, Worn by: %s",
           GET_ROOM_VNUM(IN_ROOM(obj1)),
           IN_ROOM(obj1) == NOWHERE ? "Nowhere" : obj1->getRoom()->name,
           obj1->in_obj ? obj1->in_obj->getShortDesc().c_str() : "None",
           obj1->carried_by ? GET_NAME(obj1->carried_by) : "Nobody",
           obj1->worn_by ? GET_NAME(obj1->worn_by) : "Nobody");
    mudlog(BRF, ADMLVL_GOD, true, "DUPE: Newer: In room: %d (%s), "
                                  "In object: %s, Carried by: %s, Worn by: %s",
           GET_ROOM_VNUM(IN_ROOM(obj2)),
           IN_ROOM(obj2) == NOWHERE ? "Nowhere" : obj2->getRoom()->name,
           obj2->in_obj ? obj2->in_obj->getShortDesc().c_str() : "None",
           obj2->carried_by ? GET_NAME(obj2->carried_by) : "Nobody",
           obj2->worn_by ? GET_NAME(obj2->worn_by) : "Nobody");

    // assign a new unique ID to obj2.
    obj2->id = nextObjID();
    mudlog(BRF, ADMLVL_GOD, true, "Conflicting object assigned new id: %d", obj2->id);
}

void check_unique_id(struct obj_data *obj) {
    if(obj->id == -1) {
        obj->id = nextObjID();
        obj->generation = time(nullptr);
        basic_mud_log("Object Found with ID -1. Automatically fixed to ID %d", obj->id);
    }
    auto find = uniqueObjects.find(obj->id);

    if(find != uniqueObjects.end() && find->second.first == obj->generation) {
        log_dupe_objects(find->second.second, obj);
    }
}

static void log_dupe_characters(struct char_data *ch1, struct char_data *ch2) {
    mudlog(BRF, ADMLVL_GOD, true, "DUPE: Dupe character found: %s [%d] [%" TMT ":%" I64T "]",
           withPlaceholder(ch1->getShortDesc(), "<No name>").c_str(),
           ch1->vn, ch1->generation, ch1->id);

    // assign a new unique ID to obj2.
    ch2->id = nextCharID();
    mudlog(BRF, ADMLVL_GOD, true, "Conflicting character assigned new id: %d", ch2->id);
}

void check_unique_id(struct char_data *ch) {
    if(ch->id == -1) {
        ch->id = nextCharID();
        ch->generation = time(nullptr);
        basic_mud_log("Character Found with ID -1. Automatically fixed to ID %d", ch->id);
    }
    auto find = uniqueCharacters.find(ch->id);

    if(find != uniqueCharacters.end() && find->second.first == ch->generation) {
        log_dupe_characters(find->second.second, ch);
    }
}

void add_unique_id(struct char_data *ch) {
    if(ch->id == -1) {
        ch->id = nextCharID();
        ch->generation = time(nullptr);
        basic_mud_log("Character Found with ID -1. Automatically fixed to ID %d", ch->id);
    }
    auto &o = uniqueCharacters[ch->id];
    o.first = ch->generation;
    o.second = ch;
}

char *sprintuniques(int low, int high) {
    return strdup("Temporarily disabled.");
}


/* create an object, and add it to the object list */
struct obj_data *create_obj(bool activate) {
    auto obj = new obj_data();
    obj->script = std::make_shared<script_data>(obj);
    if(activate) {
        obj->id = nextObjID();
        obj->generation = time(nullptr);
        check_unique_id(obj);
        add_unique_id(obj);
        obj->activate();
    }

    return (obj);
}


/* create a new object from a prototype */
struct obj_data *read_object(obj_vnum nr, int type, bool activate) /* and obj_rnum */
{
    auto i = nr;
    int j;

    auto proto = obj_proto.find(i);

    if (proto == obj_proto.end()) {
        basic_mud_log("Object (%c) %d does not exist in database.", type == VIRTUAL ? 'V' : 'R', nr);
        return (nullptr);
    }
    
    // Weird hack to ensure that script_data is not overritten by prototype.
    auto obj = new obj_data();
    *obj = proto->second;
    obj->script = std::make_shared<script_data>(obj);

    OBJ_LOADROOM(obj) = NOWHERE;
    
    
    if (proto->second.sbinfo) {
        CREATE(obj->sbinfo, struct obj_spellbook_spell, SPELLBOOK_SIZE);
        for (j = 0; j < SPELLBOOK_SIZE; j++) {
            obj->sbinfo[j].spellname = proto->second.sbinfo[j].spellname;
            obj->sbinfo[j].pages = proto->second.sbinfo[j].pages;
        }
    }

    if (GET_OBJ_VNUM(obj) == 65) {
        HCHARGE(obj) = 20;
    }
    if (GET_OBJ_TYPE(obj) == ITEM_FOOD) {
        if (GET_OBJ_VAL(obj, 1) == 0) {
            GET_OBJ_VAL(obj, 1) = GET_OBJ_VAL(obj, VAL_FOOD_FOODVAL);
        }
        FOOB(obj) = GET_OBJ_VAL(obj, 1);
    }
    if(activate) {
        obj->id = nextObjID();
        obj->generation = time(nullptr);
        check_unique_id(obj);
        add_unique_id(obj);
        obj->activate();
    }
    return (obj);
}


#define ZO_DEAD  999

/* update zone ages, queue for reset if necessary, and dequeue when possible */
void zone_update(uint64_t heartPulse, double deltaTime) {

    for (auto &[vn, z] : zone_table) {
        z.age += deltaTime;
        auto secs = (z.lifespan * 60);
        if(z.age < secs) continue;
        z.age -= secs;

        bool doReset = false;
        switch(z.reset_mode) {
            case 0:
                // Never reset.
            break;
            case 1:
                // reset only if zone is empty.
                if(is_empty(vn)) doReset = true;
            break;
            case 2:
                // Always reset.
                doReset = true;
            break;
            default:
                // This shouldn't happen.
                    break;
        }
        if(doReset) {
            reset_zone(vn);
            mudlog(CMP, ADMLVL_GOD, false, "Auto zone reset: %s (Zone %d)",
               z.name, vn);
        }
    }
}

#define ZCMD2 zone_table[zone].cmd[cmd_no]
static void log_zone_error(zone_rnum zone, int cmd_no, const char *message) {
    mudlog(NRM, ADMLVL_GOD, true, "SYSERR: zone file: %s", message);
    mudlog(NRM, ADMLVL_GOD, true, "SYSERR: ...offending cmd: '%c' cmd in zone #%d, line %d",
           ZCMD2.command, zone_table[zone].number, ZCMD2.line);
}

#define ZONE_ERROR(message) \
    { log_zone_error(zone, cmd_no, message); last_cmd = 0; }

/* execute the reset command table of a given zone */
void reset_zone(zone_rnum zone) {
    int cmd_no, last_cmd = 0;
    struct char_data *mob = nullptr;
    struct obj_data *obj, *obj_to;
    room_vnum rvnum;
    room_rnum rrnum;
    struct char_data *tmob = nullptr; /* for trigger assignment */
    struct obj_data *tobj = nullptr;  /* for trigger assignment */
    int mob_load = false; /* ### */
    int obj_load = false; /* ### */
    auto oproto = obj_proto.find(-1);
    auto mproto = mob_proto.find(-1);
    auto room = world.find(-1);

    auto &z = zone_table[zone];

    if (pre_reset(z.number) == false) {

        for (auto &c : z.cmd) {
            if(c.command == 'S') break;

            if (c.if_flag && !last_cmd && !mob_load && !obj_load)
                continue;

            if (!c.if_flag) { /* ### */
                mob_load = false;
                obj_load = false;
            }

            /*  This is the list of actual zone commands.  If any new
     *  zone commands are added to the game, be certain to update
     *  the list of commands in load_zone() so that the counting
     *  will still be correct. - ae.
     */
            try {
                switch (c.command) {
                case '*':            /* ignore command */
                    last_cmd = 0;
                    break;

                case 'M':            /* read a mobile */
                    room = world.find(c.arg3);
                    if (mob_proto.contains(c.arg1) && (get_vnum_count(characterVnumIndex, c.arg1) < c.arg2) && room != world.end() &&
                        (rand_number(1, 100) >= c.arg5)) {
                        int room_max = 0;
                        struct char_data *i;

                        /* First find out how many mobs of VNUM are in the mud with this rooms */
                        /* VNUM as a load point for max from room checks. */
                        /* Let's only count if room_max is in use.  If left at zero, max_in_mud will handle*/

                        if (c.arg4 > 0) {
                            for (auto i : get_vnum_list(characterVnumIndex, c.arg1)) {
                                if (MOB_LOADROOM(i) == c.arg3) {
                                    if(++room_max >= c.arg4) {
                                        // no need to keep counting more at this point...
                                        break;
                                    }
                                }
                                if (room_max >= c.arg4) {
                                    break;
                                }
                            }
                            /* Break out if room_max has been met, ignore room_max if zero */
                            if (room_max >= c.arg4) {
                                last_cmd = 0;
                                break;
                            }
                        }

                        mob = read_mobile(c.arg1, REAL);
                        /*  Set the mobs loadroom for room_max checks. */
                        MOB_LOADROOM(mob) = c.arg3;
                        char_to_room(mob, c.arg3);

                        load_mtrigger(mob);
                        tmob = mob;
                        last_cmd = 1;
                        mob_load = true;
                    } else
                        last_cmd = 0;
                    tobj = nullptr;
                    break;

                case 'O':            /* read an object */
                    room = world.find(c.arg3);
                    oproto = obj_proto.find(c.arg1);
                    if(oproto != obj_proto.end()) {
                        if(oproto->second.type_flag == ITEM_HATCH || oproto->second.type_flag == ITEM_CONTROL
                        || oproto->second.type_flag == ITEM_WINDOW || oproto->second.type_flag == ITEM_VEHICLE) {
                            c.arg2 = 1;
                            c.arg4 = 1;
                        }
                    }

                    if (obj_proto.contains(c.arg1) && get_vnum_count(objectVnumIndex, c.arg1) < c.arg2 &&
                        room != world.end() && (rand_number(1, 100) >= c.arg5)) {
                        int room_max = 0;
                        struct obj_data *k;


                        /* First find out how many obj of VNUM are in the mud with this rooms */
                        /* VNUM as a load point for max from room checks. */
                        /* Let's only count if room_max is in use.  If left at zero, max_in_mud will handle*/

                        if (c.arg4 > 0) {
                            for (auto k : get_vnum_list(objectVnumIndex, c.arg1)) {
                                if (OBJ_LOADROOM(k) == c.arg3 && (IN_ROOM(k) == c.arg3)) {
                                    if(++room_max >= c.arg4) {
                                        // no need to keep counting more at this point...
                                        break;
                                    }
                                }
                                if (room_max >= c.arg4) {
                                    /* Get rid of it if room_max has been met. */
                                    break;
                                }
                            }
                            if (room_max >= c.arg4) {
                                /* Get rid of it if room_max has been met. */
                                last_cmd = 0;
                                break;
                            }
                        }

                        obj = read_object(c.arg1, REAL);
                        obj_to_room(obj, c.arg3);
                        /* Set the loadroom for room_max checks */
                        OBJ_LOADROOM(obj) = c.arg3;

                        last_cmd = 1;
                        load_otrigger(obj);
                        tobj = obj;
                        obj_load = true;
                    } else
                        last_cmd = 0;
                    tmob = nullptr;
                    break;

                case 'P':            /* object to object */
                    if(obj_proto.contains(c.arg1) && (get_vnum_count(objectVnumIndex, c.arg1) < c.arg2) &&
                        obj_load && (rand_number(1, 100) >= c.arg5)) {

                        if (!(obj_to = get_obj_num(c.arg3))) {
                            ZONE_ERROR("target obj not found, command disabled");
                            c.command = '*';
                            break;
                        }
                        obj = read_object(c.arg1, REAL);
                        obj_to_obj(obj, obj_to);
                        last_cmd = 1;
                        load_otrigger(obj);
                        tobj = obj;
                    } else
                        last_cmd = 0;
                    tmob = nullptr;
                    break;

                case 'G':            /* obj_to_char */
                    if (!mob) {
                        ZONE_ERROR("attempt to give obj to non-existant mob, command disabled");
                        c.command = '*';
                        break;
                    }
                    if (obj_proto.contains(c.arg1) && (get_vnum_count(objectVnumIndex, c.arg1) < c.arg2) &&
                        mob_load && (rand_number(1, 100) >= c.arg5)) {
                        obj = read_object(c.arg1, REAL);
                        obj_to_char(obj, mob);
                        if (GET_MOB_SPEC(mob) != shop_keeper) {
                            randomize_eq(obj);
                        }
                        last_cmd = 1;
                        load_otrigger(obj);
                        tobj = obj;
                    } else
                        last_cmd = 0;
                    tmob = nullptr;
                    break;

                case 'E':            /* object to equipment list */
                    if (!mob) {
                        ZONE_ERROR("trying to equip non-existant mob, command disabled");
                        c.command = '*';
                        break;
                    }
                    if (obj_proto.contains(c.arg1) && (get_vnum_count(objectVnumIndex, c.arg1) < c.arg2) &&
                        mob_load && (rand_number(1, 100) >= c.arg5)) {
                        if (c.arg3 < 0 || c.arg3 >= NUM_WEARS) {
                            ZONE_ERROR("invalid equipment pos number");
                        } else {
                            obj = read_object(c.arg1, REAL);
                            IN_ROOM(obj) = IN_ROOM(mob);
                            load_otrigger(obj);
                            if (wear_otrigger(obj, mob, c.arg3)) {
                                IN_ROOM(obj) = NOWHERE;
                                equip_char(mob, obj, c.arg3);
                            } else
                                obj_to_char(obj, mob);
                            tobj = obj;
                            last_cmd = 1;
                        }
                    } else
                        last_cmd = 0;
                    tmob = nullptr;
                    break;

                case 'R': /* rem obj from room */
                    if ((obj = get_obj_in_list_num(c.arg2, world[c.arg1].contents)) != nullptr)
                        extract_obj(obj);
                    last_cmd = 1;
                    tmob = nullptr;
                    tobj = nullptr;
                    break;


                case 'D':            /* set state of door */
                    room = world.find(c.arg1);
                    if (room == world.end() || c.arg2 < 0 || c.arg2 >= NUM_OF_DIRS ||
                        (room->second.dir_option[c.arg2] == nullptr)) {
                        ZONE_ERROR("room or door does not exist, command disabled");
                        c.command = '*';
                    } else
                        switch (c.arg3) {
                            case 0:
                                REMOVE_BIT(room->second.dir_option[c.arg2]->exit_info,
                                           EX_LOCKED);
                                REMOVE_BIT(room->second.dir_option[c.arg2]->exit_info,
                                           EX_CLOSED);
                                break;
                            case 1:
                                SET_BIT(room->second.dir_option[c.arg2]->exit_info,
                                        EX_CLOSED);
                                REMOVE_BIT(room->second.dir_option[c.arg2]->exit_info,
                                           EX_LOCKED);
                                break;
                            case 2:
                                SET_BIT(room->second.dir_option[c.arg2]->exit_info,
                                        EX_LOCKED);
                                SET_BIT(room->second.dir_option[c.arg2]->exit_info,
                                        EX_CLOSED);
                                break;
                        }
                    last_cmd = 1;
                    tmob = nullptr;
                    tobj = nullptr;
                    break;

                case 'T': /* trigger command */
                    if (c.arg1 == MOB_TRIGGER && tmob) {
                        tmob->script->addTrigger(read_trigger(c.arg2), -1);
                        last_cmd = 1;
                    } else if (c.arg1 == OBJ_TRIGGER && tobj) {
                        tobj->script->addTrigger(read_trigger(c.arg2), -1);
                        last_cmd = 1;
                    } else if (c.arg1 == WLD_TRIGGER) {
                        room = world.find(c.arg3);
                        if (room == world.end()) {
                            ZONE_ERROR("Invalid room number in trigger assignment");
                        }
                        room->second.script->addTrigger(read_trigger(c.arg2), -1);
                        last_cmd = 1;
                    }

                    break;

                case 'V':
                    if (c.arg1 == MOB_TRIGGER
                    && tmob) {
                        if (!SCRIPT(tmob)) {
                            ZONE_ERROR("Attempt to give a variable to scriptless mobile");
                        } else
                            tmob->script->addVar(c.sarg1, c.sarg2);
                        last_cmd = 1;
                    } else if (c.arg1 == OBJ_TRIGGER && tobj) {
                        if (!SCRIPT(tobj)) {
                            ZONE_ERROR("Attempt to give variable to scriptless object");
                        } else
                            tobj->script->addVar(c.sarg1, c.sarg2);
                        last_cmd = 1;
                    } else if (c.arg1 == WLD_TRIGGER) {
                        if (!world.count(c.arg3)) {
                            ZONE_ERROR("Invalid room number in variable assignment");
                        } else {
                            if (!(world[c.arg3].script)) {
                                ZONE_ERROR("Attempt to give variable to scriptless object");
                            } else
                                world[c.arg3].script->addVar(c.sarg1, c.sarg2);
                            last_cmd = 1;
                        }
                    }
                    break;

                default: ZONE_ERROR("unknown cmd in reset table; cmd disabled");
                    c.command = '*';
                    break;
            }
            } catch (const std::exception &e) {
                basic_mud_log("Exception thrown in reset_zone '%d' line %d", zone, c.line);
                shutdown_game(1);
            }
        }

        z.age = 0;

        /* handle reset_wtrigger's */

        for(auto &rvnum : z.rooms) {
            room = world.find(rvnum);
            if(room == world.end()) continue;
            rrnum = rvnum;

            reset_wtrigger(&room->second);
            if (room->second.room_flags.test(ROOM_AURA) && rand_number(1, 5) >= 4) {
                send_to_room(rrnum, "The aura of regeneration covering the surrounding area disappears.\r\n");
                room->second.room_flags.reset(ROOM_AURA);
            }
            if (room->second.sector_type == SECT_LAVA) {
                room->second.geffect = 5;
            }
            if (room->second.geffect < -1) {
                send_to_room(rrnum, "The area loses some of the water flooding it.\r\n");
                room->second.geffect += 1;
            } else if (room->second.geffect == -1) {
                send_to_room(rrnum, "The area loses the last of the water flooding it in one large rush.\r\n");
                room->second.geffect = 0;
            }

            if(auto dmg = room->second.getDamage(); dmg > 0) {
                int toRepair = 0;
                if(dmg >= 100) toRepair = rand_number(5, 10);
                else if(dmg >= 10) toRepair = rand_number(1, 10);
                else if(dmg > 1) toRepair = rand_number(1, dmg);
                else toRepair = 1;
                room->second.modDamage(-toRepair);
                send_to_room(rrnum, "The area gets rebuilt a little.\r\n");
            }


            if (room->second.geffect >= 1 && rand_number(1, 4) == 4 && !room->second.isSunken() && room->second.sector_type != SECT_LAVA) {
                send_to_room(rrnum, "The lava has cooled and become solid rock.\r\n");
                room->second.geffect = 0;
            } else if (room->second.geffect >= 1 && rand_number(1, 2) == 2 && room->second.isSunken() &&
                    room->second.sector_type != SECT_LAVA) {
                send_to_room(rrnum, "The water has cooled the lava and it has become solid rock.\r\n");
                room->second.geffect = 0;
            }
        }
    } else {
        /* even if reset is blocked, age should be reset */
        z.age = 0;
    }
    post_reset(z.number);
}


/* for use in reset_zone; return TRUE if zone 'nr' is free of PC's  */
int is_empty(zone_rnum zone_nr) {
    struct descriptor_data *i;

    for (i = descriptor_list; i; i = i->next) {
        if (STATE(i) != CON_PLAYING)
            continue;
        if (IN_ROOM(i->character) == NOWHERE)
            continue;
        if (i->character->getRoom()->zone != zone_nr)
            continue;
        /*
     * if an immortal has nohassle off, he counts as present
     * added for testing zone reset triggers - Welcor
     */
        if (IS_NPC(i->character))
            continue; /* immortal switched into a mob */

        if ((GET_ADMLEVEL(i->character) >= ADMLVL_IMMORT) && (PRF_FLAGGED(i->character, PRF_NOHASSLE)))
            continue;

        return (0);
    }

    return (1);
}


/************************************************************************
*  funcs of a (more or less) general utility nature			*
************************************************************************/


/* read and allocate space for a '~'-terminated string from a given file */
char *fread_string(FILE *fl, const char *error) {
    char buf[MAX_STRING_LENGTH], tmp[520];
    char *point;
    int done = 0, length = 0, templength;

    *buf = *tmp = '\0';

    do {
        if (!fgets(tmp, 512, fl)) {
            basic_mud_log("SYSERR: fread_string: format error at string (pos %ld): %s at or near %s",
                ftell(fl), feof(fl) ? "EOF" : ferror(fl) ? "read error" : "unknown error", error);
            exit(1);
        }
        /* If there is a '~', end the string; else put an "\r\n" over the '\n'. */
        /* now only removes trailing ~'s -- Welcor */
        for (point = tmp; *point && *point != '\r' && *point != '\n'; point++);
        if (point > tmp && point[-1] == '~') {
            *(--point) = '\0';
            done = 1;
        } else {
            *point = '\r';
            *(++point) = '\n';
            *(++point) = '\0';
        }

        templength = point - tmp;

        if (length + templength >= MAX_STRING_LENGTH) {
            basic_mud_log("SYSERR: fread_string: string too large (db.c)");
            basic_mud_log("%s", error);
            exit(1);
        } else {
            strcat(buf + length, tmp);    /* strcat: OK (size checked above) */
            length += templength;
        }
    } while (!done);

    /* allocate space for the new string and copy it */
    return (strlen(buf) ? strdup(buf) : nullptr);
}

/* Called to free all allocated follow_type structs - Update by Jamie Nelson */
void free_followers(struct follow_type *k) {
    if (!k)
        return;

    if (k->next)
        free_followers(k->next);

    k->follower = nullptr;
    free(k);
}


/* release memory allocated for a char struct */
void free_char(struct char_data *ch) {
    int i;
    uniqueCharacters.erase(ch->id);

    if(ch->vn == NOBODY) {
        if (GET_NAME(ch))
            free(GET_NAME(ch));
        if (GET_VOICE(ch))
            free(GET_VOICE(ch));
        if (GET_CLAN(ch))
            free(GET_CLAN(ch));
        if (ch->title)
            free(ch->title);
        if (ch->room_description)
            free(ch->room_description);
        if (ch->look_description)
            free(ch->look_description);
        if(ch->short_description)
            free(ch->short_description);

    } else {
        auto &m = mob_proto[ch->vn];
        if (ch->name && ch->name != m.name)
            free(ch->name);
        if (ch->title && ch->title != m.title)
            free(ch->title);
        if (ch->room_description && ch->room_description != m.room_description)
            free(ch->room_description);
        if (ch->look_description && ch->look_description != m.look_description)
            free(ch->look_description);
        if (ch->short_description && ch->short_description != m.short_description)
            free(ch->short_description);
    }

    while (ch->affected)
        affect_remove(ch, ch->affected);

    /* free any assigned scripts */
    if (SCRIPT(ch))
        extract_script(ch, MOB_TRIGGER);

    /* new version of free_followers take the followers pointer as arg */
    free_followers(ch->followers);

    if (ch->desc)
        ch->desc->character = nullptr;

    /* find_char helper */
    /*
  * when free_char is called with a blank character struct, ID is set
  * to 0, and has not yet been added to the lookup table.
  */

    delete ch;
}


/* release memory allocated for an obj struct */
void free_obj(struct obj_data *obj) {
    remove_unique_id(obj);
    if (GET_OBJ_RNUM(obj) == NOWHERE) {
        free_object_strings(obj);
    } else {
        free_object_strings_proto(obj);

    }

    /* Let's make sure that we free up this memory */
    if (obj->auctname) {
        free(obj->auctname);
    }

    /* free any assigned scripts */
    if (SCRIPT(obj))
        extract_script(obj, OBJ_TRIGGER);

    if (obj->sbinfo)
        free(obj->sbinfo);

    delete obj;
}

/*
 * Steps:
 *   1: Read contents of a text file.
 *   2: Make sure no one is using the pointer in paging.
 *   3: Allocate space.
 *   4: Point 'buf' to it.
 *
 * We don't want to free() the string that someone may be
 * viewing in the pager.  page_string() keeps the internal
 * strdup()'d copy on ->showstr_head and it won't care
 * if we delete the original.  Otherwise, strings are kept
 * on ->showstr_vector but we'll only match if the pointer
 * is to the string we're interested in and not a copy.
 *
 * If someone is reading a global copy we're trying to
 * replace, give everybody using it a different copy so
 * as to avoid special cases.
 */
static int file_to_string_alloc(const char *name, char **buf) {
    int temppage;
    char temp[MAX_STRING_LENGTH];

    /* Lets not free() what used to be there unless we succeeded. */
    if (file_to_string(name, temp) < 0)
        return (-1);

    if (*buf)
        free(*buf);

    *buf = strdup(temp);
    return (0);
}


/* read contents of a text file, and place in buf */
static int file_to_string(const char *name, char *buf) {
    try {
        std::ifstream f(name);
        std::string out;
        // read all of f to out.
        std::getline(f, out, '\0');
        trim_right(out);
        strcpy(buf, out.c_str());
        return 0;
    } catch (std::exception &e) {
        return -1;
    }
}


/* clear some of the the working variables of a char */
void reset_char(struct char_data *ch) {
    int i;

    ch->followers = nullptr;
    ch->master = nullptr;
    IN_ROOM(ch) = NOWHERE;
    ch->next = nullptr;
    ch->next_fighting = nullptr;
    ch->next_in_room = nullptr;
    FIGHTING(ch) = nullptr;
    ch->position = POS_STANDING;
    ch->mob_specials.default_pos = POS_STANDING;
    ch->time.logon = time(nullptr);

    GET_LAST_TELL(ch) = NOBODY;
}


/*
 * Called during character creation after picking character class
 * (and then never again for that character).
 */
void init_char(struct char_data *ch) {
    int i;

    GET_CRANK(ch) = 0;
    GET_CLAN(ch) = strdup("None.");
    GET_ABSORBS(ch) = 0;
    ABSORBING(ch) = nullptr;
    ABSORBBY(ch) = nullptr;
    SITS(ch) = nullptr;
    BLOCKED(ch) = nullptr;
    BLOCKS(ch) = nullptr;

    /* If this is our first player make him LVL_IMPL. */
    if (players.size() == 0) {
        admin_set(ch, ADMLVL_IMPL);

        /* The implementor never goes through do_start(). */
        for(auto c : {CharStat::PowerLevel, CharStat::Ki, CharStat::Stamina}) {
            ch->set(c, 1000);
        }
    }

    /*ch->time.birth = time(0) - birth_age(ch);*/
    ch->time.logon = ch->time.created = time(nullptr);
    ch->time.played = 0.0;

    GET_HOME(ch) = 1;

    set_height_and_weight_by_race(ch);

    for (i = 1; i < SKILL_TABLE_SIZE; i++) {
        if (GET_ADMLEVEL(ch) < ADMLVL_IMPL)
            SET_SKILL(ch, i, 0);
        else
            SET_SKILL(ch, i, 100);
        SET_SKILL_BONUS(ch, i, 0);
    }

    for (i = 0; i < AF_ARRAY_MAX; i++)
        AFF_FLAGS(ch)[i] = 0;

    for (i = 0; i < 3; i++)
        GET_SAVE_MOD(ch, i) = 0;

    for (i = 0; i < 3; i++)
        GET_COND(ch, i) = (GET_ADMLEVEL(ch) == ADMLVL_IMPL ? -1 : 24);

    GET_LOADROOM(ch) = NOWHERE;
    SPEAKING(ch) = SKILL_LANG_COMMON;
}

/* returns the real number of the room with given virtual number */
room_rnum real_room(room_vnum vnum) {
    return world.count(vnum) ? vnum : NOWHERE;
}


/* returns the real number of the monster with given virtual number */
mob_rnum real_mobile(mob_vnum vnum) {
    return mob_proto.count(vnum) ? vnum : NOBODY;
}


/* returns the real number of the object with given virtual number */
obj_rnum real_object(obj_vnum vnum) {
    return obj_proto.count(vnum) ? vnum : NOTHING;
}

/* returns the real number of the room with given virtual number */
zone_rnum real_zone(zone_vnum vnum) {
    return zone_table.count(vnum) ? vnum : NOWHERE;
}


/* This procedure removes the '\r\n' from a string so that it may be
   saved to a file.  Use it only on buffers, not on the orginal
   strings. */

void strip_string(char *buffer) {
    char *ptr, *str;

    ptr = buffer;
    str = ptr;

    while ((*str = *ptr)) {
        str++;
        ptr++;
        if (*ptr == '\r')
            ptr++;
    }
}

/* External variables from config.c */

void load_default_config() {
    /****************************************************************************/
    /** This function is called only once, at boot-time.                       **/
    /** - We assume config_info is empty                          -- Welcor    **/
    /****************************************************************************/
    /****************************************************************************/
    /** Game play options.                                                     **/
    /****************************************************************************/
    CONFIG_PK_ALLOWED = pk_allowed;
    CONFIG_PT_ALLOWED = pt_allowed;
    CONFIG_LEVEL_CAN_SHOUT = level_can_shout;
    CONFIG_HOLLER_MOVE_COST = holler_move_cost;
    CONFIG_TUNNEL_SIZE = tunnel_size;
    CONFIG_MAX_EXP_GAIN = max_exp_gain;
    CONFIG_MAX_EXP_LOSS = max_exp_loss;
    CONFIG_MAX_NPC_CORPSE_TIME = max_npc_corpse_time;
    CONFIG_MAX_PC_CORPSE_TIME = max_pc_corpse_time;
    CONFIG_IDLE_VOID = idle_void;
    CONFIG_IDLE_RENT_TIME = idle_rent_time;
    CONFIG_IDLE_MAX_LEVEL = idle_max_level;
    CONFIG_DTS_ARE_DUMPS = dts_are_dumps;
    CONFIG_LOAD_INVENTORY = load_into_inventory;
    CONFIG_OK = strdup(OK);
    CONFIG_NOPERSON = strdup(NOPERSON);
    CONFIG_NOEFFECT = strdup(NOEFFECT);
    CONFIG_TRACK_T_DOORS = track_through_doors;
    CONFIG_LEVEL_CAP = level_cap;
    CONFIG_STACK_MOBS = show_mob_stacking;
    CONFIG_STACK_OBJS = show_obj_stacking;
    CONFIG_MOB_FIGHTING = mob_fighting;
    CONFIG_DISP_CLOSED_DOORS = disp_closed_doors;
    CONFIG_REROLL_PLAYER_CREATION = reroll_status;
    CONFIG_INITIAL_POINTS_POOL = initial_points;
    CONFIG_ENABLE_COMPRESSION = enable_compression;
    CONFIG_ENABLE_LANGUAGES = enable_languages;
    CONFIG_ALL_ITEMS_UNIQUE = all_items_unique;
    CONFIG_EXP_MULTIPLIER = exp_multiplier;
    /****************************************************************************/
    /** Rent / crashsave options.                                              **/
    /****************************************************************************/
    CONFIG_FREE_RENT = free_rent;
    CONFIG_MAX_OBJ_SAVE = max_obj_save;
    CONFIG_MIN_RENT_COST = min_rent_cost;
    CONFIG_AUTO_SAVE = auto_save;
    CONFIG_AUTOSAVE_TIME = autosave_time;
    CONFIG_CRASH_TIMEOUT = crash_file_timeout;
    CONFIG_RENT_TIMEOUT = rent_file_timeout;

    /****************************************************************************/
    /** Room numbers.                                                          **/
    /****************************************************************************/
    CONFIG_MORTAL_START = mortal_start_room;
    CONFIG_IMMORTAL_START = immort_start_room;
    CONFIG_FROZEN_START = frozen_start_room;
    CONFIG_DON_ROOM_1 = donation_room_1;
    CONFIG_DON_ROOM_2 = donation_room_2;
    CONFIG_DON_ROOM_3 = donation_room_3;

    /****************************************************************************/
    /** Game operation options.                                                **/
    /****************************************************************************/
    CONFIG_DFLT_PORT = DFLT_PORT;

    if (DFLT_IP)
        CONFIG_DFLT_IP = strdup(DFLT_IP);
    else
        CONFIG_DFLT_IP = nullptr;

    CONFIG_DFLT_DIR = strdup(DFLT_DIR);

    if (LOGNAME)
        CONFIG_LOGNAME = strdup(LOGNAME);
    else
        CONFIG_LOGNAME = nullptr;

    CONFIG_MAX_PLAYING = max_playing;
    CONFIG_MAX_FILESIZE = max_filesize;
    CONFIG_MAX_BAD_PWS = max_bad_pws;
    CONFIG_SITEOK_ALL = siteok_everyone;
    CONFIG_NS_IS_SLOW = nameserver_is_slow;
    CONFIG_NEW_SOCIALS = use_new_socials;
    CONFIG_OLC_SAVE = auto_save_olc;
    CONFIG_MENU = strdup(MENU);
    CONFIG_WELC_MESSG = strdup(WELC_MESSG);
    CONFIG_START_MESSG = strdup(START_MESSG);
    CONFIG_IMC_ENABLED = imc_is_enabled;
    CONFIG_EXP_MULTIPLIER = 1.0;

    /****************************************************************************/
    /** Autowiz options.                                                       **/
    /****************************************************************************/
    CONFIG_USE_AUTOWIZ = use_autowiz;
    CONFIG_MIN_WIZLIST_LEV = min_wizlist_lev;

    /****************************************************************************/
    /** Character advancement options.                                         **/
    /****************************************************************************/
    CONFIG_ALLOW_MULTICLASS = allow_multiclass;
    CONFIG_ALLOW_PRESTIGE = allow_prestige;

    /****************************************************************************/
    /** ticks menu                                                             **/
    /****************************************************************************/
    CONFIG_PULSE_VIOLENCE = pulse_violence;
    CONFIG_PULSE_MOBILE = pulse_mobile;
    CONFIG_PULSE_ZONE = pulse_zone;
    CONFIG_PULSE_CURRENT = pulse_current;
    CONFIG_PULSE_SANITY = pulse_sanity;
    CONFIG_PULSE_IDLEPWD = pulse_idlepwd;
    CONFIG_PULSE_AUTOSAVE = pulse_autosave;
    CONFIG_PULSE_USAGE = pulse_usage;
    CONFIG_PULSE_TIMESAVE = pulse_timesave;

    /****************************************************************************/
    /** Character Creation Method                                              **/
    /****************************************************************************/
    CONFIG_CREATION_METHOD = method;
}

void load_config() {
    FILE *fl;
    char line[MAX_STRING_LENGTH];
    char tag[MAX_INPUT_LENGTH];
    int num;
    float fum;
    char buf[MAX_INPUT_LENGTH];

    load_default_config();

    snprintf(buf, sizeof(buf), "%s/%s", "lib", "etc/config");
    if (!(fl = fopen(buf, "r"))) {
        snprintf(buf, sizeof(buf), "Game Config File: %s", "etc/config");
        perror(buf);
        return;
    }

    /****************************************************************************/
    /** Load the game configuration file.                                      **/
    /****************************************************************************/
    while (get_line(fl, line)) {
        split_argument(line, tag);
        num = atoi(line);
        fum = atof(line);

        switch (LOWER(*tag)) {
            case 'a':
                if (!strcasecmp(tag, "auto_save"))
                    CONFIG_AUTO_SAVE = num;
                else if (!strcasecmp(tag, "autosave_time"))
                    CONFIG_AUTOSAVE_TIME = num;
                else if (!strcasecmp(tag, "auto_save_olc"))
                    CONFIG_OLC_SAVE = num;
                else if (!strcasecmp(tag, "allow_multiclass"))
                    CONFIG_ALLOW_MULTICLASS = num;
                else if (!strcasecmp(tag, "allow_prestige"))
                    CONFIG_ALLOW_PRESTIGE = num;
                else if (!strcasecmp(tag, "auto_level"))
                    basic_mud_log("ignoring obsolete config option auto_level");
                else if (!strcasecmp(tag, "all_items_unique"))
                    CONFIG_ALL_ITEMS_UNIQUE = num;
                break;

            case 'c':
                if (!strcasecmp(tag, "crash_file_timeout"))
                    CONFIG_CRASH_TIMEOUT = num;
                else if (!strcasecmp(tag, "compression")) {
                    CONFIG_ENABLE_COMPRESSION = num;
                }
                break;

            case 'd':
                if (!strcasecmp(tag, "disp_closed_doors"))
                    CONFIG_DISP_CLOSED_DOORS = num;
                else if (!strcasecmp(tag, "dts_are_dumps"))
                    CONFIG_DTS_ARE_DUMPS = num;
                else if (!strcasecmp(tag, "donation_room_1"))
                    if (num == -1)
                        CONFIG_DON_ROOM_1 = NOWHERE;
                    else
                        CONFIG_DON_ROOM_1 = num;
                else if (!strcasecmp(tag, "donation_room_2"))
                    if (num == -1)
                        CONFIG_DON_ROOM_2 = NOWHERE;
                    else
                        CONFIG_DON_ROOM_2 = num;
                else if (!strcasecmp(tag, "donation_room_3"))
                    if (num == -1)
                        CONFIG_DON_ROOM_3 = NOWHERE;
                    else
                        CONFIG_DON_ROOM_3 = num;
                else if (!strcasecmp(tag, "dflt_dir")) {
                    if (CONFIG_DFLT_DIR)
                        free(CONFIG_DFLT_DIR);
                    if (line != nullptr && *line)
                        CONFIG_DFLT_DIR = strdup(line);
                    else
                        CONFIG_DFLT_DIR = strdup(DFLT_DIR);
                } else if (!strcasecmp(tag, "dflt_ip")) {
                    if (CONFIG_DFLT_IP)
                        free(CONFIG_DFLT_IP);
                    if (line != nullptr && *line)
                        CONFIG_DFLT_IP = strdup(line);
                    else
                        CONFIG_DFLT_IP = nullptr;
                } else if (!strcasecmp(tag, "dflt_port"))
                    CONFIG_DFLT_PORT = num;
                break;

            case 'e':
                if (!strcasecmp(tag, "enable_languages"))
                    CONFIG_ENABLE_LANGUAGES = num;
                else if (!strcasecmp(tag, "exp_multiplier"))
                    CONFIG_EXP_MULTIPLIER = fum;
                break;

            case 'f':
                if (!strcasecmp(tag, "free_rent"))
                    CONFIG_FREE_RENT = num;
                else if (!strcasecmp(tag, "frozen_start_room"))
                    CONFIG_FROZEN_START = num;
                break;

            case 'h':
                if (!strcasecmp(tag, "holler_move_cost"))
                    CONFIG_HOLLER_MOVE_COST = num;
                break;

            case 'i':
                if (!strcasecmp(tag, "idle_void"))
                    CONFIG_IDLE_VOID = num;
                else if (!strcasecmp(tag, "idle_rent_time"))
                    CONFIG_IDLE_RENT_TIME = num;
                else if (!strcasecmp(tag, "idle_max_level")) {
                    if (num >= CONFIG_LEVEL_CAP)
                        num += 1 - CONFIG_LEVEL_CAP;
                    CONFIG_IDLE_MAX_LEVEL = num;
                } else if (!strcasecmp(tag, "immort_level_ok"))
                    basic_mud_log("Ignoring immort_level_ok obsolete config");
                else if (!strcasecmp(tag, "immort_start_room"))
                    CONFIG_IMMORTAL_START = num;
                else if (!strcasecmp(tag, "imc_enabled"))
                    CONFIG_IMC_ENABLED = num;
                else if (!strcasecmp(tag, "initial_points"))
                    CONFIG_INITIAL_POINTS_POOL = num;
                break;

            case 'l':
                if (!strcasecmp(tag, "level_can_shout"))
                    CONFIG_LEVEL_CAN_SHOUT = num;
                else if (!strcasecmp(tag, "level_cap"))
                    CONFIG_LEVEL_CAP = num;
                else if (!strcasecmp(tag, "load_into_inventory"))
                    CONFIG_LOAD_INVENTORY = num;
                else if (!strcasecmp(tag, "logname")) {
                    if (CONFIG_LOGNAME)
                        free(CONFIG_LOGNAME);
                    if (line != nullptr && *line)
                        CONFIG_LOGNAME = strdup(line);
                    else
                        CONFIG_LOGNAME = nullptr;
                }
                break;

            case 'm':
                if (!strcasecmp(tag, "max_bad_pws"))
                    CONFIG_MAX_BAD_PWS = num;
                else if (!strcasecmp(tag, "max_exp_gain"))
                    CONFIG_MAX_EXP_GAIN = num;
                else if (!strcasecmp(tag, "max_exp_loss"))
                    CONFIG_MAX_EXP_LOSS = num;
                else if (!strcasecmp(tag, "max_filesize"))
                    CONFIG_MAX_FILESIZE = num;
                else if (!strcasecmp(tag, "max_npc_corpse_time"))
                    CONFIG_MAX_NPC_CORPSE_TIME = num;
                else if (!strcasecmp(tag, "max_obj_save"))
                    CONFIG_MAX_OBJ_SAVE = num;
                else if (!strcasecmp(tag, "max_pc_corpse_time"))
                    CONFIG_MAX_PC_CORPSE_TIME = num;
                else if (!strcasecmp(tag, "max_playing"))
                    CONFIG_MAX_PLAYING = num;
                else if (!strcasecmp(tag, "menu")) {
                    if (CONFIG_MENU)
                        free(CONFIG_MENU);
                    strncpy(buf, "Reading menu in load_config()", sizeof(buf));
                    CONFIG_MENU = fread_string(fl, buf);
                } else if (!strcasecmp(tag, "min_rent_cost"))
                    CONFIG_MIN_RENT_COST = num;
                else if (!strcasecmp(tag, "min_wizlist_lev")) {
                    if (num >= CONFIG_LEVEL_CAP)
                        num += 1 - CONFIG_LEVEL_CAP;
                    CONFIG_MIN_WIZLIST_LEV = num;
                } else if (!strcasecmp(tag, "mob_fighting"))
                    CONFIG_MOB_FIGHTING = num;
                else if (!strcasecmp(tag, "mortal_start_room"))
                    CONFIG_MORTAL_START = num;
                else if (!strcasecmp(tag, "method"))
                    CONFIG_CREATION_METHOD = num;
                break;

            case 'n':
                if (!strcasecmp(tag, "nameserver_is_slow"))
                    CONFIG_NS_IS_SLOW = num;
                else if (!strcasecmp(tag, "noperson")) {
                    char tmp[READ_SIZE];
                    if (CONFIG_NOPERSON)
                        free(CONFIG_NOPERSON);
                    snprintf(tmp, sizeof(tmp), "%s\r\n", line);
                    CONFIG_NOPERSON = strdup(tmp);
                } else if (!strcasecmp(tag, "noeffect")) {
                    char tmp[READ_SIZE];
                    if (CONFIG_NOEFFECT)
                        free(CONFIG_NOEFFECT);
                    snprintf(tmp, sizeof(tmp), "%s\r\n", line);
                    CONFIG_NOEFFECT = strdup(tmp);
                }
                break;

            case 'o':
                if (!strcasecmp(tag, "ok")) {
                    char tmp[READ_SIZE];
                    if (CONFIG_OK)
                        free(CONFIG_OK);
                    snprintf(tmp, sizeof(tmp), "%s\r\n", line);
                    CONFIG_OK = strdup(tmp);
                }
                break;

            case 'p':
                if (!strcasecmp(tag, "pk_allowed"))
                    CONFIG_PK_ALLOWED = num;
                else if (!strcasecmp(tag, "pt_allowed"))
                    CONFIG_PT_ALLOWED = num;
                else if (!strcasecmp(tag, "pulse_viol"))
                    CONFIG_PULSE_VIOLENCE = num;
                else if (!strcasecmp(tag, "pulse_mobile"))
                    CONFIG_PULSE_MOBILE = num;
                else if (!strcasecmp(tag, "pulse_current"))
                    CONFIG_PULSE_CURRENT = num;
                else if (!strcasecmp(tag, "pulse_zone"))
                    CONFIG_PULSE_ZONE = num;
                else if (!strcasecmp(tag, "pulse_autosave"))
                    CONFIG_PULSE_AUTOSAVE = num;
                else if (!strcasecmp(tag, "pulse_usage"))
                    CONFIG_PULSE_USAGE = num;
                else if (!strcasecmp(tag, "pulse_sanity"))
                    CONFIG_PULSE_SANITY = num;
                else if (!strcasecmp(tag, "pulse_timesave"))
                    CONFIG_PULSE_TIMESAVE = num;
                else if (!strcasecmp(tag, "pulse_idlepwd"))
                    CONFIG_PULSE_IDLEPWD = num;
                break;

            case 'r':
                if (!strcasecmp(tag, "rent_file_timeout"))
                    CONFIG_RENT_TIMEOUT = num;
                else if (!strcasecmp(tag, "reroll_stats"))
                    CONFIG_REROLL_PLAYER_CREATION = num;
                break;

            case 's':
                if (!strcasecmp(tag, "siteok_everyone"))
                    CONFIG_SITEOK_ALL = num;
                else if (!strcasecmp(tag, "start_messg")) {
                    strncpy(buf, "Reading start message in load_config()", sizeof(buf));
                    if (CONFIG_START_MESSG)
                        free(CONFIG_START_MESSG);
                    CONFIG_START_MESSG = fread_string(fl, buf);
                } else if (!strcasecmp(tag, "stack_mobs"))
                    CONFIG_STACK_MOBS = num;
                else if (!strcasecmp(tag, "stack_objs"))
                    CONFIG_STACK_OBJS = num;
                break;

            case 't':
                if (!strcasecmp(tag, "tunnel_size"))
                    CONFIG_TUNNEL_SIZE = num;
                else if (!strcasecmp(tag, "track_through_doors"))
                    CONFIG_TRACK_T_DOORS = num;
                break;

            case 'u':
                if (!strcasecmp(tag, "use_autowiz"))
                    CONFIG_USE_AUTOWIZ = num;
                else if (!strcasecmp(tag, "use_new_socials"))
                    CONFIG_NEW_SOCIALS = num;
                break;

            case 'w':
                if (!strcasecmp(tag, "welc_messg")) {
                    strncpy(buf, "Reading welcome message in load_config()", sizeof(buf));
                    if (CONFIG_WELC_MESSG)
                        free(CONFIG_WELC_MESSG);
                    CONFIG_WELC_MESSG = fread_string(fl, buf);
                }
                break;

            default:
                break;
        }
    }

    fclose(fl);
}


int64_t nextObjID() {
    int64_t id = 0;
    while(uniqueObjects.contains(id)) id++;
    return id;
}

int64_t nextCharID() {
    int64_t id = 0;
    while(uniqueCharacters.contains(id)) id++;
    return id;
}
// ^#(?<type>[ROC])(?<id>\d+)(?::(?<generation>\d+)?)?
static std::regex uid_regex(R"(^#([ROC])(\d+)(?::(\d+)?)?(!)?)", std::regex::icase);

bool isUID(const std::string& uid) {
    return std::regex_match(uid, uid_regex);
}

std::optional<UID> resolveUID(const std::string& uid) {
    // First we need to check if it matches or not.
    std::smatch match;

    if(!std::regex_search(uid, match, uid_regex)) {
        return std::nullopt;
    }

    char type = toupper(match[1].str()[0]); // First capture group
    int64_t id = std::stoll(match[2].str()); // Second capture group
    bool active = match[4].matched; // Fourth capture group
    time_t generation = 0;
    if(match[3].matched) { // Third capture group
        generation = std::stoll(match[3].str());
    }

    if(type == 'R') {
        if(world.contains(id)) return &world[id];
    } else if(type == 'O') {
        auto find = uniqueObjects.find(id);
        if(find != uniqueObjects.end()) {
            if(!generation || (find->second.first == generation)) {
                if(active && !find->second.second->isActive())
                    return std::nullopt;
                return find->second.second;
            }
        }
    } else if(type == 'C') {
        auto find = uniqueCharacters.find(id);
        if(find != uniqueCharacters.end()) {
            if(!generation || (find->second.first == generation)) {
                if(active && !find->second.second->isActive())
                    return std::nullopt;
                return find->second.second;
            }
        }
    }
    return std::nullopt;
}
