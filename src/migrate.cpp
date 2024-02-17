#include "dbat/migrate.h"

#include "dbat/comm.h"
#include "dbat/utils.h"
#include "dbat/dg_scripts.h"
#include "dbat/constants.h"
#include "dbat/genolc.h"
#include "dbat/maputils.h"
#include "dbat/config.h"
#include <filesystem>
#include <memory>
#include <iostream>
#include <vector>
#include <tuple>
#include <fstream>
#include "dbat/class.h"
#include "dbat/players.h"
#include "json.hpp"
#include "dbat/account.h"
#include "dbat/act.item.h"
#include "dbat/pfdefaults.h"
#include "dbat/spell_parser.h"
#include "dbat/shop.h"
#include "dbat/guild.h"
#include "dbat/genobj.h"
#include "dbat/saveload.h"
#include "dbat/random.h"

#define RENT_FACTOR    1
#define CRYO_FACTOR    4

#define LOC_INVENTORY    0
#define MAX_BAG_ROWS    5

#define MAX_HOUSES    1000
#define MAX_GUESTS    10

#define HOUSE_PRIVATE    0
#define HOUSE_GOD       1  /* Imm owned house             */
#define HOUSE_CLAN      2  /* Clan crash-save room        */
#define HOUSE_UNOWNED   3

#define NUM_HOUSE_TYPES 4


#define HOUSE_FLAGS(house) (house).bitvector

/* House can have up to 31 bitvectors - don't go higher */
#define HOUSE_NOGUESTS   (1 << 0)   /* Owner cannot add guests     */
#define HOUSE_FREE       (1 << 1)   /* House does not require payments */
#define HOUSE_NOIMMS     (1 << 2)   /* Imms below level 999 cannot enter */
#define HOUSE_IMPONLY    (1 << 3)   /* Imms below level 1000 cannot enter */
#define HOUSE_RENTFREE   (1 << 4)   /* No rent is charged on items left here */
#define HOUSE_SAVENORENT (1 << 5)   /* NORENT items are crashsaved too */
#define HOUSE_NOSAVE     (1 << 6)   /* Do not crash save this room - private only */

#define HOUSE_NUM_FLAGS 7

#define TOROOM(room, dir) (world[room].dir_option[dir] ? \
world[room].dir_option[dir]->to_room : NOWHERE)


static bool converting = false;

static void parse_trigger(FILE *trig_f, trig_vnum nr) {
    int t[2], k, attach_type;
    char line[256], *cmds, *s, flags[256], errors[MAX_INPUT_LENGTH];
    auto trig = std::make_shared<trig_proto>();
    trig_index[nr] = trig;
    trig->vn = nr;

    auto &z = zone_table[real_zone_by_thing(nr)];
    z.triggers.insert(nr);

    snprintf(errors, sizeof(errors), "trig vnum %d", nr);

    trig->name = fread_string(trig_f, errors);

    get_line(trig_f, line);
    k = sscanf(line, "%d %s %d", &attach_type, flags, t);
    trig->attach_type = (int8_t) attach_type;
    trig->trigger_type = (long) asciiflag_conv(flags);
    trig->narg = (k == 3) ? t[0] : 0;
    auto args = fread_string(trig_f, errors);
    if(args) trig->arglist = args;

    cmds = s = fread_string(trig_f, errors);

    trig->lines.emplace_back(strtok(s, "\r\n"));

    while ((s = strtok(nullptr, "\r\n"))) {
        trig->lines.emplace_back(s);
    }

    free(cmds);
}

static void read_line(FILE *shop_f, const char *string, void *data) {
    char buf[READ_SIZE];

    if (!get_line(shop_f, buf) || !sscanf(buf, string, data)) {
        basic_mud_log("SYSERR: Error in shop #%d, near '%s' with '%s'", SHOP_NUM(top_shop), buf, string);
        exit(1);
    }
}

static void read_guild_line(FILE *gm_f, char *string, void *data, char *type) {
    char buf[MAX_STRING_LENGTH];

    if (!get_line(gm_f, buf) || !sscanf(buf, string, data)) {
        fprintf(stderr, "Error in guild #%d, Could not get %s\n", GM_NUM(top_guild), type);
        exit(1);
    }
}

static char *read_shop_message(int mnum, room_vnum shr, FILE *shop_f, const char *why) {
    int cht, ss = 0, ds = 0, err = 0;
    char *tbuf;

    if (!(tbuf = fread_string(shop_f, why)))
        return (nullptr);

    for (cht = 0; tbuf[cht]; cht++) {
        if (tbuf[cht] != '%')
            continue;

        if (tbuf[cht + 1] == 's')
            ss++;
        else if (tbuf[cht + 1] == 'd' && (mnum == 5 || mnum == 6)) {
            if (ss == 0) {
                basic_mud_log("SYSERR: Shop #%d has %%d before %%s, message #%d.", shr, mnum);
                err++;
            }
            ds++;
        } else if (tbuf[cht + 1] != '%') {
            basic_mud_log("SYSERR: Shop #%d has invalid format '%%%c' in message #%d.", shr, tbuf[cht + 1], mnum);
            err++;
        }
    }

    if (ss > 1 || ds > 1) {
        basic_mud_log("SYSERR: Shop #%d has too many specifiers for message #%d. %%s=%d %%d=%d", shr, mnum, ss, ds);
        err++;
    }

    if (err) {
        free(tbuf);
        return (nullptr);
    }
    return (tbuf);
}

static void boot_the_guilds(FILE *gm_f, char *filename, int rec_count) {
    char *buf, buf2[256], *p, buf3[READ_SIZE];
    int temp, val, t1, t2, rv;
    int done = false;

    snprintf(buf2, sizeof(buf2), "beginning of GM file %s", filename);

    buf = fread_string(gm_f, buf2);
    while (!done) {
        if (*buf == '#') {        /* New Trainer */
            sscanf(buf, "#%d\n", &temp);
            snprintf(buf2, sizeof(buf2), "GM #%d in GM file %s", temp, filename);
            free(buf);        /* Plug memory leak! */
            top_guild = temp;
            auto &g = guild_index[temp];
            auto &z = zone_table[real_zone_by_thing(temp)];
            z.guilds.insert(temp);

            GM_NUM(top_guild) = temp;

            get_line(gm_f, buf3);
            rv = sscanf(buf3, "%d %d", &t1, &t2);
            while (t1 > -1) {
                if (rv == 1) { /* old style guilds, only skills */
                    g.skills.insert(t1);
                } else if (rv == 2) { /* new style guilds, skills and feats */
                    if (t2 == 1) {
                        g.skills.insert(t1);
                    } else if (t2 == 2) {
                        g.feats.insert(t1);
                    } else {
                        basic_mud_log("SYSERR: Invalid 2nd arg in guild file!");
                        exit(1);
                    }
                } else {
                    basic_mud_log("SYSERR: Invalid format in guild file. Expecting 2 args but got %d!", rv);
                    exit(1);
                }
                get_line(gm_f, buf3);
                rv = sscanf(buf3, "%d %d", &t1, &t2);
            }
            read_guild_line(gm_f, "%f", &g.charge, "GM_CHARGE");
            g.no_such_skill = fread_string(gm_f, buf2);
            g.not_enough_gold = fread_string(gm_f, buf2);

            read_guild_line(gm_f, "%d", &g.minlvl, "GM_MINLVL");
            read_guild_line(gm_f, "%d", &g.gm, "GM_TRAINER");

            g.gm = real_mobile(g.gm);
            read_guild_line(gm_f, "%d", &g.with_who[0], "GM_WITH_WHO");

            read_guild_line(gm_f, "%d", &g.open, "GM_OPEN");
            read_guild_line(gm_f, "%d", &g.close, "GM_CLOSE");

            CREATE(buf, char, READ_SIZE);
            get_line(gm_f, buf);
            if (buf && *buf != '#' && *buf != '$') {
                p = buf;
                for (temp = 1; temp < GW_ARRAY_MAX; temp++) {
                    if (!p || !*p)
                        break;
                    if (sscanf(p, "%d", &val) != 1) {
                        basic_mud_log("SYSERR: Can't parse GM_WITH_WHO line in %s: '%s'", buf2, buf);
                        break;
                    }
                    g.with_who[temp] = val;
                    while (isdigit(*p) || *p == '-') {
                        p++;
                    }
                    while (*p && !(isdigit(*p) || *p == '-')) {
                        p++;
                    }
                }
                free(buf);
                buf = fread_string(gm_f, buf2);
            }
        } else {
            if (*buf == '$')        /* EOF */
                done = true;
            free(buf);        /* Plug memory leak! */
        }
    }
}

static void boot_the_shops(FILE *shop_f, char *filename, int rec_count) {
    char *buf, buf2[256], *p;
    shop_vnum temp, count, new_format = false;
    int shop_temp;
    struct shop_buy_data list[MAX_SHOP_OBJ + 1]{};
    int done = false;

    snprintf(buf2, sizeof(buf2), "beginning of shop file %s", filename);

    while (!done) {
        buf = fread_string(shop_f, buf2);
        if (*buf == '#') {        /* New shop */
            sscanf(buf, "#%ld\n", &temp);
            snprintf(buf2, sizeof(buf2)-1, "shop #%ld in shop file %s", temp, filename);
            auto &sh = shop_index[temp];
            free(buf);        /* Plug memory leak! */
            sh.vnum = temp;
            auto &z = zone_table[real_zone_by_thing(sh.vnum)];
            z.shops.insert(sh.vnum);
            top_shop = temp;
            while(true) {
                read_line(shop_f, "%ld", &shop_temp);
                if(shop_temp == -1) break;
                temp = (shop_vnum)shop_temp;
                if(obj_index.count(temp)) sh.producing.push_back(temp);
            }

            read_line(shop_f, "%f", &SHOP_BUYPROFIT(top_shop));
            read_line(shop_f, "%f", &SHOP_SELLPROFIT(top_shop));

            while(true) {
                read_line(shop_f, "%ld", &shop_temp);
                if(shop_temp == -1) break;
                auto &t = sh.type.emplace_back();
                t.type = shop_temp;
            }

            sh.no_such_item1 = read_shop_message(0, SHOP_NUM(top_shop), shop_f, buf2);
            sh.no_such_item2 = read_shop_message(1, SHOP_NUM(top_shop), shop_f, buf2);
            sh.do_not_buy = read_shop_message(2, SHOP_NUM(top_shop), shop_f, buf2);
            sh.missing_cash1 = read_shop_message(3, SHOP_NUM(top_shop), shop_f, buf2);
            sh.missing_cash2 = read_shop_message(4, SHOP_NUM(top_shop), shop_f, buf2);
            sh.message_buy = read_shop_message(5, SHOP_NUM(top_shop), shop_f, buf2);
            sh.message_sell = read_shop_message(6, SHOP_NUM(top_shop), shop_f, buf2);
            read_line(shop_f, "%d", &SHOP_BROKE_TEMPER(top_shop));
            read_line(shop_f, "%ld", &SHOP_BITVECTOR(top_shop));
            read_line(shop_f, "%hd", &SHOP_KEEPER(top_shop));

            SHOP_KEEPER(top_shop) = real_mobile(SHOP_KEEPER(top_shop));
            CREATE(buf, char, READ_SIZE);
            get_line(shop_f, buf);
            p = buf;
            for (temp = 0; temp < SW_ARRAY_MAX; temp++) {
                if (!p || !*p)
                    break;
                if (sscanf(p, "%d", &count) != 1) {
                    basic_mud_log("SYSERR: Can't parse TRADE_WITH line in %s: '%s'", buf2, buf);
                    break;
                }
                SHOP_TRADE_WITH(top_shop)[temp] = count;
                while (isdigit(*p) || *p == '-') {
                    p++;
                }
                while (*p && !(isdigit(*p) || *p == '-')) {
                    p++;
                }
            }
            free(buf);
            while (temp < SW_ARRAY_MAX)
                SHOP_TRADE_WITH(top_shop)[temp++] = 0;

            while(true) {
                read_line(shop_f, "%ld", &shop_temp);
                if(shop_temp == -1) break;
                if(world.contains(shop_temp)) sh.in_room.insert(shop_temp);

            }


            read_line(shop_f, "%d", &SHOP_OPEN1(top_shop));
            read_line(shop_f, "%d", &SHOP_CLOSE1(top_shop));
            read_line(shop_f, "%d", &SHOP_OPEN2(top_shop));
            read_line(shop_f, "%d", &SHOP_CLOSE2(top_shop));

            SHOP_BANK(top_shop) = 0;
            SHOP_SORT(top_shop) = 0;
            SHOP_FUNC(top_shop) = nullptr;
        } else {
            if (*buf == '$')        /* EOF */
                done = true;
            else if (strstr(buf, VERSION3_TAG))    /* New format marker */
                new_format = true;
            free(buf);        /* Plug memory leak! */
        }
    }
}

static int check_object_spell_number(struct obj_data *obj, int val) {
    int error = false;
    const char *spellname;

    if (GET_OBJ_VAL(obj, val) == -1 ||
        GET_OBJ_VAL(obj, val) == 0)    /* i.e.: no spell */
        return (error);

    /*
   * Check for negative spells, spells beyond the top define, and any
   * spell which is actually a skill.
   */
    if (GET_OBJ_VAL(obj, val) < 0)
        error = true;
    if (GET_OBJ_VAL(obj, val) >= SKILL_TABLE_SIZE)
        error = true;
    if (skill_type(GET_OBJ_VAL(obj, val)) != SKTYPE_SPELL)
        error = true;
    if (error)
        basic_mud_log("SYSERR: Object #%d (%s) has out of range spell #%d.",
            GET_OBJ_VNUM(obj), obj->getShortDesc(), GET_OBJ_VAL(obj, val));

    /*
   * This bug has been fixed, but if you don't like the special behavior...
   */
#if 0
                                                                                                                            if (GET_OBJ_TYPE(obj) == ITEM_STAFF &&
	HAS_SPELL_ROUTINE(GET_OBJ_VAL(obj, val), MAG_AREAS | MAG_MASSES))
    log("... '%s' (#%d) uses %s spell '%s'.",
	obj->getShortDesc(),	GET_OBJ_VNUM(obj),
	HAS_SPELL_ROUTINE(GET_OBJ_VAL(obj, val), MAG_AREAS) ? "area" : "mass",
	skill_name(GET_OBJ_VAL(obj, val)));
#endif

    if (scheck)        /* Spell names don't exist in syntax check mode. */
        return (error);

    /* Now check for unnamed spells. */
    spellname = skill_name(GET_OBJ_VAL(obj, val));

    if ((spellname == unused_spellname || !strcasecmp("UNDEFINED", spellname)) && (error = true))
        basic_mud_log("SYSERR: Object #%d (%s) uses '%s' spell #%d.",
            GET_OBJ_VNUM(obj), obj->getShortDesc(), spellname,
            GET_OBJ_VAL(obj, val));

    return (error);
}

static int check_object_level(struct obj_data *obj, int val) {
    int error = false;

    if ((GET_OBJ_VAL(obj, val) < 0) && (error = true))
        basic_mud_log("SYSERR: Object #%d (%s) has out of range level #%d.",
            GET_OBJ_VNUM(obj), obj->getShortDesc(), GET_OBJ_VAL(obj, val));

    return (error);
}

static int check_bitvector_names(bitvector_t bits, size_t namecount, const char *whatami, const char *whatbits) {
    unsigned int flagnum;
    bool error = false;

    /* See if any bits are set above the ones we know about. */
    if (bits <= (~(bitvector_t) 0 >> (sizeof(bitvector_t) * 8 - namecount)))
        return (false);

    for (flagnum = namecount; flagnum < sizeof(bitvector_t) * 8; flagnum++)
        if ((1 << flagnum) & bits) {
            basic_mud_log("SYSERR: %s has unknown %s flag, bit %d (0 through %" SZT " known).", whatami, whatbits, flagnum,
                namecount - 1);
            error = true;
        }

    return (error);
}

static int check_object(struct obj_data *obj) {
    char objname[MAX_INPUT_LENGTH + 32];
    int error = false, y;

    if (GET_OBJ_WEIGHT(obj) < 0 && (error = true))
        basic_mud_log("SYSERR: Object #%d (%s) has negative weight (%" I64T ").",
            GET_OBJ_VNUM(obj), obj->getShortDesc(), GET_OBJ_WEIGHT(obj));

    if (GET_OBJ_RENT(obj) < 0 && (error = true))
        basic_mud_log("SYSERR: Object #%d (%s) has negative cost/day (%d).",
            GET_OBJ_VNUM(obj), obj->getShortDesc(), GET_OBJ_RENT(obj));

    snprintf(objname, sizeof(objname), "Object #%d (%s)", GET_OBJ_VNUM(obj), obj->getShortDesc());
    for (y = 0; y < TW_ARRAY_MAX; y++) {
        error |= check_bitvector_names(GET_OBJ_WEAR(obj)[y], wear_bits_count, objname, "object wear");
        error |= check_bitvector_names(GET_OBJ_EXTRA(obj)[y], extra_bits_count, objname, "object extra");
        error |= check_bitvector_names(GET_OBJ_PERM(obj)[y], affected_bits_count, objname, "object affect");
    }

    switch (GET_OBJ_TYPE(obj)) {
        case ITEM_DRINKCON: {
            char onealias[MAX_INPUT_LENGTH], *space = strrchr(obj->name, ' ');

            strlcpy(onealias, space ? space + 1 : obj->name, sizeof(onealias));
            if (search_block(onealias, drinknames, true) < 0 && (error = true)) {
                //log("SYSERR: Object #%d (%s) doesn't have drink type as last alias. (%s)", GET_OBJ_VNUM(obj), obj->getShortDesc(), obj->name);
            }
        }
            /* Fall through. */
        case ITEM_FOUNTAIN:
            if ((GET_OBJ_VAL(obj, 0) > 0) && (GET_OBJ_VAL(obj, 1) > GET_OBJ_VAL(obj, 0) && (error = true)))
                basic_mud_log("SYSERR: Object #%d (%s) contains (%d) more than maximum (%d).",
                    GET_OBJ_VNUM(obj), obj->getShortDesc(),
                    GET_OBJ_VAL(obj, 1), GET_OBJ_VAL(obj, 0));
            break;
        case ITEM_SCROLL:
        case ITEM_POTION:
            error |= check_object_level(obj, 0);
            error |= check_object_spell_number(obj, 1);
            error |= check_object_spell_number(obj, 2);
            error |= check_object_spell_number(obj, 3);
            break;
        case ITEM_WAND:
        case ITEM_STAFF:
            error |= check_object_level(obj, 0);
            error |= check_object_spell_number(obj, 3);
            if (GET_OBJ_VAL(obj, 2) > GET_OBJ_VAL(obj, 1) && (error = true))
                basic_mud_log("SYSERR: Object #%d (%s) has more charges (%d) than maximum (%d).",
                    GET_OBJ_VNUM(obj), obj->getShortDesc(),
                    GET_OBJ_VAL(obj, 2), GET_OBJ_VAL(obj, 1));
            break;
    }

    return (error);
}

int load_inv_backup(struct char_data *ch) {
    if (GET_LEVEL(ch) < 2)
        return (-1);

    char chx;
    FILE *source, *target;
    char source_file[20480], target_file[20480], buf2[20480];
    char alpha[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH];
    sprintf(name, GET_NAME(ch));

    if (name[0] == 'a' || name[0] == 'A' || name[0] == 'b' || name[0] == 'B' || name[0] == 'c' || name[0] == 'C' ||
        name[0] == 'd' || name[0] == 'D' || name[0] == 'e' || name[0] == 'E') {
        sprintf(alpha, "A-E");
    } else if (name[0] == 'f' || name[0] == 'F' || name[0] == 'g' || name[0] == 'G' || name[0] == 'h' ||
               name[0] == 'H' || name[0] == 'i' || name[0] == 'I' || name[0] == 'j' || name[0] == 'J') {
        sprintf(alpha, "F-J");
    } else if (name[0] == 'k' || name[0] == 'K' || name[0] == 'l' || name[0] == 'L' || name[0] == 'm' ||
               name[0] == 'M' || name[0] == 'n' || name[0] == 'N' || name[0] == 'o' || name[0] == 'O') {
        sprintf(alpha, "K-O");
    } else if (name[0] == 'p' || name[0] == 'P' || name[0] == 'q' || name[0] == 'Q' || name[0] == 'r' ||
               name[0] == 'R' || name[0] == 's' || name[0] == 'S' || name[0] == 't' || name[0] == 'T') {
        sprintf(alpha, "P-T");
    } else if (name[0] == 'u' || name[0] == 'U' || name[0] == 'v' || name[0] == 'V' || name[0] == 'w' ||
               name[0] == 'W' || name[0] == 'x' || name[0] == 'X' || name[0] == 'y' || name[0] == 'Y' ||
               name[0] == 'z' || name[0] == 'Z') {
        sprintf(alpha, "U-Z");
    }

    sprintf(source_file, "plrobjs" SLASH"%s" SLASH"%s.copy",
            alpha, ch->name);
    if (!get_filename(buf2, sizeof(buf2), NEW_OBJ_FILES, GET_NAME(ch)))
        return -1;
    sprintf(target_file, "%s", buf2);

    if (!(source = fopen(source_file, "r"))) {
        basic_mud_log("Source in load_inv_backup failed to load.");
        basic_mud_log(source_file);
        return -1;
    }

    if (!(target = fopen(target_file, "w"))) {
        basic_mud_log("Target in load_inv_backup failed to load.");
        basic_mud_log(target_file);
        return -1;
    }

    while ((chx = fgetc(source)) != EOF)
        fputc(chx, target);

    basic_mud_log("Inventory backup restore successful.");

    fclose(source);
    fclose(target);

    return 1;
}

static int inv_backup(struct char_data *ch) {
    FILE *backup;
    char buf[20480];

    char alpha[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH];
    sprintf(name, GET_NAME(ch));

    if (name[0] == 'a' || name[0] == 'A' || name[0] == 'b' || name[0] == 'B' || name[0] == 'c' || name[0] == 'C' ||
        name[0] == 'd' || name[0] == 'D' || name[0] == 'e' || name[0] == 'E') {
        sprintf(alpha, "A-E");
    } else if (name[0] == 'f' || name[0] == 'F' || name[0] == 'g' || name[0] == 'G' || name[0] == 'h' ||
               name[0] == 'H' || name[0] == 'i' || name[0] == 'I' || name[0] == 'j' || name[0] == 'J') {
        sprintf(alpha, "F-J");
    } else if (name[0] == 'k' || name[0] == 'K' || name[0] == 'l' || name[0] == 'L' || name[0] == 'm' ||
               name[0] == 'M' || name[0] == 'n' || name[0] == 'N' || name[0] == 'o' || name[0] == 'O') {
        sprintf(alpha, "K-O");
    } else if (name[0] == 'p' || name[0] == 'P' || name[0] == 'q' || name[0] == 'Q' || name[0] == 'r' ||
               name[0] == 'R' || name[0] == 's' || name[0] == 'S' || name[0] == 't' || name[0] == 'T') {
        sprintf(alpha, "P-T");
    } else if (name[0] == 'u' || name[0] == 'U' || name[0] == 'v' || name[0] == 'V' || name[0] == 'w' ||
               name[0] == 'W' || name[0] == 'x' || name[0] == 'X' || name[0] == 'y' || name[0] == 'Y' ||
               name[0] == 'z' || name[0] == 'Z') {
        sprintf(alpha, "U-Z");
    }

    sprintf(buf, "plrobjs" SLASH"%s" SLASH"%s.copy", alpha,
            ch->name);

    if (!(backup = fopen(buf, "r")))
        return -1;

    fclose(backup);

    return 1;
}

static int Crash_load(struct char_data *ch) {
    FILE *fl;
    char cmfname[MAX_STRING_LENGTH];
    char buf1[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH];
    char line[256];
    int64_t t[30], danger, zwei = 0, num_of_days;
    int orig_rent_code;
    struct obj_data *temp;
    int locate = 0, j, nr, k, cost, num_objs = 0;
    struct obj_data *obj1;
    struct obj_data *cont_row[MAX_BAG_ROWS];
    struct extra_descr_data *new_descr;
    int rentcode, timed, netcost, gold, account, nitems;
    char f1[READ_SIZE], f2[READ_SIZE], f3[READ_SIZE], f4[READ_SIZE];

    if (!get_filename(cmfname, sizeof(cmfname), NEW_OBJ_FILES, GET_NAME(ch)))
        return 1;

    if (!(fl = fopen(cmfname, "r+b"))) {
        if (errno != ENOENT) {    /* if it fails, NOT because of no file */
            sprintf(buf1, "SYSERR: READING OBJECT FILE %s (5)", cmfname);
            perror(buf1);
            send_to_char(ch,
                         "\r\n********************* NOTICE *********************\r\n"
                         "There was a problem loading your objects from disk.\r\n"
                         "Contact a God for assistance.\r\n");
        }
        if (GET_LEVEL(ch) > 1) {
            mudlog(NRM, MAX(ADMLVL_IMMORT, GET_INVIS_LEV(ch)), true,
                   "%s entering game with no equipment. Loading backup.", GET_NAME(ch));
        }
        if (!inv_backup(ch)) {
            return -1;
        } else {
            if (!load_inv_backup(ch))
                return -1;
            else if (!(fl = fopen(cmfname, "r+b"))) {
                if (errno != ENOENT) {      /* if it fails, NOT because of no file */
                    sprintf(buf1, "SYSERR: READING OBJECT FILE %s (5)", cmfname);
                    perror(buf1);
                    send_to_char(ch,
                                 "\r\n********************* NOTICE *********************\r\n"
                                 "There was a problem loading your objects from disk.\r\n"
                                 "Contact a God for assistance.\r\n");
                }
                return -1;
            }
        }
    }

    if (!feof(fl))
        get_line(fl, line);

    sscanf(line, "%d %d %d %d %d %d", &rentcode, &timed,
           &netcost, &gold, &account, &nitems);

    if (rentcode == RENT_RENTED || rentcode == RENT_TIMEDOUT) {
        num_of_days = (float) (time(nullptr) - timed) / SECS_PER_REAL_DAY;
        cost = 0;
    }
    switch (orig_rent_code = rentcode) {
        case RENT_RENTED:
            mudlog(NRM, MAX(ADMLVL_IMMORT, GET_INVIS_LEV(ch)), true, "%s un-renting and entering game.", GET_NAME(ch));
            break;
        case RENT_CRASH:
            mudlog(NRM, MAX(ADMLVL_IMMORT, GET_INVIS_LEV(ch)), true,
                   "%s retrieving crash-saved items and entering game.", GET_NAME(ch));
            break;
        case RENT_CRYO:
            mudlog(NRM, MAX(ADMLVL_IMMORT, GET_INVIS_LEV(ch)), true, "%s un-cryo'ing and entering game.", GET_NAME(ch));
            break;
        case RENT_FORCED:
        case RENT_TIMEDOUT:
            mudlog(NRM, MAX(ADMLVL_IMMORT, GET_INVIS_LEV(ch)), true,
                   "%s retrieving force-saved items and entering game.", GET_NAME(ch));
            break;
        default:
            mudlog(BRF, MAX(ADMLVL_IMMORT, GET_INVIS_LEV(ch)), true,
                   "WARNING: %s entering game with undefined rent code.", GET_NAME(ch));
            break;
    }

    for (j = 0; j < MAX_BAG_ROWS; j++)
        cont_row[j] = nullptr; /* empty all cont lists (you never know ...) */

    if (!feof(fl))
        get_line(fl, line);

    while (!feof(fl)) {
        temp = nullptr;
        /* first, we get the number. Not too hard. */
        if (*line == '#') {
            if (sscanf(line, "#%d", &nr) != 1) {
                continue;
            }
            /* we have the number, check it, load obj. */
            if (nr == NOTHING) {   /* then it is unique */
                temp = create_obj(false);
                temp->vn = NOTHING;
                GET_OBJ_SIZE(temp) = SIZE_MEDIUM;
            } else if (nr < 0) {
                continue;
            } else {
                if (nr >= 999999)
                    continue;
                temp = read_object(nr, VIRTUAL, false);
                if (!temp) {
                    get_line(fl, line);
                    continue;
                }
            }

            get_line(fl, line);

            sscanf(line, "%ld %ld %ld %ld %ld %ld %ld %ld %ld %s %s %s %s %ld %ld %ld %ld %ld %ld %ld %ld", t, t + 1, t + 2, t + 3,
                   t + 4, t + 5, t + 6, t + 7, t + 8, f1, f2, f3, f4, t + 13, t + 14, t + 15, t + 16, t + 17, t + 18,
                   t + 19, t + 20);

            locate = t[0];
            GET_OBJ_VAL(temp, 0) = t[1];
            GET_OBJ_VAL(temp, 1) = t[2];
            GET_OBJ_VAL(temp, 2) = t[3];
            GET_OBJ_VAL(temp, 3) = t[4];
            GET_OBJ_VAL(temp, 4) = t[5];
            GET_OBJ_VAL(temp, 5) = t[6];
            GET_OBJ_VAL(temp, 6) = t[7];
            GET_OBJ_VAL(temp, 7) = t[8];
            bitvector_t ex[4];
            ex[0] = asciiflag_conv(f1);
            ex[1] = asciiflag_conv(f2);
            ex[2] = asciiflag_conv(f3);
            ex[3] = asciiflag_conv(f4);
            for(auto i = 0; i < temp->extra_flags.size(); i++) temp->extra_flags.set(i, IS_SET_AR(ex, i));
            GET_OBJ_VAL(temp, 8) = t[13];
            GET_OBJ_VAL(temp, 9) = t[14];
            GET_OBJ_VAL(temp, 10) = t[15];
            GET_OBJ_VAL(temp, 11) = t[16];
            GET_OBJ_VAL(temp, 12) = t[17];
            GET_OBJ_VAL(temp, 13) = t[18];
            GET_OBJ_VAL(temp, 14) = t[19];
            GET_OBJ_VAL(temp, 15) = t[20];

            get_line(fl, line);
            /* read line check for xap. */
            if (!strcmp("XAP", line)) {  /* then this is a Xap Obj, requires
                                       special care */
                
                temp->setName(withPlaceholder(fread_string(fl, "rented object name"), "undefined"));
                temp->setShortDesc(withPlaceholder(fread_string(fl, "rented object short desc"), "undefined"));
                temp->setRoomDesc(withPlaceholder(fread_string(fl, "rented object desc"), "undefined"));
                temp->setLookDesc(withPlaceholder(fread_string(fl, "rented object adesc"), "undefined"));

                if (!get_line(fl, line) ||
                    (sscanf(line, "%ld %ld %ld %ld %ld %ld %ld %ld", t, t + 1, t + 2, t + 3, t + 4, t + 5, t + 6, t + 7) !=
                     8)) {
                    fprintf(stderr, "Format error in first numeric line (expecting _x_ args)");
                    return 0;
                }
                temp->type_flag = t[0];
                bitvector_t wear[4];
                wear[0] = t[1];
                wear[1] = t[2];
                wear[2] = t[3];
                wear[3] = t[4];
                for(auto i = 0; i < temp->wear_flags.size(); i++) temp->wear_flags.set(i, IS_SET_AR(wear, i));

                temp->weight = t[5];
                temp->cost = t[6];
                temp->cost_per_day = t[7];

                /* we're clearing these for good luck */

                for (j = 0; j < MAX_OBJ_AFFECT; j++) {
                    temp->affected[j].location = APPLY_NONE;
                    temp->affected[j].modifier = 0;
                    temp->affected[j].specific = 0;
                }

                /* maybe clear spellbook for good luck too? */
                if (GET_OBJ_TYPE(temp) == ITEM_SPELLBOOK) {
                    if (!temp->sbinfo) {
                        CREATE(temp->sbinfo, struct obj_spellbook_spell, SPELLBOOK_SIZE);
                        memset((char *) temp->sbinfo, 0, SPELLBOOK_SIZE * sizeof(struct obj_spellbook_spell));
                    }
                    for (j = 0; j < SPELLBOOK_SIZE; j++) {
                        temp->sbinfo[j].spellname = 0;
                        temp->sbinfo[j].pages = 0;
                    }
                    temp->sbinfo[0].spellname = SPELL_DETECT_MAGIC;
                    temp->sbinfo[0].pages = 1;
                }

                temp->ex_description = nullptr;

                get_line(fl, line);
                for (k = j = zwei = 0; !zwei && !feof(fl);) {
                    switch (*line) {
                        case 'E':
                            CREATE(new_descr, struct extra_descr_data, 1);
                            sprintf(buf2, "rented object edesc keyword for object #%d", nr);
                            new_descr->keyword = fread_string(fl, buf2);
                            sprintf(buf2, "rented object edesc text for object #%d keyword %s", nr, new_descr->keyword);
                            new_descr->description = fread_string(fl, buf2);
                            new_descr->next = temp->ex_description;
                            temp->ex_description = new_descr;
                            get_line(fl, line);
                            break;
                        case 'A':
                            if (j >= MAX_OBJ_AFFECT) {
                                basic_mud_log("SYSERR: Too many object affectations in loading rent file");
                                danger = 1;
                            }
                            get_line(fl, line);
                            sscanf(line, "%ld %ld %ld", t, t + 1, t + 2);

                            temp->affected[j].location = t[0];
                            temp->affected[j].modifier = t[1];
                            temp->affected[j].specific = t[2];
                            j++;
                            get_line(fl, line);
                            break;
                        case 'G':
                            get_line(fl, line);
                            sscanf(line, "%" TMT, &temp->generation);
                            get_line(fl, line);
                            break;
                        case 'U':
                            get_line(fl, line);
                            sscanf(line, "%" I64T, &temp->id);
                            get_line(fl, line);
                            break;
                        case 'S':
                            if (j >= SPELLBOOK_SIZE) {
                                basic_mud_log("SYSERR: Too many spells in spellbook loading rent file");
                                danger = 1;
                            }
                            get_line(fl, line);
                            sscanf(line, "%d %d", t, t + 1);

                            if (!temp->sbinfo) {
                                CREATE(temp->sbinfo, struct obj_spellbook_spell, SPELLBOOK_SIZE);
                                memset((char *) temp->sbinfo, 0, SPELLBOOK_SIZE * sizeof(struct obj_spellbook_spell));
                            }
                            temp->sbinfo[j].spellname = t[0];
                            temp->sbinfo[j].pages = t[1];
                            j++;
                            get_line(fl, line);
                            break;
                        case 'Z':
                            get_line(fl, line);
                            sscanf(line, "%d", &GET_OBJ_SIZE(temp));
                            get_line(fl, line);
                            break;
                        case '$':
                        case '#':
                            zwei = 1;
                            break;
                        default:
                            zwei = 1;
                            break;
                    }
                }      /* exit our for loop */
            }   /* exit our xap loop */
            if (temp != nullptr) {
                num_objs++;
                temp->script = std::make_shared<script_data>(temp);
                check_unique_id(temp);
                add_unique_id(temp);
                temp->activate();

                if (GET_OBJ_TYPE(temp) == ITEM_DRINKCON) {
                    name_from_drinkcon(temp);
                    if (GET_OBJ_VAL(temp, 1) != 0)
                        name_to_drinkcon(temp, GET_OBJ_VAL(temp, 2));
                }
                if (GET_OBJ_VNUM(temp) == 20099 || GET_OBJ_VNUM(temp) == 20098)
                    if (OBJ_FLAGGED(temp, ITEM_UNBREAKABLE))
                        temp->extra_flags.reset(ITEM_UNBREAKABLE);
                auto_equip(ch, temp, locate);
            } else {
                continue;
            }
            /*
              what to do with a new loaded item:

              if there's a list with <locate> less than 1 below this:
                (equipped items are assumed to have <locate>==0 here) then its
                container has disappeared from the file   *gasp*
                 -> put all the list back to ch's inventory
              if there's a list of contents with <locate> 1 below this:
                check if it's a container
                - if so: get it from ch, fill it, and give it back to ch (this way the
                    container has its correct weight before modifying ch)
                - if not: the container is missing -> put all the list to ch's inventory

              for items with negative <locate>:
                if there's already a list of contents with the same <locate> put obj to it
                if not, start a new list

            Confused? Well maybe you can think of some better text to be put here ...

            since <locate> for contents is < 0 the list indices are switched to
            non-negative
            */

            if (locate > 0) { /* item equipped */
                for (j = MAX_BAG_ROWS - 1; j > 0; j--)
                    if (cont_row[j]) { /* no container -> back to ch's inventory */
                        for (; cont_row[j]; cont_row[j] = obj1) {
                            obj1 = cont_row[j]->next_content;
                            obj_to_char(cont_row[j], ch);
                        }
                        cont_row[j] = nullptr;
                    }
                if (cont_row[0]) { /* content list existing */
                    if (GET_OBJ_TYPE(temp) == ITEM_CONTAINER) {
                        /* rem item ; fill ; equip again */
                        temp = unequip_char(ch, locate - 1);
                        temp->contents = nullptr; /* should be empty - but who knows */
                        for (; cont_row[0]; cont_row[0] = obj1) {
                            obj1 = cont_row[0]->next_content;
                            obj_to_obj(cont_row[0], temp);
                        }
                        equip_char(ch, temp, locate - 1);
                    } else { /* object isn't container -> empty content list */
                        for (; cont_row[0]; cont_row[0] = obj1) {
                            obj1 = cont_row[0]->next_content;
                            obj_to_char(cont_row[0], ch);
                        }
                        cont_row[0] = nullptr;
                    }
                }
            } else { /* locate <= 0 */
                for (j = MAX_BAG_ROWS - 1; j > -locate; j--)
                    if (cont_row[j]) { /* no container -> back to ch's inventory */
                        for (; cont_row[j]; cont_row[j] = obj1) {
                            obj1 = cont_row[j]->next_content;
                            obj_to_char(cont_row[j], ch);
                        }
                        cont_row[j] = nullptr;
                    }

                if (j == -locate && cont_row[j]) { /* content list existing */
                    if (GET_OBJ_TYPE(temp) == ITEM_CONTAINER) {
                        /* take item ; fill ; give to char again */
                        obj_from_char(temp);
                        temp->contents = nullptr;
                        for (; cont_row[j]; cont_row[j] = obj1) {
                            obj1 = cont_row[j]->next_content;
                            obj_to_obj(cont_row[j], temp);
                        }
                        obj_to_char(temp, ch); /* add to inv first ... */
                    } else { /* object isn't container -> empty content list */
                        for (; cont_row[j]; cont_row[j] = obj1) {
                            obj1 = cont_row[j]->next_content;
                            obj_to_char(cont_row[j], ch);
                        }
                        cont_row[j] = nullptr;
                    }
                }

                if (locate < 0 && locate >= -MAX_BAG_ROWS) {
                    /* let obj be part of content list
                       but put it at the list's end thus having the items
                       in the same order as before renting */
                    obj_from_char(temp);
                    if ((obj1 = cont_row[-locate - 1])) {
                        while (obj1->next_content)
                            obj1 = obj1->next_content;
                        obj1->next_content = temp;
                    } else
                        cont_row[-locate - 1] = temp;
                }
            } /* locate less than zero */
        } else {
            get_line(fl, line);
        }
    }

    /* Little hoarding check. -gg 3/1/98 */
    mudlog(NRM, MAX(ADMLVL_IMMORT, GET_INVIS_LEV(ch)), true, "%s (level %d) has %d objects (max %d).",
           GET_NAME(ch), GET_LEVEL(ch), num_objs, CONFIG_MAX_OBJ_SAVE);

    fclose(fl);

    if ((orig_rent_code == RENT_RENTED) || (orig_rent_code == RENT_CRYO))
        return 0;
    else
        return 1;
}

static char fread_letter(FILE *fp) {
    char c;
    do {
        c = getc(fp);
    } while (isspace(c));
    return c;
}

static void get_one_line(FILE *fl, char *buf) {
    if (fgets(buf, READ_SIZE, fl) == nullptr) {
        basic_mud_log("SYSERR: error reading help file: not terminated with $?");
        exit(1);
    }

    buf[strlen(buf) - 1] = '\0'; /* take off the trailing \n */
}

/* read direction data */
static void setup_dir(FILE *fl, room_vnum room, int dir) {
    int t[11], retval;
    char line[READ_SIZE], buf2[128];

    snprintf(buf2, sizeof(buf2), "room #%d, direction D%d", room, dir);

    auto &r = world[room];

    CREATE(r.dir_option[dir], struct room_direction_data, 1);

    auto d = r.dir_option[dir];

    d->general_description = fread_string(fl, buf2);
    d->keyword = fread_string(fl, buf2);

    if (!get_line(fl, line)) {
        basic_mud_log("SYSERR: Format error, %s", buf2);
        exit(1);
    }
    if (((retval = sscanf(line, " %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld", t, t + 1, t + 2, t + 3, t + 4, t + 5, t + 6, t + 7,
                          t + 8, t + 9, t + 10)) == 3) && (bitwarning == true)) {
        basic_mud_log("SYSERR: Format error, %s", buf2);
        exit(1);
    } else if (bitwarning == false) {

        if (t[0] == 1)
            d->exit_info = EX_ISDOOR;
        else if (t[0] == 2)
            d->exit_info = EX_ISDOOR | EX_PICKPROOF;
        else if (t[0] == 3)
            d->exit_info = EX_ISDOOR | EX_SECRET;
        else if (t[0] == 4)
            d->exit_info = EX_ISDOOR | EX_PICKPROOF | EX_SECRET;
        else
            d->exit_info = 0;

        d->key = ((t[1] == -1 || t[1] == 65535) ? NOTHING : t[1]);
        d->to_room = ((t[2] == -1 || t[2] == 65535) ? NOWHERE : t[2]);

        if (retval == 3) {
            basic_mud_log("Converting world files to include DC add ons.");
            d->dclock = 20;
            d->dchide = 20;
            d->dcskill = 0;
            d->dcmove = 0;
            d->failsavetype = 0;
            d->dcfailsave = 0;
            d->failroom = NOWHERE;
            d->totalfailroom = NOWHERE;
            if (bitsavetodisk) {
                r.save();
                converting = true;
            }
        } else if (retval == 5) {
            d->dclock = t[3];
            d->dchide = t[4];
            d->dcskill = 0;
            d->dcmove = 0;
            d->failsavetype = 0;
            d->dcfailsave = 0;
            d->failroom = NOWHERE;
            d->totalfailroom = NOWHERE;
            if (bitsavetodisk) {
                r.save();
                converting = true;
            }
        } else if (retval == 7) {
            d->dclock = t[3];
            d->dchide = t[4];
            d->dcskill = t[5];
            d->dcmove = t[6];
            d->failsavetype = 0;
            d->dcfailsave = 0;
            d->failroom = NOWHERE;
            d->totalfailroom = NOWHERE;
            if (bitsavetodisk) {
                r.save();
                converting = true;
            }
        } else if (retval == 11) {
            d->dclock = t[3];
            d->dchide = t[4];
            d->dcskill = t[5];
            d->dcmove = t[6];
            d->failsavetype = t[7];
            d->dcfailsave = t[8];
            d->failroom = t[9];
            d->totalfailroom = t[10];
        }
    }
}


/* load the rooms */
static void parse_room(FILE *fl, room_vnum virtual_nr) {
    int t[10], i, retval;
    char line[READ_SIZE], flags[128], flags2[128], flags3[128];
    char flags4[128], buf2[MAX_STRING_LENGTH], buf[128];
    bitvector_t roomFlagsHolder[4];
    struct extra_descr_data *new_descr;
    char letter;

    /* This really had better fit or there are other problems. */
    snprintf(buf2, sizeof(buf2), "room #%d", virtual_nr);

    auto zone = real_zone_by_thing(virtual_nr);
    if (zone == NOWHERE) {
        basic_mud_log("SYSERR: Room #%d is outside any zone.", virtual_nr);
        exit(1);
    }

    if(world.count(virtual_nr)) {
        basic_mud_log("SYSERR: Room #%d already exists, cannot parse!", virtual_nr);
        exit(1);
    }
    auto &z = zone_table[zone];
    auto &r = world[virtual_nr];
    z.rooms.insert(virtual_nr);
    r.script = std::make_shared<script_data>(&r);
    r.zone = zone;
    r.vn = virtual_nr;
    r.name = fread_string(fl, buf2);
    r.look_description = fread_string(fl, buf2);

    if (!get_line(fl, line)) {
        basic_mud_log("SYSERR: Expecting roomflags/sector type of room #%d but file ended!",
            virtual_nr);
        exit(1);
    }

    if ((retval = sscanf(line, " %d %s %s %s %s %d ", t, flags, flags2, flags3, flags4, t + 2)) == 6) {
        int taeller;
        roomFlagsHolder[0] = asciiflag_conv(flags);
        roomFlagsHolder[1] = asciiflag_conv(flags2);
        roomFlagsHolder[2] = asciiflag_conv(flags3);
        roomFlagsHolder[3] = asciiflag_conv(flags4);

        for(auto i = 0; i < NUM_ROOM_FLAGS; i++) if(IS_SET_AR(roomFlagsHolder, i)) r.room_flags.set(i);

        r.sector_type = t[2];
        sprintf(flags, "object #%d", virtual_nr);    /* sprintf: OK (until 399-bit integers) */
        //check_bitvector_names(r.room_flags, room_bits_count, flags, "room");
    } else {
        basic_mud_log("SYSERR: Format error in roomflags/sector type of room #%d", virtual_nr);
        exit(1);
    }

    r.func = nullptr;
    r.contents = nullptr;
    r.people = nullptr;
    r.timed = -1;

    for (i = 0; i < NUM_OF_DIRS; i++)
        r.dir_option[i] = nullptr;

    r.ex_description = nullptr;

    snprintf(buf, sizeof(buf), "SYSERR: Format error in room #%d (expecting D/E/S)", virtual_nr);

    while(true) {
        if (!get_line(fl, line)) {
            basic_mud_log("%s", buf);
            exit(1);
        }
        switch (*line) {
            case 'D':
                setup_dir(fl, virtual_nr, atoi(line + 1));
                break;
            case 'E':
                CREATE(new_descr, struct extra_descr_data, 1);
                new_descr->keyword = fread_string(fl, buf2);
                new_descr->description = fread_string(fl, buf2);
                /* fix for crashes in the editor when formatting
       * - e-descs are assumed to end with a \r\n
       * -- Welcor 09/03
       */
                {
                    char *tmp = strchr(new_descr->description, '\0');
                    if (tmp > new_descr->description && *(tmp - 1) != '\n') {
                        CREATE(tmp, char, strlen(new_descr->description) + 3);
                        sprintf(tmp, "%s\r\n", new_descr->description); /* sprintf ok : size checked above*/
                        free(new_descr->description);
                        new_descr->description = tmp;
                    }
                }
                new_descr->next = r.ex_description;
                r.ex_description = new_descr;
                break;
            case 'S':            /* end of room */
                /* DG triggers -- script is defined after the end of the room */
                letter = fread_letter(fl);
                ungetc(letter, fl);
                while (letter == 'T') {
                    dg_read_trigger(fl, &world[virtual_nr], WLD_TRIGGER);
                    letter = fread_letter(fl);
                    ungetc(letter, fl);
                }
                return;
            default:
                basic_mud_log("%s", buf);
                exit(1);
        }
    }
}

/*
 * "resulve vnums into rnums in the zone reset tables"
 *
 * Or in English: Once all of the zone reset tables have been loaded, we
 * resolve the virtual numbers into real numbers all at once so we don't have
 * to do it repeatedly while the game is running.  This does make adding any
 * room, mobile, or object a little more difficult while the game is running.
 *
 * NOTE 1: Assumes NOWHERE == NOBODY == NOTHING.
 * NOTE 2: Assumes sizeof(room_rnum) >= (sizeof(mob_rnum) and sizeof(obj_rnum))
 */

static void mob_autobalance(struct char_data *ch) {

}

static int parse_simple_mob(FILE *mob_f, struct char_data *ch, mob_vnum nr) {
    int j, t[10];
    char line[READ_SIZE];

    if (!get_line(mob_f, line)) {
        basic_mud_log("SYSERR: Format error in mob #%d, file ended after S flag!", nr);
        return 0;
    }

    if (sscanf(line, " %ld %ld %ld %ldd%ld+%ld %ldd%ld+%ld ",
               t, t + 1, t + 2, t + 3, t + 4, t + 5, t + 6, t + 7, t + 8) != 9) {
        basic_mud_log("SYSERR: Format error in mob #%d, first line after S flag\n"
            "...expecting line of form '# # # #d#+# #d#+#'", nr);
        return 0;
    }

    ch->set(CharNum::Level, t[0]);

    /* max hit = 0 is a flag that H, M, V is xdy+z */
    ch->set(CharStat::PowerLevel, t[3]);
    ch->set(CharStat::Ki, t[4]);
    ch->set(CharStat::Stamina, t[5]);
    ch->health = 1.0;
    ch->energy = 1.0;
    ch->stamina = 1.0;

    ch->mob_specials.damnodice = t[6];
    ch->mob_specials.damsizedice = t[7];
    GET_DAMAGE_MOD(ch) = t[8];

    if (!get_line(mob_f, line)) {
        basic_mud_log("SYSERR: Format error in mob #%d, second line after S flag\n"
            "...expecting line of form '# #', but file ended!", nr);
        return 0;
    }

    if (sscanf(line, " %ld %ld %ld %ld", t, t + 1, t + 2, t + 3) != 4) {
        basic_mud_log("SYSERR: Format error in mob #%d, second line after S flag\n"
            "...expecting line of form '# # # #'", nr);
        return 0;
    }

    ch->set(CharMoney::Carried, t[0]);
    ch->race = static_cast<RaceID>(t[2]);

    ch->chclass = static_cast<SenseiID>(t[3]);
    GET_SAVE_BASE(ch, SAVING_FORTITUDE) = 0;
    GET_SAVE_BASE(ch, SAVING_REFLEX) = 0;
    GET_SAVE_BASE(ch, SAVING_WILL) = 0;

    /* GET_CLASS_RANKS(ch, t[3]) = GET_LEVEL(ch); */

    if (!IS_HUMAN(ch))
        ch->affected_by.set(AFF_INFRAVISION);

    SPEAKING(ch) = SKILL_LANG_COMMON;

    if (!get_line(mob_f, line)) {
        basic_mud_log("SYSERR: Format error in last line of mob #%d\n"
            "...expecting line of form '# # #', but file ended!", nr);
        return 0;
    }

    if (sscanf(line, " %ld %ld %ld ", t, t + 1, t + 2) != 3) {
        basic_mud_log("SYSERR: Format error in last line of mob #%d\n"
            "...expecting line of form '# # #'", nr);
        return 0;
    }

    GET_POS(ch) = t[0];
    GET_DEFAULT_POS(ch) = t[1];
    ch->set(CharAppearance::Sex, t[2]);

    SPEAKING(ch) = MIN_LANGUAGES;
    set_height_and_weight_by_race(ch);

    for (j = 0; j < 3; j++)
        GET_SAVE_MOD(ch, j) = 0;

    if (MOB_FLAGGED(ch, MOB_AUTOBALANCE)) {
        mob_autobalance(ch);
    }

    return 1;
}


/*
 * interpret_espec is the function that takes espec keywords and values
 * and assigns the correct value to the mob as appropriate.  Adding new
 * e-specs is absurdly easy -- just add a new CASE statement to this
 * function!  No other changes need to be made anywhere in the code.
 *
 * CASE		: Requires a parameter through 'value'.
 * BOOL_CASE	: Being specified at all is its value.
 */

#define CASE(test)    \
    if (value && !matched && !strcasecmp(keyword, test) && (matched = false))

#define BOOL_CASE(test)    \
    if (!value && !matched && !strcasecmp(keyword, test) && (matched = true))

#define RANGE(low, high)    \
    (num_arg = MAX((low), MIN((high), (num_arg))))

static void interpret_espec(const char *keyword, const char *value, struct char_data *ch, mob_vnum nr) {
    int num_arg = 0, matched = false;
    int num, num2, num3, num4, num5, num6;
    struct affected_type af;

    /*
   * If there isn't a colon, there is no value.  While Boolean options are
   * possible, we don't actually have any.  Feel free to make some.
  */
    if (value)
        num_arg = atoi(value);

    CASE("BareHandAttack") {
        RANGE(0, 99);
        ch->mob_specials.attack_type = num_arg;
    }

    CASE("Size") {
        RANGE(SIZE_UNDEFINED, NUM_SIZES - 1);
        ch->setSize(num_arg);
    }

    CASE("Str") {
        RANGE(0, 200);
        ch->set(CharAttribute::Strength, num_arg);
    }

    CASE("StrAdd") {
        basic_mud_log("mob #%d trying to set StrAdd, rebalance its strength.",
            GET_MOB_VNUM(ch));
    }

    CASE("Int") {
        RANGE(0, 200);
        ch->set(CharAttribute::Intelligence, num_arg);
    }

    CASE("Wis") {
        RANGE(0, 200);
        ch->set(CharAttribute::Wisdom, num_arg);
    }

    CASE("Dex") {
        RANGE(0, 200);
        ch->set(CharAttribute::Agility, num_arg);
    }

    CASE("Con") {
        RANGE(0, 200);
        ch->set(CharAttribute::Constitution, num_arg);
    }

    CASE("Cha") {
        RANGE(0, 200);
        ch->set(CharAttribute::Speed, num_arg);
    }

    CASE("Hit") {
        RANGE(0, 99999);
        //GET_HIT(ch) = num_arg;
    }

    CASE("Mana") {
        RANGE(0, 99999);
        //GET_MANA(ch) = num_arg;
    }

    CASE("Moves") {
        RANGE(0, 99999);
        //GET_MOVE(ch) = num_arg;
    }

    CASE("Affect") {
        num = num2 = num3 = num4 = num5 = num6 = 0;
        sscanf(value, "%d %d %d %d %d %d", &num, &num2, &num3, &num4, &num5, &num6);
        if (num > 0) {
            af.type = num;
            af.duration = num2;
            af.modifier = num3;
            af.location = num4;
            af.bitvector = num5;
            af.specific = num6;
            affect_to_char(ch, &af);
        }
    }

    CASE("AffectV") {
        num = num2 = num3 = num4 = num5 = num6 = 0;
        sscanf(value, "%d %d %d %d %d %d", &num, &num2, &num3, &num4, &num5, &num6);
        if (num > 0) {
            af.type = num;
            af.duration = num2;
            af.modifier = num3;
            af.location = num4;
            af.bitvector = num5;
            af.specific = num6;
            affectv_to_char(ch, &af);
        }
    }

    CASE("Feat") {
        sscanf(value, "%d %d", &num, &num2);
        HAS_FEAT(ch, num) = num2;
    }

    CASE("Skill") {
        sscanf(value, "%d %d", &num, &num2);
        SET_SKILL(ch, num, num2);
    }

    CASE("SkillMod") {
        sscanf(value, "%d %d", &num, &num2);
        SET_SKILL_BONUS(ch, num, num2);
    }

    if (!matched) {
        basic_mud_log("SYSERR: Warning: unrecognized espec keyword %s in mob #%d",
            keyword, nr);
    }
}

#undef CASE
#undef BOOL_CASE
#undef RANGE

static void parse_espec(char *buf, struct char_data *ch, mob_vnum nr) {
    char *ptr;

    if ((ptr = strchr(buf, ':')) != nullptr) {
        *(ptr++) = '\0';
        while (isspace(*ptr))
            ptr++;
    }
    interpret_espec(buf, ptr, ch, nr);
}

static void mob_stats(struct char_data *mob) {
    int start = GET_LEVEL(mob) * 0.5, finish = GET_LEVEL(mob);

    if (finish < 20)
        finish = 20;

    std::unordered_map<CharAttribute, int> setTo;

    if (!IS_HUMANOID(mob)) {
        setTo[CharAttribute::Strength] = rand_number(start, finish);
        setTo[CharAttribute::Intelligence] = rand_number(start, finish) - 30;
        setTo[CharAttribute::Wisdom] = rand_number(start, finish) - 30;
        setTo[CharAttribute::Agility] = rand_number(start + 5, finish);
        setTo[CharAttribute::Constitution] = rand_number(start + 5, finish);
        setTo[CharAttribute::Speed] = rand_number(start, finish);
    } else {
        if (IS_SAIYAN(mob)) {
            setTo[CharAttribute::Strength] = rand_number(start + 10, finish);
            setTo[CharAttribute::Intelligence] = rand_number(start, finish - 10);
            setTo[CharAttribute::Wisdom] = rand_number(start, finish - 5);
            setTo[CharAttribute::Agility] = rand_number(start, finish);
            setTo[CharAttribute::Constitution] = rand_number(start + 5, finish);
            setTo[CharAttribute::Speed] = rand_number(start + 5, finish);
        } else if (IS_KONATSU(mob)) {
            setTo[CharAttribute::Strength] = rand_number(start, finish - 10);
            setTo[CharAttribute::Intelligence] = rand_number(start, finish);
            setTo[CharAttribute::Wisdom] = rand_number(start, finish);
            setTo[CharAttribute::Agility] = rand_number(start + 10, finish);
            setTo[CharAttribute::Constitution] = rand_number(start, finish);
            setTo[CharAttribute::Speed] = rand_number(start, finish);
        } else if (IS_ANDROID(mob)) {
            setTo[CharAttribute::Strength] = rand_number(start, finish);
            setTo[CharAttribute::Intelligence] = rand_number(start, finish);
            setTo[CharAttribute::Wisdom] = rand_number(start, finish - 10);
            setTo[CharAttribute::Agility] = rand_number(start, finish);
            setTo[CharAttribute::Constitution] = rand_number(start, finish);
            setTo[CharAttribute::Speed] = rand_number(start, finish);
        } else if (IS_MAJIN(mob)) {
            setTo[CharAttribute::Strength] = rand_number(start, finish);
            setTo[CharAttribute::Intelligence] = rand_number(start, finish - 10);
            setTo[CharAttribute::Wisdom] = rand_number(start, finish - 5);
            setTo[CharAttribute::Agility] = rand_number(start, finish);
            setTo[CharAttribute::Constitution] = rand_number(start + 15, finish);
            setTo[CharAttribute::Speed] = rand_number(start, finish);
        } else if (IS_TRUFFLE(mob)) {
            setTo[CharAttribute::Strength] = rand_number(start, finish - 10);
            setTo[CharAttribute::Intelligence] = rand_number(start + 15, finish);
            setTo[CharAttribute::Wisdom] = rand_number(start, finish);
            setTo[CharAttribute::Agility] = rand_number(start, finish);
            setTo[CharAttribute::Constitution] = rand_number(start, finish);
            setTo[CharAttribute::Speed] = rand_number(start, finish);
        } else if (IS_ICER(mob)) {
            setTo[CharAttribute::Strength] = rand_number(start + 5, finish);
            setTo[CharAttribute::Intelligence] = rand_number(start, finish);
            setTo[CharAttribute::Wisdom] = rand_number(start, finish);
            setTo[CharAttribute::Agility] = rand_number(start, finish);
            setTo[CharAttribute::Constitution] = rand_number(start, finish);
            setTo[CharAttribute::Speed] = rand_number(start + 10, finish);
        } else {
            setTo[CharAttribute::Strength] = rand_number(start, finish);
            setTo[CharAttribute::Intelligence] = rand_number(start, finish);
            setTo[CharAttribute::Wisdom] = rand_number(start, finish);
            setTo[CharAttribute::Agility] = rand_number(start, finish);
            setTo[CharAttribute::Constitution] = rand_number(start, finish);
            setTo[CharAttribute::Speed] = rand_number(start, finish);
        }
    }

    for(auto &[attr, val] : setTo) {
        if(val > 100) {
            val = 100;
        } else if(val < 5) {
            val = rand_number(5, 8);
        }
        mob->set(attr, val);
    }
}

static int parse_enhanced_mob(FILE *mob_f, struct char_data *ch, mob_vnum nr) {
    char line[READ_SIZE];

    parse_simple_mob(mob_f, ch, nr);

    while (get_line(mob_f, line)) {
        if (!strcmp(line, "E"))    /* end of the enhanced section */
            return 1;
        else if (*line == '#') {    /* we've hit the next mob, maybe? */
            basic_mud_log("SYSERR: Unterminated E section in mob #%d", nr);
            return 0;
        } else
            parse_espec(line, ch, nr);
    }

    basic_mud_log("SYSERR: Unexpected end of file reached after mob #%d", nr);
    return 0;
}

static int parse_mobile_from_file(FILE *mob_f, struct char_data *ch) {
    int j, t[10], retval;
    char line[READ_SIZE], *tmpptr, letter;
    char f1[128], f2[128], f3[128], f4[128], f5[128], f6[128];
    char f7[128], f8[128], buf2[128];
    mob_vnum nr = ch->vn;
    auto &z = zone_table[real_zone_by_thing(nr)];
    z.mobiles.insert(nr);

    /*
   * Mobiles should NEVER use anything in the 'player_specials' structure.
   * The only reason we have every mob in the game share this copy of the
   * structure is to save newbie coders from themselves. -gg 2/25/98
   */

    sprintf(buf2, "mob vnum %d", nr);   /* sprintf: OK (for 'buf2 >= 19') */

    /***** String data *****/
    ch->setName(fread_string(mob_f, buf2));
    ch->setShortDesc(fread_string(mob_f, buf2));
    tmpptr = (char*)ch->getShortDesc().c_str();
    if (tmpptr && *tmpptr)
        if (!strcasecmp(fname(tmpptr), "a") || !strcasecmp(fname(tmpptr), "an") ||
            !strcasecmp(fname(tmpptr), "the"))
            *tmpptr = LOWER(*tmpptr);
    ch->setRoomDesc(fread_string(mob_f, buf2));
    ch->setRoomDesc(fread_string(mob_f, buf2));

    /* *** Numeric data *** */
    if (!get_line(mob_f, line)) {
        basic_mud_log("SYSERR: Format error after string section of mob #%d\n"
            "...expecting line of form '# # # {S | E}', but file ended!", nr);
        return 0;
    }

    if ((retval = sscanf(line, "%s %s %s %s %s %s %s %s %d %c", f1, f2, f3, f4, f5, f6, f7, f8, t + 2, &letter)) == 10) {
        int taeller;

        bitvector_t mf[4], aff[4];

        mf[0] = asciiflag_conv(f1);
        mf[1] = asciiflag_conv(f2);
        mf[2] = asciiflag_conv(f3);
        mf[3] = asciiflag_conv(f4);
        for (taeller = 0; taeller < AF_ARRAY_MAX; taeller++)
            check_bitvector_names(MOB_FLAGS(ch)[taeller], action_bits_count, buf2, "mobile");
        for(auto i = 0; i < ch->mobFlags.size(); i++) ch->mobFlags.set(i, IS_SET_AR(mf, i));

        aff[0] = asciiflag_conv(f5);
        aff[1] = asciiflag_conv(f6);
        aff[2] = asciiflag_conv(f7);
        aff[3] = asciiflag_conv(f8);
        for(auto i = 0; i < ch->affected_by.size(); i++) ch->affected_by.set(i, IS_SET_AR(aff, i));

        ch->set(CharAlign::GoodEvil, t[2]);

        for (taeller = 0; taeller < AF_ARRAY_MAX; taeller++)
            check_bitvector_names(AFF_FLAGS(ch)[taeller], affected_bits_count, buf2, "mobile affect");
    } else {
        basic_mud_log("SYSERR: Format error after string section of mob #%d\n"
            "...expecting line of form '# # # {S | E}'", nr);
        exit(1);
    }

    ch->mobFlags.set(MOB_ISNPC);
    if (MOB_FLAGGED(ch, MOB_NOTDEADYET)) {
        /* Rather bad to load mobiles with this bit already set. */
        basic_mud_log("SYSERR: Mob #%d has reserved bit MOB_NOTDEADYET set.", nr);
        ch->mobFlags.reset(MOB_NOTDEADYET);
    }

    /* AGGR_TO_ALIGN is ignored if the mob is AGGRESSIVE.
  if (MOB_FLAGGED(mob_proto + i, MOB_AGGRESSIVE) && MOB_FLAGGED(mob_proto + i, MOB_AGGR_GOOD | MOB_AGGR_EVIL | MOB_AGGR_NEUTRAL))
    log("SYSERR: Mob #%d both Aggressive and Aggressive_to_Alignment.", nr); */

    /* Convert mobs to use AUTOBALANCE. Uncomment and reboot to flag all mobs AUTOBALANCE.
   * if (!MOB_FLAGGED(ch, MOB_AUTOBALANCE)) {
   *   SET_BIT_AR(MOB_FLAGS(ch), MOB_AUTOBALANCE);
   * } */

    switch (UPPER(letter)) {
        case 'S':    /* Simple monsters */
            parse_simple_mob(mob_f, ch, nr);
            break;
        case 'E':    /* Circle3 Enhanced monsters */
            parse_enhanced_mob(mob_f, ch, nr);
            mob_stats(ch);
            break;
            /* add new mob types here.. */
        default:
            basic_mud_log("SYSERR: Unsupported mob type '%c' in mob #%d", letter, nr);
            exit(1);
    }

    /* DG triggers -- script info follows mob S/E section */
    letter = fread_letter(mob_f);
    ungetc(letter, mob_f);
    while (letter == 'T') {
        dg_read_trigger(mob_f, ch, MOB_TRIGGER);
        letter = fread_letter(mob_f);
        ungetc(letter, mob_f);
    }

    for (j = 0; j < NUM_WEARS; j++)
        ch->equipment[j] = nullptr;

    /* Uncomment to force all mob files to be rewritten. Good for initial AUTOBALANCE setup.
   * if (bitsavetodisk) {
   *   add_to_save_list(zone_table[real_zone_by_thing(nr)].number, 0);
   *   converting = TRUE;
   * } */

    return 1;
}


static void parse_mobile(FILE *mob_f, mob_vnum nr) {
    auto &idx = mob_index[nr];
    idx.vn = nr;

    auto &m = mob_proto[nr];

    m.vn = nr;
    m.desc = nullptr;

    if (parse_mobile_from_file(mob_f, &m)) {

    } else { /* We used to exit in the file reading code, but now we do it here */
        exit(1);
    }
}


/* read all objects from obj file; generate index and prototypes */
static char *parse_object(FILE *obj_f, obj_vnum nr) {
    static char line[READ_SIZE];
    int64_t t[NUM_OBJ_VAL_POSITIONS + 2], j, retval;
    char *tmpptr, buf2[128];
    char f1[READ_SIZE], f2[READ_SIZE], f3[READ_SIZE], f4[READ_SIZE];
    char f5[READ_SIZE], f6[READ_SIZE], f7[READ_SIZE], f8[READ_SIZE];
    char f9[READ_SIZE], f10[READ_SIZE], f11[READ_SIZE], f12[READ_SIZE];
    struct extra_descr_data *new_descr;

    auto &o = obj_proto[nr];
    auto &idx = obj_index[nr];

    idx.vn = nr;
    o.vn = nr;

    sprintf(buf2, "object #%d", nr);    /* sprintf: OK (for 'buf2 >= 19') */

    /* *** string data *** */
    if ((o.name = fread_string(obj_f, buf2)) == nullptr) {
        basic_mud_log("SYSERR: Null obj name or format error at or near %s", buf2);
        exit(1);
    }
    auto &z = zone_table[real_zone_by_thing(nr)];
    z.objects.insert(nr);
    o.setShortDesc(fread_string(obj_f, buf2));
    tmpptr = (char*)o.getShortDesc().c_str();
    if (tmpptr && *tmpptr)
        if (!strcasecmp(fname(tmpptr), "a") || !strcasecmp(fname(tmpptr), "an") ||
            !strcasecmp(fname(tmpptr), "the"))
            *tmpptr = LOWER(*tmpptr);
    o.setRoomDesc(fread_string(obj_f, buf2));
    tmpptr = (char*)o.getRoomDesc().c_str();
    if (tmpptr && *tmpptr)
        CAP(tmpptr);
    o.look_description = fread_string(obj_f, buf2);

    /* *** numeric data *** */
    if (!get_line(obj_f, line)) {
        basic_mud_log("SYSERR: Expecting first numeric line of %s, but file ended!", buf2);
        exit(1);
    }
    if ((retval = sscanf(line, " %d %s %s %s %s %s %s %s %s %s %s %s %s", t, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10,
                                f11, f12)) == 13) {
        bitvector_t extraFlags[4], wearFlags[4], permFlags[4];

        extraFlags[0] = asciiflag_conv(f1);
        extraFlags[1] = asciiflag_conv(f2);
        extraFlags[2] = asciiflag_conv(f3);
        extraFlags[3] = asciiflag_conv(f4);
        for(auto i = 0; i < o.extra_flags.size(); i++) if(IS_SET_AR(extraFlags, i)) o.extra_flags.set(i);

        wearFlags[0] = asciiflag_conv(f5);
        wearFlags[1] = asciiflag_conv(f6);
        wearFlags[2] = asciiflag_conv(f7);
        wearFlags[3] = asciiflag_conv(f8);
        for(auto i = 0; i < o.wear_flags.size(); i++) if(IS_SET_AR(wearFlags, i)) o.wear_flags.set(i);

        permFlags[0] = asciiflag_conv(f9);
        permFlags[1] = asciiflag_conv(f10);
        permFlags[2] = asciiflag_conv(f11);
        permFlags[3] = asciiflag_conv(f12);
        for(auto i = 0; i < o.bitvector.size(); i++) if(IS_SET_AR(permFlags, i)) o.bitvector.set(i);

    } else {
        basic_mud_log("SYSERR: Format error in first numeric line (expecting 13 args, got %d), %s", retval, buf2);
        exit(1);
    }

    /* Object flags checked in check_object(). */
    GET_OBJ_TYPE(&o) = t[0];

    if (!get_line(obj_f, line)) {
        basic_mud_log("SYSERR: Expecting second numeric line of %s, but file ended!", buf2);
        exit(1);
    }

    for (j = 0; j < NUM_OBJ_VAL_POSITIONS; j++)
        t[j] = 0;

    if ((retval = sscanf(line, "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld", t, t + 1, t + 2, t + 3, t + 4, t + 5,
                         t + 6, t + 7, t + 8, t + 9, t + 10, t + 11, t + 12, t + 13, t + 14, t + 15)) >
        NUM_OBJ_VAL_POSITIONS) {
        basic_mud_log("SYSERR: Format error in second numeric line (expecting <=%d args, got %d), %s", NUM_OBJ_VAL_POSITIONS,
            retval, buf2);
        exit(1);
    }

    for (j = 0; j < NUM_OBJ_VAL_POSITIONS; j++)
        GET_OBJ_VAL(&o, j) = t[j];

    if ((GET_OBJ_TYPE(&o) == ITEM_PORTAL || \
       GET_OBJ_TYPE(&o) == ITEM_HATCH) && \
       (!GET_OBJ_VAL(&o, VAL_DOOR_DCLOCK) || \
        !GET_OBJ_VAL(&o, VAL_DOOR_DCHIDE))) {
        GET_OBJ_VAL(&o, VAL_DOOR_DCLOCK) = 20;
        GET_OBJ_VAL(&o, VAL_DOOR_DCHIDE) = 20;
        if (bitsavetodisk) {
            converting = true;
        }
    }

    /* Convert old CWG-SunTzu style armor values to CWG-Rasputin. Should no longer be needed I think.
   * if (GET_OBJ_TYPE(obj_proto + i) == ITEM_ARMOR) {
   *   if (suntzu_armor_convert(obj_proto + i)) {
   *     if(bitsavetodisk) {
   *       add_to_save_list(zone_table[real_zone_by_thing(nr)].number, 1);
   *       converting = TRUE;
   *     }
   *   }
   * }*/

    if (!get_line(obj_f, line)) {
        basic_mud_log("SYSERR: Expecting third numeric line of %s, but file ended!", buf2);
        exit(1);
    }
    if ((retval = sscanf(line, "%ld %ld %ld %ld", t, t + 1, t + 2, t + 3)) != 4) {
        if (retval == 3)
            t[3] = 0;
        else {
            basic_mud_log("SYSERR: Format error in third numeric line (expecting 4 args, got %d), %s", retval, buf2);
            exit(1);
        }
    }
    GET_OBJ_WEIGHT(&o) = t[0];
    GET_OBJ_COST(&o) = t[1];
    GET_OBJ_RENT(&o) = t[2];
    GET_OBJ_LEVEL(&o) = t[3];
    GET_OBJ_SIZE(&o) = SIZE_MEDIUM;

    /* check to make sure that weight of containers exceeds curr. quantity */
    if (GET_OBJ_TYPE(&o) == ITEM_DRINKCON ||
        GET_OBJ_TYPE(&o) == ITEM_FOUNTAIN) {
        if (GET_OBJ_WEIGHT(&o) < GET_OBJ_VAL(&o, 1))
            GET_OBJ_WEIGHT(&o) = GET_OBJ_VAL(&o, 1) + 5;
    }
    /* *** make sure portal objects have their timer set correctly *** */
    if (GET_OBJ_TYPE(&o) == ITEM_PORTAL) {
        GET_OBJ_TIMER(&o) = -1;
    }

    /* *** extra descriptions and affect fields *** */

    for (j = 0; j < MAX_OBJ_AFFECT; j++) {
        o.affected[j].location = APPLY_NONE;
        o.affected[j].modifier = 0;
        o.affected[j].specific = 0;
    }

    strcat(buf2, ", after numeric constants\n"    /* strcat: OK (for 'buf2 >= 87') */
                 "...expecting 'E', 'A', '$', or next object number");
    j = 0;

    for (;;) {
        if (!get_line(obj_f, line)) {
            basic_mud_log("SYSERR: Format error in %s", buf2);
            exit(1);
        }
        switch (*line) {
            case 'E':
                CREATE(new_descr, struct extra_descr_data, 1);
                new_descr->keyword = fread_string(obj_f, buf2);
                new_descr->description = fread_string(obj_f, buf2);
                new_descr->next = o.ex_description;
                o.ex_description = new_descr;
                break;
            case 'A':
                if (j >= MAX_OBJ_AFFECT) {
                    basic_mud_log("SYSERR: Too many A fields (%d max), %s", MAX_OBJ_AFFECT, buf2);
                    exit(1);
                }
                if (!get_line(obj_f, line)) {
                    basic_mud_log("SYSERR: Format error in 'A' field, %s\n"
                        "...expecting 2 numeric constants but file ended!", buf2);
                    exit(1);
                }

                t[1] = 0;
                if ((retval = sscanf(line, " %ld %ld %ld ", t, t + 1, t + 2)) != 3) {
                    if (retval != 2) {
                        basic_mud_log("SYSERR: Format error in 'A' field, %s\n"
                            "...expecting 2 numeric arguments, got %d\n"
                            "...offending line: '%s'", buf2, retval, line);
                        exit(1);
                    }
                }

                if (t[0] >= APPLY_DAMAGE_PERC && t[0] <= APPLY_DEFENSE_PERC) {
                    basic_mud_log("Warning: object #%d (%s) uses deprecated saving throw applies",
                        nr, GET_OBJ_SHORT(&o));
                }
                o.affected[j].location = t[0];
                o.affected[j].modifier = t[1];
                o.affected[j].specific = t[2];
                j++;
                break;
            case 'S':  /* Spells for Spellbooks*/
                if (j >= SPELLBOOK_SIZE) {
                    basic_mud_log("SYSERR: Unknown spellbook slot in S field, %s", buf2);
                    exit(1);
                }
                if (!get_line(obj_f, line)) {
                    basic_mud_log("SYSERR: Format error in 'S' field, %s\n"
                        "...expecting 2 numeric constants but file ended!", buf2);
                    exit(1);
                }

                if ((retval = sscanf(line, " %d %d ", t, t + 1)) != 2) {
                    basic_mud_log("SYSERR: Format error in 'S' field, %s\n"
                        "...expecting 2 numeric arguments, got %d\n"
                        "...offending line: '%s'", buf2, retval, line);
                    exit(1);
                }
                if (!o.sbinfo) {
                    CREATE(o.sbinfo, struct obj_spellbook_spell, SPELLBOOK_SIZE);
                }
                o.sbinfo[j].spellname = t[0];
                o.sbinfo[j].pages = t[1];
                j++;
                break;
            case 'T':  /* DG triggers */
                dg_obj_trigger(line, &o);
                break;
            case 'Z':
                if (!get_line(obj_f, line)) {
                    basic_mud_log("SYSERR: Format error in 'Z' field, %s\n"
                        "...expecting numeric constant but file ended!", buf2);
                    exit(1);
                }
                if (sscanf(line, "%d", t) != 1) {
                    basic_mud_log("SYSERR: Format error in 'Z' field, %s\n"
                        "...expecting numeric argument\n"
                        "...offending line: '%s'", buf2, line);
                    exit(1);
                }
                GET_OBJ_SIZE(&o) = t[0];
                break;
            case '$':
            case '#':
                /* Objects that set CHARM on players are bad. */
                if (OBJAFF_FLAGGED(&o, AFF_CHARM)) {
                    basic_mud_log("SYSERR: Object #%d has reserved bit AFF_CHARM set.", nr);
                    o.bitvector.reset(AFF_CHARM);
                }
                check_object(&o);
                return (line);
            default:
                basic_mud_log("SYSERR: Format error in (%c): %s", *line, buf2);
                exit(1);
        }
    }
    return line;
}

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

/* function to count how many hash-mark delimited records exist in a file */
static int count_hash_records(FILE *fl) {
    char buf[128];
    int count = 0;

    while (fgets(buf, 128, fl))
        if (*buf == '#')
            count++;

    return (count);
}

/* load the zone table and command tables */
static void load_zones(FILE *fl, char *zonename) {
    int cmd_no, num_of_cmds = 0, line_num = 0, tmp, error, arg_num, version = 1;
    char *ptr, buf[READ_SIZE], zname[READ_SIZE], buf2[MAX_STRING_LENGTH];
    int zone_fix = false;
    char t1[80], t2[80], line[MAX_STRING_LENGTH];

    strlcpy(zname, zonename, sizeof(zname));

    line_num += get_line(fl, buf);

    if (*buf == '@') {
        if (sscanf(buf, "@Version: %d", &version) != 1) {
            basic_mud_log("SYSERR: Format error in %s (version)", zname);
            basic_mud_log("SYSERR: ...Line: %s", line);
            exit(1);
        }
        line_num += get_line(fl, buf);
    }
    zone_vnum v;

    if (sscanf(buf, "#%hd", &v) != 1) {
        basic_mud_log("SYSERR: FFFFFF Format error in %s, line %d", zname, line_num);
        exit(1);
    }
    snprintf(buf2, sizeof(buf2)-1, "beginning of zone #%d", v);

    auto &z = zone_table[v];
    z.number = v;

    line_num += get_line(fl, buf);
    if ((ptr = strchr(buf, '~')) != nullptr)    /* take off the '~' if it's there */
        *ptr = '\0';
    z.builders = strdup(buf);

    line_num += get_line(fl, buf);
    if ((ptr = strchr(buf, '~')) != nullptr)    /* take off the '~' if it's there */
        *ptr = '\0';
    z.name = strdup(buf);

    line_num += get_line(fl, buf);
    if (version >= 2) {

        char zbuf1[MAX_STRING_LENGTH];
        char zbuf2[MAX_STRING_LENGTH];
        char zbuf3[MAX_STRING_LENGTH];
        char zbuf4[MAX_STRING_LENGTH];

        if (sscanf(buf, " %hd %hd %d %d %s %s %s %s %d %d", &z.bot, &z.top, &z.lifespan,
                   &z.reset_mode, zbuf1, zbuf2, zbuf3, zbuf4, &z.min_level, &z.max_level) != 10) {
            basic_mud_log("SYSERR: Format error in 10-constant line of %s", zname);
            exit(1);
        }

        z.zone_flags[0] = asciiflag_conv(zbuf1);
        z.zone_flags[1] = asciiflag_conv(zbuf2);
        z.zone_flags[2] = asciiflag_conv(zbuf3);
        z.zone_flags[3] = asciiflag_conv(zbuf4);

    } else if (sscanf(buf, " %hd %hd %d %d ", &z.bot, &z.top, &z.lifespan, &z.reset_mode) != 4) {
        /*
     * This may be due to the fact that the zone has no builder.  So, we just attempt
     * to fix this by copying the previous 2 last reads into this variable and the
     * last one.
     */
        basic_mud_log("SYSERR: Format error in numeric constant line of %s, attempting to fix.", zname);
        if (sscanf(z.name, " %hd %hd %d %d ", &z.bot, &z.top, &z.lifespan, &z.reset_mode) != 4) {
            basic_mud_log("SYSERR: Could not fix previous error, aborting game.");
            exit(1);
        } else {
            free(z.name);
            z.name = strdup(z.builders);
            free(z.builders);
            z.builders = strdup("None.");
            zone_fix = true;
        }
    }
    if (z.bot > z.top) {
        basic_mud_log("SYSERR: Zone %d bottom (%d) > top (%d).", z.number, z.bot, z.top);
        exit(1);
    }

    for (auto c = 0;true;c++) {
        get_line(fl, buf);
        if(buf[0] == '*') continue;

        if(strchr("$S", buf[0])) {
            break;
        }

        auto &zc = z.cmd.emplace_back();
        zc.command = buf[0];

        if(zc.command == 'V') { /* a string-arg command */
            if (sscanf(&buf[1], " %d %d %d %d %d %d %79s %79[^\f\r\n\t\v]", &tmp,&zc.arg1, &zc.arg2, &zc.arg3,
                       &zc.arg4, &zc.arg5, t1, t2) != 8)
                error = 1;
            else {
                zc.sarg1 = t1;
                zc.sarg2 = t2;
            }
        } else {
            if ((arg_num = sscanf(&buf[1], " %d %d %d %d %d %d ", &tmp, &zc.arg1, &zc.arg2, &zc.arg3, &zc.arg4,
                                  &zc.arg5)) != 6) {
                if (arg_num != 5) {
                    error = 1;
                } else {
                    zc.arg5 = 0;
                }
            }
        }

        zc.if_flag = tmp;

        if (error) {
            basic_mud_log("SYSERR: Format error in %s, line %d: '%s'", zname, c, buf);
            exit(1);
        }
        zc.line = c;
    }
}

static void discrete_load(FILE *fl, int mode, char *filename) {
    int nr = -1, last;
    char line[READ_SIZE];

    const char *modes[] = {"world", "mob", "obj", "ZON", "SHP", "HLP", "trg"};
    /* modes positions correspond to DB_BOOT_xxx in db.h */

    for (;;) {
        /*
     * we have to do special processing with the obj files because they have
     * no end-of-record marker :(
     */
        if (mode != DB_BOOT_OBJ || nr < 0)
            if (!get_line(fl, line)) {
                if (nr == -1) {
                    basic_mud_log("SYSERR: %s file %s is empty!", modes[mode], filename);
                } else {
                    basic_mud_log("SYSERR: Format error in %s after %s #%d\n"
                        "...expecting a new %s, but file ended!\n"
                        "(maybe the file is not terminated with '$'?)", filename,
                        modes[mode], nr, modes[mode]);
                }
                exit(1);
            }
        if (*line == '$')
            return;

        if (*line == '#') {
            last = nr;
            if (sscanf(line, "#%d", &nr) != 1) {
                basic_mud_log("SYSERR: Format error after %s #%d", modes[mode], last);
                exit(1);
            }
            if (nr >= 99999)
                return;
            else
                switch (mode) {
                    case DB_BOOT_WLD:
                        parse_room(fl, nr);
                        break;
                    case DB_BOOT_MOB:
                        parse_mobile(fl, nr);
                        break;
                    case DB_BOOT_TRG:
                        parse_trigger(fl, nr);
                        break;
                    case DB_BOOT_OBJ:
                        strlcpy(line, parse_object(fl, nr), sizeof(line));
                        break;
                }
        } else {
            basic_mud_log("SYSERR: Format error in %s file %s near %s #%d", modes[mode],
                filename, modes[mode], nr);
            basic_mud_log("SYSERR: ... offending line: '%s'", line);
            exit(1);
        }
    }
}


static void index_boot(int mode) {
    const char *index_filename, *prefix = nullptr;    /* nullptr or egcs 1.1 complains */
    FILE *db_index, *db_file;
    int rec_count = 0, size[2];
    char buf2[PATH_MAX], buf1[MAX_STRING_LENGTH];

    switch (mode) {
        case DB_BOOT_WLD:
            prefix = WLD_PREFIX;
            break;
        case DB_BOOT_MOB:
            prefix = MOB_PREFIX;
            break;
        case DB_BOOT_OBJ:
            prefix = OBJ_PREFIX;
            break;
        case DB_BOOT_ZON:
            prefix = ZON_PREFIX;
            break;
        case DB_BOOT_SHP:
            prefix = SHP_PREFIX;
            break;
        case DB_BOOT_HLP:
            prefix = HLP_PREFIX;
            break;
        case DB_BOOT_TRG:
            prefix = TRG_PREFIX;
            break;
        case DB_BOOT_GLD:
            prefix = GLD_PREFIX;
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
            if (mode == DB_BOOT_ZON)
                rec_count++;
            else if (mode == DB_BOOT_HLP)
                rec_count += count_alias_records(db_file);
            else
                rec_count += count_hash_records(db_file);
        }

        fclose(db_file);
        fscanf(db_index, "%s\n", buf1);
    }

    /* Exit if 0 records, unless this is shops */
    if (!rec_count) {
        if (mode == DB_BOOT_SHP || mode == DB_BOOT_GLD)
            return;
        basic_mud_log("SYSERR: boot error - 0 records counted in %s/%s.", prefix,
            index_filename);
        exit(1);
    }

    /*
   * NOTE: "bytes" does _not_ include strings or other later malloc'd things.
   */
    switch (mode) {
        case DB_BOOT_TRG:
            break;
        case DB_BOOT_WLD:
            size[0] = sizeof(struct room_data) * rec_count;
            basic_mud_log("   %d rooms, %d bytes.", rec_count, size[0]);
            break;
        case DB_BOOT_MOB:
            size[0] = sizeof(struct index_data) * rec_count;
            size[1] = sizeof(struct char_data) * rec_count;
            basic_mud_log("   %d mobs, %d bytes in index, %d bytes in prototypes.", rec_count, size[0], size[1]);
            break;
        case DB_BOOT_OBJ:
            size[0] = sizeof(struct index_data) * rec_count;
            size[1] = sizeof(struct obj_data) * rec_count;
            basic_mud_log("   %d objs, %d bytes in index, %d bytes in prototypes.", rec_count, size[0], size[1]);
            break;
        case DB_BOOT_ZON:
            size[0] = sizeof(struct zone_data) * rec_count;
            basic_mud_log("   %d zones, %d bytes.", rec_count, size[0]);
            break;
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
            case DB_BOOT_WLD:
            case DB_BOOT_OBJ:
            case DB_BOOT_MOB:
            case DB_BOOT_TRG:
                discrete_load(db_file, mode, buf2);
                break;
            case DB_BOOT_ZON:
                load_zones(db_file, buf2);
                break;
            case DB_BOOT_HLP:
                load_help(db_file, buf2);
                break;
            case DB_BOOT_SHP:
                boot_the_shops(db_file, buf2, rec_count);
                break;
            case DB_BOOT_GLD:
                boot_the_guilds(db_file, buf2, rec_count);
                break;
        }

        fclose(db_file);
        fscanf(db_index, "%s\n", buf1);
    }
    fclose(db_index);
}


/* remove ^M's from file output */
/* There may be a similar function in Oasis (and I'm sure
   it's part of obuild).  Remove this if you get a
   multiple definition error or if it you want to use a
   substitute
*/
static void kill_ems(char *str) {
    char *ptr1, *ptr2, *tmp;

    tmp = str;
    ptr1 = str;
    ptr2 = str;

    while (*ptr1) {
        if ((*(ptr2++) = *(ptr1++)) == '\r')
            if (*ptr1 == '\r')
                ptr1++;
    }
    *ptr2 = '\0';
}


/* Separate a 4-character id tag from the data it precedes */
static void tag_argument(char *argument, char *tag) {
    char *tmp = argument, *ttag = tag, *wrt = argument;
    int i;

    for (i = 0; i < 4; i++)
        *(ttag++) = *(tmp++);
    *ttag = '\0';

    while (*tmp == ':' || *tmp == ' ')
        tmp++;

    while (*tmp)
        *(wrt++) = *(tmp++);
    *wrt = '\0';
}


static void load_affects(FILE *fl, struct char_data *ch, int violence) {
    int num, num2, num3, num4, num5, num6, i;
    char line[MAX_INPUT_LENGTH + 1];
    struct affected_type af;

    i = 0;
    do {
        get_line(fl, line);
        num = num2 = num3 = num4 = num5 = num6 = 0;
        sscanf(line, "%d %d %d %d %d %d", &num, &num2, &num3, &num4, &num5, &num6);
        if (num != 0) {
            af.type = num;
            af.duration = num2;
            af.modifier = num3;
            af.location = num4;
            af.bitvector = num5;
            af.specific = num6;
            if (violence)
                affectv_to_char(ch, &af);
            else
                affect_to_char(ch, &af);
            i++;
        }
    } while (num != 0);
}


static void load_skills(FILE *fl, struct char_data *ch, bool mods) {
    int num = 0, num2 = 0, num3 = 0;
    char line[MAX_INPUT_LENGTH + 1];

    do {
        get_line(fl, line);
        sscanf(line, "%d %d %d", &num, &num2, &num3);
        if (num != 0) {
            if (mods) {
                SET_SKILL_BONUS(ch, num, num2);
            } else {
                SET_SKILL(ch, num, num2);
            }
            SET_SKILL_PERF(ch, num, num3);
        }
    } while (num != 0);
}

static void load_bonuses(FILE *fl, struct char_data *ch, bool mods) {
    int num[52] = {0}, i;
    char line[MAX_INPUT_LENGTH + 1];

    get_line(fl, line);
    sscanf(line,
           "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
           &num[0], &num[1], &num[2], &num[3], &num[4], &num[5], &num[6], &num[7], &num[8], &num[9], &num[10], &num[11],
           &num[12], &num[13], &num[14], &num[15], &num[16], &num[17], &num[18], &num[19], &num[20], &num[21], &num[22],
           &num[23], &num[24], &num[25], &num[26], &num[27], &num[28], &num[29], &num[30], &num[31], &num[32], &num[33],
           &num[34], &num[35], &num[36], &num[37], &num[38], &num[39], &num[40], &num[41], &num[42], &num[43], &num[44],
           &num[45], &num[46], &num[47], &num[48], &num[49], &num[50], &num[51]);
    for (i = 0; i < 52; i++) {
        if (num[i] > 0) {
            GET_BONUS(ch, i) = num[i];
        }
    }
}

#define LOAD_HIT    0
#define LOAD_MANA    1
#define LOAD_MOVE    2
#define LOAD_KI        3
#define LOAD_LIFE       4

static void load_HMVS(struct char_data *ch, const char *line, int mode) {
    int64_t num = 0, num2 = 0;

    sscanf(line, "%" I64T "/%" I64T "", &num, &num2);

    switch (mode) {
        case LOAD_HIT:
            //GET_HIT(ch) = num;
            break;

        case LOAD_MANA:
            //GET_MANA(ch) = num;
            break;

        case LOAD_MOVE:
            //GET_MOVE(ch) = num;
            break;

        case LOAD_KI:
            //GET_KI(ch) = num;
            break;
    }
}

static void load_BASE(struct char_data *ch, const char *line, int mode) {
    int64_t num = 0;

    sscanf(line, "%" I64T "", &num);

    switch (mode) {
        case LOAD_HIT:
            ch->set(CharStat::PowerLevel, num);
            break;

        case LOAD_MANA:
            ch->set(CharStat::Ki, num);
            break;

        case LOAD_MOVE:
            ch->set(CharStat::Stamina, num);
            break;

        case LOAD_LIFE:
            //GET_LIFEFORCE(ch) = num;
            break;
    }
}

static void load_majin(struct char_data *ch, const char *line) {
    int64_t num = 0;

    sscanf(line, "%" I64T "", &num);
    GET_MAJINIZED(ch) = num;

}

static void load_molt(struct char_data *ch, const char *line) {
    int64_t num = 0;

    sscanf(line, "%" I64T "", &num);
    GET_MOLT_EXP(ch) = num;

}

/* new load_char reads ASCII Player Files */
/* Load a char, TRUE if loaded, FALSE if not */
static int load_char(const char *name, struct char_data *ch) {
    int id = 0, i, num = 0, num2 = 0, num3 = 0;
    FILE *fl = nullptr;
    char fname[READ_SIZE];
    char buf[128], buf2[128], line[MAX_INPUT_LENGTH], tag[6];
    char f1[128], f2[128], f3[128], f4[128];

    if (!get_filename(fname, sizeof(fname), PLR_FILE, name))
        return (-1);
    else {
        if (!(fl = fopen(fname, "r"))) {
            mudlog(NRM, ADMLVL_GOD, true, "SYSERR: Couldn't open player file %s", fname);
            return (-1);
        }

        /* character initializations */
        /* initializations necessary to keep some things straight */
        ch->affected = nullptr;
        ch->affectedv = nullptr;
        for (i = 1; i <= SKILL_TABLE_SIZE; i++) {
            SET_SKILL(ch, i, 0);
            SET_SKILL_BONUS(ch, i, 0);
            SET_SKILL_PERF(ch, i, 0);
        }

        GET_LOG_USER(ch) = strdup("NOUSER");
        GET_SUPPRESS(ch) = PFDEF_SKIN;
        GET_FURY(ch) = PFDEF_HAIRL;
        GET_CLAN(ch) = strdup("None.");
        GET_HOME(ch) = PFDEF_HOMETOWN;
        GET_WEIGHT(ch) = PFDEF_WEIGHT;

        ch->health = 1.0;
        GET_RELAXCOUNT(ch) = PFDEF_EYE;
        GET_BLESSLVL(ch) = PFDEF_HEIGHT;
        ch->life = 1.0;
        // GET_LIFEFORCE(ch) = PFDEF_BASEPL;
        GET_LIFEPERC(ch) = PFDEF_WEIGHT;
        GET_STUPIDKISS(ch) = 0;
        GET_POS(ch) = POS_STANDING;
        GET_MAJINIZED(ch) = PFDEF_BASEPL;
        GET_GAUNTLET(ch) = PFDEF_GAUNTLET;
        ch->energy = 1.0;
        ch->stamina = 1.0;


        GET_LOADROOM(ch) = PFDEF_LOADROOM;
        GET_INVIS_LEV(ch) = PFDEF_INVISLEV;
        GET_FREEZE_LEV(ch) = PFDEF_FREEZELEV;
        GET_WIMP_LEV(ch) = PFDEF_WIMPLEV;

        GET_OLC_ZONE(ch) = PFDEF_OLC;

        ch->time.birth = ch->time.created = ch->time.maxage = 0;
        ch->followers = nullptr;

        while (get_line(fl, line)) {
            bitvector_t flags[4];
            tag_argument(line, tag);

            switch (*tag) {
                case 'A':
                    if (!strcmp(tag, "Ac  ")) ;
                    else if (!strcmp(tag, "Act ")) {
                        sscanf(line, "%s %s %s %s", f1, f2, f3, f4);
                        flags[0] = asciiflag_conv(f1);
                        flags[1] = asciiflag_conv(f2);
                        flags[2] = asciiflag_conv(f3);
                        flags[3] = asciiflag_conv(f4);
                        for(auto f = 0; f < ch->playerFlags.size(); f++) {
                            if(IS_SET_AR(flags, f)) ch->playerFlags.set(f);
                        }
                    } else if (!strcmp(tag, "Aff ")) {
                        sscanf(line, "%s %s %s %s", f1, f2, f3, f4);
                        flags[0] = asciiflag_conv(f1);
                        flags[1] = asciiflag_conv(f2);
                        flags[2] = asciiflag_conv(f3);
                        flags[3] = asciiflag_conv(f4);
                        for(auto f = 0; f < ch->affected_by.size(); f++) {
                            if(IS_SET_AR(flags, f)) ch->affected_by.set(f);
                        }
                    } else if (!strcmp(tag, "Affs")) load_affects(fl, ch, 0);
                    else if (!strcmp(tag, "Affv")) load_affects(fl, ch, 1);
                    else if (!strcmp(tag, "AdmL")) ch->set(CharNum::AdmLevel, atoi(line));
                    else if (!strcmp(tag, "Abso")) GET_ABSORBS(ch) = atoi(line);
                    else if (!strcmp(tag, "AdmF")) {
                        sscanf(line, "%s %s %s %s", f1, f2, f3, f4);
                        flags[0] = asciiflag_conv(f1);
                        flags[1] = asciiflag_conv(f2);
                        flags[2] = asciiflag_conv(f3);
                        flags[3] = asciiflag_conv(f4);
                        for(auto f = 0; f < ch->admflags.size(); f++) {
                            if(IS_SET_AR(flags, f)) ch->admflags.set(f);
                        }
                    } else if (!strcmp(tag, "Alin")) ch->set(CharAlign::GoodEvil, atoi(line));
                    else if (!strcmp(tag, "Aura")) ch->set(CharAppearance::Aura, atoi(line));
                    break;

                case 'B':
                    if (!strcmp(tag, "Bank")) ch->set(CharMoney::Bank, atoi(line));
                    else if (!strcmp(tag, "Bki ")) load_BASE(ch, line, LOAD_MANA);
                    else if (!strcmp(tag, "Blss")) GET_BLESSLVL(ch) = atoi(line);
                    else if (!strcmp(tag, "Boam")) GET_BOARD(ch, 0) = atoi(line);
                    else if (!strcmp(tag, "Boai")) GET_BOARD(ch, 1) = atoi(line);
                    else if (!strcmp(tag, "Boac")) GET_BOARD(ch, 2) = atoi(line);
                    else if (!strcmp(tag, "Boad")) GET_BOARD(ch, 3) = atoi(line);
                    else if (!strcmp(tag, "Boab")) GET_BOARD(ch, 4) = atoi(line);
                    else if (!strcmp(tag, "Bonu")) load_bonuses(fl, ch, false);
                    else if (!strcmp(tag, "Boos")) GET_BOOSTS(ch) = atoi(line);
                    else if (!strcmp(tag, "Bpl ")) load_BASE(ch, line, LOAD_HIT);
                    else if (!strcmp(tag, "Brth")) ch->time.birth = atol(line);
                    else if (!strcmp(tag, "Bst ")) load_BASE(ch, line, LOAD_MOVE);
                    break;

                case 'C':
                    if (!strcmp(tag, "Cha ")) ch->set(CharAttribute::Speed, atoi(line));
                    else if (!strcmp(tag, "Clan")) GET_CLAN(ch) = strdup(line);
                    else if (!strcmp(tag, "Clar")) GET_CRANK(ch) = atoi(line);
                    else if (!strcmp(tag, "Clas"))
                        ch->chclass = static_cast<SenseiID>(std::min(14, atoi(line)));
                    else if (!strcmp(tag, "Colr")) {
                        sscanf(line, "%d %s", &num, buf2);
                    } else if (!strcmp(tag, "Con ")) ch->set(CharAttribute::Constitution, atoi(line));
                    else if (!strcmp(tag, "Cool")) GET_COOLDOWN(ch) = atoi(line);
                    else if (!strcmp(tag, "Crtd")) ch->time.created = atol(line);
                    break;

                case 'D':
                    if (!strcmp(tag, "Deat")) GET_DTIME(ch) = atoi(line);
                    else if (!strcmp(tag, "Deac")) GET_DCOUNT(ch) = atoi(line);
                    else if (!strcmp(tag, "Desc")) ch->look_description = fread_string(fl, buf2);
                    else if (!strcmp(tag, "Dex ")) ch->set(CharAttribute::Agility, atoi(line));
                    else if (!strcmp(tag, "Drnk")) GET_COND(ch, DRUNK) = atoi(line);
                    else if (!strcmp(tag, "Damg")) GET_DAMAGE_MOD(ch) = atoi(line);
                    else if (!strcmp(tag, "Droo")) GET_DROOM(ch) = atoi(line);
                    break;

                case 'E':
                    if (!strcmp(tag, "Exp ")) ch->exp = atoi(line);
                    else if (!strcmp(tag, "Eali")) ch->set(CharAlign::LawChaos, atoi(line));
                    else if (!strcmp(tag, "Ecls")) {

                    } else if (!strcmp(tag, "Eye ")) ch->set(CharAppearance::EyeColor, atoi(line));
                    break;

                case 'F':
                    if (!strcmp(tag, "Fisd")) GET_FISHD(ch) = atoi(line);
                    else if (!strcmp(tag, "Frez")) GET_FREEZE_LEV(ch) = atoi(line);
                    else if (!strcmp(tag, "Forc")) GET_FORGET_COUNT(ch) = atoi(line);
                    else if (!strcmp(tag, "Forg")) GET_FORGETING(ch) = atoi(line);
                    else if (!strcmp(tag, "Fury")) GET_FURY(ch) = atoi(line);
                    break;

                case 'G':
                    if (!strcmp(tag, "Gold")) ch->set(CharMoney::Carried, atoi(line));
                    else if (!strcmp(tag, "Gaun")) GET_GAUNTLET(ch) = atoi(line);
                    else if (!strcmp(tag, "Geno")) GET_GENOME(ch, 0) = atoi(line);
                    else if (!strcmp(tag, "Gen1")) GET_GENOME(ch, 1) = atoi(line);
                    break;

                case 'H':
                    if (!strcmp(tag, "Hit ")) load_HMVS(ch, line, LOAD_HIT);
                    else if (!strcmp(tag, "Hite")) ch->setHeight(atoi(line));
                    else if (!strcmp(tag, "Home")) GET_HOME(ch) = atoi(line);
                    else if (!strcmp(tag, "Host")) {}
                    else if (!strcmp(tag, "Hrc ")) ch->set(CharAppearance::HairColor, atoi(line));
                    else if (!strcmp(tag, "Hrl ")) ch->set(CharAppearance::HairLength, atoi(line));
                    else if (!strcmp(tag, "Hrs ")) ch->set(CharAppearance::HairStyle, atoi(line));
                    else if (!strcmp(tag, "Hung")) GET_COND(ch, HUNGER) = atoi(line);
                    break;

                case 'I':
                    if (!strcmp(tag, "Id  ")) GET_IDNUM(ch) = atol(line);
                    else if (!strcmp(tag, "INGl")) GET_INGESTLEARNED(ch) = atoi(line);
                    else if (!strcmp(tag, "Int ")) ch->set(CharAttribute::Intelligence, atoi(line));
                    else if (!strcmp(tag, "Invs")) GET_INVIS_LEV(ch) = atoi(line);
                    break;

                case 'K':
                    if (!strcmp(tag, "Ki  ")) load_HMVS(ch, line, LOAD_KI);
                    else if (!strcmp(tag, "Kaio")) GET_KAIOKEN(ch) = atoi(line);
                    break;

                case 'L':
                    if (!strcmp(tag, "Last")) ch->time.logon = atol(line);
                    else if (!strcmp(tag, "Lern")) ch->modPractices(atoi(line));
                    else if (!strcmp(tag, "Levl")) ch->set(CharNum::Level, atoi(line));
                    else if (!strcmp(tag, "LF  ")) load_BASE(ch, line, LOAD_LIFE);
                    else if (!strcmp(tag, "LFPC")) GET_LIFEPERC(ch) = atoi(line);
                    else if (!strcmp(tag, "Lila")) GET_LIMBCOND(ch, 1) = atoi(line);
                    else if (!strcmp(tag, "Lill")) GET_LIMBCOND(ch, 3) = atoi(line);
                    else if (!strcmp(tag, "Lira")) GET_LIMBCOND(ch, 0) = atoi(line);
                    else if (!strcmp(tag, "Lirl")) GET_LIMBCOND(ch, 2) = atoi(line);
                    else if (!strcmp(tag, "Lint")) GET_LINTEREST(ch) = atoi(line);
                    else if (!strcmp(tag, "Lpla")) GET_LPLAY(ch) = atoi(line);

                    break;

                case 'M':
                    if (!strcmp(tag, "Mana")) load_HMVS(ch, line, LOAD_MANA);
                    else if (!strcmp(tag, "Mexp")) load_molt(ch, line);
                    else if (!strcmp(tag, "Mlvl")) GET_MOLT_LEVEL(ch) = atoi(line);
                    else if (!strcmp(tag, "Move")) load_HMVS(ch, line, LOAD_MOVE);
                    else if (!strcmp(tag, "Mcls")) {

                    } else if (!strcmp(tag, "Maji")) MAJINIZED(ch) = atoi(line);
                    else if (!strcmp(tag, "Majm")) load_majin(ch, line);
                    else if (!strcmp(tag, "Mimi"))
                        ch->mimic = (RaceID)atoi(line);
                    else if (!strcmp(tag, "MxAg")) ch->time.maxage = atol(line);
                    break;

                case 'N':
                    if (!strcmp(tag, "Name")) GET_PC_NAME(ch) = strdup(line);
                    break;

                case 'O':
                    if (!strcmp(tag, "Olc ")) GET_OLC_ZONE(ch) = atoi(line);
                    break;

                case 'P':
                    if (!strcmp(tag, "Phas")) ch->set(CharAppearance::DistinguishingFeature, atoi(line));
                    else if (!strcmp(tag, "Phse")) GET_PHASE(ch) = atoi(line);
                    else if (!strcmp(tag, "Plyd")) ch->time.played = atol(line);
#ifdef ASCII_SAVE_POOFS
                    else if (!strcmp(tag, "PfIn")) POOFIN(ch) = strdup(line);
                    else if (!strcmp(tag, "PfOt")) POOFOUT(ch) = strdup(line);
#endif
                    else if (!strcmp(tag, "Pole")) GET_POLE_BONUS(ch) = atoi(line);
                    else if (!strcmp(tag, "Posi")) GET_POS(ch) = atoi(line);
                    else if (!strcmp(tag, "Pref")) {
                        sscanf(line, "%s %s %s %s", f1, f2, f3, f4);
                        flags[0] = asciiflag_conv(f1);
                        flags[1] = asciiflag_conv(f2);
                        flags[2] = asciiflag_conv(f3);
                        flags[3] = asciiflag_conv(f4);
                        for(auto f = 0; f < ch->pref.size(); f++) {
                            if(IS_SET_AR(flags, f)) ch->pref.set(f);
                        }
                    } else if (!strcmp(tag, "Prff")) GET_PREFERENCE(ch) = atoi(line);
                    break;

                case 'R':
                    if (!strcmp(tag, "Race")) ch->race = (RaceID)atoi(line);
                    else if (!strcmp(tag, "Raci")) ch->set(CharNum::RacialPref, atoi(line));
                    else if (!strcmp(tag, "rDis")) GET_RDISPLAY(ch) = strdup(line);
                    else if (!strcmp(tag, "Rela")) GET_RELAXCOUNT(ch) = atoi(line);
                    else if (!strcmp(tag, "Rtim")) GET_RTIME(ch) = atoi(line);
                    else if (!strcmp(tag, "Rad1")) GET_RADAR1(ch) = atoi(line);
                    else if (!strcmp(tag, "Rad2")) GET_RADAR2(ch) = atoi(line);
                    else if (!strcmp(tag, "Rad3")) GET_RADAR3(ch) = atoi(line);
                    else if (!strcmp(tag, "Room")) GET_LOADROOM(ch) = atoi(line);
                    else if (!strcmp(tag, "RPfe")) GET_FEATURE(ch) = strdup(line);
                    break;

                case 'S':
                    if (!strcmp(tag, "Sex ")) ch->set(CharAppearance::Sex, atoi(line));
                    else if (!strcmp(tag, "Ship")) GET_SHIP(ch) = atoi(line);
                    else if (!strcmp(tag, "Scoo")) GET_SDCOOLDOWN(ch) = atoi(line);
                    else if (!strcmp(tag, "Shpr")) GET_SHIPROOM(ch) = atoi(line);
                    else if (!strcmp(tag, "Skil")) load_skills(fl, ch, false);
                    else if (!strcmp(tag, "Skn ")) ch->set(CharAppearance::SkinColor, atoi(line));
                    else if (!strcmp(tag, "Size")) ch->setSize(atoi(line));
                    else if (!strcmp(tag, "SklB")) load_skills(fl, ch, true);
                    else if (!strcmp(tag, "SkRc")) ch->modPractices(atoi(line));
                    else if (!strcmp(tag, "SkCl")) {
                        sscanf(line, "%d %d", &num2, &num3);
                        ch->modPractices(num3);
                    } else if (!strcmp(tag, "Slot")) ch->skill_slots = atoi(line);
                    else if (!strcmp(tag, "Spek")) SPEAKING(ch) = atoi(line);
                    else if (!strcmp(tag, "Str ")) ch->set(CharAttribute::Strength, atoi(line));
                    else if (!strcmp(tag, "Stuk")) ch->stupidkiss = atoi(line);
                    else if (!strcmp(tag, "Supp")) GET_SUPPRESS(ch) = atoi(line);
                    break;

                case 'T':
                    if (!strcmp(tag, "Tgro")) GET_TGROWTH(ch) = atoi(line);
                    else if (!strcmp(tag, "Tcla")) {
                        switch(atoi(line)) {
                            case 1: // great requirements... range is 0.2 to 0.3.
                                ch->transBonus = Random::get<double>(0.2, 0.3);
                            break;
                            case 2:
                                ch->transBonus = Random::get<double>(-0.1, 0.1);
                            break;
                            case 3:
                                ch->transBonus = Random::get<double>(-0.3, -0.2);
                            break;
                            default:
                                ch->transBonus = Random::get<double>(-0.3, 0.3);
                        }

                    }
                    else if (!strcmp(tag, "Tcos")) {
                        sscanf(line, "%d %d", &num2, &num3);
                        //GET_TRANSCOST(ch, num2) = num3;
                    } else if (!strcmp(tag, "Thir")) GET_COND(ch, THIRST) = atoi(line);
                    else if (!strcmp(tag, "Thr1")) GET_SAVE_MOD(ch, 0) = atoi(line);
                    else if (!strcmp(tag, "Thr2")) GET_SAVE_MOD(ch, 1) = atoi(line);
                    else if (!strcmp(tag, "Thr3")) GET_SAVE_MOD(ch, 2) = atoi(line);
                    else if (!strcmp(tag, "Thr4") || !strcmp(tag, "Thr5")); /* Discard extra saves */
                    else if (!strcmp(tag, "ThB1")) GET_SAVE_BASE(ch, 0) = atoi(line);
                    else if (!strcmp(tag, "ThB2")) GET_SAVE_BASE(ch, 1) = atoi(line);
                    else if (!strcmp(tag, "ThB3")) GET_SAVE_BASE(ch, 2) = atoi(line);
                    else if (!strcmp(tag, "Trag")) ch->set(CharTrain::Agility, atoi(line));
                    else if (!strcmp(tag, "Trco")) ch->set(CharTrain::Constitution, atoi(line));
                    else if (!strcmp(tag, "Trin")) ch->set(CharTrain::Intelligence, atoi(line));
                    else if (!strcmp(tag, "Trsp")) ch->set(CharTrain::Speed, atoi(line));
                    else if (!strcmp(tag, "Trst")) ch->set(CharTrain::Strength, atoi(line));
                    else if (!strcmp(tag, "Trwi")) ch->set(CharTrain::Wisdom, atoi(line));
                    break;
                case 'U':
                    if (!strcmp(tag, "Upgr")) GET_UP(ch) = atoi(line);
                    else if (!strcmp(tag, "User")) {
                        if (GET_LOG_USER(ch)) {
                            free(GET_LOG_USER(ch));
                        }
                        GET_LOG_USER(ch) = strdup(line);
                    }
                    break;
                case 'V':
                    if (!strcmp(tag, "Voic")) GET_VOICE(ch) = strdup(line);
                    break;

                case 'W':
                    if (!strcmp(tag, "Wate")) GET_WEIGHT(ch) = atoi(line);
                    else if (!strcmp(tag, "Wimp")) GET_WIMP_LEV(ch) = atoi(line);
                    else if (!strcmp(tag, "Wis ")) ch->set(CharAttribute::Wisdom, atoi(line));
                    break;

                default:
                    sprintf(buf, "SYSERR: Unknown tag %s in pfile %s", tag, name);
            }
        }
    }

    if (!ch->time.created) {
        basic_mud_log("No creation timestamp for user %s, using current time", GET_NAME(ch));
        ch->time.created = time(nullptr);
    }

    ch->generation = ch->time.created;

    if (!ch->time.birth) {
        basic_mud_log("No birthday for user %s, using standard starting age determination", GET_NAME(ch));
        ch->time.birth = time(nullptr) - birth_age(ch);
    }

    /* initialization for imms */
    if (GET_ADMLEVEL(ch) >= ADMLVL_IMMORT) {
        for (i = 1; i <= SKILL_TABLE_SIZE; i++)
            SET_SKILL(ch, i, 100);
        GET_COND(ch, HUNGER) = -1;
        GET_COND(ch, THIRST) = -1;
        GET_COND(ch, DRUNK) = -1;
    }


    if (IS_ANDROID(ch)) {
        GET_COND(ch, HUNGER) = -1;
        GET_COND(ch, THIRST) = -1;
        GET_COND(ch, DRUNK) = -1;
    }
    fclose(fl);
    return (id);
}

/* Return a filename given a house vnum */
int House_get_filename(room_vnum vnum, char *filename, size_t maxlen) {
    if (vnum == NOWHERE)
        return (0);

    snprintf(filename, maxlen, LIB_HOUSE"%d.house", vnum);
    return (1);
}

int House_load(room_vnum rvnum) {
    FILE *fl;
    char f1[READ_SIZE], f2[READ_SIZE], f3[READ_SIZE], f4[READ_SIZE];
    char cmfname[MAX_STRING_LENGTH];
    char buf1[MAX_STRING_LENGTH];
    char buf2[MAX_STRING_LENGTH];
    char line[256];
    int64_t t[21], danger, zwei = 0;
    struct obj_data *temp;
    int locate = 0, j, nr, k, num_objs = 0;
    struct obj_data *obj1;
    struct obj_data *cont_row[MAX_BAG_ROWS];
    struct extra_descr_data *new_descr;
    room_rnum rrnum = rvnum;

    if (!world.contains(rvnum))
        return 0;

    if (!House_get_filename(rvnum, cmfname, sizeof(cmfname)))
        return 0;

    if (!(fl = fopen(cmfname, "r+b"))) {
        if (errno != ENOENT) {  /* if it fails, NOT because of no file */
            sprintf(buf1, "SYSERR: READING HOUSE FILE %s (5)", cmfname);
            perror(buf1);
        }
        return 0;
    }

    for (j = 0; j < MAX_BAG_ROWS; j++)
        cont_row[j] = nullptr; /* empty all cont lists (you never know ...) */

    if (!feof(fl))
        get_line(fl, line);
    while (!feof(fl)) {
        temp = nullptr;
        /* first, we get the number. Not too hard. */
        if (*line == '#') {
            if (sscanf(line, "#%d", &nr) != 1) {
                continue;
            }
            /* we have the number, check it, load obj. */
            if (nr == NOTHING) {   /* then it is unique */
                temp = create_obj(false);
                temp->vn = NOTHING;
            } else if (nr < 0) {
                continue;
            } else {
                if (nr >= 999999)
                    continue;
                temp = read_object(nr, VIRTUAL, false);
                if (!temp) {
                    get_line(fl, line);
                    continue;
                }
            }

            get_line(fl, line);
            sscanf(line, "%ld %ld %ld %ld %ld %ld %ld %ld %ld %s %s %s %s %ld %ld %ld %ld %ld %ld %ld %ld", t, t + 1, t + 2, t + 3,
                   t + 4, t + 5, t + 6, t + 7, t + 8, f1, f2, f3, f4, t + 13, t + 14, t + 15, t + 16, t + 17, t + 18,
                   t + 19, t + 20);
            locate = t[0];
            GET_OBJ_VAL(temp, 0) = t[1];
            GET_OBJ_VAL(temp, 1) = t[2];
            GET_OBJ_VAL(temp, 2) = t[3];
            GET_OBJ_VAL(temp, 3) = t[4];
            GET_OBJ_VAL(temp, 4) = t[5];
            GET_OBJ_VAL(temp, 5) = t[6];
            GET_OBJ_VAL(temp, 6) = t[7];
            GET_OBJ_VAL(temp, 7) = t[8];
            bitvector_t ex[4];
            ex[0] = asciiflag_conv(f1);
            ex[1] = asciiflag_conv(f2);
            ex[2] = asciiflag_conv(f3);
            ex[3] = asciiflag_conv(f4);
            for(auto i = 0; i < temp->extra_flags.size(); i++) temp->extra_flags.set(i, IS_SET_AR(ex, i));
            GET_OBJ_VAL(temp, 8) = t[13];
            GET_OBJ_VAL(temp, 9) = t[14];
            GET_OBJ_VAL(temp, 10) = t[15];
            GET_OBJ_VAL(temp, 11) = t[16];
            GET_OBJ_VAL(temp, 12) = t[17];
            GET_OBJ_VAL(temp, 13) = t[18];
            GET_OBJ_VAL(temp, 14) = t[19];
            GET_OBJ_VAL(temp, 15) = t[20];
            GET_OBJ_POSTED(temp) = nullptr;
            GET_OBJ_POSTTYPE(temp) = 0;

            get_line(fl, line);
            /* read line check for xap. */
            if (!strcmp("XAP", line)) {  /* then this is a Xap Obj, requires
                                       special care */
                
                temp->setName(withPlaceholder(fread_string(fl, buf2), "undefined"));
                temp->setShortDesc(withPlaceholder(fread_string(fl, buf2), "undefined"));
                temp->setRoomDesc(withPlaceholder(fread_string(fl, buf2), "undefined"));
                temp->setLookDesc(withPlaceholder(fread_string(fl, buf2), "undefined"));

                if (!get_line(fl, line) ||
                    (sscanf(line, "%ld %ld %ld %ld %ld %ld %ld %ld", t, t + 1, t + 2, t + 3, t + 4, t + 5, t + 6, t + 7) !=
                     8)) {
                    fprintf(stderr, "Format error in first numeric line (expecting _x_ args)");
                    return 0;
                }
                temp->type_flag = t[0];
                bitvector_t wear[4];
                wear[0] = t[1];
                wear[1] = t[2];
                wear[2] = t[3];
                wear[3] = t[4];
                for(auto i = 0; i < temp->wear_flags.size(); i++) temp->wear_flags.set(i, IS_SET_AR(wear, i));
                temp->weight = t[5];
                temp->cost = t[6];
                temp->cost_per_day = t[7];

                /* buf2 is error codes pretty much */
                //strcat(buf2, ", after numeric constants (expecting E/#xxx)");

                /*add_unique_id(temp);*/
                /* we're clearing these for good luck */

                for (j = 0; j < MAX_OBJ_AFFECT; j++) {
                    temp->affected[j].location = APPLY_NONE;
                    temp->affected[j].modifier = 0;
                    temp->affected[j].specific = 0;
                }

                temp->ex_description = nullptr;

                get_line(fl, line);
                for (k = j = zwei = 0; !zwei && !feof(fl);) {
                    switch (*line) {
                        case 'E':
                            CREATE(new_descr, struct extra_descr_data, 1);
                            new_descr->keyword = fread_string(fl, buf2);
                            new_descr->description = fread_string(fl, buf2);
                            new_descr->next = temp->ex_description;
                            temp->ex_description = new_descr;
                            get_line(fl, line);
                            break;
                        case 'A':
                            if (j >= MAX_OBJ_AFFECT) {
                                basic_mud_log("SYSERR: Too many object affectations in loading house file");
                                danger = 1;
                            }
                            get_line(fl, line);
                            sscanf(line, "%ld %ld %ld", t, t + 1, t + 2);
                            temp->affected[j].location = t[0];
                            temp->affected[j].modifier = t[1];
                            temp->affected[j].specific = t[2];
                            j++;
                            get_line(fl, line);
                            break;

                        case 'G':
                            get_line(fl, line);
                            sscanf(line, "%" TMT, &temp->generation);
                            get_line(fl, line);
                            break;
                        case 'U':
                            get_line(fl, line);
                            sscanf(line, "%" I64T, &temp->id);
                            get_line(fl, line);
                            break;
                        case 'S':
                            if (j >= SPELLBOOK_SIZE) {
                                basic_mud_log("SYSERR: Too many spells in spellbook loading rent file");
                                danger = 1;
                            }
                            get_line(fl, line);
                            sscanf(line, "%d %d", t, t + 1);

                            if (!temp->sbinfo) {
                                CREATE(temp->sbinfo, struct obj_spellbook_spell, SPELLBOOK_SIZE);
                                memset((char *) temp->sbinfo, 0, SPELLBOOK_SIZE * sizeof(struct obj_spellbook_spell));
                            }
                            temp->sbinfo[j].spellname = t[0];
                            temp->sbinfo[j].pages = t[1];
                            j++;
                            get_line(fl, line);
                            break;
                        case 'Z':
                            get_line(fl, line);
                            sscanf(line, "%d", &GET_OBJ_SIZE(temp));
                            get_line(fl, line);
                            break;
                        case '$':
                        case '#':
                            zwei = 1;
                            break;
                        default:
                            zwei = 1;
                            break;
                    }
                }      /* exit our for loop */
            }   /* exit our xap loop */
            if (temp != nullptr) {
                check_unique_id(temp);
                add_unique_id(temp);
                temp->script = std::make_shared<script_data>(temp);
                temp->activate();
                num_objs++;
                obj_to_room(temp, rrnum);
            }

/*No need to check if its equipped since rooms can't equip things --firebird_223*/
            for (j = MAX_BAG_ROWS - 1; j > -locate; j--)
                if (cont_row[j]) { /* no container -> back to ch's inventory */
                    for (; cont_row[j]; cont_row[j] = obj1) {
                        obj1 = cont_row[j]->next_content;
                        obj_to_room(cont_row[j], rrnum);
                    }
                    cont_row[j] = nullptr;
                }

            if (j == -locate && cont_row[j]) { /* content list existing */
                if (GET_OBJ_TYPE(temp) == ITEM_CONTAINER) {
                    /* take item ; fill ; give to char again */
                    obj_from_room(temp);
                    temp->contents = nullptr;
                    for (; cont_row[j]; cont_row[j] = obj1) {
                        obj1 = cont_row[j]->next_content;
                        obj_to_obj(cont_row[j], temp);
                    }
                    obj_to_room(temp, rrnum); /* add to inv first ... */
                } else { /* object isn't container -> empty content list */
                    for (; cont_row[j]; cont_row[j] = obj1) {
                        obj1 = cont_row[j]->next_content;
                        obj_to_room(cont_row[j], rrnum);
                    }
                    cont_row[j] = nullptr;
                }
            }

            if (locate < 0 && locate >= -MAX_BAG_ROWS) {
                /* let obj be part of content list
                   but put it at the list's end thus having the items
                   in the same order as before renting */
                obj_from_room(temp);
                if ((obj1 = cont_row[-locate - 1])) {
                    while (obj1->next_content)
                        obj1 = obj1->next_content;
                    obj1->next_content = temp;
                } else
                    cont_row[-locate - 1] = temp;
            }
        } else {
            get_line(fl, line);
        }
    }


    fclose(fl);

    return 1;
}


struct house_control_rec {
    room_vnum vn;        /* vnum of this house		*/
    room_vnum atrium;        /* vnum of atrium		*/
    int16_t exit_num;        /* direction of house's exit	*/
    time_t built_on;        /* date this house was built	*/
    int mode;            /* mode of ownership		*/
    long owner;            /* idnum of house's owner	*/
    int num_of_guests;        /* how many guests for house	*/
    long guests[MAX_GUESTS];    /* idnums of house's guests	*/
    time_t last_payment;        /* date of last house payment   */
    long bitvector;
    long builtby;
    long spare2;
    long spare3;
    long spare4;
    long spare5;
    long spare6;
    long spare7;
};

/* call from boot_db - will load control recs, load objs, set atrium bits */
/* should do sanity checks on vnums & remove invalid records */
void House_boot() {
    int num_of_houses = 0;
    struct house_control_rec temp_house;
    FILE *fl;

    if (!(fl = fopen(HCONTROL_FILE, "rb"))) {
        if (errno == ENOENT)
            basic_mud_log("   No houses to load. File '%s' does not exist.", HCONTROL_FILE);
        else
            perror("SYSERR: " HCONTROL_FILE);
        return;
    }
    while (!feof(fl) && num_of_houses < MAX_HOUSES) {
        fread(&temp_house, sizeof(struct house_control_rec), 1, fl);

        if (feof(fl))
            break;

        if (world.contains(temp_house.vn))
            House_load(temp_house.vn);
    }

    fclose(fl);
}

void boot_db_world_legacy() {

    basic_mud_log("Loading legacy world data...");
    basic_mud_log("Loading zone table.");
    index_boot(DB_BOOT_ZON);

    basic_mud_log("Loading triggers and generating index.");
    index_boot(DB_BOOT_TRG);

    basic_mud_log("Loading rooms.");
    index_boot(DB_BOOT_WLD);

    basic_mud_log("Checking start rooms.");
    check_start_rooms();

    basic_mud_log("Loading mobs and generating index.");
    index_boot(DB_BOOT_MOB);

    basic_mud_log("Loading objs and generating index.");
    index_boot(DB_BOOT_OBJ);

    basic_mud_log("Loading disabled commands list...");
    load_disabled();

    basic_mud_log("Loading shops.");
    index_boot(DB_BOOT_SHP);

    basic_mud_log("Loading guild masters.");
    index_boot(DB_BOOT_GLD);
    boot_db_shadow();
}

static void boot_db_legacy() {
    boot_db_time();
    boot_db_textfiles();
    boot_db_spellfeats();
    boot_db_world_legacy();
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

// ACTUAL MIGRATION STUFF BELOW...

static std::string stripAnsi(const std::string& str) {
    return processColors(str, false, nullptr);
}

struct old_ship_data {
    std::string name;
    std::set<vnum> vnums;
    std::optional<vnum> hatch_room{};
    std::optional<vnum> ship_obj{};
    std::optional<vnum> location{};
};

static struct old_ship_data gships[] = {
        {"Falcon", {3900, 3901, 3902, 3903, 3904}, 3900, 3900, 408},
        {"Simurgh", {3905, 3996, 3997, 3998, 3999}, 3905, 3905, 12002},
        {"Zypher", {3906, 3907, 3908, 3909, 3910}, 3906, 3906, 4250},
        {"Valkyrie", {3911, 3912, 3913, 3914, 3915}, 3911, 3911, 2323},
        {"Phoenix", {3916, 3917, 3918, 3919, 3920}, 3916, 3916, 408},
        {"Merganser", {3921, 3922, 3923, 3924, 3925}, 3921, 3921, 2323},
        {"Wraith", {3926, 3927, 3928, 3929, 3930}, 3930, 3930, 11626},
        {"Ghost", {3931, 3932, 3933, 3934, 3935}, 3935, 3935, 8194},
        {"Wisp", {3936, 3937, 3938, 3939, 3940}, 3940, 3940, 12002},
        {"Eagle", {3941, 3942, 3943, 3944, 3945}, 3945, 3945, 4250},

        {"Spectral", {3946, 3947, 3948, 3949, 3950}, 3950, {}, {}},
        {"Raven", {3951, 3952, 3953, 3954, 3955, 3961}, 3955, {}, {}},
        {"Marauder", {3956, 3957, 3958, 3959, 3960}, 3960, {}, {}},
        {"Vanir", {3962, 3963, 3964, 3965}, 3965, {}, {}},
        {"Aesir", {3966, 3967, 3968, 3969, 3970}, 3970, {}, {}},
        {"Undine", {3971, 3972, 3973, 3974, 3975}, 3975, {}, {}},
        {"Banshee", {3976, 3977, 3978, 3979, 3980}, 3980, {}, {}},
        {"Hydra", {3981, 3982, 3983, 3984, 3985}, 3985, {}, {}},
        {"Medusa", {3986, 3987, 3988, 3989, 3990}, 3990, {}, {}},
        {"Pegasus", {3991, 3992, 3993, 3994, 3995}, 3995, {}, {}},
        {"Zel 1", {5824}, 5824, {}, {}},
        {"Zel 2", {5825}, 5825, {}, {}}
};

static struct old_ship_data customs[] = {
        {"Yun-Yammka", {19900, 19901, 19902}, 19901, 19900, {}},
        {"The Dark Archon", {19903, 19912, 19913, 19914}, 19912, 19916, {}},
        {"The Zafir Krakken", {19904, 19905, 19906, 19907}, 19905, 19905, {}},
        {"Crimson Talon", {19908, 19909, 19910, 19911}, 19908, 19910, {}},
        {"Rust Bucket", {19915, 19916, 19918, 19930}, 19915, 19921, {}},
        {"The Adamant", {19917, 19949, 19955, 19956}, 19949, 19966, {}},
        {"Vanguard", {19919, 19920, 19921, 19922}, 19920, 19926, {}},
        {"The Glacial Shadow", {19925, 19923, 19924, 19926}, 19923, 19931, {}},
        {"The Molecular Dynasty", {19927, 19928, 19929, 19954}, 19927, 19936, {}},
        {"Poseidon", {19931, 19933, 19932, 19934}, 19931, 19941, {}},
        {"Sakana Mirai", {19935, 19936, 19937, 19938}, 19935, 19946, {}},
        {"Earth Far Freighter Enterjection", {19939, 19940, 19941, 19942}, 19939, 19951, {}},
        {"Soaring Angel", {19943, 19944, 19945, 19946}, 19943, 19956, {}},
        {"The Grey Wolf", {19947, 19948, 19978, 19979}, 19947, 19961, {}},
        {"The Event Horizon", {19950, 19951, 19980, 19981}, 19950, 19971, {}},
        {"Fleeting Star", {19952, 19953, 19957, 19958}, 19952, 19976, {}},
        {"Makenkosappo", {19959, 19960, 19961, 19962}, 19959, 19981, {}},
        {"The Nightingale", {19963, 19964, 19965, 19982}, 19963, 19986, {}},
        {"The Honey Bee", {19966, 19967, 19968, 19969}, 19966, 19991, {}},
        {"The Bloodrose", {19970, 19971, 19972, 19973}, 19970, 19996, {}},
        {"The Stimato", {19974, 19975, 19976, 19977}, 19974, {}, {}},
        {"The Tatsumaki", {15805, 15806, 15807, 15808}, 15805, 15805, {}},
        {"Shattered Soul", {15800, 15801, 15802, 15803}, 15800, {}, {}}
};

struct AreaDef {
    std::string name;
    AreaType type{AreaType::Region};
    std::optional<vnum> parent, orbit;
    std::optional<double> gravity;
    std::set<std::size_t> roomFlags{};
    std::vector<std::pair<std::size_t, std::size_t>> roomRanges;
    std::set<vnum> roomIDs{}, roomSkips{};
    std::bitset<NUM_AREA_FLAGS> flags;
};

vnum assembleArea(const AreaDef &def) {
    auto vn = area_data::getNextID();
    auto &a = areas[vn];
    a.vn = vn;
    a.name = def.name;
    a.type = def.type;
    a.flags = def.flags;

    if(def.gravity) {
        a.gravity = def.gravity.value();
    }

    if(def.parent) {
        auto &p = areas[def.parent.value()];
        p.children.insert(vn);
        a.parent = p.vn;
    }

    std::set<vnum> rooms = def.roomIDs;
    a.extraVn = def.orbit;
    if(a.type == AreaType::CelestialBody && a.extraVn) {
        rooms.insert(a.extraVn.value());
    }

    for(auto &[start, end] : def.roomRanges) {
        for(auto i = start; i <= end; i++) {
            auto found = world.find(i);
            if(found == world.end()) continue;
            rooms.insert(i);
        }
    }

    if(!def.roomFlags.empty()) {
        for(auto &[vn, room] : world) {
            for(auto &f : def.roomFlags) {
                if(room.room_flags.test(f)) {
                    rooms.insert(vn);
                    break;
                }
            }
        }
    }

    for(auto r: def.roomSkips) rooms.erase(r);

    basic_mud_log("Assembling Area: %s, Rooms: %d", def.name.c_str(), rooms.size());

    for(auto r : rooms) {
        auto found = world.find(r);
        if(found == world.end()) continue;
        auto &room = found->second;
        if(room.area) continue;
        room.area = vn;
        a.rooms.insert(r);
    }

    return vn;

}


void migrate_grid() {
    AreaDef adef;
    adef.name = "Admin Land";
    adef.roomRanges.emplace_back(0, 16);
    adef.roomIDs = {16694, 16698};
    adef.type = AreaType::Dimension;
    auto admin_land = assembleArea(adef);

    AreaDef mudschooldef;
    mudschooldef.name = "MUD School";
    mudschooldef.roomRanges.emplace_back(100, 154);
    mudschooldef.type = AreaType::Dimension;
    auto mud_school = assembleArea(mudschooldef);

    AreaDef mvdef;
    mvdef.name = "Multiverse";
    mvdef.type = AreaType::Dimension;
    auto multiverse = assembleArea(mvdef);

    AreaDef xvdef;
    xvdef.name = "Xenoverse";
    xvdef.type = AreaType::Dimension;
    auto xenoverse = assembleArea(xvdef);

    AreaDef u7def;
    u7def.name = "Universe 7";
    u7def.type = AreaType::Dimension;
    u7def.parent = multiverse;
    auto universe7 = assembleArea(u7def);

    AreaDef mplane;
    mplane.name = "Mortal Plane";
    mplane.type = AreaType::Dimension;
    mplane.parent = universe7;
    auto mortal_plane = assembleArea(mplane);

    AreaDef cplane;
    cplane.name = "Celestial Plane";
    cplane.type = AreaType::Dimension;
    cplane.parent = universe7;
    auto celestial_plane = assembleArea(cplane);

    AreaDef spacedef;
    spacedef.name = "Depths of Space";
    spacedef.type = AreaType::Region;
    spacedef.parent = mortal_plane;
    // Insert every room id from mapnums (the 2d array) into spacedef.roomIDs...
    for(auto &row : mapnums) {
        for(auto &col : row) {
            spacedef.roomIDs.insert(col);
        }
    }
    auto space = assembleArea(spacedef);

    std::unordered_map<std::string, AreaDef> areaDefs;

    { // Earth miscellaneous rooms...
        auto &w = areaDefs["West City"];
        w.roomRanges.emplace_back(19500, 19558);
        w.roomIDs.insert(19576);
        w.roomIDs.insert(19599);
        w.roomIDs.insert(178);
        auto &s = areaDefs["Silver Mine"];
        s.roomIDs.insert(19577);
        auto &n = areaDefs["Nexus City"];
        n.roomIDs.insert(5827);
        n.roomIDs.insert(199);
        n.roomIDs.insert(19);
        n.roomIDs.insert(20);
        n.roomIDs.insert(25);
        n.roomIDs.insert(29);
        n.roomIDs.insert(81);
        n.roomIDs.insert(97);
        n.roomIDs.insert(98);
        n.roomIDs.insert(99);
        n.roomIDs.insert(19001);
        n.roomIDs.insert(19007);
        n.roomIDs.insert(23);
    }

    {// Vegeta misc..
        auto &v = areaDefs["Vegetos City"];
        v.roomIDs.insert(15700);
        v.roomIDs.insert(82);
        v.roomIDs.insert(19003);
        v.roomIDs.insert(179);
        auto &b = areaDefs["Blood Dunes"];
        b.roomIDs.insert(155);
        b.roomIDs.insert(156);
    }

    {
        // Frigid misc...
        auto &i = areaDefs["Ice Crown City"];
        i.roomIDs.insert(83);
        i.roomIDs.insert(19002);
        i.roomIDs.insert(180);
    }

    {
        // Aether misc...
        auto &h = areaDefs["Haven City"];
        h.roomIDs.insert(85);
        h.roomIDs.insert(183);
        h.roomIDs.insert(19005);
        h.roomIDs.insert(19008);
    }

    {
        // yardrat...
        auto &y = areaDefs["Yardra City"];
        y.roomIDs.insert(26);
    }
    {
        // Konack
        auto &t = areaDefs["Tiranoc City"];
        t.roomIDs.insert(86);
        t.roomIDs.insert(181);
        t.roomIDs.insert(19004);
    }

    {
        // Namek stuff..
        auto &k = areaDefs["Kakureta Village"];
        k.roomRanges.emplace_back(14400, 14499);
        auto &s = areaDefs["Senzu Village"];
        s.roomIDs.insert(84);
        s.roomIDs.insert(182);
        auto &e = areaDefs["Elder Village"];
        e.roomIDs.insert(19006);
    }

    {
        // kanassa misc...
        auto &a = areaDefs["Aquis City"];
        a.roomIDs.insert(177);
    }

    for(auto &[rv, room] : world) {
        if(room.area) continue;
        auto sense = sense_location_name(rv);
        if(sense != "Unknown.") {
            auto &area = areaDefs[sense];
            area.roomIDs.insert(rv);
        }
    }

    std::unordered_map<std::string, vnum> areaObjects;

    for(auto &[name, def] : areaDefs) {
        def.name = name;
        def.type = AreaType::Region;
        auto aent = assembleArea(def);
        areaObjects[name] = aent;
    }

    AreaDef pearth;
    pearth.name = "@GEarth@n";
    pearth.type = AreaType::CelestialBody;
    pearth.parent = space;
    pearth.orbit = 50;
    auto planet_earth = assembleArea(pearth);

    AreaDef pvegeta;
    pvegeta.name = "@YVegeta@n";
    pvegeta.type = AreaType::CelestialBody;
    pvegeta.parent = space;
    pvegeta.gravity = 10.0;
    pvegeta.orbit = 53;
    auto planet_vegeta = assembleArea(pvegeta);

    AreaDef pfrigid;
    pfrigid.name = "@CFrigid@n";
    pfrigid.type = AreaType::CelestialBody;
    pfrigid.parent = space;
    pfrigid.orbit = 51;
    auto planet_frigid = assembleArea(pfrigid);

    AreaDef pnamek;
    pnamek.name = "@gNamek@n";
    pnamek.type = AreaType::CelestialBody;
    pnamek.parent = space;
    pnamek.orbit = 54;
    auto planet_namek = assembleArea(pnamek);

    AreaDef pkonack;
    pkonack.name = "@MKonack@n";
    pkonack.type = AreaType::CelestialBody;
    pkonack.parent = space;
    pkonack.orbit = 52;
    auto planet_konack = assembleArea(pkonack);

    AreaDef paether;
    paether.name = "@MAether@n";
    paether.type = AreaType::CelestialBody;
    paether.parent = space;
    paether.orbit = 55;
    auto planet_aether = assembleArea(paether);

    AreaDef pyardrat;
    pyardrat.name = "@mYardrat@n";
    pyardrat.type = AreaType::CelestialBody;
    pyardrat.parent = space;
    pyardrat.orbit = 56;
    auto planet_yardrat = assembleArea(pyardrat);

    AreaDef pkanassa;
    pkanassa.name = "@BKanassa@n";
    pkanassa.type = AreaType::CelestialBody;
    pkanassa.parent = space;
    pkanassa.orbit = 58;
    auto planet_kanassa = assembleArea(pkanassa);

    AreaDef pcerria;
    pcerria.name = "@RCerria@n";
    pcerria.type = AreaType::CelestialBody;
    pcerria.parent = space;
    pcerria.orbit = 198;
    auto planet_cerria = assembleArea(pcerria);

    AreaDef parlia;
    parlia.name = "@GArlia@n";
    parlia.type = AreaType::CelestialBody;
    parlia.parent = space;
    parlia.orbit = 59;
    auto planet_arlia = assembleArea(parlia);

    AreaDef pzenith;
    pzenith.name = "@BZenith@n";
    pzenith.type = AreaType::CelestialBody;
    pzenith.parent = space;
    pzenith.orbit = 57;
    auto moon_zenith = assembleArea(pzenith);
    for(const auto& name : {"Ancient Castle", "Utatlan City", "Zenith Jungle"}) {
        auto vn = areaObjects[name];
        auto &a = areas[vn];
        a.parent = moon_zenith;
        auto &m = areas[moon_zenith];
        m.children.insert(vn);
    }


    AreaDef ucdef;
    ucdef.name = "Underground Cavern";
    ucdef.parent = moon_zenith;
    ucdef.roomRanges.emplace_back(62900, 63000);
    auto underground_cavern = assembleArea(ucdef);

    for(auto &p : {planet_earth, planet_aether, planet_namek, moon_zenith}) {
		auto &planet = areas[p];
        planet.flags.set(AREA_ETHER);
    }

    for(auto &p : {planet_earth, planet_aether, planet_vegeta, planet_frigid}) {
        auto &planet = areas[p];
        planet.flags.set(AREA_MOON);
    }

    AreaDef zelakinfarm;
    zelakinfarm.name = "Zelakin's Farm";
    zelakinfarm.parent = xenoverse;
    zelakinfarm.roomRanges.emplace_back(5896, 5899);
    auto zelakin_farm = assembleArea(zelakinfarm);

    AreaDef hbtcdef;
    hbtcdef.name = "Hyperbolic Time Chamber";
    hbtcdef.parent = universe7;
    hbtcdef.roomRanges.emplace_back(64000, 64097);
    hbtcdef.type = AreaType::Dimension;
    auto hbtc = assembleArea(hbtcdef);

    AreaDef bodef;
    bodef.name = "The Black Omen";
    bodef.parent = space;
    bodef.roomIDs.insert(19053);
    bodef.roomIDs.insert(19039);
    for(auto &[r, room] : world) {
        if(icontains(stripAnsi(room.name), "Black Omen")) bodef.roomIDs.insert(r);
    }
    bodef.roomIDs.insert(19050);
    bodef.type = AreaType::Vehicle;
    auto black_omen = assembleArea(bodef);

    AreaDef earthduel;
    earthduel.name = "Duel Dome";
    earthduel.parent = planet_earth;
    earthduel.roomRanges.emplace_back(160, 176);
    auto earth_duel_dome = assembleArea(earthduel);

    AreaDef earthwmat;
    earthwmat.name = "World Martial Arts Building";
    earthwmat.parent = planet_earth;
    earthwmat.roomRanges.emplace_back(3800, 3834);
    earthwmat.roomRanges.emplace_back(19578, 19598);
    earthwmat.roomRanges.emplace_back(19570, 19573);
    earthwmat.roomIDs = {19574, 19575};
    auto earth_wmat = assembleArea(earthwmat);

    AreaDef capsulecorp;
    capsulecorp.name = "Capsule Corporation";
    capsulecorp.parent = areaObjects["West City"];
    capsulecorp.roomRanges.emplace_back(19559, 19569);
    auto capsule_corp = assembleArea(capsulecorp);

    AreaDef threestarelem;
    threestarelem.name = "Three Star Elementary";
    threestarelem.parent = planet_earth;
    threestarelem.roomRanges.emplace_back(5800, 5823);
    threestarelem.roomIDs.insert(5826);
    auto three_star_elem = assembleArea(threestarelem);

    AreaDef gerol;
    gerol.name = "Gero's Lab";
    gerol.parent = planet_earth;
    gerol.roomRanges.emplace_back(7701, 7753);
    auto gero_lab = assembleArea(gerol);

    AreaDef shadowrain;
    shadowrain.name = "Shadowrain City";
    shadowrain.parent = planet_earth;
    shadowrain.roomRanges.emplace_back(9111, 9199);
    auto shadowrain_city = assembleArea(shadowrain);

    AreaDef kingcastle;
    kingcastle.name = "King Castle";
    kingcastle.parent = planet_earth;
    kingcastle.roomRanges.emplace_back(12600, 12627);
    auto king_castle = assembleArea(kingcastle);

    AreaDef orangestar;
    orangestar.name = "Orange Star Highschool";
    orangestar.parent = planet_earth;
    orangestar.roomRanges.emplace_back(16400, 16499);
    auto orange_star = assembleArea(orangestar);

    AreaDef ath;
    ath.name = "Athletic Field";
    ath.parent = orange_star;
    ath.roomRanges.emplace_back(15900, 15937);
    auto athletic_field = assembleArea(ath);

    AreaDef oak;
    oak.name = "Inside an Oak Tree";
    oak.parent = areaObjects["Northern Plains"];
    oak.roomRanges.emplace_back(16200, 16210);
    oak.roomIDs = {19199};
    auto oak_tree = assembleArea(oak);

    AreaDef edfhq;
    edfhq.name = "EDF Headquarters";
    edfhq.parent = planet_earth;
    edfhq.type = AreaType::Structure;
    edfhq.roomRanges.emplace_back(9101, 9110);
    auto edf_hq = assembleArea(edfhq);

    AreaDef bar;
    bar.name = "Bar";
    bar.parent = planet_earth;
    bar.type = AreaType::Structure;
    bar.roomRanges.emplace_back(18100, 18114);
    auto bar_ = assembleArea(bar);

    AreaDef themoon;
    themoon.name = "The Moon";
    themoon.parent = space;
    themoon.type = AreaType::CelestialBody;
    themoon.gravity = 10.0;
    auto moon = assembleArea(themoon);

    AreaDef luncrat;
    luncrat.name = "Lunar Crater";
    luncrat.parent = moon;
    luncrat.roomRanges.emplace_back(63300, 63311);
    auto lunar_crater = assembleArea(luncrat);

    AreaDef cratpass;
    cratpass.name = "Crater Passage";
    cratpass.parent = moon;
    cratpass.roomRanges.emplace_back(63312, 63336);
    auto crater_passage = assembleArea(cratpass);

    AreaDef darkside;
    darkside.name = "Darkside Crater";
    darkside.parent = moon;
    darkside.roomRanges.emplace_back(63337, 63362);
    auto darkside_crater = assembleArea(darkside);

    AreaDef moonstone;
    moonstone.name = "Moonstone Quarry";
    moonstone.parent = moon;
    moonstone.roomRanges.emplace_back(63381, 63392);
    auto moonstone_quarry = assembleArea(moonstone);

    AreaDef intrepidbase;
    intrepidbase.name = "Intrepid Base";
    intrepidbase.parent = moon;
    intrepidbase.roomRanges.emplace_back(63363, 63380);
    intrepidbase.roomRanges.emplace_back(63393, 63457);
    auto intrepid_base = assembleArea(intrepidbase);

    AreaDef fortemple;
    fortemple.name = "Forgotten Temple";
    fortemple.parent = moon;
    fortemple.roomRanges.emplace_back(63458, 63499);
    auto forgotten_temple = assembleArea(fortemple);

    for(auto child : areas[moon].children) {
        auto &a = areas[child];
        for(auto r : a.rooms) {
            ROOM_FLAGS(r).reset(ROOM_EARTH);
        }
    }

    AreaDef prideplains;
    prideplains.name = "Pride Plains";
    prideplains.parent = planet_vegeta;
    prideplains.roomRanges.emplace_back(19700, 19711);
    auto pride_plains = assembleArea(prideplains);

    AreaDef pridesomething;
    pridesomething.name = "Pride Something";
    pridesomething.parent = planet_vegeta;
    pridesomething.roomRanges.emplace_back(19740, 19752);
    auto pride_something = assembleArea(pridesomething);

    AreaDef pridejungle;
    pridejungle.name = "Pride Jungle";
    pridejungle.parent = planet_vegeta;
    pridejungle.roomRanges.emplace_back(19712, 19718);
    pridejungle.roomRanges.emplace_back(19753, 19789);
    auto pride_jungle = assembleArea(pridejungle);

    AreaDef pridecave;
    pridecave.name = "Pride Cave";
    pridecave.parent = planet_vegeta;
    pridecave.roomRanges.emplace_back(9400, 9499);
    auto pride_cave = assembleArea(pridecave);

    AreaDef pridedesert;
    pridedesert.name = "Pride Desert";
    pridedesert.parent = planet_vegeta;
    pridedesert.roomRanges.emplace_back(19719, 19739);
    pridedesert.roomIDs.insert(19790);
    auto pride_desert = assembleArea(pridedesert);

    AreaDef rocktail;
    rocktail.name = "Rocktail Camp";
    rocktail.parent = planet_vegeta;
    rocktail.roomRanges.emplace_back(61030, 61044);
    rocktail.roomIDs.insert(19198);
    auto rocktail_camp = assembleArea(rocktail);

    AreaDef lavaarena;
    lavaarena.name = "Lava Arena";
    lavaarena.parent = planet_frigid;
    lavaarena.roomRanges.emplace_back(12900, 12918);
    auto lava_arena = assembleArea(lavaarena);

    AreaDef strangecliff;
    strangecliff.name = "Strange Cliff";
    strangecliff.parent = planet_namek;
    strangecliff.roomRanges.emplace_back(12800, 12813);
    auto strange_cliff = assembleArea(strangecliff);

    AreaDef stonehallway;
    stonehallway.name = "Stone Hallway";
    stonehallway.parent = planet_namek;
    stonehallway.roomRanges.emplace_back(12814, 12831);
    stonehallway.roomSkips.insert(12825);
    auto stone_hallway = assembleArea(stonehallway);

    AreaDef tranquilpalm;
    tranquilpalm.name = "Tranquil Palm Dojo";
    tranquilpalm.parent = planet_namek;
    tranquilpalm.roomRanges.emplace_back(12832, 12868);
    auto tranquil_palm_dojo = assembleArea(tranquilpalm);

    AreaDef namekunder;
    namekunder.name = "Namekian Underground";
    namekunder.parent = planet_namek;
    namekunder.roomRanges.emplace_back(64700, 65009);
    auto namek_underground = assembleArea(namekunder);

    AreaDef advkindojo;
    advkindojo.name = "Advanced Kinetic Dojo";
    advkindojo.parent = planet_aether;
    advkindojo.roomRanges.emplace_back(17743, 17751);
    auto advanced_kinetic_dojo = assembleArea(advkindojo);

    AreaDef lostcity;
    lostcity.name = "Lost City";
    lostcity.parent = planet_kanassa;
    lostcity.roomRanges.emplace_back(7600, 7686);
    auto lost_city = assembleArea(lostcity);

    AreaDef aqtower;
    aqtower.name = "Aquis Tower";
    aqtower.parent = areaObjects["Aquis City"];
    aqtower.roomRanges.emplace_back(12628, 12666);
    auto aquis_tower = assembleArea(aqtower);

    AreaDef moaipalace;
    moaipalace.name = "Moai's Palace";
    moaipalace.parent = planet_arlia;
    moaipalace.roomRanges.emplace_back(12667, 12699);
    auto moai_palace = assembleArea(moaipalace);

    AreaDef darkthorne;
    darkthorne.name = "DarkThorne Compound";
    darkthorne.parent = planet_arlia;
    darkthorne.roomRanges.emplace_back(18150, 18169);
    auto darkthorne_compound = assembleArea(darkthorne);


    std::unordered_map<int, vnum> planetMap = {
            {ROOM_EARTH, planet_earth},
            {ROOM_VEGETA, planet_vegeta},
            {ROOM_FRIGID, planet_frigid},
            {ROOM_NAMEK, planet_namek},
            {ROOM_YARDRAT, planet_yardrat},
            {ROOM_KONACK, planet_konack},
            {ROOM_AETHER, planet_aether},
            {ROOM_KANASSA, planet_kanassa},
            {ROOM_ARLIA, planet_arlia},
            {ROOM_CERRIA, planet_cerria},
    };

    basic_mud_log("Attempting to deduce Areas to Planets...");
    for(auto &[vnum, room] : world) {
        // check for planetMap flags and, if found, bind the area this room belongs to, to the respective planet.

        for(auto &p : planetMap) {
            if(!room.area) continue;
            if(room.room_flags.test(p.first)) {
                auto avn = room.area.value();
                auto &a = areas[avn];
                auto &pl = areas[p.second];
                pl.children.insert(avn);
                a.parent = p.second;
                break;
            }
        }
    }
    basic_mud_log("Done deducing Areas to Planets.");


    AreaDef nodef;
    nodef.name = "Northran";
    nodef.parent = xenoverse;
    nodef.type = AreaType::Dimension;
    nodef.roomRanges.emplace_back(17900, 17999);
    auto northran = assembleArea(nodef);

    AreaDef celdef;
    celdef.name = "Celestial Corp";
    celdef.parent = space;
    celdef.type = AreaType::Structure;
    celdef.roomRanges.emplace_back(16305, 16399);
    for(auto &[rv, room] : world) {
        if(icontains(stripAnsi(room.name), "Celestial Corp")) celdef.roomIDs.insert(rv);
    }
    auto celestial_corp = assembleArea(celdef);

    AreaDef gneb;
    gneb.name = "Green Nebula Mall";
    gneb.parent = space;
    gneb.type = AreaType::Structure;
    gneb.roomRanges.emplace_back(17200, 17276);
    gneb.roomIDs.insert(184);
    auto green_nebula = assembleArea(gneb);

    AreaDef cooler;
    cooler.name = "Cooler's Ship";
    cooler.parent = space;
    cooler.type = AreaType::Structure;
    for(auto &[rv, room] : world) {
        if(icontains(stripAnsi(room.name), "Cooler's Ship")) {
            cooler.roomIDs.insert(rv);
        }
    }
    auto cooler_ship = assembleArea(cooler);

    AreaDef alph;
    alph.name = "Alpharis";
    alph.type = AreaType::Structure;
    alph.parent = space;
    for(auto &[rv, room] : world) {
        if(icontains(stripAnsi(room.name), "Alpharis")) alph.roomIDs.insert(rv);
    }
    auto alpharis = assembleArea(alph);

    AreaDef dzone;
    dzone.name = "Dead Zone";
    dzone.parent = universe7;
    dzone.type = AreaType::Dimension;
    for(auto &[rv, room] : world) {
        if(icontains(stripAnsi(room.name), "Dead Zone")) dzone.roomIDs.insert(rv);
    }
    auto dead_zone = assembleArea(dzone);

    AreaDef bast;
    bast.name = "Blasted Asteroid";
    bast.parent = space;
    bast.type = AreaType::CelestialBody;
    for(auto &[rv, room] : world) {
        if(icontains(stripAnsi(room.name), "Blasted Asteroid")) bast.roomIDs.insert(rv);
    }
    auto blasted_asteroid = assembleArea(bast);


    AreaDef listres;
    listres.name = "Lister's Restaurant";
    listres.parent = xenoverse;
    listres.type = AreaType::Structure;
    for(auto &[rv, room] : world) {
        if(icontains(stripAnsi(room.name), "Lister's Restaurant")) listres.roomIDs.insert(rv);
    }
    listres.roomIDs = {18640};
    auto listers_restaurant = assembleArea(listres);

    AreaDef scasino;
    scasino.name = "Shooting Star Casino";
    scasino.type = AreaType::Structure;
    scasino.parent = xenoverse;
    for(auto &[rv, room] : world) {
        if(icontains(stripAnsi(room.name), "Shooting Star Casino")) scasino.roomIDs.insert(rv);
    }
    auto shooting_star_casino = assembleArea(scasino);

    AreaDef outdef;
    outdef.name = "The Outpost";
    outdef.parent = celestial_plane;
	outdef.type = AreaType::Structure;
    for(auto &[rv, room] : world) {
        if(icontains(stripAnsi(room.name), "The Outpost")) outdef.roomIDs.insert(rv);
    }
    auto outpost = assembleArea(outdef);

    AreaDef kyem;
    kyem.name = "King Yemma's Domain";
    kyem.parent = celestial_plane;
    kyem.roomRanges.emplace_back(6000, 6030);
    kyem.roomSkips.insert(6017);
    kyem.roomIDs.insert(6295);
    auto king_yemma = assembleArea(kyem);

    AreaDef snway;
    snway.name = "Snake Way";
    snway.parent = celestial_plane;
    snway.roomRanges.emplace_back(6031, 6099);
    snway.roomIDs.insert(6017);
    auto snake_way = assembleArea(snway);

    AreaDef nkai;
    nkai.name = "North Kai's Planet";
    nkai.parent = celestial_plane;
    nkai.gravity = 10.0;
    nkai.type = AreaType::CelestialBody;
    nkai.roomRanges.emplace_back(6100, 6138);
    auto north_kai = assembleArea(nkai);

    AreaDef serp;
    serp.name = "Serpent's Castle";
    serp.parent = snake_way;
    serp.type = AreaType::Structure;
    serp.roomRanges.emplace_back(6139, 6166);
    auto serpents_castle = assembleArea(serp);

    AreaDef gkai;
    gkai.name = "Grand Kai's Planet";
    gkai.parent = celestial_plane;
    gkai.type = AreaType::CelestialBody;
    gkai.roomRanges.emplace_back(6800, 6960);
    auto grand_kai = assembleArea(gkai);

    AreaDef gkaipalace;
    gkaipalace.name = "Grand Kai's Palace";
    gkaipalace.parent = grand_kai;
    gkaipalace.type = AreaType::Structure;
    gkaipalace.roomRanges.emplace_back(6961, 7076);
    auto grand_kais_palace = assembleArea(gkaipalace);

    AreaDef maze;
    maze.name = "Maze of Echoes";
    maze.parent = celestial_plane;
    maze.roomRanges.emplace_back(7100, 7199);
    auto maze_of_echoes = assembleArea(maze);

    AreaDef cat;
    cat.name = "Dark Catacomb";
    cat.parent = maze_of_echoes;
    cat.roomRanges.emplace_back(7200, 7245);
    auto dark_catacomb = assembleArea(cat);

    AreaDef twi;
    twi.name = "Twilight Cavern";
    twi.parent = celestial_plane;
    twi.roomRanges.emplace_back(7300, 7499);
    auto twilight_cavern = assembleArea(twi);

    AreaDef helldef;
    helldef.name = "Hell";
    helldef.parent = celestial_plane;
    helldef.roomRanges.emplace_back(6200, 6298);
    helldef.roomSkips.insert(6295);
    auto hell = assembleArea(helldef);

    AreaDef hellhouse;
    hellhouse.name = "Hell - Old House";
    hellhouse.parent = hell;
    hellhouse.roomRanges.emplace_back(61000, 61007);
    auto hell_old_house = assembleArea(hellhouse);

    AreaDef gyukihouse;
    gyukihouse.name = "Gyuki's House";
    gyukihouse.parent = planet_earth;
    gyukihouse.roomRanges.emplace_back(61015, 61026);
    auto gyukis_house = assembleArea(gyukihouse);

    AreaDef hfields;
    hfields.name = "Hell Fields";
    hfields.parent = hell;
    hfields.roomRanges.emplace_back(6200, 6300);
    auto hell_fields = assembleArea(hfields);

    AreaDef hsands;
    hsands.name = "Sands of Time";
    hsands.parent = hell;
    hsands.roomRanges.emplace_back(6300, 6348);
    auto sands_of_time = assembleArea(hsands);

    AreaDef hchaotic;
    hchaotic.name = "Chaotic Spiral";
    hchaotic.parent = hell;
    hchaotic.roomRanges.emplace_back(6349, 6399);
    auto chaotic_spiral = assembleArea(hchaotic);

    AreaDef hfirecity;
    hfirecity.name = "Hellfire City";
    hfirecity.parent = hell;
    hfirecity.roomRanges.emplace_back(6400, 6529);
    hfirecity.roomIDs = {6568, 6569, 6600, 6699};
    auto hellfire_city = assembleArea(hfirecity);

    AreaDef fbagdojo;
    fbagdojo.name = "Flaming Bag Dojo";
    fbagdojo.type = AreaType::Structure;
    fbagdojo.parent = hellfire_city;
    fbagdojo.roomRanges.emplace_back(6530, 6568);
    auto flaming_bag_dojo = assembleArea(fbagdojo);

    AreaDef etrailgrave;
    etrailgrave.name = "Entrail Graveyard";
    etrailgrave.parent = hellfire_city;
    etrailgrave.roomRanges.emplace_back(6601, 6689);
    auto entrail_graveyard = assembleArea(etrailgrave);

    AreaDef psihnon;
    psihnon.name = "Sihnon";
    psihnon.parent = space;
    psihnon.type = AreaType::CelestialBody;
    psihnon.roomRanges.emplace_back(3600, 3699);
    auto planet_sihnon = assembleArea(psihnon);

    AreaDef majdef;
    majdef.name = "Majinton";
    majdef.parent = planet_sihnon;
    majdef.type = AreaType::Dimension;
    majdef.roomRanges.emplace_back(3700, 3797);
    auto majinton = assembleArea(majdef);

    AreaDef wistower;
    wistower.name = "Wisdom Tower";
    wistower.parent = planet_namek;
    wistower.type = AreaType::Structure;
    wistower.roomRanges.emplace_back(9600, 9666);
    auto wisdom_tower = assembleArea(wistower);

    AreaDef veld;
    veld.name = "Veldryth Mountains";
    veld.parent = planet_konack;
    veld.roomRanges.emplace_back(9300, 9355);
    auto veldryth_mountains = assembleArea(veld);

    AreaDef machia;
    machia.name = "Machiavilla";
    machia.parent = planet_konack;
    machia.type = AreaType::Structure;
    machia.roomRanges.emplace_back(12743, 12798);
    machia.roomRanges.emplace_back(12700, 12761);
    machia.roomIDs.insert(9356);
    auto machiavilla = assembleArea(machia);

    AreaDef laron;
    laron.name = "Laron Forest";
    laron.parent = planet_konack;
    laron.roomRanges.emplace_back(19200, 19299);
    auto laron_forest = assembleArea(laron);

    AreaDef nazr;
    nazr.name = "Nazrin Village";
    nazr.parent = planet_konack;
    nazr.roomRanges.emplace_back(19300, 19347);
    nazr.roomIDs = {19398};
    auto nazrin_village = assembleArea(nazr);

    AreaDef nazchief;
    nazchief.name = "Chieftain's House";
    nazchief.type = AreaType::Structure;
    nazchief.parent = nazrin_village;
    nazchief.roomRanges.emplace_back(19348, 19397);
    auto chieftains_house = assembleArea(nazchief);

    AreaDef shmaze;
    shmaze.name = "Shadow Maze";
    shmaze.type = AreaType::Structure;
    shmaze.parent = chieftains_house;
    shmaze.roomRanges.emplace_back(19400, 19499);
    auto shadow_maze = assembleArea(shmaze);

    AreaDef monbal;
    monbal.name = "Monastery of Balance";
    monbal.type = AreaType::Structure;
    monbal.parent = planet_konack;
    monbal.roomRanges.emplace_back(9500, 9599);
    monbal.roomRanges.emplace_back(9357, 9364);
    monbal.roomIDs.insert(9365);
    auto monastery_of_balance = assembleArea(monbal);

    AreaDef futschool;
    futschool.name = "Future School";
    futschool.parent = xenoverse;
    futschool.type = AreaType::Dimension;
    futschool.roomRanges.emplace_back(15938, 15999);
    auto future_school = assembleArea(futschool);

    AreaDef udfhq;
    udfhq.name = "UDF Headquarters";
    udfhq.parent = space;
    udfhq.type = AreaType::Structure;
    udfhq.roomRanges.emplace_back(18000, 18059);
    auto udf_headquarters = assembleArea(udfhq);

    AreaDef hspire;
    hspire.name = "The Haven Spire";
    hspire.parent = space;
    hspire.type = AreaType::Structure;
    hspire.roomRanges.emplace_back(18300, 18341);
    auto haven_spire = assembleArea(hspire);

    AreaDef knoit;
    knoit.name = "Kame no Itto";
    knoit.parent = space;
    knoit.type = AreaType::Structure;
    knoit.roomRanges.emplace_back(18400, 18460);
    auto kame_no_itto = assembleArea(knoit);

    AreaDef neonirvana;
    neonirvana.name = "Neo Nirvana";
    neonirvana.parent = space;
    neonirvana.type = AreaType::Structure;
    neonirvana.roomRanges.emplace_back(13500, 13552);
    neonirvana.roomRanges.emplace_back(14782, 14790);
    auto neo_nirvana = assembleArea(neonirvana);

    AreaDef neohologram;
    neohologram.name = "Hologram Combat";
    neohologram.parent = neo_nirvana;
    neohologram.roomRanges.emplace_back(13553, 13567);
    auto neo_hologram_combat = assembleArea(neohologram);

    AreaDef neonexusfield;
    neonexusfield.name = "Nexus Field";
    neonexusfield.parent = neo_hologram_combat;
    neonexusfield.roomRanges.emplace_back(13568, 13612);
    auto neo_nexus_field = assembleArea(neonexusfield);

    AreaDef neonamekgrassyisland;
    neonamekgrassyisland.name = "Namek: Grassy Island";
    neonamekgrassyisland.parent = neo_hologram_combat;
    neonamekgrassyisland.roomRanges.emplace_back(13613, 13657);
    auto neo_namek_grassy_island = assembleArea(neonamekgrassyisland);

    AreaDef neoslavemarket;
    neoslavemarket.name = "Slave Market";
    neoslavemarket.parent = neo_hologram_combat;
    neoslavemarket.roomRanges.emplace_back(13658, 13702);
    auto neo_slave_market = assembleArea(neoslavemarket);

    AreaDef neokanassa;
    neokanassa.name = "Kanassa: Blasted Battlefield";
    neokanassa.parent = neo_hologram_combat;
    neokanassa.roomRanges.emplace_back(13703, 13747);
    auto neo_kanassa_blasted_battlefield = assembleArea(neokanassa);

    AreaDef neosilentglade;
    neosilentglade.name = "Silent Glade";
    neosilentglade.parent = neo_hologram_combat;
    neosilentglade.roomRanges.emplace_back(13748, 13792);
    auto neo_silent_glade = assembleArea(neosilentglade);

    AreaDef neohell;
    neohell.name = "Hell - Flat Plains";
    neohell.parent = neo_hologram_combat;
    neohell.roomRanges.emplace_back(13793, 13837);
    auto neo_hell_flat_plains = assembleArea(neohell);

    AreaDef neosandydesert;
    neosandydesert.name = "Sandy Desert";
    neosandydesert.parent = neo_hologram_combat;
    neosandydesert.roomRanges.emplace_back(13838, 13882);
    auto neo_sandy_desert = assembleArea(neosandydesert);

    AreaDef neotopicasnowfield;
    neotopicasnowfield.name = "Topica Snowfield";
    neotopicasnowfield.parent = neo_hologram_combat;
    neotopicasnowfield.roomRanges.emplace_back(13883, 13927);
    auto neo_topica_snow_field = assembleArea(neotopicasnowfield);

    AreaDef neogerolab;
    neogerolab.name = "Gero's Lab";
    neogerolab.parent = neo_hologram_combat;
    neogerolab.roomRanges.emplace_back(13928, 14517);
    auto neo_geros_lab = assembleArea(neogerolab);

    AreaDef neocandyland;
    neocandyland.name = "Candy Land";
    neocandyland.parent = neo_hologram_combat;
    neocandyland.roomRanges.emplace_back(14518, 14562);
    auto neo_candy_land = assembleArea(neocandyland);

    AreaDef neoancestralmountains;
    neoancestralmountains.name = "Ancestral Mountains";
    neoancestralmountains.parent = neo_hologram_combat;
    neoancestralmountains.roomRanges.emplace_back(14563, 14607);
    auto neo_ancestral_mountains = assembleArea(neoancestralmountains);

    AreaDef neoelzthuanforest;
    neoelzthuanforest.name = "Elzthuan Forest";
    neoelzthuanforest.parent = neo_hologram_combat;
    neoelzthuanforest.roomRanges.emplace_back(14608, 14652);
    auto neo_elzthuan_forest = assembleArea(neoelzthuanforest);

    AreaDef neoyardracity;
    neoyardracity.name = "Yardra City";
    neoyardracity.parent = neo_hologram_combat;
    neoyardracity.roomRanges.emplace_back(14653, 14697);
    auto neo_yardra_city = assembleArea(neoyardracity);

    AreaDef neoancientcoliseum;
    neoancientcoliseum.name = "Ancient Coliseum";
    neoancientcoliseum.parent = neo_hologram_combat;
    neoancientcoliseum.roomRanges.emplace_back(14698, 14742);
    auto neo_ancient_coliseum = assembleArea(neoancientcoliseum);

    AreaDef fortrancomplex;
    fortrancomplex.name = "Fortran Complex";
    fortrancomplex.parent = neo_nirvana;
    fortrancomplex.roomRanges.emplace_back(14743, 14772);
    auto fortran_complex = assembleArea(fortrancomplex);

    AreaDef revolutionpark;
    revolutionpark.name = "Revolution Park";
    revolutionpark.parent = neo_nirvana;
    revolutionpark.roomRanges.emplace_back(14773, 14802);
    auto revolution_park = assembleArea(revolutionpark);

    AreaDef akatsukilabs;
    akatsukilabs.name = "Akatsuki Labs";
    akatsukilabs.parent = neo_nirvana;
    akatsukilabs.roomRanges.emplace_back(14800, 14893);
    auto akatsuki_labs = assembleArea(akatsukilabs);

    AreaDef southgal;
    southgal.name = "South Galaxy";
    southgal.parent = mortal_plane;
    southgal.roomIDs = {64300, 64399};
    auto south_galaxy = assembleArea(southgal);

    AreaDef undergroundpassage;
    undergroundpassage.name = "Underground Passage";
    undergroundpassage.parent = planet_namek;
    undergroundpassage.roomRanges.emplace_back(12869, 12899);
    auto underground_passage = assembleArea(undergroundpassage);

    AreaDef shatplan;
    shatplan.name = "Shattered Planet";
    shatplan.parent = south_galaxy;
    shatplan.type = AreaType::CelestialBody;
    shatplan.roomRanges.emplace_back(64301, 64399);
    auto shattered_planet = assembleArea(shatplan);

    AreaDef wzdef;
    wzdef.name = "War Zone";
    wzdef.parent = xenoverse;
    wzdef.type = AreaType::Structure;
    wzdef.roomRanges.emplace_back(17700, 17702);
    auto war_zone = assembleArea(wzdef);

    AreaDef corlight;
    corlight.name = "Corridor of Light";
    corlight.parent = war_zone;
    corlight.roomRanges.emplace_back(17703, 17722);
    auto corridor_of_light = assembleArea(corlight);

    AreaDef cordark;
    cordark.name = "Corridor of Darkness";
    cordark.parent = war_zone;
    cordark.roomRanges.emplace_back(17723, 17743);
    auto corridor_of_darkness = assembleArea(cordark);

    AreaDef soisland;
    soisland.name = "South Ocean Island";
    soisland.parent = planet_earth;
    soisland.roomRanges.emplace_back(6700, 6758);
    auto south_ocean_island = assembleArea(soisland);

    AreaDef hhouse;
    hhouse.name = "Haunted House";
    hhouse.parent = xenoverse;
    hhouse.type = AreaType::Dimension;
    hhouse.roomRanges.emplace_back(18600, 18693);
    auto haunted_house = assembleArea(hhouse);

    AreaDef roc;
    roc.name = "Random Occurences, WTF?";
    roc.parent = xenoverse;
    roc.type = AreaType::Dimension;
    roc.roomRanges.emplace_back(18700, 18776);
    auto random_occurences = assembleArea(roc);

    AreaDef galstrong;
    galstrong.name = "Galaxy's Strongest Tournament";
    galstrong.parent = space;
    galstrong.type = AreaType::Structure;
    galstrong.roomRanges.emplace_back(17875, 17894);
    auto galaxy_strongest_tournament = assembleArea(galstrong);

    AreaDef arwater;
    arwater.name = "Arena - Water";
    arwater.parent = galaxy_strongest_tournament;
    arwater.roomRanges.emplace_back(17800, 17824);
    auto arena_water = assembleArea(arwater);

    AreaDef arring;
    arring.name = "Arena - The Ring";
    arring.parent = galaxy_strongest_tournament;
    arring.roomRanges.emplace_back(17825, 17849);
    auto arena_ring = assembleArea(arring);

    AreaDef arsky;
    arsky.name = "Arena - In the Sky";
    arsky.parent = galaxy_strongest_tournament;
    arsky.roomRanges.emplace_back(17850, 17875);
    auto arena_sky = assembleArea(arsky);

    AreaDef stdef;
    stdef.name = "Structures";
    auto structures = assembleArea(stdef);

    AreaDef spdef;
    spdef.name = "Spaceships";
    auto spaceships = assembleArea(spdef);

    auto crunch_ship = [&](old_ship_data &data) {

        AreaDef sdata;
        sdata.name = data.name;
        sdata.roomIDs = data.vnums;
        sdata.type = AreaType::Vehicle;
        sdata.parent = spaceships;
        auto ship = assembleArea(sdata);
        auto &s = areas[ship];
        if(data.ship_obj) s.extraVn = data.ship_obj.value();

        return ship;
    };

    for(auto &sd : gships) {
        crunch_ship(sd);
    }

    for(auto &sd : customs) {
        crunch_ship(sd);
    }

    // A very luxurious player custom home
    AreaDef dunnoHouse;
    dunnoHouse.name = "Dunno's House";
    dunnoHouse.parent = xenoverse;
    dunnoHouse.roomIDs = {19009, 19010, 19011, 19012, 19013, 19014, 19015, 19016, 19017, 19018,
                          19019, 19020, 19021, 19022, 19023};
    auto dunno_house = assembleArea(dunnoHouse);

    // This looks like an unused old player home, seems like it's attached to Cherry Blossom Mountain?
    AreaDef mountainFortress;
    mountainFortress.name = "Mountaintop Fortress";
    mountainFortress.parent = xenoverse;
    mountainFortress.roomIDs = {19025, 19026, 19027, 19028, 19029, 19030, 19031, 19032,
                                19033, 19034, 19035, 19036, 19037, 19038, 19024};
    auto mountain_fortress = assembleArea(mountainFortress);

    // Personal Ships / Pods...
    for(auto vn = 45000; vn <= 45199; vn++) {
        auto ovn = vn + 1000;
        auto o = obj_proto.find(ovn);
        if(o == obj_proto.end()) continue;
        old_ship_data shipData;
        shipData.name = o->second.name;
        shipData.ship_obj = ovn;
        shipData.vnums.insert(vn);
        shipData.hatch_room = vn;
        auto v = crunch_ship(shipData);
    }

    AreaDef sphouses;
    sphouses.name = "Small Player Houses";
    sphouses.parent = structures;
    auto small_player_houses = assembleArea(sphouses);

    int count = 1;
    for(auto i = 18800; i != 18896; i += 4) {
        AreaDef house;
        house.name = fmt::format("Small Player House {}", count++);
        house.roomRanges.emplace_back(i, i+3);
        house.parent = small_player_houses;
        assembleArea(house);
    }

    AreaDef mdhouses;
    mdhouses.name = "Deluxe Player Houses";
    mdhouses.parent = structures;
    auto medium_player_houses = assembleArea(mdhouses);

    count = 1;
    for(auto i = 18900; i != 18995; i += 5) {
        AreaDef house;
        house.name = fmt::format("Deluxe Player House {}", count++);
        house.roomRanges.emplace_back(i, i+4);
        house.parent = medium_player_houses;
        assembleArea(house);
    }

    AreaDef lphouses;
    lphouses.name = "Excellent Player Houses";
    lphouses.parent = structures;
    auto large_player_houses = assembleArea(lphouses);

    count = 1;
    for(auto i = 19100; i != 19195; i += 5) {
        AreaDef house;
        house.name = fmt::format("Excellent Player House {}", count++);
        house.roomRanges.emplace_back(i, i+4);
        house.parent = large_player_houses;
        assembleArea(house);
    }

    AreaDef pdimen;
    pdimen.name = "Personal Pocket Dimensions";
    auto personal_dimensions = assembleArea(pdimen);

    int counter = 1;
    for(auto vn = 19800; vn <= 19899; vn++) {
        AreaDef pdim;
        pdim.name = "Personal Pocket Dimension " + std::to_string(counter++);
        pdim.parent = personal_dimensions;
        pdim.roomIDs.insert(vn);
        pdim.type = AreaType::Dimension;
        pdim.gravity = 1000.0;
        auto pd = assembleArea(pdim);
    }

    AreaDef misc;
    misc.name = "Miscellaneous";
    for(auto &[rv, room] : world) {
        if(!room.area) misc.roomIDs.insert(rv);
    }
    auto misc_area = assembleArea(misc);

    for(auto r : {
        300, 800, 1150, 1180, 1287, 1428, 1456, 1506, 1636, 1710, 19510, 2141, 13020, // earth vnums
    	4264, 4300, 4351, 4400, 4600, 4800, 5100, 5150, 5165, 5200, 5500, 4944, // frigid vnums
        8006, 8300, 8400, 8447, 8500, 8600, 8700, 8800, 8900, 8954, 9200, 9700, 9855, 9864, 9900, 9949, // konack
        2226, 2600, 2616, 2709, 2800, 2899, 2615, // vegeta
        11600, 10182, 10474, 13300, 10203, 10922, 11600, // namek
        12010, 12103, 12300, 12400, 12480, 12010, // Aether
        14008, 14100, 14200, 14300, // yardrat
        17531, 7950, 17420, // cerria
        3412, 3520, 19600, // zenith
        14904, 15655, // kanassa
        16009, 16544, 16600 // Arlia
    }) {
        ROOM_FLAGS(r).set(ROOM_LANDING);
    }
}

static std::vector<std::pair<std::string, vnum>> characterToAccount;

void migrate_accounts() {
	// user files are stored in <cwd>/user/<folder>/<file>.usr so we'll need to do recursive iteration using
    // C++ std::filesystem...

    auto path = std::filesystem::current_path() / "user";
    if(!std::filesystem::exists(path)) {
        basic_mud_log("No user directory found, skipping account migration.");
        return;
    }

    for(auto &p : std::filesystem::recursive_directory_iterator(path)) {
        if(p.path().extension() != ".usr") continue;
        auto accFile = p.path().parent_path().filename().string();

        // Open file for reading...
        std::ifstream file(p.path());

        // Step 1: create an ID for this account...
        auto id = account_data::getNextID();

        // Now let's get a new account_data...
        auto a = std::make_shared<account_data>();
        accounts[id] = a;

        // Moving forward, we assume that every account file is using the above structure and is valid.
        // Don't second-guess it, just process.

        // Line 1: Name (string)
        std::getline(file, a->name);

        // Line 2: Email Address (string)
        std::getline(file, a->email);

        // Line 3: password (clear text, will hash...)
        std::string pass;
        std::getline(file, pass);
        if(!a->setPassword(pass)) basic_mud_log("Error hashing %s's password: %s", a->name.c_str(), pass.c_str());

        // Line 4: slots (int)
        std::string slots;
        std::getline(file, slots);
        a->slots = std::stoi(slots);

        // Line 5: current RPP (int)
        std::string rpp;
        std::getline(file, rpp);
        a->rpp = std::stoi(rpp);

        // Now for the Character lines, they either contain a name, or they contain "Empty".
        // "Empty" is not a character. It's a placeholder for an empty slot.
        // For now, we will move through all five lines, and for any that do not contain "Empty",
        // we insert the character name into the characterToAccount map.

        for(int i = 0;i < 5; i++) {
            std::string charName;
            std::getline(file, charName);
            if(charName != "Empty") {
                characterToAccount.emplace_back(charName, id);
            }
        }

        // Line 11: adminLevel (int)
        std::string adminLevel;
        std::getline(file, adminLevel);
        a->adminLevel = std::stoi(adminLevel);

        // Line 12: customFile present (bool)
        std::string customFile;
        std::getline(file, customFile);
        bool custom = std::stoi(customFile);

        // if custom, then we want to open a sister file that ends in .cus in the same directory and read every line
        // beyond the first into a.customs.
        if(custom) {
            auto customPath = p.path();
            customPath.replace_extension(".cus");
            std::ifstream customFile(customPath);
            std::string line;
            std::getline(customFile, line); // skip the first line
            while(std::getline(customFile, line)) {
                a->customs.emplace_back(line);
            }
            customFile.close();
        }

        // Line 13: RPP bank (unused)
        std::string rppBank;
        std::getline(file, rppBank);
        auto bank = std::stoi(rppBank);
        file.close();
        a->vn = id;

    }
}

void migrate_characters() {
    // Unlike accounts, player files are indexed. However, unless their name showed up in an account,
    // there's no point migrating them.

    // The procedure we will use is: iterate through characterToAccount and attempt to load the character.
    // if we can load them, we'll convert them and bind them to the appropriate account.

    for(auto &[cname, accID] : characterToAccount) {
        auto ch = new char_data();
        ch->script = std::make_shared<script_data>(ch);
        if(load_char(cname.c_str(), ch) < 0) {
            basic_mud_log("Error loading %s for account migration.", cname.c_str());
            delete ch;
            continue;
        }
        auto id = ch->id;
        auto p = std::make_shared<player_data>();
        p->id = id;
        players[id] = p;
        p->id = id;
        if(!ch->generation) ch->generation = time(nullptr);
        p->character = ch;
        p->name = ch->name;
        auto a = accounts[accID];
        accounts[accID] = a;
        p->account = a;
        a->adminLevel = std::max(a->adminLevel, GET_ADMLEVEL(ch));
        a->characters.emplace_back(id);
        ch->in_room = ch->load_room;
        ch->was_in_room = ch->load_room;
        uniqueCharacters[id] = std::make_pair(ch->generation, ch);
    }

    // migrate sense files...
    auto path = std::filesystem::current_path() / "sense";
    if(!std::filesystem::exists(path)) {
        basic_mud_log("No sense directory found, skipping account migration.");
        return;
    }

    for(auto &p : std::filesystem::recursive_directory_iterator(path)) {
        if(p.path().extension() != ".sen") continue;

        // use the file stem against findPlayer...
        auto name = p.path().stem().string();
        auto ch = findPlayer(name);
        if(!ch) {
            basic_mud_log("Error loading %s for sense migration.", name.c_str());
            continue;
        }
        auto pa = players[ch->id];
        // The file contains a sequence of lines, with each line containing a number.
		// The number is the vnum of a mobile the player's sensed.
        // We will read each line and insert the vnum into the player's sensed list.
        std::ifstream file(p.path());
        std::string line;
        while(std::getline(file, line)) {
            try {
                auto vnum = std::stoi(line);
                if(mob_proto.contains(vnum)) pa->senseMemory.insert(vnum);
            } catch(...) {
                basic_mud_log("Error parsing %s for sense migration.", line.c_str());
            }
        }
        file.close();
    }

    path = std::filesystem::current_path() / "intro";
    if(!std::filesystem::exists(path)) {
        basic_mud_log("No intro directory found, skipping intro migration.");
        return;
    }

    for(auto &p : std::filesystem::recursive_directory_iterator(path)) {
        if(p.path().extension() != ".itr") continue;

        // use the file stem against findPlayer...
        auto name = p.path().stem().string();
        auto ch = findPlayer(name);
        if(!ch) {
            basic_mud_log("Error loading %s for dub migration.", name.c_str());
            continue;
        }

        auto pa = players[ch->id];

		// The file contains a series of lines.
        // Each line looks like: <name> <dub>
        // We will need to use findPlayer on name, and then save id->dub to ch->player_specials->dubNames.
        // ignore if <name> == "Gibbles"

        std::ifstream file(p.path());
        std::string line;
        while(std::getline(file, line)) {
            auto pos = line.find(' ');
            if(pos == std::string::npos) continue;
            auto name = line.substr(0, pos);
            auto dub = line.substr(pos + 1);
            if(name == "Gibbles") continue;
            auto pc = findPlayer(name);
            if(!pc) continue;
            pa->dubNames[pc->id] = dub;
        }
    }

    path = std::filesystem::current_path() / "plrvars";
    if(!std::filesystem::exists(path)) {
        basic_mud_log("No intro directory found, skipping intro migration.");
        return;
    }

    for(auto &p : std::filesystem::recursive_directory_iterator(path)) {
        if(p.path().extension() != ".mem") continue;

        // use the file stem against findPlayer...
        auto name = p.path().stem().string();
        auto ch = findPlayer(name);
        if(!ch) {
            basic_mud_log("Error loading %s for variable migration.", name.c_str());
            continue;
        }

        // The file contains a series of lines.
        // each line looks like this: <varname> <context> <data>
        // where varname is a single-word string, context is an integer, and data is a string - although it might be an
        // empty string.
        std::ifstream file(p.path());
        std::string line;
        while(std::getline(file, line)) {
            auto pos = line.find(' ');
            if(pos == std::string::npos) continue;
            auto varname = line.substr(0, pos);
            auto pos2 = line.find(' ', pos + 1);
            if(pos2 == std::string::npos) continue;
            auto context = line.substr(pos + 1, pos2 - pos - 1);
            auto data = line.substr(pos2 + 1);

            try {
                ch->script->addVar(varname, data);
            } catch(...) {
                basic_mud_log("Error parsing %s for variable migration.", line.c_str());
            }
        }
    }

    path = std::filesystem::current_path() / "plralias";
    if(!std::filesystem::exists(path)) {
        basic_mud_log("No plralias directory found, skipping alias migration.");
        return;
    }

    for(auto &p : std::filesystem::recursive_directory_iterator(path)) {
        if(p.path().extension() != ".alias") continue;

        // use the file stem against findPlayer...
        auto name = p.path().stem().string();
        auto ch = findPlayer(name);
        if(!ch) {
            basic_mud_log("Error loading %s for alias migration.", name.c_str());
            continue;
        }
        auto pa = players.find(ch->id);
        if(pa == players.end()) {
            basic_mud_log("Error loading %s for alias migration.", name.c_str());
            continue;
        }


        std::ifstream file(p.path());
        std::string line;

        // Alias files are stored as sequences of this:
        // "%d\n%s\n%d\n%s\n%d\n", in order: alias name string length (size_t), alias name string,
        // replacement string length  (size_t), replacement string, alias type (a bool)

        while(std::getline(file, line)) {
            auto &a = pa->second->aliases.emplace_back();
            std::getline(file, a.name);
            std::getline(file, line);
            std::getline(file, a.replacement);
            std::getline(file, line);
            a.type = atoi(line.c_str());
        }
    }

    path = std::filesystem::current_path() / "plrobjs";

    for(auto &p : std::filesystem::recursive_directory_iterator(path)) {
        if(p.path().extension() != ".new") continue;

        // use the file stem against findPlayer...
        auto name = p.path().stem().string();
        auto ch = findPlayer(name);
        if(!ch) {
            basic_mud_log("Error loading %s for object migration.", name.c_str());
            continue;
        }

		auto result = Crash_load(ch);

    }
}

void migrate_db() {
    boot_db_legacy();
    House_boot();

    migrate_grid();

    migrate_accounts();

    try {
        migrate_characters();
    } catch(std::exception &e) {
        basic_mud_log("Error migrating characters: %s", e.what());
    }
}



void run_migration() {
    game::init_locale();
    load_config();
    chdir("lib");
    migrate_db();

    runSave();
}