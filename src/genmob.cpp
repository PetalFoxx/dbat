/************************************************************************
 * Generic OLC Library - Mobiles / genmob.c			v1.0	*
 * Copyright 1996 by Harvey Gilpin					*
 * Copyright 1997-2001 by George Greer (greerga@circlemud.org)		*
 ************************************************************************/

#include "dbat/genmob.h"
#include "dbat/utils.h"
#include "dbat/db.h"
#include "dbat/genolc.h"
#include "dbat/shop.h"
#include "dbat/genzon.h"
#include "dbat/guild.h"
#include "dbat/dg_scripts.h"
#include "dbat/handler.h"
#include "dbat/dg_olc.h"
#include "dbat/class.h"
#include "dbat/races.h"
#include "dbat/spells.h"
#include "dbat/objsave.h"
#include "dbat/players.h"
#include "dbat/account.h"

/* From db.c */
void init_mobile_skills();

int update_mobile_strings(struct char_data *t, struct char_data *f);

void check_mobile_strings(struct char_data *mob);

void check_mobile_string(mob_vnum i, char **string, const char *dscr);

int write_mobile_espec(mob_vnum mvnum, struct char_data *mob, FILE *fd);

int copy_mobile_strings(struct char_data *t, struct char_data *f);

#if CONFIG_GENOLC_MOBPROG
int write_mobile_mobprog(mob_vnum mvnum, struct char_data *mob, FILE *fd);
#endif

/* local functions */
void extract_mobile_all(mob_vnum vnum);

int add_mobile(struct char_data *mob, mob_vnum vnum) {
    mob_vnum rnum, found = false;
    struct char_data *live_mob;

    if ((rnum = real_mobile(vnum)) != NOBODY) {
        /* Copy over the mobile and free() the old strings. */
        copy_mobile(&mob_proto[rnum], mob);

        /* Now re-point all existing mobile strings to here. */
        for (live_mob = character_list; live_mob; live_mob = live_mob->next)
            if (rnum == live_mob->vn)
                update_mobile_strings(live_mob, &mob_proto[rnum]);

        dirty_npc_prototypes.insert(rnum);
        basic_mud_log("GenOLC: add_mobile: Updated existing mobile #%d.", vnum);
        return rnum;
    }

    auto &m = mob_proto[vnum];
    m = *mob;

    m.vn = 0;
    copy_mobile_strings(&m, mob);
    auto &ix = mob_index[vnum];
    ix.vn = vnum;

    basic_mud_log("GenOLC: add_mobile: Added mobile %d.", vnum);

#if CONFIG_GENOLC_MOBPROG
    GET_MPROG(OLC_MOB(d)) = OLC_MPROGL(d);
    GET_MPROG_TYPE(OLC_MOB(d)) = (OLC_MPROGL(d) ? OLC_MPROGL(d)->type : 0);
    while (OLC_MPROGL(d)) {
      GET_MPROG_TYPE(OLC_MOB(d)) |= OLC_MPROGL(d)->type;
      OLC_MPROGL(d) = OLC_MPROGL(d)->next;
    }
#endif

    auto zvnum = real_zone_by_thing(vnum);
    auto &z = zone_table[zvnum];
    z.mobiles.insert(vnum);
    dirty_npc_prototypes.insert(vnum);
    return found;
}

int copy_mobile(struct char_data *to, struct char_data *from) {
    free_mobile_strings(to);
    *to = *from;
    check_mobile_strings(from);
    copy_mobile_strings(to, from);
    return true;
}

void extract_mobile_all(mob_vnum vnum) {
    struct char_data *next, *ch;

    for (ch = character_list; ch; ch = next) {
        next = ch->next;
        if (GET_MOB_VNUM(ch) == vnum)
            extract_char(ch);
    }
}

int delete_mobile(mob_rnum refpt) {
    struct char_data *live_mob;
    int counter, cmd_no;
    mob_vnum vnum;
    zone_rnum zone;

    if (!mob_proto.count(refpt)) {
        basic_mud_log("SYSERR: GenOLC: delete_mobile: Invalid rnum %d.", refpt);
        return NOBODY;
    }

    vnum = mob_index[refpt].vn;
    extract_mobile_all(vnum);
    auto &z = zone_table[real_zone_by_thing(refpt)];
    z.mobiles.erase(refpt);

    /* Update live mobile rnums.  */
    for (live_mob = character_list; live_mob; live_mob = live_mob->next)
        GET_MOB_RNUM(live_mob) -= (GET_MOB_RNUM(live_mob) >= refpt);

    /* Update zone table.  */
    for (auto &[zone, z] : zone_table) {
        z.cmd.erase(std::remove_if(z.cmd.begin(), z.cmd.end(), [refpt](auto &cmd) { return cmd.command == 'M' && cmd.arg1 == refpt; }));
    }

    /* Update shop keepers.  */
    for (auto &sh : shop_index) {
        /* Find the shop for this keeper and reset it's keeper to
         * -1 to keep the shop so it could be assigned to someone else */
        if (sh.second.keeper == refpt) {
            sh.second.keeper = NOBODY;
        }
    }

    /* Update guild masters */
    for (auto &g : guild_index) {
        /* Find the guild for this trainer and reset it's trainer to
         * -1 to keep the guild so it could be assigned to someone else */
        if (g.second.gm == refpt) {
            g.second.gm = NOBODY;
        }
    }

    mob_proto.erase(vnum);
    mob_index.erase(vnum);
    save_mobiles(real_zone_by_thing(vnum));

    return refpt;
}

int copy_mobile_strings(struct char_data *t, struct char_data *f) {
    if (f->name)
        t->name = strdup(f->name);
    if (f->title)
        t->title = strdup(f->title);
    if (f->short_description)
        t->short_description = strdup(f->short_description);
    if (f->room_description)
        t->room_description = strdup(f->room_description);
    if (f->look_description)
        t->look_description = strdup(f->look_description);
    return true;
}

int update_mobile_strings(struct char_data *t, struct char_data *f) {
    if (f->name)
        t->name = f->name;
    if (f->title)
        t->title = f->title;
    if (f->short_description)
        t->short_description = f->short_description;
    if (f->room_description)
        t->room_description = f->room_description;
    if (f->look_description)
        t->look_description = f->look_description;
    return true;
}

int free_mobile_strings(struct char_data *mob) {
    if (mob->name)
        free(mob->name);
    if (mob->title)
        free(mob->title);
    if (mob->short_description)
        free(mob->short_description);
    if (mob->room_description)
        free(mob->room_description);
    if (mob->look_description)
        free(mob->look_description);
    return true;
}


/* Free a mobile structure that has been edited. Take care of existing mobiles 
 * and their mob_proto!  */
int free_mobile(struct char_data *mob) {
    mob_rnum i;

    if (mob == nullptr)
        return false;

    /* Non-prototyped mobile.  Also known as new mobiles.  */
    if ((i = GET_MOB_RNUM(mob)) == NOBODY) {
        free_mobile_strings(mob);
        /* free script proto list */
        free_proto_script(mob, MOB_TRIGGER);
    } else {    /* Prototyped mobile. */
        if (mob->name && mob->name != mob_proto[i].name)
            free(mob->name);
        if (mob->title && mob->title != mob_proto[i].title)
            free(mob->title);
        if (mob->short_description && mob->short_description != mob_proto[i].short_description)
            free(mob->short_description);
        if (mob->room_description && mob->room_description != mob_proto[i].room_description)
            free(mob->room_description);
        if (mob->look_description && mob->look_description != mob_proto[i].look_description)
            free(mob->look_description);
    }
    while (mob->affected)
        affect_remove(mob, mob->affected);

    /* free any assigned scripts */
    if (SCRIPT(mob))
        extract_script(mob, MOB_TRIGGER);

    free(mob);
    return true;
}

int save_mobiles(zone_rnum zone_num) {
    if (!zone_table.count(zone_num)) {
        basic_mud_log("SYSERR: GenOLC: save_mobiles: Invalid real zone number %d.", zone_num);
        return false;
    }

    auto &z = zone_table[zone_num];
    z.save_mobiles();
    return true;
}

#if CONFIG_GENOLC_MOBPROG
int write_mobile_mobprog(mob_vnum mvnum, struct char_data *mob, FILE *fd)
{
  char wmmarg[MAX_STRING_LENGTH], wmmcom[MAX_STRING_LENGTH];
  MPROG_DATA *mob_prog;

  for (mob_prog = GET_MPROG(mob); mob_prog; mob_prog = mob_prog->next) {
    wmmarg[MAX_STRING_LENGTH - 1] = '\0';
    wmmcom[MAX_STRING_LENGTH - 1] = '\0';
    strip_cr(strncpy(wmmarg, mob_prog->arglist, MAX_STRING_LENGTH - 1));
    strip_cr(strncpy(wmmcom, mob_prog->comlist, MAX_STRING_LENGTH - 1));
    fprintf(fd,	"%s %s~\n"
        "%s%c\n",
    medit_get_mprog_type(mob_prog), wmmarg,
    wmmcom, STRING_TERMINATOR
    );
    if (mob_prog->next == nullptr)
      fputs("|\n", fd);
  }
  return TRUE;
}
#endif

int write_mobile_espec(mob_vnum mvnum, struct char_data *mob, FILE *fd) {
    struct affected_type *aff;
    int i;

    if (get_size(mob) != mob->race->getSize())
        fprintf(fd, "Size: %d\n", get_size(mob));
    if (GET_ATTACK(mob) != 0)
        fprintf(fd, "BareHandAttack: %d\n", GET_ATTACK(mob));
    if (GET_STR(mob) != 0)
        fprintf(fd, "Str: %d\n", GET_STR(mob));
    if (GET_DEX(mob) != 0)
        fprintf(fd, "Dex: %d\n", GET_DEX(mob));
    if (GET_INT(mob) != 0)
        fprintf(fd, "Int: %d\n", GET_INT(mob));
    if (GET_WIS(mob) != 0)
        fprintf(fd, "Wis: %d\n", GET_WIS(mob));
    if (GET_CON(mob) != 0)
        fprintf(fd, "Con: %d\n", GET_CON(mob));
    if (GET_CHA(mob) != 0)
        fprintf(fd, "Cha: %d\n", GET_CHA(mob));
    if (&mob_proto[real_mobile(mvnum)] != mob) { /* Not saving a prototype */
        fprintf(fd,
                "Hit: %" I64T "\nMaxHit: %" I64T "\nMana: %" I64T "\nMaxMana: %" I64T "\nMoves: %" I64T "\nMaxMoves: %" I64T "\n",
                GET_HIT(mob), GET_MAX_HIT(mob), (mob->getCurKI()), GET_MAX_MANA(mob),
                (mob->getCurST()), GET_MAX_MOVE(mob));
        for (aff = mob->affected; aff; aff = aff->next)
            if (aff->type)
                fprintf(fd, "Affect: %d %d %d %d %d %d\n", aff->type, aff->duration,
                        aff->modifier, aff->location, (int) aff->bitvector, aff->specific);
        for (aff = mob->affectedv; aff; aff = aff->next)
            if (aff->type)
                fprintf(fd, "AffectV: %d %d %d %d %d %d\n", aff->type, aff->duration,
                        aff->modifier, aff->location, (int) aff->bitvector, aff->specific);
    }
    for (i = 0; i <= NUM_FEATS_DEFINED; i++)
        if (HAS_FEAT(mob, i))
            fprintf(fd, "Feat: %d %d\n", i, HAS_FEAT(mob, i));
    for (i = 0; i < SKILL_TABLE_SIZE; i++)
        if (GET_SKILL_BASE(mob, i))
            fprintf(fd, "Skill: %d %d\n", i, HAS_FEAT(mob, i));
    for (i = 0; i <= NUM_FEATS_DEFINED; i++)
        if (GET_SKILL_BONUS(mob, i))
            fprintf(fd, "SkillMod: %d %d\n", i, HAS_FEAT(mob, i));
    fputs("E\n", fd);
    return true;
}


int write_mobile_record(mob_vnum mvnum, struct char_data *mob, FILE *fd) {

    char ldesc[MAX_STRING_LENGTH], ddesc[MAX_STRING_LENGTH];
    char fbuf1[MAX_STRING_LENGTH], fbuf2[MAX_STRING_LENGTH];
    char fbuf3[MAX_STRING_LENGTH], fbuf4[MAX_STRING_LENGTH];
    char abuf1[MAX_STRING_LENGTH], abuf2[MAX_STRING_LENGTH];
    char abuf3[MAX_STRING_LENGTH], abuf4[MAX_STRING_LENGTH];

    ldesc[MAX_STRING_LENGTH - 1] = '\0';
    ddesc[MAX_STRING_LENGTH - 1] = '\0';
    strip_cr(strncpy(ldesc, GET_LDESC(mob), MAX_STRING_LENGTH - 1));
    strip_cr(strncpy(ddesc, GET_DDESC(mob), MAX_STRING_LENGTH - 1));

    fprintf(fd, "#%d\n"
                "%s%c\n"
                "%s%c\n"
                "%s%c\n"
                "%s%c\n",
            mvnum,
            GET_ALIAS(mob), STRING_TERMINATOR,
            GET_SDESC(mob), STRING_TERMINATOR,
            ldesc, STRING_TERMINATOR,
            ddesc, STRING_TERMINATOR
    );

    sprintascii(fbuf1, MOB_FLAGS(mob)[0]);
    sprintascii(fbuf2, MOB_FLAGS(mob)[1]);
    sprintascii(fbuf3, MOB_FLAGS(mob)[2]);
    sprintascii(fbuf4, MOB_FLAGS(mob)[3]);
    sprintascii(abuf1, AFF_FLAGS(mob)[0]);
    sprintascii(abuf2, AFF_FLAGS(mob)[1]);
    sprintascii(abuf3, AFF_FLAGS(mob)[2]);
    sprintascii(abuf4, AFF_FLAGS(mob)[3]);

    fprintf(fd, "%s %s %s %s %s %s %s %s %d E\n"
                "%d %d %d %" I64T "d%" I64T "+%" I64T " %dd%d+%d\n",
            fbuf1, fbuf2, fbuf3, fbuf4,
            abuf1, abuf2, abuf3, abuf4,
            GET_ALIGNMENT(mob),
            GET_HITDICE(mob), GET_FISHD(mob), 10 - (GET_ARMOR(mob) / 10),
            GET_HIT(mob), (mob->getCurKI()), (mob->getCurST()), GET_NDD(mob), GET_SDD(mob),
            GET_DAMAGE_MOD(mob)
    );
    fprintf(fd, "%d 0 %d %d\n"
                "%d %d %d\n",
            GET_GOLD(mob), GET_RACE(mob), GET_CLASS(mob),
            GET_POS(mob), GET_DEFAULT_POS(mob), GET_SEX(mob)
    );

    if (write_mobile_espec(mvnum, mob, fd) < 0)
        basic_mud_log("SYSERR: GenOLC: Error writing E-specs for mobile #%d.", mvnum);

    script_save_to_disk(fd, mob, MOB_TRIGGER);


#if CONFIG_GENOLC_MOBPROG
    if (write_mobile_mobprog(mvnum, mob, fd) < 0)
      log("SYSERR: GenOLC: Error writing MobProgs for mobile #%d.", mvnum);
#endif

    return true;
}

void check_mobile_strings(struct char_data *mob) {
    mob_vnum mvnum = mob_index[mob->vn].vn;
    check_mobile_string(mvnum, &GET_LDESC(mob), "long description");
    check_mobile_string(mvnum, &GET_DDESC(mob), "detailed description");
    check_mobile_string(mvnum, &GET_ALIAS(mob), "alias list");
    check_mobile_string(mvnum, &GET_SDESC(mob), "short description");
}

void check_mobile_string(mob_vnum i, char **string, const char *dscr) {
    if (*string == nullptr || **string == '\0') {
        char smbuf[128];
        sprintf(smbuf, "GenOLC: Mob #%d has an invalid %s.", i, dscr);
        mudlog(BRF, ADMLVL_GOD, true, smbuf);
        if (*string)
            free(*string);
        *string = strdup("An undefined string.");
    }
}

nlohmann::json mob_special_data::serialize() {
    nlohmann::json j;
    if(attack_type) j["attack_type"] = attack_type;
    if(default_pos != POS_STANDING) j["default_pos"] = default_pos;
    if(damnodice) j["damnodice"] = damnodice;
    if(damsizedice) j["damsizedice"] = damsizedice;

    return j;
}

void mob_special_data::deserialize(const nlohmann::json &j) {
    if(j.contains("attack_type")) attack_type = j["attack_type"];
    if(j.contains("default_pos")) default_pos = j["default_pos"];
    if(j.contains("damnodice")) damnodice = j["damnodice"];
    if(j.contains("damsizedice")) damsizedice = j["damsizedice"];
}

mob_special_data::mob_special_data(const nlohmann::json &j) : mob_special_data() {
    deserialize(j);
}

nlohmann::json abil_data::serialize() {
    nlohmann::json j;

    if(str) j["str"] = str;
    if(intel) j["intel"] = intel;
    if(wis) j["wis"] = wis;
    if(dex) j["dex"] = dex;
    if(con) j["con"] = con;
    if(cha) j["cha"] = cha;

    return j;
}

void abil_data::deserialize(const nlohmann::json &j) {
    if(j.contains("str")) str = j["str"];
    if(j.contains("intel")) intel = j["intel"];
    if(j.contains("wis")) wis = j["wis"];
    if(j.contains("dex")) dex = j["dex"];
    if(j.contains("con")) con = j["con"];
    if(j.contains("cha")) cha = j["cha"];
}

abil_data::abil_data(const nlohmann::json &j) : abil_data() {
    deserialize(j);
}

nlohmann::json time_data::serialize() {
    nlohmann::json j;

    if(birth) j["birth"] = birth;
    if(created) j["created"] = created;
    if(maxage) j["maxage"] = maxage;
    if(logon) j["logon"] = logon;
    if(played) j["played"] = played;

    return j;
}

void time_data::deserialize(const nlohmann::json &j) {
    if(j.contains("birth")) birth = j["birth"];
    if(j.contains("created")) created = j["created"];
    if(j.contains("maxage")) maxage = j["maxage"];
    if(j.contains("logon")) logon = j["logon"];
    if(j.contains("played")) played = j["played"];
}

time_data::time_data(const nlohmann::json &j) : time_data() {
    deserialize(j);
}

nlohmann::json char_data::serializeBase() {
    auto j = serializeUnit();

    if(title && strlen(title)) j["title"] = title;
    if(size != SIZE_UNDEFINED) j["size"] = size;
    if(sex) j["sex"] = sex;
    if(race) j["race"] = race->getID();
    if(hairl) j["hairl"] = hairl;
    if(hairs) j["hairs"] = hairs;
    if(hairc) j["hairc"] = hairc;
    if(skin) j["skin"] = skin;
    if(eye) j["eye"] = eye;
    if(distfea) j["distfea"] = distfea;
    if(race_level) j["race_level"] = race_level;
    if(level_adj) j["level_adj"] = level_adj;
    if(level) j["level"] = level;
    if(admlevel) j["admlevel"] = admlevel;
    if(chclass) j["chclass"] = chclass->getID();
    if(weight) j["weight"] = weight;
    if(height) j["height"] = height;
    if(alignment) j["alignment"] = alignment;
    if(alignment_ethic) j["alignment_ethic"] = alignment_ethic;

    for(auto i = 0; i < NUM_AFF_FLAGS; i++)
        if(IS_SET_AR(affected_by, i)) j["affected_by"].push_back(i);

    if(basepl) j["basepl"] = basepl;
    if(baseki) j["baseki"] = baseki;
    if(basest) j["basest"] = basest;

    if(armor) j["armor"] = armor;
    if(damage_mod) j["damage_mod"] = damage_mod;

    auto real = real_abils.serialize();
    if(!real.empty()) j["real_abils"] = real;

    return j;
}

void char_data::deserializeBase(const nlohmann::json &j) {
    deserializeUnit(j);

    if(j.contains("title")) title = strdup(j["title"].get<std::string>().c_str());
    if(j.contains("size")) size = j["size"];
    if(j.contains("sex")) sex = j["sex"];
    ::race::race_id r = ::race::human;
    if(j.contains("race")) r = j["race"].get<::race::race_id>();
    race = ::race::race_map[r];
    if(j.contains("hairl")) hairl = j["hairl"];
    if(j.contains("hairs")) hairs = j["hairs"];
    if(j.contains("hairc")) hairc = j["hairc"];
    if(j.contains("skin")) skin = j["skin"];
    if(j.contains("eye")) eye = j["eye"];
    if(j.contains("distfea")) distfea = j["distfea"];
    if(j.contains("race_level")) race_level = j["race_level"];
    if(j.contains("level_adj")) level_adj = j["level_adj"];

    ::sensei::sensei_id c = ::sensei::commoner;
    if(j.contains("chclass")) c = j["chclass"].get<::sensei::sensei_id>();
    chclass = ::sensei::sensei_map[c];

    if(j.contains("level")) level = j["level"];

    if(j.contains("weight")) weight = j["weight"];
    if(j.contains("height")) height = j["height"];
    if(j.contains("alignment")) alignment = j["alignment"];
    if(j.contains("alignment_ethic")) alignment_ethic = j["alignment_ethic"];

    if(j.contains("affected_by"))
        for(auto &i : j["affected_by"])
            SET_BIT_AR(affected_by, i.get<int>());

    if(j.contains("basepl")) basepl = j["basepl"];
    if(j.contains("baseki")) baseki = j["baseki"];
    if(j.contains("basest")) basest = j["basest"];
    if(j.contains("real_abils")) real_abils.deserialize(j["real_abils"]);
    if(j.contains("armor")) armor = j["armor"];
    if(j.contains("damage_mod")) damage_mod = j["damage_mod"];
    if(j.contains("mob_specials")) mob_specials.deserialize(j["mob_specials"]);
    if(j.contains("act")) for(auto &i : j["act"]) SET_BIT_AR(act, i.get<int>());

}

nlohmann::json char_data::serializeProto() {
    auto j = serializeBase();

    for(auto i = 0; i < NUM_MOB_FLAGS; i++)
        if(IS_SET_AR(act, i)) j["act"].push_back(i);

    auto ms = mob_specials.serialize();
    if(!ms.empty()) j["mob_specials"] = ms;

    for(auto p : proto_script) {
        if(trig_index.contains(p)) j["proto_script"].push_back(p);
    }

    return j;
}

nlohmann::json char_data::serializeInstance() {
    auto j = serializeBase();
    if(generation) j["generation"] = generation;

    if(admlevel) j["admlevel"] = admlevel;
    for(auto i = 0; i < NUM_ADMFLAGS; i++)
        if(IS_SET_AR(admflags, i)) j["admflags"].push_back(i);

    if(health < 1.0) j["health"] = health;
    if(energy < 1.0) j["energy"] = energy;
    if(stamina < 1.0) j["stamina"] = stamina;
    if(life < 1.0) j["life"] = life;

    if(gold) j["gold"] = gold;
    if(bank_gold) j["bank_gold"] = bank_gold;

    if(exp) j["exp"] = exp;

    if(was_in_room != NOWHERE) j["was_in_room"] = was_in_room;
    auto td = time.serialize();
    if(!td.empty()) j["time"] = td;



    for(auto i = 0; i < 4; i++) {
        if(limb_condition[i]) j["limb_condition"].push_back(std::make_pair(i, limb_condition[i]));
    }

    for(auto i = 0; i < NUM_CONDITIONS; i++) {
        if(conditions[i]) j["conditions"].push_back(std::make_pair(i, conditions[i]));
    }

    if(freeze_level) j["freeze_level"] = freeze_level;
    if(invis_level) j["invis_level"] = invis_level;
    if(wimp_level) j["wimp_level"] = wimp_level;
    if(world.contains(load_room)) j["load_room"] = load_room;
    if(world.contains(hometown)) j["hometown"] = hometown;

    if(contents) j["contents"] = serializeContents();

    auto eq = serializeEquipment();
    if(!eq.empty()) j["equipment"] = eq;

    if(IS_NPC(this)) {
        // mob flags.
        for(auto i = 0; i < NUM_MOB_FLAGS; i++) if(IS_SET_AR(act, i)) j["act"].push_back(i);
    } else {
        // player flags.
        for(auto i = 0; i < NUM_PLR_FLAGS; i++) if(IS_SET_AR(act, i)) j["act"].push_back(i);
        for(auto i = 0; i < NUM_PRF_FLAGS; i++) if(IS_SET_AR(pref, i)) j["pref"].push_back(i);
    }

    for(auto &[skill_id, s] : skill) {
        auto sk = s.serialize();
        if(!sk.empty()) j["skill"].push_back(std::make_pair(skill_id, sk));
    }

    if(speaking) j["speaking"] = speaking;
    if(preference) j["preference"] = preference;

    if(trainstr) j["trainstr"] = trainstr;
    if(trainint) j["trainint"] = trainint;
    if(traincon) j["traincon"] = traincon;
    if(trainwis) j["trainwis"] = trainwis;
    if(trainagl) j["trainagl"] = trainagl;
    if(trainspd) j["trainspd"] = trainspd;

    if(practice_points) j["practice_points"] = practice_points;

    for(auto a = affected; a; a = a->next) {
        if(a->type) j["affected"].push_back(a->serialize());
    }

    for(auto a = affectedv; a; a = a->next) {
        if(a->type) j["affectedv"].push_back(a->serialize());
    }

    if(absorbs) j["absorbs"] = absorbs;
    if(blesslvl) j["blesslvl"] = blesslvl;
    for(auto i = 0; i < 5; i++) {
        if(lboard[i]) j["lboard"].push_back(std::make_pair(i, lboard[i]));
    }

    for(auto i = 0; i < MAX_BONUSES; i++) {
        if(bonuses[i]) j["bonuses"].push_back(i);
    }

    if(boosts) j["boosts"] = boosts;

    if(clan && strlen(clan)) j["clan"] = clan;
    if(crank) j["crank"] = crank;
    if(con_cooldown) j["con_cooldown"] = con_cooldown;
    if(deathtime) j["deathtime"] = deathtime;
    if(dcount) j["dcount"] = dcount;
    if(death_type) j["death_type"] = death_type;
    if(damage_mod) j["damage_mod"] = damage_mod;
    if(droom) j["droom"] = droom;
    if(accuracy_mod) j["accuracy_mod"] = accuracy_mod;
    for(auto i = 0; i < 2; i++) {
        if(genome[i]) j["genome"].push_back(std::make_pair(i, genome[i]));
    }
    if(gauntlet) j["gauntlet"] = gauntlet;
    if(ingestLearned) j["ingestLearned"] = ingestLearned;
    if(kaioken) j["kaioken"] = kaioken;
    if(lifeperc) j["lifeperc"] = lifeperc;
    if(lastint) j["lastint"] = lastint;
    if(lastpl) j["lastpl"] = lastpl;
    if(moltexp) j["moltexp"] = moltexp;
    if(moltlevel) j["moltlevel"] = moltlevel;
    if(majinize) j["majinize"] = majinize;
    if(majinizer) j["majinizer"] = majinizer;
    if(mimic) j["mimic"] = mimic->getID();
    if(olc_zone) j["olc_zone"] = olc_zone;
    if(starphase) j["starphase"] = starphase;
    if(accuracy) j["accuracy"] = accuracy;
    if(position) j["position"] = position;
    if(powerattack) j["powerattack"] = powerattack;
    if(racial_pref) j["racial_pref"] = racial_pref;
    if(rdisplay) j["rdisplay"] = rdisplay;
    if(relax_count) j["relax_count"] = relax_count;
    if(radar1) j["radar1"] = radar1;
    if(radar2) j["radar2"] = radar2;
    if(radar3) j["radar3"] = radar3;
    if(feature) j["feature"] = feature;
    if(ship) j["ship"] = ship;
    if(con_sdcooldown) j["con_sdcooldown"] = con_sdcooldown;
    if(shipr) j["shipr"] = shipr;
    if(skill_slots) j["skill_slots"] = skill_slots;
    if(stupidkiss) j["stupidkiss"] = stupidkiss;
    if(suppression) j["suppression"] = suppression;
    if(tail_growth) j["tail_growth"] = tail_growth;
    for(auto i = 0; i < 6; i++) {
        if(transcost[i]) j["transcost"].push_back(std::make_pair(i, transcost[i]));
    }
    for(auto i = 0; i < 3; i++) {
        if(saving_throw[i]) j["saving_throw"].push_back(std::make_pair(i, saving_throw[i]));
    }
    for(auto i = 0; i < 3; i++) {
        if(apply_saving_throw[i]) j["apply_saving_throw"].push_back(std::make_pair(i, apply_saving_throw[i]));
    }
    if(upgrade) j["upgrade"] = upgrade;
    if(voice && strlen(voice)) j["voice"] = voice;

    if(script && script->global_vars) {
        for(auto v = script->global_vars; v; v = v->next) {
            j["dgvariables"].push_back(v->serialize());
        }
    }

    return j;
}

nlohmann::json char_data::serializePlayer() {
    auto j = serializeInstance();

    if(relax_count) j["relax_count"] = relax_count;
    if(ingestLearned) j["ingestLearned"] = ingestLearned;

    if (poofin && strlen(poofin)) j["poofin"] = poofin;
    if (poofout && strlen(poofout)) j["poofout"] = poofout;
    if(players.contains(last_tell)) j["last_tell"] = last_tell;

    return j;
}


void char_data::deserializeInstance(const nlohmann::json &j, bool isActive) {
    deserializeBase(j);

    if(j.contains("generation")) generation = j["generation"];
    check_unique_id(this);
    add_unique_id(this);

    if(j.contains("admlevel")) admlevel = j["admlevel"];

    if(j.contains("admflags"))
        for(auto &i : j["admflags"])
            SET_BIT_AR(admflags, i.get<int>());

    if(j.contains("hometown")) hometown = j["hometown"];

    if(j.contains("time")) {
        time.deserialize(j["time"]);
    }

    if(j.contains("health")) health = j["health"];
    if(j.contains("energy")) energy = j["energy"];
    if(j.contains("stamina")) stamina = j["stamina"];
    if(j.contains("life")) life = j["life"];

    if(j.contains("limb_condition")) {
        for(auto &i : j["limb_condition"]) {
            limb_condition[i[0].get<int>()] = i[1];
        }
    }

    if(j.contains("gold")) gold = j["gold"];
    if(j.contains("bank_gold")) bank_gold = j["bank_gold"];
    if(j.contains("exp")) exp = j["exp"];

    if(j.contains("was_in_room")) was_in_room = j["was_in_room"];

    if(j.contains("skill")) {
        for(auto &i : j["skill"]) {
            auto id = i[0].get<uint16_t>();
            skill.emplace(id, i[1]);
        }
    }

    if(j.contains("contents")) {
        deserializeContents(j["contents"], isActive);
    }

    if(j.contains("equipment")) {
        deserializeEquipment(j["equipment"], isActive);
    }

    if(j.contains("affected")) {
        auto ja = j["affected"];
        // reverse iterate using .rbegin() and .rend() while filling out
        // the linked list.
        for(auto it = ja.rbegin(); it != ja.rend(); ++it) {
            auto a = new affected_type(*it);
            a->next = affected;
            affected = a;
        }
    }

    if(j.contains("affectedv")) {
        auto ja = j["affectedv"];
        // reverse iterate using .rbegin() and .rend() while filling out
        // the linked list.
        for(auto it = ja.rbegin(); it != ja.rend(); ++it) {
            auto a = new affected_type(*it);
            a->next = affectedv;
            affectedv = a;
        }
    }

    if(j.contains("absorbs")) absorbs = j["absorbs"];
    if(j.contains("blesslvl")) blesslvl = j["blesslvl"];
    if(j.contains("lboard")) {
        for(auto &i : j["lboard"]) {
            lboard[i[0].get<int>()] = i[1];
        }
    }

    if(j.contains("bonuses")) {
        for(auto &i : j["bonuses"]) {
            bonuses[i.get<int>()] = true;
        }
    }

    if(j.contains("boosts")) boosts = j["boosts"];

    if(j.contains("clan")) clan = strdup(j["clan"].get<std::string>().c_str());
    if(j.contains("crank")) crank = j["crank"];
    if(j.contains("con_cooldown")) con_cooldown = j["con_cooldown"];
    if(j.contains("deathtime")) deathtime = j["deathtime"];
    if(j.contains("dcount")) dcount = j["dcount"];
    if(j.contains("death_type")) death_type = j["death_type"];

    if(j.contains("conditions")) {
        for(auto &i : j["conditions"]) {
            conditions[i[0].get<int>()] = i[1];
        }
    }

    if(j.contains("damage_mod")) damage_mod = j["damage_mod"];
    if(j.contains("droom")) droom = j["droom"];
    if(j.contains("accuracy_mod")) accuracy_mod = j["accuracy_mod"];
    if(j.contains("genome")) {
        for(auto &i : j["genome"]) {
            genome[i[0].get<int>()] = i[1];
        }
    }
    if(j.contains("gauntlet")) gauntlet = j["gauntlet"];
    if(j.contains("ingestLearned")) ingestLearned = j["ingestLearned"];
    if(j.contains("kaioken")) kaioken = j["kaioken"];
    if(j.contains("lifeperc")) lifeperc = j["lifeperc"];
    if(j.contains("lastint")) lastint = j["lastint"];
    if(j.contains("lastpl")) lastpl = j["lastpl"];
    if(j.contains("moltexp")) moltexp = j["moltexp"];
    if(j.contains("moltlevel")) moltlevel = j["moltlevel"];
    if(j.contains("majinize")) majinize = j["majinize"];
    if(j.contains("majinizer")) majinizer = j["majinizer"];
    if(j.contains("mimic")) {
        auto rid = j["mimic"].get<::race::race_id>();
        mimic = ::race::race_map[rid];
    }
    if(j.contains("olc_zone")) olc_zone = j["olc_zone"];
    if(j.contains("starphase")) starphase = j["starphase"];
    if(j.contains("accuracy")) accuracy = j["accuracy"];
    if(j.contains("position")) position = j["position"];
    if(j.contains("powerattack")) powerattack = j["powerattack"];
    if(j.contains("racial_pref")) racial_pref = j["racial_pref"];
    if(j.contains("rdisplay")) rdisplay = strdup(j["rdisplay"].get<std::string>().c_str());
    if(j.contains("relax_count")) relax_count = j["relax_count"];
    if(j.contains("radar1")) radar1 = j["radar1"];
    if(j.contains("radar2")) radar2 = j["radar2"];
    if(j.contains("radar3")) radar3 = j["radar3"];
    if(j.contains("feature")) feature = strdup(j["feature"].get<std::string>().c_str());
    if(j.contains("ship")) ship = j["ship"];
    if(j.contains("con_sdcooldown")) con_sdcooldown = j["con_sdcooldown"];
    if(j.contains("shipr")) shipr = j["shipr"];
    if(j.contains("skill_slots")) skill_slots = j["skill_slots"];
    if(j.contains("stupidkiss")) stupidkiss = j["stupidkiss"];
    if(j.contains("suppression")) suppression = j["suppression"];
    if(j.contains("tail_growth")) tail_growth = j["tail_growth"];
    if(j.contains("transcost")) {
        for(auto t : j["transcost"]) {
            transcost[t[0].get<int>()] = t[1];
        }
    }
    if(j.contains("saving_throw")) {
        for(auto t : j["saving_throw"]) {
            saving_throw[t[0].get<int>()] = t[1];
        }
    }
    if(j.contains("apply_saving_throw")) {
        for(auto t : j["apply_saving_throw"]) {
            apply_saving_throw[t[0].get<int>()] = t[1];
        }
    }
    if(j.contains("upgrade")) upgrade = j["upgrade"];
    if(j.contains("voice")) voice = strdup(j["voice"].get<std::string>().c_str());
    if(j.contains("wimp_level")) wimp_level = j["wimp_level"];

    if(!proto_script.empty()) {
        assign_triggers(this, OBJ_TRIGGER);
    }

    if(j.contains("dgvariables")) {
        if(!script) script = new script_data(this);
        auto jv = j["dgvariables"];
        // use reverse iteration to fill out script->global_vars
        for(auto it = jv.rbegin(); it != jv.rend(); ++it) {
            auto v = new trig_var_data(*it);
            v->next = script->global_vars;
            script->global_vars = v;
        }
    }

    if(j.contains("pref")) {
        for(auto &i : j["pref"]) {
            SET_BIT_AR(pref, i.get<int>());
        }
    }

    if(j.contains("load_room")) load_room = j["load_room"];

}

void char_data::deserializeProto(const nlohmann::json &j) {
    deserializeBase(j);

    if(j.contains("proto_script")) {
        for(auto p : j["proto_script"]) proto_script.emplace_back(p.get<trig_vnum>());
    }

}

void char_data::deserializePlayer(const nlohmann::json &j, bool isActive) {
    deserializeInstance(j, isActive);


}

void char_data::deserializeMobile(const nlohmann::json &j) {
    deserializeBase(j);


}

char_data::char_data(const nlohmann::json &j) : char_data() {
    deserializeProto(j);

    if (!IS_HUMAN(this))
        if (!AFF_FLAGGED(this, AFF_INFRAVISION))
            SET_BIT_AR(AFF_FLAGS(this), AFF_INFRAVISION);

    SPEAKING(this) = SKILL_LANG_COMMON;
    set_height_and_weight_by_race(this);

    SET_BIT_AR(act, MOB_ISNPC);
    if(MOB_FLAGGED(this, MOB_NOTDEADYET)) {
        REMOVE_BIT_AR(MOB_FLAGS(this), MOB_NOTDEADYET);
    }

    aff_abils = real_abils;

}

nlohmann::json skill_data::serialize() {
    nlohmann::json j = nlohmann::json::object();

    if(level) j["level"] = level;
    if(perfs) j["perfs"] = perfs;

    return j;
}

void skill_data::deserialize(const nlohmann::json &j) {
    if(j.contains("level")) level = j["level"];
    if(j.contains("perfs")) perfs = j["perfs"];
}

skill_data::skill_data(const nlohmann::json &j) : skill_data() {
    deserialize(j);
}

void char_data::deserializeContents(const nlohmann::json &j, bool isActive) {
    for(const auto& jo : j) {
        auto obj = new obj_data();
        obj->deserializeInstance(jo, isActive);
        obj_to_char(obj, this);
    }
}

void char_data::deserializeEquipment(const nlohmann::json &j, bool isActive) {
    for(const auto& jo : j) {
        // each jo is a two-element array. the first element is the id number of the equip
        // location, and the second is the item dump data.
        int id = jo[0];
        auto data = jo[1];
        auto obj = new obj_data();
        obj->deserializeInstance(data, isActive);
        // autoequip has a decrementer for some reason, so we'll increment.
        auto_equip(this, obj, id+1);
    }
}

nlohmann::json char_data::serializeEquipment() {
    auto j = nlohmann::json::array();
    for(auto i = 1; i < NUM_WEARS; i++) {
        if(auto obj = GET_EQ(this, i); obj) {
            j.push_back(std::make_pair(i, obj->serializeInstance()));
        }
    }

    return j;
}

nlohmann::json alias_data::serialize() {
    auto j = nlohmann::json::object();

    if(!name.empty()) j["name"] = name;
    if(!replacement.empty()) j["replacement"] = replacement;
    if(type) j["type"] = type;

    return j;
}

alias_data::alias_data(const nlohmann::json &j) : alias_data() {
    if(j.contains("name")) name = j["name"].get<std::string>();
    if(j.contains("replacement")) replacement = j["replacement"].get<std::string>();
    if(j.contains("type")) type = j["type"];
}

nlohmann::json player_data::serialize() {
    auto j = nlohmann::json::object();
    j["id"] = id;
    j["name"] = name;
    if(account) j["account"] = account->vn;

    for(auto &a : aliases) {
        j["aliases"].push_back(a.serialize());
    }

    for(auto &i : sensePlayer) {
        j["sensePlayer"].push_back(i);
    }

    for(auto &i : senseMemory) {
        j["senseMemory"].push_back(i);
    }

    for(auto &i : dubNames) {
        j["dubNames"].push_back(i);
    }

    for(auto i = 0; i < NUM_COLOR; i++) {
        if(color_choices[i] && strlen(color_choices[i])) j["color_choices"].push_back(std::make_pair(i, color_choices[i]));
    }

    if(character) {
        j["character"] = character->serializePlayer();
    }

    return j;
}

player_data::player_data(const nlohmann::json &j) {
    id = j["id"];
    name = j["name"].get<std::string>();
    if(j.contains("account")) {
        auto accID = j["account"].get<vnum>();
        auto accFind = accounts.find(accID);
        if(accFind != accounts.end()) account = &accFind->second;
    }

    if(j.contains("aliases")) {
        for(auto ja : j["aliases"]) {
            aliases.emplace_back(ja);
        }
    }

    if(j.contains("sensePlayer")) {
        for(auto &i : j["sensePlayer"]) {
            sensePlayer.insert(i.get<int64_t>());
        }
    }

    if(j.contains("senseMemory")) {
        for(auto &i : j["senseMemory"]) {
            senseMemory.insert(i.get<vnum>());
        }
    }

    if(j.contains("dubNames")) {
        for(auto &i : j["dubNames"]) {
            dubNames.emplace(i[0].get<int64_t>(), i[1].get<std::string>());
        }
    }

    if(j.contains("color_choices")) {
        for(auto &i : j["color_choices"]) {
            color_choices[i[0].get<int>()] = strdup(i[1].get<std::string>().c_str());
        }
    }

    if(j.contains("character")) {
        character = new char_data();
        character->deserializePlayer(j["character"], false);
    }

}

void char_data::activate() {
    next = character_list;
    character_list = this;
    auto find = mob_index.find(vn);
    if(find != mob_index.end()) {
        find->second.mobs.insert(this);
    }
    if(contents) activateContents();
    for(auto i = 0; i < NUM_WEARS; i++) {
        if(GET_EQ(this, i)) {
            auto obj = GET_EQ(this, i);
            obj->activate();
        }
    }
    if(affected) {
        next_affect = affect_list;
        affect_list = this;
    }
    if(affectedv) {
        next_affectv = affectv_list;
        affectv_list = this;
    }
}


void char_data::deactivate() {
    struct char_data *temp;
    REMOVE_FROM_LIST(this, character_list, next, temp);
    auto find = mob_index.find(vn);
    if(find != mob_index.end()) {
        find->second.mobs.erase(this);
    }
    if(affected) {
        REMOVE_FROM_LIST(this, affect_list, next_affect, temp);
    }
    if(affectedv) {
        REMOVE_FROM_LIST(this, affectv_list, next_affectv, temp);
    }
    if(contents) deactivateContents();
    for(auto i = 0; i < NUM_WEARS; i++) {
        if(GET_EQ(this, i)) {
            auto obj = GET_EQ(this, i);
            obj->deactivate();
        }
    }
}

nlohmann::json affected_type::serialize() {
    auto j = nlohmann::json::object();

    if(type) j["type"] = type;
    if(duration) j["duration"] = duration;
    if(modifier) j["modifier"] = modifier;
    if(location) j["location"] = location;
    if(specific) j["specific"] = specific;
    if(bitvector) j["bitvector"] = bitvector;

    return j;
}

affected_type::affected_type(const nlohmann::json &j) {
    if(j.contains("type")) type = j["type"];
    if(j.contains("duration")) duration = j["duration"];
    if(j.contains("modifier")) modifier = j["modifier"];
    if(j.contains("location")) location = j["location"];
    if(j.contains("specific")) specific = j["specific"];
    if(j.contains("bitvector")) bitvector = j["bitvector"];
}

double char_data::getWeight(bool base) {
    double total = 0;
    if(!IS_NPC(this)) {
        total += GET_PC_WEIGHT(this);
    } else {
        total += weight;
    }
    if(!base) total += getAffectModifier(APPLY_CHAR_WEIGHT);
    return total;
}

double char_data::getTotalWeight() {
    return getWeight() + getCarriedWeight();
}

std::string char_data::getUID() {
    return fmt::format("#C{}:{}", id, generation);
}

bool char_data::isActive() {
    if(IS_NPC(this)) {
        return id != NOTHING;
    }
    return desc != nullptr;
}

nlohmann::json char_data::serializeLocation() {
    auto j = nlohmann::json::object();
    j["in_room"] = in_room;
    if(!IS_NPC(this)) {
        if(was_in_room != NOWHERE) j["was_in_room"] = was_in_room;
    }

    return j;
}

nlohmann::json char_data::serializeRelations() {
    auto j = nlohmann::json::object();

    return j;
}

void char_data::save() {
    if(id == NOTHING) return;
    // dirty_characters.insert(id);
    if(!IS_NPC(this)) dirty_players.insert(id);
}

bool char_data::isProvidingLight() {
    if(!IS_NPC(this) && PLR_FLAGGED(this, PLR_AURALIGHT)) return true;
    for(auto i = 0; i < NUM_WEARS; i++) if(auto e = GET_EQ(this, i); e) if(e->isProvidingLight()) return true;
    return false;
}

struct room_data* char_data::getRoom() {
    auto roomFound = world.find(in_room);
    if(roomFound != world.end()) return &roomFound->second;
    return nullptr;
}