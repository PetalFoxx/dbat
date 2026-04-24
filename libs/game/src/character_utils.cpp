#include "dbat/db/characters.h"
#include "dbat/db/utils.h"
#include "dbat/db/consts/aligns.h"
#include "dbat/game/character_utils.h"
#include "dbat/game/races.h"
#include "dbat/game/spells.h"
#include "dbat/game/comm.h"
#include "dbat/game/class.h"
#include "dbat/game/fight.h"
#include "dbat/game/act.movement.h"
#include "dbat/game/relocate.h"
#include "dbat/game/room_utils.h"
#include "dbat/game/object_utils.h"
#include "dbat/game/descriptor_utils.h"
#include "dbat/game/zone_utils.h"
#include "dbat/game/db.h"
#include "dbat/game/random.h"
#include "dbat/game/act.informative.h"
#include "dbat/game/stringutils.h"
#include "dbat/game/extract.h"
#include "dbat/game/affect.h"
#include "dbat/game/fileop.h"
#include "dbat/game/genzon.h"
#include "dbat/game/feats.h"
#include "dbat/game/time.h"
#include "dbat/game/weather.h"

#include <string>

#define ABS(x) ((x) < 0 ? -(x) : (x))

const char* juggleRaceName(char_data *ch, bool capitalized) {
    static char buf[256];

    int apparent = ch->race;

    switch(apparent) {
        case RACE_HOSHIJIN:
            if(ch->mimic > -1) apparent = ch->mimic;
            break;
        case RACE_HALFBREED:
            switch(RACIAL_PREF(ch)) {
                case 1:
                    apparent = RACE_HUMAN;
                    break;
                case 2:
                    apparent = RACE_SAIYAN;
                    break;
            }
            break;
        case RACE_ANDROID:
            switch(RACIAL_PREF(ch)) {
                case 1:
                    apparent = RACE_ANDROID;
                    break;
                case 2:
                    apparent = RACE_HUMAN;
                    break;
                case 3:
                    if(capitalized) {
                        return "Robotic-Humanoid";
                    } else {
                        return "robotic-humanoid";
                    }
            }
            break;
        case RACE_SAIYAN:
            if(PLR_FLAGGED(ch, PLR_TAILHIDE)) {
                apparent = RACE_HUMAN;
            }
            break;
    }

    if(capitalized) {
        snprintf(buf, sizeof(buf), "%s", pc_race_types[apparent]);
        return buf;
    } else {
        snprintf(buf, sizeof(buf), "%s", race_names[apparent]);
        return buf;
    }
}

void restore_by(char_data *ch, char_data *healer) {
    restore(ch, true);
    act("You have been fully healed by $N!", FALSE, ch, 0, healer, TO_CHAR | TO_SLEEP);
}

void restore(char_data *ch, bool announce) {
    restoreVitals(ch, announce);
    restoreLimbs(ch, announce);
    restoreStatus(ch, announce);
    restoreLF(ch, announce);
}

void resurrect(char_data *ch, int mode) {
    restore(ch, true);
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_ETHEREAL);
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_SPIRIT);
    REMOVE_BIT_AR(PLR_FLAGS(ch), PLR_PDEATH);
    char_from_room(ch);
    if (GET_DROOM(ch) != NOWHERE && GET_DROOM(ch) != 0 && GET_DROOM(ch) != 1) {
        char_to_room(ch, real_room(GET_DROOM(ch)));
    }
    else {
        char_to_room(ch, real_room(sensei_start_room(ch->chclass)));
    }
    look_at_room(IN_ROOM(ch), ch, 0);

    int dur = 100;
    switch(mode) {
        case 1:
            return;
        case 0:
            dur = 40;
            break;
        case 2:
            dur = 50;
            break;
    }

    if (GET_LEVEL(ch) > 9) {
        int losschance = axion_dice(0);
        send_to_char(ch, "@RThe the strain of this type of revival has caused you to be in a weakened state for 100 hours (Game time)! Strength, constitution, wisdom, intelligence, speed, and agility have been reduced by 8 points for the duration.@n\r\n");
        int str = -8, intel = -8, wis = -8, spd = -8, con = -8, agl = -8;
        str = -1 * (ch->real_abils.str / 5);
        intel = -1 * (ch->real_abils.intel / 5);
        wis = -1 * (ch->real_abils.wis / 5);
        spd = -1 * (ch->real_abils.cha / 5);
        con = -1 * (ch->real_abils.con / 5);
        agl = -1 * (ch->real_abils.dex / 5);
        assign_affect(ch, AFF_WEAKENED_STATE, SKILL_WARP, dur, str, con, intel, agl, wis, spd);
        if (losschance >= 100) {
            int psloss = rand_number(100, 300);
            GET_PRACTICES(ch, GET_CLASS(ch)) -= psloss;
            send_to_char(ch, "@R...and a loss of @r%d@R PS!@n", psloss);
            if (GET_PRACTICES(ch, GET_CLASS(ch)) < 0) {
                GET_PRACTICES(ch, GET_CLASS(ch)) = 0;
            }
        }
    }
    GET_DTIME(ch) = 0;
    act("$n's body forms in a pool of @Bblue light@n.", TRUE, ch, nullptr, nullptr, TO_ROOM);
}

void ghostify(char_data *ch) {
    restore(ch, true);
    SET_BIT_AR(AFF_FLAGS(ch), AFF_SPIRIT);
    SET_BIT_AR(AFF_FLAGS(ch), AFF_ETHEREAL);

    if (!PRF_FLAGGED(ch, PRF_LKEEP)) {
        if (PLR_FLAGGED(ch, PLR_CLLEG)) {
            REMOVE_BIT_AR(PLR_FLAGS(ch), PLR_CLLEG);
        }
        if (PLR_FLAGGED(ch, PLR_CRLEG)) {
            REMOVE_BIT_AR(PLR_FLAGS(ch), PLR_CRLEG);
        }
        if (PLR_FLAGGED(ch, PLR_CRARM)) {
            REMOVE_BIT_AR(PLR_FLAGS(ch), PLR_CRARM);
        }
        if (PLR_FLAGGED(ch, PLR_CLARM)) {
            REMOVE_BIT_AR(PLR_FLAGS(ch), PLR_CLARM);
        }
    }

    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_KNOCKED);
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_SLEEP);
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_PARALYZE);
}

void teleport_to(char_data *ch, IDXTYPE rnum) {
    char_from_room(ch);
    char_to_room(ch, real_room(rnum));
    look_at_room(IN_ROOM(ch), ch, 0);
    update_pos(ch);
}

bool in_room_range(char_data *ch, IDXTYPE low_rnum, IDXTYPE high_rnum) {
    return GET_ROOM_VNUM(IN_ROOM(ch)) >= low_rnum && GET_ROOM_VNUM(IN_ROOM(ch)) <= high_rnum;
}

bool in_past(char_data *ch) {
    return ROOM_FLAGGED(IN_ROOM(ch), ROOM_PAST);
}

bool is_newbie(char_data *ch) {
    return GET_LEVEL(ch) < 9;
}

bool in_northran(char_data *ch) {
    return in_room_range(ch, 17900, 17999);
}

static std::map<int, uint16_t> grav_threshold = {
        {10, 5000},
        {20, 20000},
        {30, 50000},
        {40, 100000},
        {50, 200000},
        {100, 400000},
        {200, 1000000},
        {300, 5000000},
        {400, 8000000},
        {500, 15000000},
        {1000, 25000000},
        {5000, 100000000},
        {10000, 200000000}
};

bool can_tolerate_gravity(char_data *ch, int grav) {
    if(IS_NPC(ch)) return true;
    int tolerance = 0;
    tolerance = MAX(tolerance, sensei_grav_tolerance(ch->chclass));
    if(tolerance >= grav)
        return true;
    return getMaxPL(ch) >= grav_threshold[grav];
}

int calcTier(char_data *ch) {
    int tier = ch->level / 10;
    if((ch->level % 10) == 0)
        tier--;
    tier = MAX(tier, 0);
    tier = MIN(tier, 9);
    return tier;
}

int64_t calc_soft_cap(char_data *ch) {
    int tier = calcTier(ch);
    auto softmap = get_race(ch->race)->getSoftMap(ch);
    return ch->level * softmap[tier];
}

bool is_soft_cap(char_data *ch, int64_t type) {
    return is_soft_cap(ch, type, 1.0);
}

bool is_soft_cap(char_data *ch, int64_t type, long double mult) {
    if(IS_NPC(ch))
        return true;

    if(ch->level >= 100) {
        return false;
    }
    int64_t cur_cap = calc_soft_cap(ch) * mult;

    int64_t against = 0;

    switch(get_race(ch->race)->getSoftType(ch)) {
        case dbat::race::Fixed:
            switch(type) {
                case 0:
                    against = (getBasePL(ch));
                    break;
                case 1:
                    against = (getBaseKI(ch));
                    break;
                case 2:
                    against = (getBaseST(ch));
                    break;
            }
            break;
        case dbat::race::Variable:
            against = (getBasePL(ch)) + (getBaseKI(ch)) + (getBaseST(ch));
            if(IS_ANDROID(ch) && type > 0) {
                cur_cap += type;
            }
            break;
    }
    return against >= cur_cap;
}

int wearing_android_canister(char_data *ch) {
    if(!IS_ANDROID(ch))
        return 0;
    struct obj_data* obj = GET_EQ(ch, WEAR_BACKPACK);
    if(!obj)
        return 0;
    switch(GET_OBJ_VNUM(obj)) {
        case 1806:
            return 1;
        case 1807:
            return 2;
        default:
            return 0;
    }
}

int calcGravCost(char_data *ch, int64_t num) {
    int cost = 0;
    if(!can_tolerate_gravity(ch, ROOM_GRAVITY(IN_ROOM(ch))))
        cost = ROOM_GRAVITY(IN_ROOM(ch)) ^ 2;

    if (!num) {
        if(cost) {
            send_to_char(ch, "You sweat bullets straining against the current gravity.\r\n");
        }
        if ((getCurST(ch)) > cost) {
            decCurST(ch, cost);
            return 1;
        }
        else {
            decCurST(ch, cost);
            return 0;
        }
    }
    else {
        return (getCurST(ch)) > (cost + num);
    }
}

int64_t getCurHealth(char_data *ch) {
    return getCurPL(ch);
}

int64_t getMaxHealth(char_data *ch) {
    return getMaxPL(ch);
}

double getCurHealthPercent(char_data *ch) {
    return getCurPLPercent(ch);
}

int64_t getPercentOfCurHealth(char_data *ch, double amt) {
    return getPercentOfCurPL(ch, amt);
}

int64_t getPercentOfMaxHealth(char_data *ch, double amt) {
    return getPercentOfMaxPL(ch, amt);
}

bool isFullHealth(char_data *ch) {
    return isFullPL(ch);
}

int64_t setCurHealth(char_data *ch, int64_t amt) {
    ch->hit = MAX(0L, ABS(amt));
    return ch->hit;
}

int64_t setCurHealthPercent(char_data *ch, double amt) {
    ch->health = clampHealth(amt);
    return getCurHealth(ch);
}

int64_t incCurHealth(char_data *ch, int64_t amt, bool limit_max) {
    double newhealth = ch->health + safeDiv((double)ABS(amt), (double)getEffMaxPL(ch));
    newhealth = fixnan(newhealth);
    if(limit_max)
        ch->health = MIN(1.0, newhealth);
    else
        ch->health = newhealth;
    ch->health = clampHealth(ch->health);
    return getCurHealth(ch);
};

int64_t decCurHealth(char_data *ch, int64_t amt, int64_t floor) {
    double fl = 0.0;
    if(floor > 0)
        fl = safeDiv((double)floor, (double)getEffMaxPL(ch));
    double newhealth = ch->health - safeDiv((double)ABS(amt), (double)getEffMaxPL(ch));
    newhealth = fixnan(newhealth);
    ch->health = MAX(fl, newhealth);
    ch->health = clampHealth(ch->health);
    return getCurHealth(ch);
}

int64_t incCurHealthPercent(char_data *ch, double amt, bool limit_max) {
    ch->health += ABS(amt);
    if(limit_max)
        ch->health = MIN(1.0, ch->health);
    return getCurHealth(ch);
}

int64_t decCurHealthPercent(char_data *ch, double amt, int64_t floor) {
    double fl = 0.0;
    if(floor > 0)
        fl = (double)floor / (double)getEffMaxPL(ch);
    ch->health = MAX(fl, ch->health-ABS(amt));
    return getCurHealth(ch);
}

void restoreHealth(char_data *ch, bool announce) {
    if(!isFullHealth(ch)) ch->health = 1;
}

int64_t healCurHealth(char_data *ch, int64_t amt) {
    return incCurHealth(ch, amt, true);
}

int64_t harmCurHealth(char_data *ch, int64_t amt) {
    return decCurHealth(ch, amt, 0);
}

int64_t getMaxPLTrans(char_data *ch) {
    auto form = get_race(ch->race)->getCurForm(ch);
    int64_t total = 0;
    if(form.flag) {
        total = (form.bonus + getEffBasePL(ch)) * form.mult;
    } else {
        total = getEffBasePL(ch) * form.mult;
    }
    total += ch->max_hit;
    return total;
}

int64_t getMaxPL(char_data *ch) {
    int64_t total = getMaxPLTrans(ch);
    if(GET_KAIOKEN(ch) > 0) {
        total += (total / 10) * GET_KAIOKEN(ch);
    }
    if(AFF_FLAGGED(ch, AFF_METAMORPH)) {
        total += (total * .6);
    }
    return total;
}

int64_t getCurPL(char_data *ch) {
    double health = clampHealth(ch->health);
    if(ch->suppression > 0){
        return (int64_t)(getEffMaxPL(ch) * MIN(health, (double)ch->suppression/100));
    } else {
        return (int64_t)(getEffMaxPL(ch) * health);
    }
}

int64_t getEffBasePL(char_data *ch) {
    if(ch->original) return getEffBasePL(ch->original);
    if(ch->clones) {
        return getBasePL(ch) / (ch->clones + 1);
    } else {
        return getBasePL(ch);
    }
}

int64_t getBasePL(char_data *ch) {
    return ch->basepl;
}

double getCurPLPercent(char_data *ch) {
    return (double)getCurPL(ch) / (double)getMaxPL(ch);
}

int64_t getPercentOfCurPL(char_data *ch, double amt) {
    return getCurPL(ch) * ABS(amt);
}

int64_t getPercentOfMaxPL(char_data *ch, double amt) {
    return getMaxPL(ch) * ABS(amt);
}

bool isFullPL(char_data *ch) {
    return ch->health >= 1.0;
}

int64_t getCurKI(char_data *ch) {
    return (int64_t)(getMaxKI(ch) * clampHealth(ch->energy));
}

int64_t getMaxKI(char_data *ch) {
    auto form = get_race(ch->race)->getCurForm(ch);
    int64_t total = 0;
    if(form.flag) {
        total = (form.bonus + getEffBaseKI(ch)) * form.mult;
    } else {
        total = getEffBaseKI(ch);
    }
    total += ch->max_ki;
    return total;
}

int64_t getEffBaseKI(char_data *ch) {
    if(ch->original) return getEffBaseKI(ch->original);
    if(ch->clones) {
        return getBaseKI(ch) / (ch->clones + 1);
    } else {
        return getBaseKI(ch);
    }
}

int64_t getBaseKI(char_data *ch) {
    return ch->baseki;
}

double getCurKIPercent(char_data *ch) {
    return (double)getCurKI(ch) / (double)getMaxKI(ch);
}

int64_t getPercentOfCurKI(char_data *ch, double amt) {
    return getCurKI(ch) * ABS(amt);
}

int64_t getPercentOfMaxKI(char_data *ch, double amt) {
    return getMaxKI(ch) * ABS(amt);
}

bool isFullKI(char_data *ch) {
    return ch->energy >= 1.0;
}

int64_t setCurKI(char_data *ch, int64_t amt) {
    int64_t maxki = getMaxKI(ch);
    // how much percent of maxki is amt? set energy to that.
    ch->energy = safeDiv((double)ABS(amt), (double)maxki);
    return getCurKI(ch);
}

int64_t setCurKIPercent(char_data *ch, double amt) {
    ch->energy = clampHealth(amt);
    return getCurKI(ch);
}

int64_t incCurKI(char_data *ch, int64_t amt, bool limit_max) {
    double newenergy = ch->energy + safeDiv((double)ABS(amt), (double)getMaxKI(ch));
    newenergy = fixnan(newenergy);
    if(limit_max)
        ch->energy = MIN(1.0, newenergy);
    else
        ch->energy = newenergy;
    ch->energy = clampHealth(ch->energy);
    return getCurKI(ch);
};

int64_t decCurKI(char_data *ch, int64_t amt, int64_t floor) {
    double fl = 0.0;
    if(floor > 0)
        fl = safeDiv((double)floor, (double)getMaxKI(ch));
    double newenergy = ch->energy - safeDiv((double)ABS(amt), (double)getMaxKI(ch));
    newenergy = fixnan(newenergy);
    ch->energy = MAX(fl, newenergy);
    ch->energy = clampHealth(ch->energy);
    return getCurKI(ch);
}

int64_t incCurKIPercent(char_data *ch, double amt, bool limit_max) {
    if(limit_max)
        ch->energy = MIN(1.0, ch->energy+ABS(amt));
    else
        ch->energy += ABS(amt);
    return getCurKI(ch);
}

int64_t decCurKIPercent(char_data *ch, double amt, int64_t floor) {
    if(!strcasecmp(ch->name, "Wayland")) {
        send_to_char(ch, "decCurKIPercent called with: %f\r\n", amt);
    }
    double fl = 0.0;
    if(floor > 0)
        fl = (double)floor / (double)getMaxKI(ch);
    ch->energy = MAX(fl, ch->energy-ABS(amt));
    return getCurKI(ch);
}

void restoreKI(char_data *ch, bool announce) {
    if(!isFullKI(ch)) ch->energy = 1;
}

int64_t getCurST(char_data *ch) {
    return (int64_t)(getMaxST(ch) * clampHealth(ch->stamina));
}

int64_t getMaxST(char_data *ch) {
    auto form = get_race(ch->race)->getCurForm(ch);
    int64_t total = 0;
    if(form.flag) {
        total = (form.bonus + getEffBaseST(ch)) * form.mult;
    } else {
        total = getEffBaseST(ch);
    }
    total += ch->max_move;
    return total;
}

int64_t getEffBaseST(char_data *ch) {
    if(ch->original) return getEffBaseST(ch->original);
    if(ch->clones) {
        return getBaseST(ch) / (ch->clones + 1);
    } else {
        return getBaseST(ch);
    }
}

int64_t getBaseST(char_data *ch) {
    return ch->basest;
}

double getCurSTPercent(char_data *ch) {
    return (double)getCurST(ch) / (double)getMaxST(ch);
}

int64_t getPercentOfCurST(char_data *ch, double amt) {
    return getCurST(ch) * ABS(amt);
}

int64_t getPercentOfMaxST(char_data *ch, double amt) {
    return getMaxST(ch) * ABS(amt);
}

bool isFullST(char_data *ch) {
    return ch->stamina >= 1;
}

int64_t setCurST(char_data *ch, int64_t amt) {
    ch->move = MAX(0L, ABS(amt));
    return ch->move;
}

int64_t setCurSTPercent(char_data *ch, double amt) {
    ch->move = MAX(0L, (int64_t)(getMaxST(ch) * ABS(amt)));
    return ch->move;
}

int64_t incCurST(char_data *ch, int64_t amt, bool limit_max) {
    double newstamina = ch->stamina + safeDiv((double)ABS(amt), (double)getMaxST(ch));
    newstamina = fixnan(newstamina);
    if(limit_max)
        ch->stamina = MIN(1.0, newstamina);
    else
        ch->stamina = newstamina;
    ch->stamina = clampHealth(ch->stamina);
    return getCurST(ch);
};

int64_t decCurST(char_data *ch, int64_t amt, int64_t floor) {
    double fl = 0.0;
    if(floor > 0)
        fl = safeDiv((double)floor, (double)getMaxST(ch));
    double newstamina = ch->stamina - safeDiv((double)ABS(amt), (double)getMaxST(ch));
    newstamina = fixnan(newstamina);
    ch->stamina = MAX(fl, newstamina);
    ch->stamina = clampHealth(ch->stamina);
    return getCurST(ch);
}

int64_t incCurSTPercent(char_data *ch, double amt, bool limit_max) {
    if(limit_max)
        ch->stamina = MIN(1.0, ch->stamina+ABS(amt));
    else
        ch->stamina += ABS(amt);
    return getMaxST(ch);
}

int64_t decCurSTPercent(char_data *ch, double amt, int64_t floor) {
    double fl = 0.0;
    if(floor > 0)
        fl = (double)floor / (double)getMaxST(ch);
    ch->stamina = MAX(fl, ch->stamina-ABS(amt));
    return getCurST(ch);
}

void restoreST(char_data *ch, bool announce) {
    if(!isFullST(ch)) ch->stamina = 1;
}

int64_t getCurLF(char_data *ch) {
    return (int64_t)(getMaxLF(ch) * clampHealth(ch->life));
}

int64_t getMaxLF(char_data *ch) {
    return (IS_DEMON(ch) ? (((GET_MAX_MANA(ch) * 0.5) + (GET_MAX_MOVE(ch) * 0.5)) * 0.75) + GET_LIFEBONUSES(ch) : (IS_KONATSU(ch) ? (((GET_MAX_MANA(ch) * 0.5) + (GET_MAX_MOVE(ch) * 0.5)) * 0.85) + GET_LIFEBONUSES(ch) : (GET_MAX_MANA(ch) * 0.5) + (GET_MAX_MOVE(ch) * 0.5) + GET_LIFEBONUSES(ch)));
}

double getCurLFPercent(char_data *ch) {
    return ch->life;
}

int64_t getPercentOfCurLF(char_data *ch, double amt) {
    return getCurLF(ch) * ABS(amt);
}

int64_t getPercentOfMaxLF(char_data *ch, double amt) {
    return getMaxLF(ch) * ABS(amt);
}

bool isFullLF(char_data *ch) {
    return ch->life >= 1.0;
}

int64_t setCurLF(char_data *ch, int64_t amt) {
    ch->life = MAX(0L, ABS(amt));
    return getCurLF(ch);
}

int64_t setCurLFPercent(char_data *ch, double amt) {
    ch->life = MAX(0L, (int64_t)(getMaxLF(ch) * ABS(amt)));
    return getCurLF(ch);
}

int64_t incCurLF(char_data *ch, int64_t amt, bool limit_max) {
    double newlife = ch->life + safeDiv((double)ABS(amt), (double)getMaxLF(ch));
    newlife = fixnan(newlife);
    if(limit_max)
        ch->life = MIN(1.0, newlife);
    else
        ch->life = newlife;
    ch->life = clampHealth(ch->life);
    return getCurLF(ch);
};

int64_t decCurLF(char_data *ch, int64_t amt, int64_t floor) {
    double fl = 0.0;
    if(floor > 0)
        fl = safeDiv((double)floor, (double)getMaxLF(ch));
    double newlife = ch->life - safeDiv((double)ABS(amt), (double)getMaxLF(ch));
    newlife = fixnan(newlife);
    ch->life = MAX(fl, newlife);
    ch->life = clampHealth(ch->life);
    return getCurLF(ch);
}

int64_t incCurLFPercent(char_data *ch, double amt, bool limit_max) {
    if(limit_max)
        ch->life = MIN(1.0, ch->life+ABS(amt));
    else
        ch->life += ABS(amt);
    return getCurLF(ch);
}

int64_t decCurLFPercent(char_data *ch, double amt, int64_t floor) {
    double fl = 0.0;
    if(floor > 0)
        fl = (double)floor / (double)getMaxLF(ch);
    ch->life = MAX(fl, ch->life-ABS(amt));
    return getCurLF(ch);
}

void restoreLF(char_data *ch, bool announce) {
    if(!isFullLF(ch)) ch->life = 1;
}

bool isFullVitals(char_data *ch) {
    return isFullHealth(ch) && isFullKI(ch) && isFullST(ch);
}

void restoreVitals(char_data *ch, bool announce) {
    restoreHealth(ch, announce);
    restoreKI(ch, announce);
    restoreST(ch, announce);
}

void restoreStatus(char_data *ch, bool announce) {
    cureStatusKnockedOut(ch, announce);
    cureStatusBurn(ch, announce);
    cureStatusPoison(ch, announce);
}

void setStatusKnockedOut(char_data *ch) {
    SET_BIT_AR(AFF_FLAGS(ch), AFF_KNOCKED);
    if (AFF_FLAGGED(ch, AFF_FLYING)) {
        REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_FLYING);
        GET_ALT(ch) = 0;
    }
    GET_POS(ch) = POS_SLEEPING;
}

void cureStatusKnockedOut(char_data *ch, bool announce) {
    if (AFF_FLAGGED(ch, AFF_KNOCKED)) {
        if(announce) {
            act("@W$n@W is no longer senseless, and wakes up.@n", FALSE, ch, 0, 0, TO_ROOM);
            send_to_char(ch, "You are no longer knocked out, and wake up!@n\r\n");
        }

        if (CARRIED_BY(ch)) {
            if (GET_ALIGNMENT(CARRIED_BY(ch)) > 50) {
                carry_drop(CARRIED_BY(ch), 0);
            } else {
                carry_drop(CARRIED_BY(ch), 1);
            }
        }

        REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_KNOCKED);
        GET_POS(ch) = POS_SITTING;
    }
}

void cureStatusBurn(char_data *ch, bool announce) {
    if (AFF_FLAGGED(ch, AFF_BURNED)) {
        if(announce) {
            send_to_char(ch, "Your burns are healed now.\r\n");
            act("$n@w's burns are now healed.@n", TRUE, ch, 0, 0, TO_ROOM);
        }
        REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_BURNED);
    }
}

void cureStatusPoison(char_data *ch, bool announce) {
    act("@C$n@W suddenly looks a lot better!@b", FALSE, ch, 0, 0, TO_NOTVICT);
    affect_from_char(ch, SPELL_POISON);
}

static const std::map<int, std::string> limb_names = {
        {1, "right arm"},
        {2, "left arm"},
        {3, "right leg"},
        {4, "left leg"}
};

void restoreLimbs(char_data *ch, bool announce) {

    for(const auto& l : limb_names) {
        if(announce) {
            if(GET_LIMBCOND(ch, l.first) <= 0)
                send_to_char(ch, "Your %s grows back!\r\n", l.second.c_str());
            else if (GET_LIMBCOND(ch, l.first) < 50)
                send_to_char(ch, "Your %s is no longer broken!\r\n", l.second.c_str());
        }
        GET_LIMBCOND(ch, l.first) = 100;
    }

    char_gain_tail(ch, announce);
}

int64_t gainBasePL(char_data *ch, int64_t amt, bool trans_mult) {
    ch->basepl += amt;
    return ch->basepl;
}

int64_t gainBaseST(char_data *ch, int64_t amt, bool trans_mult) {
    ch->basest += amt;
    return ch->basest;
}

int64_t gainBaseKI(char_data *ch, int64_t amt, bool trans_mult) {
    ch->baseki += amt;
    return ch->baseki;
}

void gainBaseAll(char_data *ch, int64_t amt, bool trans_mult) {
    gainBasePL(ch, amt, trans_mult);
    gainBaseKI(ch, amt, trans_mult);
    gainBaseST(ch, amt, trans_mult);
}

int64_t loseBasePL(char_data *ch, int64_t amt, bool trans_mult) {
    ch->basepl = MAX(1L, ch->basepl-amt);
    return ch->basepl;
}

int64_t loseBaseST(char_data *ch, int64_t amt, bool trans_mult) {
    ch->basest = MAX(1L, ch->basest-amt);
    return ch->basest;
}

int64_t loseBaseKI(char_data *ch, int64_t amt, bool trans_mult) {
    ch->baseki = MAX(1L, ch->baseki-amt);
    return ch->baseki;
}

void loseBaseAll(char_data *ch, int64_t amt, bool trans_mult) {
    loseBasePL(ch, amt, trans_mult);
    loseBaseKI(ch, amt, trans_mult);
    loseBaseST(ch, amt, trans_mult);
}

int64_t gainBasePLPercent(char_data *ch, double amt, bool trans_mult) {
    return gainBasePL(ch, ch->basepl * amt, trans_mult);
}

int64_t gainBaseKIPercent(char_data *ch, double amt, bool trans_mult) {
    return gainBaseKI(ch, ch->baseki * amt, trans_mult);
}

int64_t gainBaseSTPercent(char_data *ch, double amt, bool trans_mult) {
    return gainBaseST(ch, ch->basest * amt, trans_mult);
}

int64_t loseBasePLPercent(char_data *ch, double amt, bool trans_mult) {
    return loseBasePL(ch, ch->basepl * amt, trans_mult);
}

int64_t loseBaseKIPercent(char_data *ch, double amt, bool trans_mult) {
    return loseBaseKI(ch, ch->baseki * amt, trans_mult);
}

int64_t loseBaseSTPercent(char_data *ch, double amt, bool trans_mult) {
    return loseBaseST(ch, ch->basest * amt, trans_mult);
}

void gainBaseAllPercent(char_data *ch, double amt, bool trans_mult) {
    gainBasePLPercent(ch, amt, trans_mult);
    gainBaseKIPercent(ch, amt, trans_mult);
    gainBaseSTPercent(ch, amt, trans_mult);
}

void loseBaseAllPercent(char_data *ch, double amt, bool trans_mult) {
    loseBasePLPercent(ch, amt, trans_mult);
    loseBaseKIPercent(ch, amt, trans_mult);
    loseBaseSTPercent(ch, amt, trans_mult);
}

int64_t getMaxCarryWeight(char_data *ch) {
    return MAX(1L, (getMaxPL(ch) / 200) + (GET_STR(ch) * 50));
}

int64_t getCurGearWeight(char_data *ch) {
    int64_t total_weight = 0;

    for (int i = 0; i < NUM_WEARS; i++) {
        if (GET_EQ(ch, i)) {
            total_weight += GET_OBJ_WEIGHT(GET_EQ(ch, i));
        }
    }
    return total_weight;
}

int64_t getCurCarriedWeight(char_data *ch) {
    return getCurGearWeight(ch) + ch->carry_weight;
}

int64_t getAvailableCarryWeight(char_data *ch) {
    return getMaxCarryWeight(ch) - getCurCarriedWeight(ch);
}

// speednar is in utils.cpp

int64_t getEffMaxPL(char_data *ch) {
    int64_t maxpl = getMaxPL(ch);
    if(IS_NPC(ch)) {
        return maxpl;
    }
    double snar = speednar(ch);
    return maxpl * (1.0 - snar);
}

bool isWeightedPL(char_data *ch) {
    return getMaxPL(ch) > getEffMaxPL(ch);
}

void apply_kaioken(char_data *ch, int times, bool announce) {
    GET_KAIOKEN(ch) = times;
    REMOVE_BIT_AR(PLR_FLAGS(ch), PLR_POWERUP);

    if(announce) {
        send_to_char(ch, "@rA dark red aura bursts up around your body as you achieve Kaioken x %d!@n\r\n", times);
        act("@rA dark red aura bursts up around @R$n@r as they achieve a level of Kaioken!@n", TRUE, ch, 0, 0, TO_ROOM);
    }
}

void remove_kaioken(char_data *ch, int8_t announce) {
    int8_t kaio = GET_KAIOKEN(ch);
    if(!kaio) {
        return;
    }
    GET_KAIOKEN(ch) = 0;

    switch(announce) {
        case 1:
            send_to_char(ch, "You drop out of kaioken.\r\n");
            act("$n@w drops out of kaioken.@n", TRUE, ch, 0, 0, TO_ROOM);
            break;
        case 2:
            send_to_char(ch, "You lose focus and your kaioken disappears.\r\n");
            act("$n loses focus and $s kaioken aura disappears.", TRUE, ch, 0, 0, TO_ROOM);
    }
}

void dispel_ash(struct char_data *ch)
{
 struct obj_data *obj, *next_obj, *ash = nullptr;
 int there = FALSE;

 for (obj = world[IN_ROOM(ch)].contents; obj; obj = next_obj) {
     next_obj = obj->next_content;
  if (GET_OBJ_VNUM(obj) == 1306) {
   there = TRUE;
   ash = obj;
  }
 }

 if (ash) {
  int roll = axion_dice(0);
  if (GET_OBJ_COST(ash) == 3) {
   if (GET_INT(ch) > roll) {
    act("@GYou clear the air with the shockwaves of your power!@n", TRUE, ch, ash, 0, TO_CHAR);
    act("@C$n@G clears the air with the shockwaves of $s power!@n", TRUE, ch, ash, 0, TO_ROOM);
    extract_obj(ash);
   }
  } else if (GET_OBJ_COST(ash) == 2) {
   if (GET_INT(ch) + 10 > roll) {
    act("@GYou clear the air with the shockwaves of your power!@n", TRUE, ch, ash, 0, TO_CHAR);
    act("@C$n@G clears the air with the shockwaves of $s power!@n", TRUE, ch, ash, 0, TO_ROOM);
    extract_obj(ash);
   }
  } else if (GET_OBJ_COST(ash) == 1) {
   if (GET_INT(ch) + 20 > roll) {
    act("@GYou clear the air with the shockwaves of your power!@n", TRUE, ch, ash, 0, TO_CHAR);
    act("@C$n@G clears the air with the shockwaves of $s power!@n", TRUE, ch, ash, 0, TO_ROOM);
    extract_obj(ash);
   }
  }
 }

}

int has_group(struct char_data *ch)
{

  struct follow_type *k, *next;

 if (!AFF_FLAGGED(ch, AFF_GROUP))
  return (FALSE);

 if (ch->followers) {
  for (k = ch->followers; k; k = next) {
   next = k->next;
   if (!AFF_FLAGGED(k->follower, AFF_GROUP)) {
    continue;
   } else {
    return (TRUE);
   }
  }
 } else if (ch->master) {
  if (!AFF_FLAGGED(ch->master, AFF_GROUP))
   return (FALSE);
  else
   return (TRUE);
 }

 return (FALSE);
}

const char *report_party_health(struct char_data *ch)
{

  if (!AFF_FLAGGED(ch, AFF_GROUP))
   return ("");

  if (!ch->followers && !ch->master)
   return ("");

  struct follow_type *k, *next;
  int count = 0, stam1 = 8, stam2 = 8, stam3 = 8, stam4 = 8, plc1 = 4, plc2 = 4, plc3 = 4, plc4 = 4;
  struct char_data *party1 = NULL, *party2 = NULL, *party3 = NULL, *party4 = NULL;
  int64_t plperc1 = 0, plperc2 = 0, plperc3 = 0, plperc4 = 0;
  int64_t kiperc1 = 0, kiperc2 = 0, kiperc3 = 0, kiperc4 = 0;
  char result_party_health[MAX_STRING_LENGTH], result1[MAX_STRING_LENGTH], result2[MAX_STRING_LENGTH], result3[MAX_STRING_LENGTH], result4[MAX_STRING_LENGTH], result5[MAX_STRING_LENGTH];

  const char *plcol[5] = {"@r",
                          "@y",
                          "@Y",
                          "@G",
                          ""
                         };

  const char *exhaust[9] = {"Exhausted", /* 0/7 */
                            "Strained", /* 1/7 */
                            "Very Tired", /* 2/7 */
                            "Tired", /* 3/7 */
                            "Kinda Tired", /* 4/7 */
                            "Very Winded", /* 5/7 */
                            "Winded", /* 6/7 */
                            "Energetic",  /* 7/7 */
                            "?????????"
                           };

  const char *excol[9] = {"@r", /* 0/7 */
                          "@R", /* 1/7 */
                          "@R", /* 2/7 */
                          "@M", /* 3/7 */
                          "@M", /* 4/7 */
                          "@M", /* 5/7 */
                          "@G", /* 6/7 */
                          "@g",  /* 7/7 */
                          "@w"
                         };

  if (ch->followers) {
   for (k = ch->followers; k; k = next) {
    next = k->next;
    if (!AFF_FLAGGED(k->follower, AFF_GROUP))
     continue;
    if (k->follower != ch) {
     count += 1;
     if (count == 1) {
      party1 = k->follower;
      plperc1 = (GET_HIT(party1) * 100) / GET_MAX_HIT(party1);
      kiperc1 = (GET_CHARGE(party1) * 100) / GET_MAX_MANA(party1);

      if (plperc1 >= 80)
       plc1 = 3;
      else if (plperc1 >= 50)
       plc1 = 2;
      else if (plperc1 >= 30)
       plc1 = 1;
      else
       plc1 = 0;

      if ((getCurST(party1)) >= GET_MAX_MOVE(party1)) {
       stam1 = 7;
      } else if ((getCurST(party1)) >= GET_MAX_MOVE(party1) * .9) {
       stam1 = 6;
      } else if ((getCurST(party1)) >= GET_MAX_MOVE(party1) * .8) {
       stam1 = 5;
      } else if ((getCurST(party1)) >= GET_MAX_MOVE(party1) * .7) {
       stam1 = 4;
      } else if ((getCurST(party1)) >= GET_MAX_MOVE(party1) * .5) {
       stam1 = 3;
      } else if ((getCurST(party1)) >= GET_MAX_MOVE(party1) * .4) {
       stam1 = 2;
      } else if ((getCurST(party1)) >= GET_MAX_MOVE(party1) * .2) {
       stam1 = 1;
      } else {
       stam1 = 0;
      }
     } else if (count == 2) {
      party2 = k->follower;
      plperc2 = (GET_HIT(party2) * 100) / GET_MAX_HIT(party2);
      kiperc2 = (GET_CHARGE(party2) * 100) / GET_MAX_MANA(party2);

      if (plperc2 >= 80)
       plc2 = 3;
      else if (plperc2 >= 50)
       plc2 = 2;
      else if (plperc2 >= 30)
       plc2 = 1;
      else
       plc2 = 0;

      if ((getCurST(party2)) >= GET_MAX_MOVE(party2)) {
       stam2 = 7;
      } else if ((getCurST(party2)) >= GET_MAX_MOVE(party2) * .9) {
       stam2 = 6;
      } else if ((getCurST(party2)) >= GET_MAX_MOVE(party2) * .8) {
       stam2 = 5;
      } else if ((getCurST(party2)) >= GET_MAX_MOVE(party2) * .7) {
       stam2 = 4;
      } else if ((getCurST(party2)) >= GET_MAX_MOVE(party2) * .5) {
       stam2 = 3;
      } else if ((getCurST(party2)) >= GET_MAX_MOVE(party2) * .4) {
       stam2 = 2;
      } else if ((getCurST(party2)) >= GET_MAX_MOVE(party2) * .2) {
       stam2 = 1;
      } else {
       stam2 = 0;
      }
     } else if (count == 3) {
      party3 = k->follower;
      plperc3 = (GET_HIT(party3) * 100) / GET_MAX_HIT(party3);
      kiperc3 = (GET_CHARGE(party3) * 100) / GET_MAX_MANA(party3);

      if (plperc3 >= 80)
       plc3 = 3;
      else if (plperc3 >= 50)
       plc3 = 2;
      else if (plperc3 >= 30)
       plc3 = 1;
      else
       plc3 = 0;

      if ((getCurST(party3)) >= GET_MAX_MOVE(party3)) {
       stam3 = 7;
      } else if ((getCurST(party3)) >= GET_MAX_MOVE(party3) * .9) {
       stam3 = 6;
      } else if ((getCurST(party3)) >= GET_MAX_MOVE(party3) * .8) {
       stam3 = 5;
      } else if ((getCurST(party3)) >= GET_MAX_MOVE(party3) * .7) {
       stam3 = 4;
      } else if ((getCurST(party3)) >= GET_MAX_MOVE(party3) * .5) {
       stam3 = 3;
      } else if ((getCurST(party3)) >= GET_MAX_MOVE(party3) * .4) {
       stam3 = 2;
      } else if ((getCurST(party3)) >= GET_MAX_MOVE(party3) * .2) {
       stam3 = 1;
      } else {
       stam3 = 0;
      }
     } else if (count == 4) {
      party4 = k->follower;
      plperc4 = (GET_HIT(party4) * 100) / GET_MAX_HIT(party4);
      kiperc4 = (GET_CHARGE(party4) * 100) / GET_MAX_MANA(party4);

      if (plperc4 >= 80)
       plc4 = 3;
      else if (plperc4 >= 50)
       plc4 = 2;
      else if (plperc4 >= 30)
       plc4 = 1;
      else
       plc4 = 0;

      if ((getCurST(party4)) >= GET_MAX_MOVE(party4)) {
       stam4 = 7;
      } else if ((getCurST(party4)) >= GET_MAX_MOVE(party4) * .9) {
       stam4 = 6;
      } else if ((getCurST(party4)) >= GET_MAX_MOVE(party4) * .8) {
       stam4 = 5;
      } else if ((getCurST(party4)) >= GET_MAX_MOVE(party4) * .7) {
       stam4 = 4;
      } else if ((getCurST(party4)) >= GET_MAX_MOVE(party4) * .5) {
       stam4 = 3;
      } else if ((getCurST(party4)) >= GET_MAX_MOVE(party4) * .4) {
       stam4 = 2;
      } else if ((getCurST(party4)) >= GET_MAX_MOVE(party4) * .2) {
       stam4 = 1;
      } else {
       stam4 = 0;
      }
     }
    }
   }
   sprintf(result1, "@D[@BG@D]-------@mF@D------- -------@mF@D------- -------@mF@D------- -------@mF@D-------[@BG@D] <@RV@Y%s@R>@n\n", add_commas(GET_GROUPKILLS(ch)));
   sprintf(result2, "@D[@BR@D]@C%-15s %-15s %-15s %-15s@D[@BR@D]@n\n", party1 ? get_i_name(ch, party1) : "Empty", party2 ? get_i_name(ch, party2) : "Empty", party3 ? get_i_name(ch, party3) : "Empty", party4 ? get_i_name(ch, party4) : "Empty");
   sprintf(result3, "@D[@BO@D]@RPL@D:%s%11" I64T "@w%s @RPL@D:%s%11" I64T "@w%s @RPL@D:%s%11" I64T "@w%s @RPL@D:%s%11" I64T "@w%s@D[@BO@D]@n\n", plcol[plc1], plperc1, "%", plcol[plc2], plperc2, "%", plcol[plc3], plperc3, "%", plcol[plc4], plperc4, "%");
   sprintf(result4, "@D[@BU@D]@cCharge@D:@B%7" I64T "@w%s @cCharge@D:@B%7" I64T "@w%s @cCharge@D:@B%7" I64T "@w%s @cCharge@D:@B%7" I64T "@w%s@D[@BU@D]@n\n", kiperc1, "%", kiperc2, "%", kiperc3, "%", kiperc4, "%");
   sprintf(result5, "@D[@BP@D]@gSt@D:%s%12s @gSt@D:%s%12s @gSt@D:%s%12s @gSt@D:%s%12s@D[@BP@D]@n", excol[stam1], exhaust[stam1], excol[stam2], exhaust[stam2], excol[stam3], exhaust[stam3], excol[stam4], exhaust[stam4]);
   sprintf(result_party_health, "%s%s%s%s%s\n", result1, result2, result3, result4, result5);
   ch->temp_prompt = strdup(result_party_health);
   return (ch->temp_prompt);
  } else if (ch->master && AFF_FLAGGED(ch->master, AFF_GROUP)) {
      party1 = ch->master;
      plperc1 = (GET_HIT(party1) * 100) / GET_MAX_HIT(party1);
      kiperc1 = (GET_CHARGE(party1) * 100) / GET_MAX_MANA(party1);

      if (plperc1 >= 80)
       plc1 = 3;
      else if (plperc1 >= 50)
       plc1 = 2;
      else if (plperc1 >= 30)
       plc1 = 1;
      else
       plc1 = 0;

      if ((getCurST(party1)) >= GET_MAX_MOVE(party1)) {
       stam1 = 7;
      } else if ((getCurST(party1)) >= GET_MAX_MOVE(party1) * .9) {
       stam1 = 6;
      } else if ((getCurST(party1)) >= GET_MAX_MOVE(party1) * .8) {
       stam1 = 5;
      } else if ((getCurST(party1)) >= GET_MAX_MOVE(party1) * .7) {
       stam1 = 4;
      } else if ((getCurST(party1)) >= GET_MAX_MOVE(party1) * .5) {
       stam1 = 3;
      } else if ((getCurST(party1)) >= GET_MAX_MOVE(party1) * .4) {
       stam1 = 2;
      } else if ((getCurST(party1)) >= GET_MAX_MOVE(party1) * .2) {
       stam1 = 1;
      } else {
       stam1 = 0;
      }
    count = 1;

    for (k = party1->followers; k; k = next) {
     next = k->next;
     if (!AFF_FLAGGED(k->follower, AFF_GROUP))
      continue;
     if (k->follower != ch) {
      count += 1;
      if (count == 2) {
       party2 = k->follower;
       plperc2 = (GET_HIT(party2) * 100) / GET_MAX_HIT(party2);
       kiperc2 = (GET_CHARGE(party2) * 100) / GET_MAX_MANA(party2);

       if (plperc2 >= 80)
        plc2 = 3;
       else if (plperc2 >= 50)
        plc2 = 2;
       else if (plperc2 >= 30)
        plc2 = 1;
       else
        plc2 = 0;

       if ((getCurST(party2)) >= GET_MAX_MOVE(party2)) {
        stam2 = 7;
       } else if ((getCurST(party2)) >= GET_MAX_MOVE(party2) * .9) {
        stam2 = 6;
       } else if ((getCurST(party2)) >= GET_MAX_MOVE(party2) * .8) {
        stam2 = 5;
       } else if ((getCurST(party2)) >= GET_MAX_MOVE(party2) * .7) {
        stam2 = 4;
       } else if ((getCurST(party2)) >= GET_MAX_MOVE(party2) * .5) {
        stam2 = 3;
       } else if ((getCurST(party2)) >= GET_MAX_MOVE(party2) * .4) {
        stam2 = 2;
       } else if ((getCurST(party2)) >= GET_MAX_MOVE(party2) * .2) {
        stam2 = 1;
       } else {
        stam2 = 0;
       }
      } else if (count == 3) {
       party3 = k->follower;
       plperc3 = (GET_HIT(party3) * 100) / GET_MAX_HIT(party3);
       kiperc3 = (GET_CHARGE(party3) * 100) / GET_MAX_MANA(party3);

       if (plperc3 >= 80)
        plc3 = 3;
       else if (plperc3 >= 50)
        plc3 = 2;
       else if (plperc3 >= 30)
        plc3 = 1;
       else
        plc3 = 0;

       if ((getCurST(party3)) >= GET_MAX_MOVE(party3)) {
        stam3 = 7;
       } else if ((getCurST(party3)) >= GET_MAX_MOVE(party3) * .9) {
        stam3 = 6;
       } else if ((getCurST(party3)) >= GET_MAX_MOVE(party3) * .8) {
        stam3 = 5;
       } else if ((getCurST(party3)) >= GET_MAX_MOVE(party3) * .7) {
        stam3 = 4;
       } else if ((getCurST(party3)) >= GET_MAX_MOVE(party3) * .5) {
        stam3 = 3;
       } else if ((getCurST(party3)) >= GET_MAX_MOVE(party3) * .4) {
        stam3 = 2;
       } else if ((getCurST(party3)) >= GET_MAX_MOVE(party3) * .2) {
        stam3 = 1;
       } else {
        stam3 = 0;
       }
      } else if (count == 4) {
       party4 = k->follower;
       plperc4 = (GET_HIT(party4) * 100) / GET_MAX_HIT(party4);
       kiperc4 = (GET_CHARGE(party4) * 100) / GET_MAX_MANA(party4);

       if (plperc4 >= 80)
        plc4 = 3;
       else if (plperc4 >= 50)
        plc4 = 2;
       else if (plperc4 >= 30)
        plc4 = 1;
       else
        plc4 = 0;

       if ((getCurST(party4)) >= GET_MAX_MOVE(party4)) {
        stam4 = 7;
       } else if ((getCurST(party4)) >= GET_MAX_MOVE(party4) * .9) {
        stam4 = 6;
       } else if ((getCurST(party4)) >= GET_MAX_MOVE(party4) * .8) {
        stam4 = 5;
       } else if ((getCurST(party4)) >= GET_MAX_MOVE(party4) * .7) {
        stam4 = 4;
       } else if ((getCurST(party4)) >= GET_MAX_MOVE(party4) * .5) {
        stam4 = 3;
       } else if ((getCurST(party4)) >= GET_MAX_MOVE(party4) * .4) {
        stam4 = 2;
       } else if ((getCurST(party4)) >= GET_MAX_MOVE(party4) * .2) {
        stam4 = 1;
       } else {
        stam4 = 0;
       }
      }
    } /* Is follower */
   } /* End for */

   sprintf(result1, "@D[@BG@D]-------@YL@D------- -------@mF@D------- -------@mF@D------- -------@mF@D-------[@BG@D]@n\n");
   sprintf(result2, "@D[@BR@D]@C%-15s %-15s %-15s %-15s@D[@BR@D]@n\n", party1 ? get_i_name(ch, party1) : "Empty", party2 ? get_i_name(ch, party2) : "Empty", party3 ? get_i_name(ch, party3) : "Empty", party4 ? get_i_name(ch, party4) : "Empty");
   sprintf(result3, "@D[@BO@D]@RPL@D:%s%11" I64T "@w%s @RPL@D:%s%11" I64T "@w%s @RPL@D:%s%11" I64T "@w%s @RPL@D:%s%11" I64T "@w%s@D[@BO@D]@n\n", plcol[plc1], plperc1, "%", plcol[plc2], plperc2, "%", plcol[plc3], plperc3, "%", plcol[plc4], plperc4, "%");
   sprintf(result4, "@D[@BU@D]@cCharge@D:@B%7" I64T "@w%s @cCharge@D:@B%7" I64T "@w%s @cCharge@D:@B%7" I64T "@w%s @cCharge@D:@B%7" I64T "@w%s@D[@BU@D]@n\n", kiperc1, "%", kiperc2, "%", kiperc3, "%", kiperc4, "%");
   sprintf(result5, "@D[@BP@D]@gSt@D:%s%12s @gSt@D:%s%12s @gSt@D:%s%12s @gSt@D:%s%12s@D[@BP@D]@n", excol[stam1], exhaust[stam1], excol[stam2], exhaust[stam2], excol[stam3], exhaust[stam3], excol[stam4], exhaust[stam4]);
   sprintf(result_party_health, "%s%s%s%s%s\n", result1, result2, result3, result4, result5);
   ch->temp_prompt = strdup(result_party_health);
   return (ch->temp_prompt);
  } else {
   return ("");
  }

}


/* Check to see if anything interferes with their "knowing" the skill */
int know_skill(struct char_data *ch, int skill)
{

 int know = 0;

 if (GET_SKILL(ch, skill) > 0)
  know = 1;
 if (GET_STUPIDKISS(ch) == skill)
  know = 2;

 if (know == 0) {
  send_to_char(ch, "You do not know how to perform %s.\r\n", spell_info[skill].name);
  know = 0;
 } else if (know == 2) {
  send_to_char(ch, "@WYou try to use @M%s@W but lingering thoughts of a certain kiss distracts you!@n\r\n", spell_info[skill].name);
  send_to_char(ch, "You must sleep in order to cure this.\r\n");
  know = 0;
 }

 return (know);
}


void null_affect(struct char_data *ch, int aff_flag)
{

  struct affected_type *af, *next_af;

  for (af = ch->affected; af; af = next_af) {
    next_af = af->next;
    if (af->location == APPLY_NONE && af->bitvector == aff_flag)
      affect_remove(ch, af);
  }
}

void assign_affect(struct char_data *ch, int aff_flag, int skill, int dur, int str, int con, int intel, int agl, int wis, int spd)
{
  struct affected_type af[6];
  int num = 0;

  if (dur <= 0)
   dur = 1;

  if (str == 0 && con == 0 && wis == 0 && intel == 0 && agl == 0 && spd == 0) {
   af[num].type = skill;
   af[num].duration = dur;
   af[num].modifier = 0;
   af[num].location = APPLY_NONE;
   af[num].bitvector = aff_flag;
   affect_join(ch, &af[num], FALSE, FALSE, FALSE, FALSE);
   num += 1;
  }
  if (str != 0) {
   af[num].type = skill;
   af[num].duration = dur;
   af[num].modifier = str;
   af[num].location = APPLY_STR;
   af[num].bitvector = aff_flag;
   affect_join(ch, &af[num], FALSE, FALSE, FALSE, FALSE);
   num += 1;
  }
  if (con != 0) {
   af[num].type = skill;
   af[num].duration = dur;
   af[num].modifier = con;
   af[num].location = APPLY_CON;
   af[num].bitvector = aff_flag;
   affect_join(ch, &af[num], FALSE, FALSE, FALSE, FALSE);
   num += 1;
  }
  if (intel != 0) {
   af[num].type = skill;
   af[num].duration = dur;
   af[num].modifier = intel;
   af[num].location = APPLY_INT;
   af[num].bitvector = aff_flag;
   affect_join(ch, &af[num], FALSE, FALSE, FALSE, FALSE);
   num += 1;
  }
  if (agl != 0) {
   af[num].type = skill;
   af[num].duration = dur;
   af[num].modifier = agl;
   af[num].location = APPLY_DEX;
   af[num].bitvector = aff_flag;
   affect_join(ch, &af[num], FALSE, FALSE, FALSE, FALSE);
   num += 1;
  }
  if (spd != 0) {
   af[num].type = skill;
   af[num].duration = dur;
   af[num].modifier = spd;
   af[num].location = APPLY_CHA;
   af[num].bitvector = aff_flag;
   affect_join(ch, &af[num], FALSE, FALSE, FALSE, FALSE);
   num += 1;
  }
  if (wis != 0) {
   af[num].type = skill;
   af[num].duration = dur;
   af[num].modifier = wis;
   af[num].location = APPLY_WIS;
   af[num].bitvector = aff_flag;
   affect_join(ch, &af[num], FALSE, FALSE, FALSE, FALSE);
   num += 1;
  }
}

int sec_roll_check(struct char_data *ch)
{

 int figure = 0, chance = 0, outcome = 0;

 figure = 10 + (GET_LEVEL(ch) * 1.6);

 chance = axion_dice(0) + axion_dice(0) + rand_number(0, 20);

 if (figure >= chance)
  outcome = 1;

 return (outcome);

}

int get_measure(struct char_data *ch, int height, int weight)
{
  int amt = 0;
  if (!PLR_FLAGGED(ch, PLR_OOZARU) && (!IS_ICER(ch) || !IS_TRANSFORMED(ch)) && GET_GENOME(ch, 0) < 9) {
   if (height > 0) {
    amt = height;
   } else if (weight > 0) {
    amt = weight;
   }
  } else if (IS_ICER(ch) && PLR_FLAGGED(ch, PLR_TRANS1)) {
   if (height > 0) {
    amt = height * 3;
   } else if (weight > 0) {
    amt = weight * 4;
   }
  } else if (IS_ICER(ch) && PLR_FLAGGED(ch, PLR_TRANS2)) {
   if (height > 0) {
    amt = height * 3;
   } else if (weight > 0) {
    amt = weight * 5;
   }
  } else if (IS_ICER(ch) && PLR_FLAGGED(ch, PLR_TRANS3)) {
   if (height > 0) {
    amt = height * 1.5;
   } else if (weight > 0) {
    amt = weight * 2;
   }
  } else if (IS_ICER(ch) && PLR_FLAGGED(ch, PLR_TRANS4)) {
   if (height > 0) {
    amt = height * 2;
   } else if (weight > 0) {
    amt = weight * 3;
   }
  } else if (PLR_FLAGGED(ch, PLR_OOZARU) || GET_GENOME(ch, 0) == 9) {
   if (height > 0) {
    amt = height * 10;
   } else if (weight > 0) {
    amt = weight * 50;
   }
  }

 return (amt);
}


int64_t physical_cost(struct char_data *ch, int skill)
{

 int64_t result = 0;

 if (skill == SKILL_PUNCH)
  result = GET_MAX_HIT(ch) / 500;
 else if (skill == SKILL_KICK)
  result = GET_MAX_HIT(ch) / 350;
 else if (skill == SKILL_ELBOW)
  result = GET_MAX_HIT(ch) / 400;
 else if (skill == SKILL_KNEE)
  result = GET_MAX_HIT(ch) / 300;
 else if (skill == SKILL_UPPERCUT)
  result = GET_MAX_HIT(ch) / 200;
 else if (skill == SKILL_ROUNDHOUSE)
  result = GET_MAX_HIT(ch) / 150;
 else if (skill == SKILL_HEELDROP)
  result = GET_MAX_HIT(ch) / 80;
 else if (skill == SKILL_SLAM)
  result = GET_MAX_HIT(ch) / 90;

 int cou1 = 1 + rand_number(1, 20), cou2 = cou1 + rand_number(1, 6);

 result += rand_number(cou1, cou2);

 if (GET_SKILL_BASE(ch, SKILL_STYLE) >= 100) {
  result -= result * 0.4;
 } else if (GET_SKILL_BASE(ch, SKILL_STYLE) >= 75) {
  if (IS_TSUNA(ch)) {
   result -= result * 0.40;
  } else if (IS_TAPION(ch) && (skill == SKILL_PUNCH || skill == SKILL_KICK)) {
   result -= result * 0.35;
  } else if (IS_JINTO(ch)) {
   if (GET_SKILL_BASE(ch, skill) >= 100) {
    result -= result * 0.45;
   } else {
    result -= result * 0.25;
   }
  } else {
   result -= result * 0.25;
  }
 } else if (GET_SKILL_BASE(ch, SKILL_STYLE) >= 50) {
  result -= result * 0.25;
 }

 if (IS_ANDROID(ch)) {
  result *= 0.25;
 }

 return (result);
}

int trans_cost(struct char_data *ch, int trans)
{

 if (GET_TRANSCOST(ch, trans) == 0) {
  return (50);
 } else {
  return (0);
 }
}

/* This returns the trans requirement for the player based on their trans class */
/* 3 = Great, 2 = Average, 1 = Terrible */
/* Rillao: transloc, add new transes here */
int trans_req(struct char_data *ch, int trans)
{

 int requirement = 0;

 if (IS_HUMAN(ch)) {
  switch (trans)
  {
   case 1:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 1500000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 1800000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 2100000;
    }
    break;
   case 2:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 37500000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 35000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 32500000;
    }
    break;
   case 3:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 200000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 190000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 185000000;
    }
    break;
   case 4:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 1400000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 1200000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 1100000000;
    }
    break;
  }
 } /* End Human Requirement */

 if (IS_HALFBREED(ch)) {
  switch (trans)
  {
   case 1:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 1500000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 1400000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 1200000;
    }
    break;
   case 2:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 60000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 55000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 50000000;
    }
    break;
   case 3:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 1200000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 1050000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 950000000;
    }
    break;
  }
 } /* End Halfbreed Requirement */

 if (IS_SAIYAN(ch)) {
  if (PLR_FLAGGED(ch, PLR_LSSJ)) {
   switch (trans)
   {
    case 1:
     if (GET_TRANSCLASS(ch) == 1) {
      requirement = 600000;
     } else if (GET_TRANSCLASS(ch) == 2) {
      requirement = 500000;
     } else if (GET_TRANSCLASS(ch) == 3) {
      requirement = 450000;
     }
     break;
     case 2:
     if (GET_TRANSCLASS(ch) == 1) {
      requirement = 300000000;
     } else if (GET_TRANSCLASS(ch) == 2) {
      requirement = 250000000;
     } else if (GET_TRANSCLASS(ch) == 3) {
      requirement = 225000000;
     }
     break;
   }
  } else {
   switch (trans)
   {
    case 1:
     if (GET_TRANSCLASS(ch) == 1) {
      requirement = 1400000;
     } else if (GET_TRANSCLASS(ch) == 2) {
      requirement = 1200000;
     } else if (GET_TRANSCLASS(ch) == 3) {
      requirement = 1100000;
     }
     break;
    case 2:
     if (GET_TRANSCLASS(ch) == 1) {
      requirement = 60000000;
     } else if (GET_TRANSCLASS(ch) == 2) {
      requirement = 55000000;
     } else if (GET_TRANSCLASS(ch) == 3) {
      requirement = 50000000;
     }
     break;
    case 3:
     if (GET_TRANSCLASS(ch) == 1) {
      requirement = 160000000;
     } else if (GET_TRANSCLASS(ch) == 2) {
      requirement = 150000000;
     } else if (GET_TRANSCLASS(ch) == 3) {
      requirement = 140000000;
     }
     break;
    case 4:
     if (GET_TRANSCLASS(ch) == 1) {
      requirement = 1800000000;
     } else if (GET_TRANSCLASS(ch) == 2) {
      requirement = 1625000000;
     } else if (GET_TRANSCLASS(ch) == 3) {
      requirement = 1400000000;
     }
    break;
   }
  }
 } /* End Saiyan Requirement */

 if (IS_NAMEK(ch)) {
  switch (trans)
  {
   case 1:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 400000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 360000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 335000;
    }
    break;
   case 2:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 10000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 9500000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 8000000;
    }
    break;
   case 3:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 240000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 220000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 200000000;
    }
    break;
   case 4:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 950000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 900000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 875000000;
    }
    break;
  }
 } /* End Namek Requirement */

 if (IS_ICER(ch)) {
  switch (trans)
  {
   case 1:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 550000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 500000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 450000;
    }
    break;
   case 2:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 20000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 17500000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 15000000;
    }
    break;
   case 3:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 180000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 150000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 125000000;
    }
    break;
   case 4:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 880000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 850000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 820000000;
    }
    break;
  }
 } /* End Icer Requirement */

 if (IS_MAJIN(ch)) {
  switch (trans)
  {
   case 1:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 2400000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 2200000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 2000000;
    }
    break;
   case 2:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 50000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 45000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 40000000;
    }
    break;
   case 3:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 1800000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 1550000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 1300000000;
    }
    break;
  }
 } /* End Majin Requirement */

 if (IS_TRUFFLE(ch)) {
  switch (trans)
  {
   case 1:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 3800000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 3600000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 3500000;
    }
    break;
   case 2:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 400000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 300000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 200000000;
    }
    break;
    case 3:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 1550000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 1450000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 1250000000;
    }
    break;
  }
 } /* End Truffle Requirement */

 if (IS_MUTANT(ch)) {
  switch (trans)
  {
   case 1:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 200000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 180000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 160000;
    }
    break;
   case 2:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 30000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 27500000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 25000000;
    }
    break;
   case 3:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 750000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 700000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 675000000;
    }
    break;
  }
 } /* End Mutant Requirement */

 if (IS_KAI(ch)) {
  switch (trans)
  {
   case 1:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 3250000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 3000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 2850000;
    }
    break;
   case 2:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 700000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 650000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 625000000;
    }
    break;
    case 3:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 1500000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 1300000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 1250000000;
    }
    break;
  }
 } /* End Kai Requirement */

 if (IS_KONATSU(ch)) {
  switch (trans)
  {
   case 1:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 2000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 1800000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 1600000;
    }
    break;
   case 2:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 250000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 225000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 200000000;
    }
    break;
   case 3:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 1600000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 1400000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 1300000000;
    }
    break;
  }
 } /* End Konatsu Requirement */

 if (IS_ANDROID(ch)) {
  switch (trans)
  {
   case 1:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 1200000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 1000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 850000;
    }
    break;
   case 2:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 8500000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 8000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 7750000;
    }
    break;
   case 3:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 55000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 50000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 40000000;
    }
    break;
   case 4:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 325000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 300000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 275000000;
    }
    break;
   case 5:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 900000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 800000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 750000000;
    }
    break;
   case 6:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 1300000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 1200000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 1100000000;
    }
    break;
  }
 } /* End Android Requirement */

 if (IS_BIO(ch)) {
  switch (trans)
  {
   case 1:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 2000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 1800000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 1700000;
    }
    break;
   case 2:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 30000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 25000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 20000000;
    }
    break;
   case 3:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 235000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 220000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 210000000;
    }
    break;
   case 4:
    if (GET_TRANSCLASS(ch) == 1) {
     requirement = 1500000000;
    } else if (GET_TRANSCLASS(ch) == 2) {
     requirement = 1300000000;
    } else if (GET_TRANSCLASS(ch) == 3) {
     requirement = 1150000000;
    }
    break;
  }
 } /* End Bioandroid Requirement */

 return (requirement);
}


const char *disp_align(struct char_data *ch)
{
 int align;

 if (GET_ALIGNMENT(ch) < -800) { // Horrifically Evil
  align = 8;
 } else if (GET_ALIGNMENT(ch) < -600) { // Terrible
  align = 7;
 } else if (GET_ALIGNMENT(ch) < -300) { // Villain
  align = 6;
 } else if (GET_ALIGNMENT(ch) < -50) {  // Crook
  align = 5;
 } else if (GET_ALIGNMENT(ch) < 51) {   // Neutral
  align = 4;
 } else if (GET_ALIGNMENT(ch) < 300) {  // Do-gooder
  align = 3;
 } else if (GET_ALIGNMENT(ch) < 600) {  // Hero
  align = 2;
 } else if (GET_ALIGNMENT(ch) < 800) {  // Valiant
  align = 1;
 } else {                               // Saintly
  align = 0;
 }

 return (alignments[align]);
}


/* This creates a player's sense memory file for the first time. */
void senseCreate(struct char_data *ch)
{
  char fname[40];
  FILE *fl;
  /* Write Sense File */
  if (!get_filename(fname, sizeof(fname), SENSE_FILE, GET_NAME(ch)))
    return;

  if( !(fl = fopen(fname, "w")) ) {
    log("ERROR: could not save sense memory of, %s, to filename, %s.", GET_NAME(ch), fname);
    return;
  }

  fprintf(fl, "0\n");

  fclose(fl);
  return;
}

int read_sense_memory(struct char_data *ch, struct char_data *vict) {
  char fname[40], line[256];
  int known = FALSE, idnums = -1337;
  FILE *fl;

  /* Read Sense File */
  if (vict == NULL) {
    log("Noone.");
    return 0;
  }

  if (!get_filename(fname, sizeof(fname), SENSE_FILE, GET_NAME(ch))) {
    senseCreate(ch);
  }
  if (!(fl = fopen(fname, "r"))) {
    return 2;
  }
  if (vict == ch) {
    fclose(fl);
    return 0;
  }

  while (!feof(fl)) {
    get_line(fl, line);
    sscanf(line, "%d\n", &idnums);
    if (IS_NPC(vict)) {
     if (idnums == GET_MOB_VNUM(vict)) {
      known = TRUE;
     }
    } else {
     if (idnums == GET_ID(vict)) {
      known = TRUE;
     }
    }
  }
   fclose(fl);

   if (known == TRUE)
    return 1;
   else
    return 0;
}

/* This writes a player's sense memory to file. */
void sense_memory_write(struct char_data *ch, struct char_data *vict)
{
  FILE *file;
  char fname[40], line[256];
  int idnums[500] = {0};
  FILE *fl;
  int count = 0, x = 0, id_sample;

  /* Read Sense File */
  if (!get_filename(fname, sizeof(fname), SENSE_FILE, GET_NAME(ch))) {
    senseCreate(ch);
  }
  if (!(file = fopen(fname, "r"))) {
    return;
  }
  while (!feof(file) || count < 498) {
    get_line(file, line);
    sscanf(line, "%d\n", &id_sample);
    idnums[count] = id_sample;
    count++;
  }
  fclose(file);

  /* Write Sense File */

  if (!get_filename(fname, sizeof(fname), SENSE_FILE, GET_NAME(ch)))
    return;

  if( !(fl = fopen(fname, "w")) ) {
    log("ERROR: could not save sense memory file, %s, to filename, %s.", GET_NAME(ch), fname);
    return;
  }

  while(x < count) {
    if (x == 0 || idnums[x-1] != idnums[x]) {
     if (!IS_NPC(vict)) {
      if (idnums[x] != GET_ID(vict)) {
       fprintf(fl, "%d\n", idnums[x]);
      }
     } else {
      if (idnums[x] != GET_MOB_VNUM(vict)) {
       fprintf(fl, "%d\n", idnums[x]);
      }
     }
    }
    x++;
  }

  if (!IS_NPC(vict)) {
   fprintf(fl, "%d\n", GET_ID(vict));
  } else {
   fprintf(fl, "%d\n", GET_MOB_VNUM(vict));
  }

  fclose(fl);
  return;
}

/* Will they manage to pursue a fleeing enemy? */
int roll_pursue(struct char_data *ch, struct char_data *vict)
{

 int skill, perc = axion_dice(0);

 if (ch == NULL || vict == NULL)
  return (FALSE);

 if (!IS_NPC(ch)) {
  skill = GET_SKILL(ch, SKILL_PURSUIT);
 } else if (IS_NPC(ch) && !MOB_FLAGGED(ch, MOB_SENTINEL)) {
  skill = GET_LEVEL(ch);
  if (ROOM_FLAGGED(IN_ROOM(vict), ROOM_NOMOB))
   skill = -1;
 } else {
  skill = -1;
 }

 if (!IS_NPC(vict)) {
  if (IS_NPC(ch) && !(vict->desc)) {
   skill = -1;
  }
 }

 if (skill > perc) {
  int inroom = GET_ROOM_VNUM(IN_ROOM(ch));
  act("@C$n@R pursues after the fleeing @c$N@R!@n", TRUE, ch, 0, vict, TO_NOTVICT);  
  char_from_room(ch);
  char_to_room(ch, IN_ROOM(vict));
  act("@GYou pursue right after @c$N@G!@n", TRUE, ch, 0, vict, TO_CHAR);
  act("@C$n@R pursues after you!@n", TRUE, ch, 0, vict, TO_VICT);
  act("@C$n@R pursues after the fleeing @c$N@R!@n", TRUE, ch, 0, vict, TO_NOTVICT);

  struct follow_type *k, *next;

  if (ch->followers) {
   for (k = ch->followers; k; k = next) {
    next = k->next;
    if ((GET_ROOM_VNUM(IN_ROOM(k->follower)) == inroom) && (GET_POS(k->follower) >= POS_STANDING) && (!AFF_FLAGGED(ch, AFF_ZANZOKEN) || (AFF_FLAGGED(ch, AFF_GROUP) && AFF_FLAGGED(k->follower, AFF_GROUP)))) {
     act("You follow $N.", TRUE, k->follower, 0, ch, TO_CHAR);
     act("$n follows after $N.", TRUE, k->follower, 0, ch, TO_NOTVICT);
     act("$n follows after you.", TRUE, k->follower, 0, ch, TO_VICT);
     char_from_room(k->follower);
     char_to_room(k->follower, IN_ROOM(ch));
    }
   }
  }
  REMOVE_BIT_AR(AFF_FLAGS(vict), AFF_PURSUIT);
  return (TRUE);
 } else {
  send_to_char(ch, "@RYou fail to pursue after them!@n\r\n");
  if (FIGHTING(ch)) {
   stop_fighting(ch);
  } if (FIGHTING(vict)) {
   stop_fighting(vict);
  }
  return (FALSE);
 }

}


char *sense_location(struct char_data *ch)
{

	char *message;
    CREATE(message, char, MAX_INPUT_LENGTH);
	int roomnum = GET_ROOM_VNUM(IN_ROOM(ch)), num = 0;
	if ((num = real_zone_by_thing(roomnum)) != NOWHERE) {
		num = real_zone_by_thing(roomnum);
	}

	switch (num) {
	case 2:
		sprintf(message, "East of Nexus City");
		break;
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		if (roomnum < 795)
			sprintf(message, "Nexus City");
		else
			sprintf(message, "South Ocean");
		break;
	case 8:
	case 9:
	case 10:
	case 11:
		if (roomnum < 1133)
			sprintf(message, "South Ocean");
		else if (roomnum < 1179)
			sprintf(message, "Nexus Field");
		else
			sprintf(message, "Cherry Blossom Mountain");
		break;
	case 12:
	case 13:
		if (roomnum < 1287)
			sprintf(message, "Cherry Blossom Mountain");
		else
			sprintf(message, "Sandy Desert");
		break;
	case 14:
		if (roomnum < 1428)
			sprintf(message, "Sandy Desert");
		else if (roomnum < 1484)
			sprintf(message, "Northern Plains");
		else if (roomnum < 1496)
			sprintf(message, "Korin's Tower");
		else
			sprintf(message, "Kami's Lookout");
		break;
	case 15:
		if (roomnum < 1577)
			sprintf(message, "Kami's Lookout");
		else if (roomnum < 1580)
			sprintf(message, "Northern Plains");
		else if (roomnum < 1589)
			sprintf(message, "Kami's Lookout");
		else
			sprintf(message, "Shadow Forest");
		break;
	case 16:
		sprintf(message, "Shadow Forest");
		break;
	case 17:
	case 18:
		if (roomnum < 1715)
			sprintf(message, "Decrepit Area");
		else
			sprintf(message, "Inside Cherry Blossom Mountain");
		break;
	case 19:
		sprintf(message, "West City");
		break;
	case 20:
		if (roomnum < 2012)
			sprintf(message, "West City");
		else if (roomnum > 2070)
			sprintf(message, "West City");
		else
			sprintf(message, "Silver Mine");
		break;
	case 21:
		if (roomnum < 2141)
			sprintf(message, "West City");
		else
			sprintf(message, "Hercule Beach");
		break;
	case 22:
		sprintf(message, "Vegetos City");
		break;
	case 23:
	case 24:
		if (roomnum < 2334)
			sprintf(message, "Vegetos City");
		else if (roomnum > 2462)
			sprintf(message, "Vegetos City");
		else
			sprintf(message, "Vegetos Palace");
		break;
	case 25:
	case 26:
		if (roomnum < 2616)
			sprintf(message, "Blood Dunes");
		else
			sprintf(message, "Ancestral Mountains");
		break;
	case 27:
		if (roomnum < 2709)
			sprintf(message, "Ancestral Mountains");
		else if (roomnum < 2736)
			sprintf(message, "Destopa Swamp");
		else
			sprintf(message, "Swamp Base");
		break;
	case 28:
		sprintf(message, "Pride Forest");
		break;
	case 29:
	case 30:
	case 31:
		sprintf(message, "Pride Tower");
		break;
	case 32:
		sprintf(message, "Ruby Cave");
		break;
	case 34:
		sprintf(message, "Utatlan City");
		break;
	case 35:
		sprintf(message, "Zenith Jungle");
		break;
	case 40:
	case 41:
	case 42:
		sprintf(message, "Ice Crown City");
		break;
	case 43:
		if (roomnum < 4351)
			sprintf(message, "Ice Highway");
		else
			sprintf(message, "Topica Snowfield");
		break;
	case 44:
	case 45:
		sprintf(message, "Glug's Volcano");
		break;
	case 46:
	case 47:
		sprintf(message, "Platonic Sea");
		break;
	case 48:
		sprintf(message, "Slave City");
		break;
	case 49:
		if (roomnum < 4915)
			sprintf(message, "Descent Down Icecrown");
		else if (roomnum != 4915 && roomnum < 4994)
			sprintf(message, "Topica Snowfield");
		else
			sprintf(message, "Ice Highway");
		break;
	case 50:
		sprintf(message, "Mirror Shard Maze");
		break;
	case 51:
		if (roomnum < 5150)
			sprintf(message, "Acturian Woods");
		else if (roomnum < 5165)
			sprintf(message, "Desolate Demesne");
		else
			sprintf(message, "Chateau Ishran");
		break;
	case 52:
		sprintf(message, "Wyrm Spine Mountain");
		break;
	case 53:
	case 54:
		sprintf(message, "Aromina Hunting Preserve");
		break;
	case 55:
		sprintf(message, "Cloud Ruler Temple");
		break;
	case 56:
		sprintf(message, "Koltoan Mine");
		break;
	case 78:
		sprintf(message, "Orium Cave");
		break;
	case 79:
		sprintf(message, "Crystalline Forest");
		break;
	case 80:
	case 81:
	case 82:
		sprintf(message, "Tiranoc City");
		break;
	case 83:
		sprintf(message, "Great Oroist Temple");
		break;
	case 84:
		if (roomnum < 8447)
			sprintf(message, "Elsthuan Forest");
		else
			sprintf(message, "Mazori Farm");
		break;
	case 85:
		sprintf(message, "Dres");
		break;
	case 86:
		sprintf(message, "Colvian Farm");
		break;
	case 87:
		sprintf(message, "Saint Alucia");
		break;
	case 88:
		if (roomnum < 8847)
			sprintf(message, "Meridius Memorial");
		else
			sprintf(message, "Battlefields");
		break;
	case 89:
		if (roomnum < 8954)
			sprintf(message, "Desert of Illusion");
		else
			sprintf(message, "Plains of Confusion");
		break;
	case 90:
		sprintf(message, "Shadowlas Temple");
		break;
	case 92:
		sprintf(message, "Turlon Fair");
		break;
	case 97:
		sprintf(message, "Wetlands");
		break;
	case 98:
		if (roomnum < 9855)
			sprintf(message, "Wetlands");
		else if (roomnum < 9866)
			sprintf(message, "Kerberos");
		else
			sprintf(message, "Shaeras Mansion");
		break;
	case 99:
		if (roomnum < 9907)
			sprintf(message, "Slavinos Ravine");
		else if (roomnum < 9960)
			sprintf(message, "Kerberos");
		else
			sprintf(message, "Furian Citadel");
		break;
	case 100:
	case 101:
	case 102:
	case 103:
	case 104:
	case 105:
	case 106:
	case 107:
	case 108:
	case 109:
	case 110:
	case 111:
	case 112:
	case 113:
	case 114:
	case 115:
		sprintf(message, "Namekian Wilderness");
		break;
	case 116:
		if (roomnum < 11672)
			sprintf(message, "Senzu Village");
		else if (roomnum > 11672 && roomnum < 11698)
			sprintf(message, "Senzu Village");
		else
			sprintf(message, "Guru's House");
		break;
	case 117:
	case 118:
	case 119:
		sprintf(message, "Crystalline Cave");
		break;
	case 120:
		sprintf(message, "Haven City");
		break;
	case 121:
		if (roomnum < 12103)
			sprintf(message, "Haven City");
		else
			sprintf(message, "Serenity Lake");
		break;
	case 122:
		sprintf(message, "Serenity Lake");
		break;
	case 123:
		sprintf(message, "Kaiju Forest");
		break;
	case 124:
		if (roomnum < 12480)
			sprintf(message, "Ortusian Temple");
		else
			sprintf(message, "Silent Glade");
		break;
	case 125:
		sprintf(message, "Near Serenity Lake");
		break;
	case 130:
	case 131:
		if (roomnum < 13153)
			sprintf(message, "Satan City");
		else if (roomnum == 13153)
			sprintf(message, "West City");
		else if (roomnum == 13154)
			sprintf(message, "Nexus City");
		else
			sprintf(message, "South Ocean");
		break;
	case 132:
		if (roomnum < 13232)
			sprintf(message, "Frieza's Ship");
		else
			sprintf(message, "Namekian Wilderness");
		break;
	case 133:
		sprintf(message, "Elder Village");
		break;
	case 134:
		sprintf(message, "Satan City");
		break;
	case 140:
		sprintf(message, "Yardra City");
		break;
	case 141:
		sprintf(message, "Jade Forest");
		break;
	case 142:
		sprintf(message, "Jade Cliff");
		break;
	case 143:
		sprintf(message, "Mount Valaria");
		break;
	case 149:
	case 150:
		sprintf(message, "Aquis City");
		break;
	case 151:
	case 152:
	case 153:
		sprintf(message, "Kanassan Ocean");
		break;
	case 154:
		sprintf(message, "Kakureta Village");
		break;
	case 155:
		sprintf(message, "Captured Aether City");
		break;
	case 156:
		sprintf(message, "Yunkai Pirate Base");
		break;
	case 160:
	case 161:
		sprintf(message, "Janacre");
		break;
	case 165:
		sprintf(message, "Arlian Wasteland");
		break;
	case 166:
		sprintf(message, "Arlian Mine");
		break;
	case 167:
		sprintf(message, "Kilnak Caverns");
		break;
	case 168:
		sprintf(message, "Kemabra Wastes");
		break;
	case 169:
		sprintf(message, "Dark of Arlia");
		break;
	case 174:
		sprintf(message, "Fistarl Volcano");
		break;
	case 175:
	case 176:
		sprintf(message, "Cerria Colony");
		break;
	case 182:
		sprintf(message, "Below Tiranoc");
		break;
	case 196:
		sprintf(message, "Ancient Castle");
		break;
	default:
		sprintf(message, "Unknown.");
		break;
	}

	return (message);
}

void reveal_hiding(struct char_data *ch, int type)
{
 if (IS_NPC(ch) || !AFF_FLAGGED(ch, AFF_HIDE))
  return;

 int rand1 = rand_number(-5, 5), rand2 = rand_number(-5, 5), bonus = 0;

 if (AFF_FLAGGED(ch, AFF_LIQUEFIED)) {
  bonus = 10;
 }

 if (type == 0) { /* Automatic reveal. */
   act("@MYou feel as though what you just did may have revealed your hiding spot...@n", TRUE, ch, 0, 0, TO_CHAR);
   act("@M$n moves a little and you notice them!@n", TRUE, ch, 0, 0, TO_ROOM);
   REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_HIDE);
 } else if (type == 1) { /* Their skill at hiding failed, reveal */
  if (GET_SKILL(ch, SKILL_HIDE) + bonus < axion_dice(0)) {
   act("@MYou feel as though what you just did may have revealed your hiding spot...@n", TRUE, ch, 0, 0, TO_CHAR);
   act("@M$n moves a little and you notice them!@n", TRUE, ch, 0, 0, TO_ROOM);
   REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_HIDE);
  }
 } else if (type == 2) { /* They were spotted */
  struct descriptor_data *d;
  struct char_data *tch = NULL;
  for (d = descriptor_list; d; d = d->next) {
   if (STATE(d) != CON_PLAYING)
    continue;
    tch = d->character;

   if (tch == ch)
    continue;

   if (IN_ROOM(tch) != IN_ROOM(ch))
    continue;
  
   if (GET_SKILL(tch, SKILL_SPOT) + rand1 >= GET_SKILL(ch, SKILL_HIDE) + rand2) {
    REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_HIDE);
    act("@M$N seems to notice you!@n", TRUE, ch, 0, tch, TO_CHAR);
    act("@MYou notice $n's movements reveal $s hiding spot!@n", TRUE, ch, 0, tch, TO_VICT);
    act("@MYou notice $N look keenly somewhere nearby. At that spot you see $n hiding!@n", TRUE, ch, 0, tch, TO_NOTVICT);
    return;
   }
  }
 } else if (type == 3) { /* They were heard, reveal with different messages. */
  struct descriptor_data *d;
  struct char_data *tch = NULL;

  act("@MThe scouter makes some beeping sounds as you tinker with its buttons.@n", TRUE, ch, 0, 0, TO_CHAR);
  for (d = descriptor_list; d; d = d->next) {
   if (STATE(d) != CON_PLAYING)
    continue;
   tch = d->character;

   if (tch == ch)
    continue;

   if (IN_ROOM(tch) != IN_ROOM(ch))
    continue;

   if (GET_SKILL(tch, SKILL_LISTEN) > axion_dice(0)) {
    switch (type) {
     case 3:
      act("@MYou notice some beeping sounds that sound really close by.@n", TRUE, ch, 0, tch, TO_VICT);
      break;
     default:
      act("@MYou notice some sounds coming from this room but can't seem to locate the source...@n", TRUE, ch, 0, tch, TO_VICT);
      break;
    }
   }
  }
 } else if (type == 4) { /* You were found from search! */
   REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_HIDE);
 }
}

int block_calc(struct char_data *ch)
{
  struct char_data *blocker = NULL;

  if (BLOCKED(ch)) {
   blocker = BLOCKED(ch);
  } else {
   return (1);
  }

   if (GET_SPEEDI(ch) < GET_SPEEDI(blocker) && GET_POS(blocker) > POS_SITTING) {
     if (!AFF_FLAGGED(blocker, AFF_BLIND) && !PLR_FLAGGED(blocker, PLR_EYEC)) {
       int minimum = GET_CHA(blocker) + rand_number(5, 20);
       if (minimum > 100) {
        minimum = 100;
       }
       if (!GET_SKILL(ch, SKILL_ESCAPE_ARTIST) || (GET_SKILL(ch, SKILL_ESCAPE_ARTIST) && GET_SKILL(ch, SKILL_ESCAPE_ARTIST)  < rand_number(minimum, 120))) {
        act("$n tries to leave, but can't outrun $N!", TRUE, ch, 0, blocker, TO_NOTVICT);
        act("$n tries to leave, but can't outrun you!", TRUE, ch, 0, blocker, TO_VICT);
        act("You try to leave, but can't outrun $N!", TRUE, ch, 0, blocker, TO_CHAR);
        if (AFF_FLAGGED(ch, AFF_FLYING) && !AFF_FLAGGED(blocker, AFF_FLYING) && GET_ALT(ch) == 1) {
         send_to_char(blocker, "You're now floating in the air.\r\n");
         SET_BIT_AR(AFF_FLAGS(blocker), AFF_FLYING);
         GET_ALT(blocker) = GET_ALT(ch);
        } else if (AFF_FLAGGED(ch, AFF_FLYING) && !AFF_FLAGGED(blocker, AFF_FLYING) && GET_ALT(ch) == 2) {
         send_to_char(blocker, "You're now floating high in the sky.\r\n");
         SET_BIT_AR(AFF_FLAGS(blocker), AFF_FLYING);
         GET_ALT(blocker) = GET_ALT(ch);
        }
        return (0);
       } else {
        act("$n proves $s great skill and escapes from $N's attempted block!", TRUE, ch, 0, blocker, TO_NOTVICT);
        act("$n proves $s great skill and escapes from your attempted block!", TRUE, ch, 0, blocker, TO_VICT);
        act("Using your great skill you manage to escape from $N's attempted block!", TRUE, ch, 0, blocker, TO_CHAR);
        BLOCKED(ch) = NULL;
        BLOCKS(blocker) = NULL;
       }
    } else {
        act("$n proves $s great skill and escapes from $N's attempted block!", TRUE, ch, 0, blocker, TO_NOTVICT);
        act("$n proves $s great skill and escapes from your attempted block!", TRUE, ch, 0, blocker, TO_VICT);
        act("Using your great skill you manage to escape from $N's attempted block!", TRUE, ch, 0, blocker, TO_CHAR);
        BLOCKED(ch) = NULL;
        BLOCKS(blocker) = NULL;
    }
   } else if (GET_POS(blocker) <= POS_SITTING) {
    act("$n proves $s great skill and escapes from $N!", TRUE, ch, 0, blocker, TO_NOTVICT);
    act("$n proves $s great skill and escapes from you!", TRUE, ch, 0, blocker, TO_VICT);
    act("Using your great skill you manage to escape from $N!", TRUE, ch, 0, blocker, TO_CHAR);
    BLOCKED(ch) = NULL;
    BLOCKS(blocker) = NULL;
   } else if (GET_POS(blocker) > POS_SITTING) {
    act("$n proves $s great skill and escapes from $N's attempted block!", TRUE, ch, 0, blocker, TO_NOTVICT);
    act("$n proves $s great skill and escapes from your attempted block!", TRUE, ch, 0, blocker, TO_VICT);
    act("Using your great skill you manage to escape from $N's attempted block!", TRUE, ch, 0, blocker, TO_CHAR);
    BLOCKED(ch) = NULL;
    BLOCKS(blocker) = NULL;
   }

  return (1);
}

int64_t molt_threshold(struct char_data *ch)
{

 int64_t threshold = 0, max = 2000000000;

 max *= 250;

 if (!IS_ARLIAN(ch))
  return (0);
 else if (GET_MOLT_LEVEL(ch) < 100) {
  threshold = (((GET_MOLT_LEVEL(ch) + 1) * (getBasePL(ch) * 0.02)) * GET_CON(ch)) / 4;
  threshold = threshold * 0.25;
 } else if (GET_MOLT_LEVEL(ch) < 200) {
  threshold = (((GET_MOLT_LEVEL(ch) + 1) * (getBasePL(ch) * 0.02)) * GET_CON(ch)) / 2;
  threshold = threshold * 0.20;
 } else if (GET_MOLT_LEVEL(ch) < 400) {
  threshold = (((GET_MOLT_LEVEL(ch) + 1) * (getBasePL(ch) * 0.02)) * GET_CON(ch));
  threshold = threshold * 0.17;
 } else if (GET_MOLT_LEVEL(ch) < 800) {
  threshold = (((GET_MOLT_LEVEL(ch) + 1) * (getBasePL(ch) * 0.02)) * GET_CON(ch)) * 2;
  threshold = threshold * 0.15;
 } else {
  threshold = (((GET_MOLT_LEVEL(ch) + 1) * (getBasePL(ch) * 0.02)) * GET_CON(ch)) * 4;
  threshold = threshold * 0.12;
 }

 return MIN(threshold, max);
}

int armor_evolve(struct char_data *ch)
{
 int value = 0;

 if (GET_MOLT_LEVEL(ch) <= 5) {
  value = 8;
 } else if (GET_MOLT_LEVEL(ch) <= 10) {
  value = 12;
 } else if (GET_MOLT_LEVEL(ch) <= 20) {
  value = 15;
 } else if (GET_MOLT_LEVEL(ch) <= 30) {
  value = 20;
 } else if (GET_MOLT_LEVEL(ch) <= 40) {
  value = 30;
 } else if (GET_MOLT_LEVEL(ch) <= 50) {
  value = 50;
 } else if (GET_MOLT_LEVEL(ch) <= 75) {
  value = 100;
 } else if (GET_MOLT_LEVEL(ch) <= 100) {
  value = 150;
 } else if (GET_MOLT_LEVEL(ch) <= 500) {
  value = 200;
 } else {
  value = 220;
 }

 return (value);
}

/* This handles arlian exoskeleton molting */
void handle_evolution(struct char_data *ch, int64_t dmg)
{

 /* Reject NPCs and non-arlians */
 if (IS_NPC(ch) || !IS_ARLIAN(ch)) {
  return;
 }
 
 int64_t moltgain = 0;

 moltgain = dmg * 0.5;
 if (GET_LEVEL(ch) == 100)
  moltgain += 100000;
 else if (GET_LEVEL(ch) >= 90)
  moltgain += GET_LEVEL(ch) * 1000;
 else if (GET_LEVEL(ch) >= 75)
  moltgain += GET_LEVEL(ch) * 500;
 else if (GET_LEVEL(ch) >= 50)
  moltgain += GET_LEVEL(ch) * 250;
 else if (GET_LEVEL(ch) >= 10)
  moltgain += GET_LEVEL(ch) * 50;

 GET_MOLT_EXP(ch) += moltgain;
 
 if (AFF_FLAGGED(ch, AFF_SPIRIT)) {
  send_to_char(ch, "You are dead and all evolution experience is reduced. Gains are divided by 100 or reduced to a minimum of 1.\r\n");
  moltgain /= 100;
 }

 if (GET_MOLT_EXP(ch) > molt_threshold(ch)) {
  if (GET_MOLT_LEVEL(ch) <= GET_LEVEL(ch) * 2 || GET_LEVEL(ch) >= 100) {
   GET_MOLT_EXP(ch) = 0;
   GET_MOLT_LEVEL(ch) += 1;
   double rand1 = 0.02;
   double rand2 = 0.03;
   if (rand_number(1, 4) == 3) {
    rand1 += 0.02;
    rand2 += 0.02;
   } else if (rand_number(1, 4) >= 3) {
    rand1 += 0.01;
    rand2 += 0.01;
   }
   int armorgain = 0;
   int64_t plgain = getBasePL(ch) * rand1, stamgain = getBaseST(ch) * rand2;
   armorgain = armor_evolve(ch);
   gainBasePL(ch, plgain);
   gainBaseST(ch, stamgain);
   GET_ARMOR(ch) += armorgain;
   if (GET_ARMOR(ch) > 50000) {
    GET_ARMOR(ch) = 50000;
   }
   act("@gYour @De@Wx@wo@Ds@Wk@we@Dl@We@wt@Do@Wn@g begins to crack. You quickly shed it and reveal a stronger version that was growing beneath it! At the same time you feel your adrenal sacs to be more efficient@n", TRUE, ch, 0, 0, TO_CHAR);
   act("@G$n's@g @De@Wx@wo@Ds@Wk@we@Dl@We@wt@Do@Wn@g begins to crack. Suddenly $e sheds the damaged @De@Wx@wo@Ds@Wk@we@Dl@We@wt@Do@Wn and reveals a stronger version that had been growing underneath!@n", TRUE, ch, 0, 0, TO_ROOM);
   send_to_char(ch, "@D[@RPL@W: @G+%s@D] [@gStamina@W: @G+%s@D] [@wArmor Index@W: @G+%s@D]@n\r\n", add_commas(plgain), add_commas(stamgain), GET_ARMOR(ch) >= 50000 ? "50k CAP" : add_commas(armorgain));
  } else {
   send_to_char(ch, "@gYou are unable to evolve while your evolution level is higher than twice your character level.@n\r\n");
  }
 }

}

void demon_refill_lf(struct char_data *ch, int64_t num)
{
 struct char_data *tch = NULL;

  for (tch = world[IN_ROOM(ch)].people; tch; tch = tch->next_in_room) {
   if (!IS_DEMON(tch))
    continue;
   if ((getCurLF(tch)) >= (getMaxLF(tch)))
    continue;
   else {
       incCurLF(ch, num);
    act("@CYou feel the life energy from @c$N@C's cursed body flow out and you draw it into yourself!@n", TRUE, tch, 0, ch, TO_CHAR);
   }
  }
}


void mob_talk(struct char_data *ch, const char *speech)
{

 struct char_data *tch = NULL, *vict = NULL;
 int stop = 1;

  if (IS_NPC(ch)) {
   return;
  }

  for (tch = world[IN_ROOM(ch)].people; tch; tch = tch->next_in_room) {
   if (!IS_NPC(tch))
    continue;
   if (!IS_HUMANOID(tch))
    continue;
   if (stop == 0)
    continue;
   else {
    vict = tch;
    stop = mob_respond(ch, vict, speech);
    if (rand_number(1, 2) == 2) {
     stop = 0;
    }
   }
  } /* End for loop */
} /* End Mob Talk */

int mob_respond(struct char_data *ch, struct char_data *vict, const char *speech)
{
    if (ch != NULL && vict != NULL) {
     if (!IS_NPC(ch) && IS_NPC(vict)) {
       if ((strstr(speech, "hello") || strstr(speech, "greet") || strstr(speech, "Hello") || strstr(speech, "Greet")) && !FIGHTING(vict)) {
        send_to_room(IN_ROOM(vict), "\r\n");
        if (IS_HUMAN(vict) || IS_HALFBREED(vict)) {
         switch (rand_number(1, 4)) {
           case 1:
            act("@w$n@W says, '@CYes, hello to you as well $N.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 2:
            act("@w$n@W says, '@CHello!@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 3:
            act("@w$n@W says, '@CHi, $N, how are you doing?@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 4:
            act("@w$n@W says, '@CGreetings, $N. What are you up to?@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
         }
        } /* End Human Section */
        else if (IS_SAIYAN(vict)) {
         switch (rand_number(1, 4)) {
           case 1:
            act("@w$n@W says, '@CHmph, hi.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 2:
            act("@w$n@W says, '@CHello weakling.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 3:
            act("@w$n@W says, '@C$N do all weaklings like you waste time in idle talk?@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 4:
            act("@w$n@W says, '@C$N, you are not welcome around me.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
         }
        } /* End Saiyan Section */
        else if (IS_ICER(vict)) {
         switch (rand_number(1, 4)) {
           case 1:
            act("@w$n@W says, '@CHa ha... Yes, hello.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 2:
            act("@w$n@W says, '@CAh a polite greeting. It's good to know your kind isn't totally worthless.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 3:
            act("@w$n@W says, '@C$N, hello. Now leave me be.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 4:
            act("@w$n@W says, '@C$N, you are below me. Now begone.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
         }
        } /* End Icer Section */
        else if (IS_KONATSU(vict)) {
         switch (rand_number(1, 4)) {
           case 1:
            act("@w$n@W says, '@CGreetings, $N, may your travels be well.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 2:
            act("@w$n@W says, '@CHello.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 3:
            act("@w$n@W says, '@C$N, hello.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 4:
            act("@w$n@W says, '@C$N, it is nice to meet you.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
         }
        } /* End Konatsu Section */
        else if (IS_NAMEK(vict)) {
         switch (rand_number(1, 4)) {
           case 1:
            act("@w$n@W says, '@CHello.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 2:
            act("@w$n@W says, '@CA peaceful greeting to you, $N.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 3:
            act("@w$n@W says, '@C$N, hello. What is your business here?@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 4:
            act("@w$n@W says, '@C$N, greetings.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
         }
        } /* End Namek Section */
        else if (IS_ARLIAN(vict)) {
         switch (rand_number(1, 4)) {
           case 1:
            act("@w$n@W says, '@CPeace, stranger.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 2:
            act("@w$n@W says, '@CStay out of my way.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 3:
            act("@w$n@W says, '@C$N, what is your business here?@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 4:
            act("@w$n@W says, '@C...Hello.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
         }
        } /* End Arlian Section */
        else if (IS_ANDROID(vict)) {
         act("@w$n@W says, '@C...@W'@n", TRUE, vict, 0, ch, TO_ROOM);
        } /* End Android Section */
        else if (IS_MAJIN(vict)) {
         switch (rand_number(1, 2)) {
           case 1:
            act("@w$n@W says, '@CHa ha...@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
           case 2:
            act("@w$n@W says, '@CHello. What candy you want to be?@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            break;
         }
        } /* End MAJIN Section */
        else if (IS_TRUFFLE(vict)) {
         switch (rand_number(1, 3)) {
           case 1:
            if (IS_SAIYAN(ch)) {
             act("@w$n@W says, '@CEwww, dirty monkey...@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            } else {
             act("@w$n@W says, '@CHello.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            }
            break;
           case 2:
            if (IS_SAIYAN(ch)) {
             act("@w$n@W says, '@CEwww, dirty monkey...@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            } else {
            act("@w$n@W says, '@C$N, hello. You are a curious individual.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            }
            break;
           case 3:
            if (IS_SAIYAN(ch)) {
             act("@w$n@W says, '@CEwww, dirty monkey...@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            } else {
            act("@w$n@W says, '@C$N, hello. What's your IQ?@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            }
            break;
         }
        } /* End Truffle Section */
        else {
         act("Hmph, yeah hi.", TRUE, vict, 0, ch, TO_ROOM);
        }
       } /* End Hello Section */

       if ((strstr(speech, "spar") || strstr(speech, "Spar")) && !FIGHTING(vict)) {
        send_to_room(IN_ROOM(vict), "\r\n");

        if(vict->original == ch) {
            act("@w$n@W says, '@C$N, sure. I'll spar with you.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            SET_BIT_AR(MOB_FLAGS(vict), MOB_SPAR);
            return 0;
        }

        if (GET_LEVEL(vict) > 4 && GET_ALIGNMENT(vict) >= 0) {
         memory_rec *names;
         int remember = FALSE;

          for (names = MEMORY(vict); names && !remember; names = names->next) {
            if (names->id != GET_IDNUM(ch))
             continue;

             remember = TRUE;
          }
          
         if (remember == TRUE) {
          act("@w$n@W says, '@C$N you will die by my hand!@W'@n", TRUE, vict, 0, ch, TO_ROOM);
          return 1;
         } else if (MOB_FLAGGED(vict, MOB_NOKILL)) {
          act("@w$n@W says, '@C$N, I have no need to spar with you.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
          return 1;
         } else if (MOB_FLAGGED(vict, MOB_AGGRESSIVE)) {
          act("@w$n@W says, '@C$N, I will kill you instead.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
          return 1;
         } else if (MOB_FLAGGED(vict, MOB_DUMMY)) {
          act("@w$n@W says, '@C...@W'@n", TRUE, vict, 0, ch, TO_ROOM);
          return 1;
         } else if (GET_MAX_HIT(ch) > GET_MAX_HIT(vict) * 2) {
          act("@w$n@W says, '@C$N, no way will I spar. I already know I would lose badly.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
          return 1;
         } else if (GET_MAX_HIT(vict) > GET_MAX_HIT(ch) * 2) {
          act("@w$n@W says, '@C$N, you wouldn't last very long.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
          return 1;
         } else if (GET_HIT(vict) < GET_MAX_HIT(vict) * .8) {
          act("@w$n@W says, '@C$N, I need to recover first.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
          return 1;
         } else if (rand_number(1, 50) >= 40 && !MOB_FLAGGED(vict, MOB_SPAR)) {
          act("@w$n@W says, '@C$N, maybe in a bit.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
          return 1;
         } else {
          if (MOB_FLAGGED(vict, MOB_SPAR)) {
           act("@w$n@W says, '@C$N, fine our match will wait till later then.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
           REMOVE_BIT_AR(MOB_FLAGS(vict), MOB_SPAR);
          } else {
           act("@w$n@W says, '@C$N, sure. I'll spar with you.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
           SET_BIT_AR(MOB_FLAGS(vict), MOB_SPAR);
          }
          return 0;
         }
        }
        else if (GET_LEVEL(vict) > 4 && GET_ALIGNMENT(vict) < 0) {
          act("@w$n@W says, '@CSpar? I don't play games, I play for blood...@W'@n", TRUE, vict, 0, ch, TO_ROOM);
          return 1;
        }
        else {
          act("@w$n@W says, '@CSpar? I prefer not to thank you...@W'@n", TRUE, vict, 0, ch, TO_ROOM);
          return 1;
        }
       } /* End challenge section */
       if (strstr(speech, "goodbye") || strstr(speech, "Goodbye") || strstr(speech, "bye") || strstr(speech, "Bye")) {
        send_to_room(IN_ROOM(vict), "\r\n");
          if (GET_ALIGNMENT(vict) >= 0) {
           if (GET_SEX(vict) == SEX_MALE) {
            if (GET_SEX(ch) == SEX_FEMALE) {
             act("@w$n@W says, '@C$N, goodbye babe.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            }
            else {
             act("@w$n@W says, '@C$N, goodbye.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            }
           }
           else if (GET_SEX(vict) == SEX_FEMALE) {
            if (GET_SEX(ch) == SEX_MALE) {
             act("@w$n@W says, '@C$N, goodbye...@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            }
            else {
             act("@w$n@W says, '@C$N, bye.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            }
           }
           else {
             act("@w$n@W says, '@C$N, goodbye.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
           }
          }
          if (GET_ALIGNMENT(vict) < 0) {
           if (GET_SEX(vict) == SEX_MALE) {
            if (GET_SEX(ch) == SEX_FEMALE) {
             act("@w$n@W says, '@CGoodbye. Eh heh heh.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            }
            else {
             act("@w$n@W says, '@CSo long and good ridance.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            }
           }
           else if (GET_SEX(vict) == SEX_FEMALE) {
            if (GET_SEX(ch) == SEX_MALE) {
             act("@w$n@W says, '@CGoodbye then...@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            }
            else {
             act("@w$n@W says, '@C$N, no one wanted you around anyway.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
            }
           }
           else {
             act("@w$n@W says, '@CFine get lost.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
           }
         }
       } /* End goodbye If */
       if (strstr(speech, "train") || strstr(speech, "Train") || strstr(speech, "exercise") || strstr(speech, "Exercise")) {
        send_to_room(IN_ROOM(vict), "\r\n");
        if (GET_ALIGNMENT(vict) >= 0 && !MOB_FLAGGED(vict, MOB_NOKILL)) {
         if (GET_LEVEL(vict) > 4 && GET_LEVEL(vict) < 10) {
          act("@w$n@W says, '@CTraining is good for the body. I think I may need to go workout myself.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
         }
         if (GET_LEVEL(vict) >= 10 && GET_LEVEL(vict) < 30) {
          act("@w$n@W says, '@CI think I might need a little more training...@W'@n", TRUE, vict, 0, ch, TO_ROOM);
         }
         if (GET_LEVEL(vict) >= 30 && GET_LEVEL(vict) < 60) {
          act("@w$n@W says, '@CI'm pretty tough already. Though I should probably work on my skills.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
         }
         if (GET_LEVEL(vict) >= 60) {
          act("@w$n@W says, '@CI'm on top of my game.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
         }
         if (GET_LEVEL(vict) < 5) {
          act("@w$n@W says, '@CI really need to bust ass and train.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
         }
        }
        if (GET_ALIGNMENT(vict) < 0 && !MOB_FLAGGED(vict, MOB_NOKILL)) {
         if (GET_LEVEL(vict) > 4 && GET_LEVEL(vict) < 10) {
          act("@w$n@W says, '@CWell maybe I could use some more training.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
         }
         if (GET_LEVEL(vict) >= 10 && GET_LEVEL(vict) < 30) {
          act("@w$n@W says, '@CTrain? Yeah it has become harder to take what I want....@W'@n", TRUE, vict, 0, ch, TO_ROOM);
         }
         if (GET_LEVEL(vict) >= 30 && GET_LEVEL(vict) < 60) {
          act("@w$n@W says, '@CTrain? I don't need to train to take you!@W'@n", TRUE, vict, 0, ch, TO_ROOM);
         }
         if (GET_LEVEL(vict) >= 60) {
          act("@w$n@W says, '@CTraining won't save you when I tire of your continued life.@W'@n", TRUE, vict, 0, ch, TO_ROOM);
         }
         if (GET_LEVEL(vict) < 5) {
          act("@w$n@W says, '@CYes. I need to train so I can reach the top. Then everyone will have to listen to me!@W'@n", TRUE, vict, 0, ch, TO_ROOM);
         }
        }
       }
       return 1;
      }
    } /* End Valid targets Loop. */
    return 1;
}

bool is_sparring(struct char_data *ch)
{

 if (IS_NPC(ch) && MOB_FLAGGED(ch, MOB_SPAR)) {
  return TRUE;
 }
 if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_SPAR)) {
  return TRUE;
 }
 return FALSE;
}

char *introd_calc(struct char_data *ch)
{
  char *sex, *race;
  static char intro[100];

  *intro = '\0';

  if (IS_NPC(ch)) { /* How the hell... */
   return ("IAMERROR");
  }

  if (IS_HALFBREED(ch)) {
   if (RACIAL_PREF(ch) == 1) {
    race = strdup("human");
   } else if (RACIAL_PREF(ch) == 2) {
    race = strdup("saiyan");
   } else {
    race = strdup(RACE(ch));
   }
    sex = strdup(MAFE(ch));
  } else if (IS_ANDROID(ch)) {
   if (RACIAL_PREF(ch) == 1) {
    race = strdup("android");
   } else if (RACIAL_PREF(ch) == 2) {
    race = strdup("human");
   } else if (RACIAL_PREF(ch) == 3) {
    race = strdup("robotic-humanoid");
   } else {
    race = strdup(RACE(ch));
   }
    sex = strdup(MAFE(ch));
  } else {
   sex = strdup(MAFE(ch));
   race = strdup(RACE(ch));
  }
  sprintf(intro, "%s %s %s", AN(sex), sex, race);
  if (sex) {
   free(sex);
  }
  if (race) {
   free(race);
  }

  return (intro);
}


double speednar(struct char_data *ch)
{

 double result = 0;
 int64_t carried = getCurCarriedWeight(ch);
 int64_t can_carry = CAN_CARRY_W(ch);

 if (carried >= can_carry) {
  result = 1.0;
 } else if (carried >= can_carry * 0.95) {
  result = 0.95;
 } else if (carried >= can_carry * 0.9) {
  result = 0.90;
 } else if (carried >= can_carry * 0.85) {
  result = 0.85;
 } else if (carried >= can_carry * 0.80) {
  result = 0.80;
 } else if (carried >= can_carry * 0.75) {
  result = 0.75;
 } else if (carried >= can_carry * 0.70) {
  result = 0.70;
 } else if (carried >= can_carry * 0.65) {
  result = 0.65;
 } else if (carried >= can_carry * 0.60) {
  result = 0.60;
 } else if (carried >= can_carry * 0.55) {
  result = 0.55;
 } else if (carried >= can_carry * 0.50) {
  result = 0.50;
 } else if (carried >= can_carry * 0.45) {
  result = 0.45;
 } else if (carried >= can_carry * 0.40) {
  result = 0.40;
 } else if (carried >= can_carry * 0.35) {
  result = 0.35;
 } else if (carried >= can_carry * 0.30) {
  result = 0.30;
 } else if (carried >= can_carry * 0.25) {
  result = 0.25;
 } else if (carried >= can_carry * 0.20) {
  result = 0.20;
 } else if (carried >= can_carry * 0.15) {
  result = 0.15;
 } else if (carried >= can_carry * 0.10) {
  result = 0.10;
 } else if (carried >= can_carry * 0.05) {
  result = 0.05;
 } else if (carried >= can_carry * 0.01) {
  result = 0.01;
 }

 return (result);

}

int64_t gear_exp(struct char_data *ch, int64_t exp)
{

 if (IS_NPC(ch)) {
  return 0;
 }

 double ratio = 2 * (1-speednar(ch));
 ratio += 1.0;
 return exp * ratio;
}

int planet_check(struct char_data *ch, struct char_data *vict)
{

 if (ch == NULL) {
  log("ERROR: planet_check called without ch!");
  return 0;
 } else if (vict == NULL) {
  log("ERROR: planet_check called without vict!");
  return 0;
 } else {
  int success = 0;
  if (GET_ADMLEVEL(vict) <= 0) {
   if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_EARTH) && ROOM_FLAGGED(IN_ROOM(vict), ROOM_EARTH)) {
    success = 1;
   } else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_FRIGID) && ROOM_FLAGGED(IN_ROOM(vict), ROOM_FRIGID)) {
    success = 1;
   } else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_NAMEK) && ROOM_FLAGGED(IN_ROOM(vict), ROOM_NAMEK)) {
    success = 1;
   } else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_VEGETA) && ROOM_FLAGGED(IN_ROOM(vict), ROOM_VEGETA)) {
    success = 1;
   } else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_AETHER) && ROOM_FLAGGED(IN_ROOM(vict), ROOM_AETHER)) {
    success = 1;
   } else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_KONACK) && ROOM_FLAGGED(IN_ROOM(vict), ROOM_KONACK)) {
    success = 1;
   } else if (PLANET_ZENITH(IN_ROOM(ch)) && PLANET_ZENITH(IN_ROOM(vict))) {
    success = 1;
   } else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_KANASSA) && ROOM_FLAGGED(IN_ROOM(vict), ROOM_KANASSA)) {
    success = 1;
   } else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_YARDRAT) && ROOM_FLAGGED(IN_ROOM(vict), ROOM_YARDRAT)) {
    success = 1;
   } else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_AL) && ROOM_FLAGGED(IN_ROOM(vict), ROOM_AL)) {
    success = 1;
   } else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_HELL) && ROOM_FLAGGED(IN_ROOM(vict), ROOM_HELL)) {
    success = 1;
   } else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_ARLIA) && ROOM_FLAGGED(IN_ROOM(vict), ROOM_ARLIA)) {
    success = 1;
   } else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_NEO) && ROOM_FLAGGED(IN_ROOM(vict), ROOM_NEO)) {
    success = 1;
   } else if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_CERRIA) && ROOM_FLAGGED(IN_ROOM(vict), ROOM_CERRIA)) {
    success = 1;
   }
  }
  return (success);
 }
}

void purge_homing(struct char_data *ch)
{

 struct obj_data *obj = NULL, *next_obj = NULL;
 for (obj = world[IN_ROOM(ch)].contents; obj; obj = next_obj) {
  next_obj = obj->next_content;
  if (GET_OBJ_VNUM(obj) == 80 || GET_OBJ_VNUM(obj) == 81) {
   if (TARGET(obj) == ch || USER(obj) == ch) {
    act("$p @wloses its target and flies off into the distance.@n", TRUE, 0, obj, 0, TO_ROOM);
    extract_obj(obj);
    continue;
   }
  }
 }

}

void improve_skill(struct char_data *ch, int skill, int num)
{
  int percent = GET_SKILL(ch, skill);
  int newpercent, roll = 1200;
  char skillbuf[MAX_STRING_LENGTH];

  if (IS_NPC(ch))
   return;

  if (!num) {
   num = 2;
  }

 if (AFF_FLAGGED(ch, AFF_SHOCKED))
  return;

 if (GET_FORGETING(ch) == skill)
  return;

 if (GET_SKILL_BASE(ch, skill) >= 90) {
  roll += 800;
 } else if (GET_SKILL_BASE(ch, skill) >= 75) {
  roll += 600;
 } else if (GET_SKILL_BASE(ch, skill) >= 50) {
  roll += 400;
 }

 if (skill == SKILL_PARRY || skill == SKILL_DODGE || skill == SKILL_BARRIER || skill == SKILL_BLOCK || skill == SKILL_ZANZOKEN || skill == SKILL_TSKIN) {
  if (GET_BONUS(ch, BONUS_MASOCHISTIC) > 0) {
   return;
  }
 }

 if (!SPAR_TRAIN(ch)) {
  if (num == 0) {
   roll -= 400;
  } else if (num == 1) {
   roll -= 200;
  }
 } else {
  if (num == 0) {
   roll -= 500;
  } else if (num == 1) {
   roll -= 400;
  } else {
   roll -= 200;
  }
 }
 
 if (FIGHTING(ch) != NULL && IS_NPC(FIGHTING(ch)) && MOB_FLAGGED(FIGHTING(ch), MOB_DUMMY)) {
  roll -= 100;
 }

 if (IS_TRUFFLE(ch) || (IS_BIO(ch) && (GET_GENOME(ch, 0) == 6 || GET_GENOME(ch, 1) == 6))) {
  roll *= 0.5;
 } else if (IS_MAJIN(ch)) {
  roll += roll * .3;
 } else if (IS_BIO(ch) && skill == SKILL_ABSORB) {
  roll -= roll * .15;
 } else if (IS_HOSHIJIN(ch) && (skill == SKILL_PUNCH || skill == SKILL_KICK || skill == SKILL_KNEE || skill == SKILL_ELBOW || skill == SKILL_UPPERCUT || skill == SKILL_ROUNDHOUSE || skill == SKILL_SLAM || skill == SKILL_HEELDROP || skill == SKILL_DAGGER || skill == SKILL_SWORD || skill == SKILL_CLUB || skill == SKILL_GUN || skill == SKILL_SPEAR || skill == SKILL_BRAWL)) {
  roll = roll * 0.30;
 }

 if (FIGHTING(ch) != NULL && IS_NPC(FIGHTING(ch)) && MOB_FLAGGED(FIGHTING(ch), MOB_DUMMY)) {
  roll -= 100;
 }
 
 if (GET_BONUS(ch, BONUS_QUICK_STUDY) > 0) {
  roll -= roll * .25;
 } else if (GET_BONUS(ch, BONUS_SLOW_LEARNER) > 0) {
  roll += roll * .25;
 }

 if (GET_ASB(ch) > 0) {
  roll -= (roll * 0.01) * GET_ASB(ch);
 }

 roll = MAX(roll, 300);

  if (rand_number(1, roll) > ((GET_INT(ch) * 2) + GET_WIS(ch))) {
     return;
  }
  if ((percent > 99 || percent <= 0)) {
     return;
  }
  if (IS_MAJIN(ch) && GET_SKILL(ch, skill) >= 75) {
   switch (skill) {
    case 407:
    case 408:
    case 409:
    case 425:
    case 431:
    case 449:
    case 450:
    case 451:
    case 452:
    case 453:
    case 454:
    case 455:
    case 456:
    case 465:
    case 466:
    case 467:
    case 468:
    case 469:
    case 470:
    case 499:
    case 501:
    case 530:
    case 531:
    case 538:
     /* Do nothing. */
     break;
    default:
     return;
   }
  } else if (IS_MAJIN(ch) && skill == 425) {
    roll += 250;
  }

  /*
  if ((IS_JINTO(ch) || IS_TSUNA(ch) || IS_DABURA(ch) || IS_TAPION(ch) && GET_SKILL(ch, SKILL_SENSE) >= 75) && skill == SKILL_SENSE) {
    return;
  }
  
  if ((IS_BARDOCK(ch) || IS_KURZAK(ch) || IS_FRIEZA(ch) || IS_GINYU(ch) || IS_ANDSIX(ch) && GET_SKILL(ch, SKILL_SENSE) >= 50) && skill == SKILL_SENSE) {
    return;
  }
   */

  newpercent = 1;
  percent += newpercent;
  SET_SKILL(ch, skill, percent);
  if (newpercent >= 1) {
     sprintf(skillbuf, "@WYou feel you have learned something new about @G%s@W.@n\r\n", spell_info[skill].name);
     send_to_char(ch, skillbuf);
     if (GET_SKILL_BASE(ch, skill) >= 100) {
      send_to_char(ch, "You learned a lot by mastering that skill.\r\n");
      if (perf_skill(skill)) {
       send_to_char(ch, "You can now choose a perfection for this skill (help perfection).\r\n");
      }
      if (IS_KONATSU(ch) && skill == SKILL_PARRY) {
       SET_SKILL(ch, skill, GET_SKILL_BASE(ch, skill) + 5);
      }
      if (GET_LEVEL(ch) < 100) {
       GET_EXP(ch) += level_exp(ch, GET_LEVEL(ch) + 1) / 20;
      } else {
       gain_exp(ch, 5000000);
      }
     }
  }
}


/* Check if making CH follow VICTIM will create an illegal */
/* Follow "Loop/circle"                                    */
bool circle_follow(struct char_data *ch, struct char_data *victim)
{
  struct char_data *k;

  for (k = victim; k; k = k->master) {
    if (k == ch)
      return (TRUE);
  }

  return (FALSE);
}



/* Called when stop following persons, or stopping charm */
/* This will NOT do if a character quits/dies!!          */
void stop_follower(struct char_data *ch)
{
  struct follow_type *j, *k;

  if (ch->master == NULL) {
    core_dump();
    return;
  }

    act("You stop following $N.", FALSE, ch, 0, ch->master, TO_CHAR);
    act("$n stops following $N.", TRUE, ch, 0, ch->master, TO_NOTVICT);
  if (!(DEAD(ch->master) || (ch->master->desc && STATE(ch->master->desc) == CON_MENU)))
    act("$n stops following you.", TRUE, ch, 0, ch->master, TO_VICT);
 
  if (has_group(ch))
   GET_GROUPKILLS(ch) = 0;

  if (ch->master->followers->follower == ch) {  /* Head of follower-list? */
    k = ch->master->followers;
    ch->master->followers = k->next;
    free(k);
  } else {                      /* locate follower who is not head of list */
    for (k = ch->master->followers; k->next->follower != ch; k = k->next);

    j = k->next;
    k->next = j->next;
    free(j);
  }

  ch->master = NULL;
}

int num_followers_charmed(struct char_data *ch)
{
  struct follow_type *lackey;
  int total = 0;

  /* Summoned creatures don't count against total */
  for (lackey = ch->followers; lackey; lackey = lackey->next)
    if (AFF_FLAGGED(lackey->follower, AFF_CHARM) &&
        !AFF_FLAGGED(lackey->follower, AFF_SUMMONED) &&
        lackey->follower->master == ch)
      total++;

  return (total);
}

void switch_leader(struct char_data *old, struct char_data *new_leader)
{ 
struct follow_type *f; 
struct char_data *tch = NULL;

  for (f = old->followers; f; f = f->next) { 
    if (f->follower == new_leader) {
      tch = new_leader;
      stop_follower(tch);
    }
    if (f->follower != new_leader)
    { 
      tch = f->follower;
      stop_follower(tch);
      add_follower(tch, new_leader);
    } 
  } 
}
/* Called when a character that follows/is followed dies */
void die_follower(struct char_data *ch)
{
  struct follow_type *j, *k;

  if (ch->master)
    stop_follower(ch);

  for (k = ch->followers; k; k = j) { 
   j = k->next; 
   stop_follower(k->follower); 
  } 
}



/* Do NOT call this before having checked if a circle of followers */
/* will arise. CH will follow leader                               */
void add_follower(struct char_data *ch, struct char_data *leader)
{
  struct follow_type *k;

  if (ch->master) {
    core_dump();
    return;
  }

  ch->master = leader;

  CREATE(k, struct follow_type, 1);

  k->follower = ch;
  k->next = leader->followers;
  leader->followers = k;

  act("You now follow $N.", FALSE, ch, 0, leader, TO_CHAR);
  if (IN_ROOM(ch) != NOWHERE && IN_ROOM(leader) != NOWHERE && CAN_SEE(leader, ch)) {
    act("$n starts following you.", TRUE, ch, 0, leader, TO_VICT);  
  act("\r\n$n starts to follow $N.", TRUE, ch, 0, leader, TO_NOTVICT);
  }
}

int count_metamagic_feats(struct char_data *ch)
{
  int count = 0;                /* Number of Metamagic Feats Known */

  if (HAS_FEAT(ch, FEAT_STILL_SPELL))
    count++;

  if (HAS_FEAT(ch, FEAT_SILENT_SPELL))
    count++;

  if (HAS_FEAT(ch, FEAT_QUICKEN_SPELL))
    count++;

  if (HAS_FEAT(ch, FEAT_MAXIMIZE_SPELL))
    count++;

  if (HAS_FEAT(ch, FEAT_HEIGHTEN_SPELL))
    count++;

  if (HAS_FEAT(ch, FEAT_EXTEND_SPELL))
    count++;

  if (HAS_FEAT(ch, FEAT_EMPOWER_SPELL))
    count++;

  return count;
}

int default_admin_flags_mortal[] =
  { -1 };

int default_admin_flags_immortal[] =
  { ADM_SEEINV, ADM_SEESECRET, ADM_FULLWHERE, ADM_NOPOISON, ADM_WALKANYWHERE,
    ADM_NODAMAGE, ADM_NOSTEAL, -1 };

int default_admin_flags_builder[] =
  { -1 };

int default_admin_flags_god[] =
  { ADM_ALLSHOPS, ADM_TELLALL, ADM_KNOWWEATHER, ADM_MONEY, ADM_EATANYTHING,
    ADM_NOKEYS, -1 };

int default_admin_flags_grgod[] =
  { ADM_TRANSALL, ADM_FORCEMASS, ADM_ALLHOUSES, -1 };

int default_admin_flags_impl[] =
  { ADM_SWITCHMORTAL, ADM_INSTANTKILL, ADM_CEDIT, -1 };

int *default_admin_flags[ADMLVL_IMPL + 1] = {
  default_admin_flags_mortal,
  default_admin_flags_immortal,
  default_admin_flags_builder,
  default_admin_flags_god,
  default_admin_flags_grgod,
  default_admin_flags_impl
};

void admin_set(struct char_data *ch, int value)
{
  void run_autowiz(void);
  int i;
  int orig = GET_ADMLEVEL(ch);

  if (GET_ADMLEVEL(ch) == value)
    return;
  if (GET_ADMLEVEL(ch) < value) { /* Promotion */
    mudlog(BRF, MAX(ADMLVL_IMMORT, GET_INVIS_LEV(ch)), TRUE,
           "%s promoted from %s to %s", GET_NAME(ch), admin_level_names[GET_ADMLEVEL(ch)],
           admin_level_names[value]);
    while (GET_ADMLEVEL(ch) < value) {
      GET_ADMLEVEL(ch)++;
      for (i = 0; default_admin_flags[GET_ADMLEVEL(ch)][i] != -1; i++)
        SET_BIT_AR(ADM_FLAGS(ch), default_admin_flags[GET_ADMLEVEL(ch)][i]);
    }
    run_autowiz();
    if (orig < ADMLVL_IMMORT && value >= ADMLVL_IMMORT) {
      SET_BIT_AR(PRF_FLAGS(ch), PRF_LOG2);
      SET_BIT_AR(PRF_FLAGS(ch), PRF_HOLYLIGHT);
      SET_BIT_AR(PRF_FLAGS(ch), PRF_ROOMFLAGS);
      SET_BIT_AR(PRF_FLAGS(ch), PRF_AUTOEXIT);
    }
    if (GET_ADMLEVEL(ch) >= ADMLVL_IMMORT) {
      for (i = 0; i < 3; i++)
        GET_COND(ch, i) = (char) -1;
      SET_BIT_AR(PRF_FLAGS(ch), PRF_HOLYLIGHT);
    }
    return;
  }
  if (GET_ADMLEVEL(ch) > value) { /* Demotion */
    mudlog(BRF, MAX(ADMLVL_IMMORT, GET_INVIS_LEV(ch)), TRUE,
           "%s demoted from %s to %s", GET_NAME(ch), admin_level_names[GET_ADMLEVEL(ch)],
           admin_level_names[value]);
    while (GET_ADMLEVEL(ch) > value) {
      for (i = 0; default_admin_flags[GET_ADMLEVEL(ch)][i] != -1; i++)
        REMOVE_BIT_AR(ADM_FLAGS(ch), default_admin_flags[GET_ADMLEVEL(ch)][i]);
      GET_ADMLEVEL(ch)--;
    }
    run_autowiz();
    if (orig >= ADMLVL_IMMORT && value < ADMLVL_IMMORT) {
      REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_LOG1);
      REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_LOG2);
      REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_NOHASSLE);
      REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_HOLYLIGHT);
      REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_ROOMFLAGS);
    }
    return;
  }
}

struct time_info_data *age(struct char_data *ch)
{
  static struct time_info_data player_age;

  player_age = *mud_time_passed(time(0), ch->time.birth);

  return (&player_age);
}

/* Used to roll starting PL/KI/ST in character creation */
int roll_stats(struct char_data *ch, int type, int bonus)
{

  int pool = 0, base_num = bonus, max_num = bonus;
  int powerlevel = 0, ki = 1, stamina = 2;

  if (type == powerlevel) {
   base_num = ch->real_abils.str * 3;
   max_num = ch->real_abils.str * 5;
  } else if (type == ki) {
   base_num = ch->real_abils.intel * 3;
   max_num = ch->real_abils.intel * 5;
  } else if (type == stamina) {
   base_num = ch->real_abils.con * 3;
   max_num = ch->real_abils.con * 5;
  }

 pool = rand_number(base_num, max_num) + bonus;
 
 return (pool);
}




/* For Getting An Intro Name */
const char *get_i_name(struct char_data *ch, struct char_data *vict) {
  char fname[40], filler[50], scrap[100], line[256];
  static char name[50];
  int known = FALSE;
  FILE *fl;

  /* Read Introduction File */
  if (vict == NULL) {
    return ("");
  }

  if (IS_NPC(ch) || IS_NPC(vict)) {
   return (RACE(vict));
  }

  if (vict == ch) {
    return ("");
  }

  if (!get_filename(fname, sizeof(fname), INTRO_FILE, GET_NAME(ch))) {
    return (RACE(vict));
  }
  else if (!(fl = fopen(fname, "r"))) {
    return (RACE(vict));
  }

  while (!feof(fl)) {
    get_line(fl, line);
    sscanf(line, "%s %s\n", filler, scrap);
    if (!strcasecmp(GET_NAME(vict), filler)) {
     sprintf(name, "%s", scrap);
     known = TRUE;
    }
  }
  fclose(fl);

   if (known == TRUE)
    return (name);
   else
    return (RACE(vict));
}


int can_grav(struct char_data *ch)
{
   /* Gravity Related */
   if (ROOM_GRAVITY(IN_ROOM(ch)) == 10 && GET_MAX_HIT(ch) < 5000 && !IS_BARDOCK(ch) && !IS_NPC(ch)) {
    send_to_char(ch, "You are hardly able to move in this gravity!\r\n");
    return 0;
   }
   else if (ROOM_GRAVITY(IN_ROOM(ch)) == 20 && GET_MAX_HIT(ch) < 20000) {
    send_to_char(ch, "You are hardly able to move in this gravity!\r\n");
    return 0;
   }   
   else if (ROOM_GRAVITY(IN_ROOM(ch)) == 30 && GET_MAX_HIT(ch) < 50000) {
    send_to_char(ch, "You are hardly able to move in this gravity!\r\n");
    return 0;
   }
   else if (ROOM_GRAVITY(IN_ROOM(ch)) == 40 && GET_MAX_HIT(ch) < 100000) {
    send_to_char(ch, "You are hardly able to move in this gravity!\r\n");
    return 0;
   }
   else if (ROOM_GRAVITY(IN_ROOM(ch)) == 50 && GET_MAX_HIT(ch) < 200000) {
    send_to_char(ch, "You are hardly able to move in this gravity!\r\n");
    return 0;
   }
   else if (ROOM_GRAVITY(IN_ROOM(ch)) == 100 && GET_MAX_HIT(ch) < 400000) {
    send_to_char(ch, "You are hardly able to move in this gravity!\r\n");
    return 0;
   }
   else if (ROOM_GRAVITY(IN_ROOM(ch)) == 200 && GET_MAX_HIT(ch) < 1000000) {
    send_to_char(ch, "You are hardly able to move in this gravity!\r\n");
    return 0;
   }
   else if (ROOM_GRAVITY(IN_ROOM(ch)) == 300 && GET_MAX_HIT(ch) < 5000000) {
    send_to_char(ch, "You are hardly able to move in this gravity!\r\n");
    return 0;
   }
   else if (ROOM_GRAVITY(IN_ROOM(ch)) == 400 && GET_MAX_HIT(ch) < 8000000) {
    send_to_char(ch, "You are hardly able to move in this gravity!\r\n");
    return 0;
   }
   else if (ROOM_GRAVITY(IN_ROOM(ch)) == 500 && GET_MAX_HIT(ch) < 15000000) {
    send_to_char(ch, "You are hardly able to move in this gravity!\r\n");
    return 0;
   }
   else if (ROOM_GRAVITY(IN_ROOM(ch)) == 1000 && GET_MAX_HIT(ch) < 25000000) {
    send_to_char(ch, "You are hardly able to move in this gravity!\r\n");
    return 0;
   }
   else if (ROOM_GRAVITY(IN_ROOM(ch)) == 5000 && GET_MAX_HIT(ch) < 100000000) {
    send_to_char(ch, "You are hardly able to move in this gravity!\r\n");
    return 0;
   }
   else if (ROOM_GRAVITY(IN_ROOM(ch)) == 10000 && GET_MAX_HIT(ch) < 200000000) {
    send_to_char(ch, "You are hardly able to move in this gravity!\r\n");
    return 0;
   }
   else {
    return 1;
   }
}


/* If they can preform the attack or perform the attack on target. */
int can_kill(struct char_data *ch, struct char_data *vict, struct obj_data *obj, int num)
{
  /* Target Related */
  if (!IS_NPC(ch) && PLR_FLAGGED(ch, PLR_HEALT)) {
   send_to_char(ch, "You are inside a healing tank!\r\n");
   return 0;
  }
  if (IS_CARRYING_W(ch) > CAN_CARRY_W(ch)) {
   send_to_char(ch, "You are weighted down too much!\r\n");
   return 0;
  } 
   if (vict) {
   if (GET_HIT(vict) <= 0 && FIGHTING(vict)) {
    return 0;
   }
   if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return 0;
   } else if (vict == ch) {
    send_to_char(ch, "That's insane, don't hurt yourself. Hurt others! That's the key to life ^_^\r\n");
    return 0;
   } else if (vict->gooptime > 0) {
    send_to_char(ch, "It seems like it'll be hard to kill them right now...\r\n");
    return 0;
   } else if (CARRYING(ch)) {
    send_to_char(ch, "You are too busy protecting the person on your shoulder!\r\n");
    return 0;
   } else if (CARRIED_BY(vict)) {
    send_to_char(ch, "They are being protected by someone else!\r\n");
    return 0;
   } else if (AFF_FLAGGED(vict, AFF_PARALYZE)) {
    send_to_char(ch, "They are a statue, just leave them alone...\r\n");
    return 0;
   } else if (MOB_FLAGGED(vict, MOB_NOKILL)) {
    send_to_char(ch, "But they are not to be killed!\r\n");
    return 0;
   } else if (MAJINIZED(ch) == GET_ID(vict)) {
    send_to_char(ch, "You can not harm your master!\r\n");
    return 0;
   } else if (GET_BONUS(ch, BONUS_COWARD) > 0 && GET_MAX_HIT(vict) > GET_MAX_HIT(ch) + (GET_MAX_HIT(ch) * .5) && !FIGHTING(ch)) {
    send_to_char(ch, "You are too cowardly to start anything with someone so much stronger than yourself!\r\n");
    return 0;
   } else if (MAJINIZED(vict) == GET_ID(ch)) {
    send_to_char(ch, "You can not harm your servant.\r\n");
    return 0;
   } else if ((GRAPPLING(ch) && GRAPTYPE(ch) != 3) || (GRAPPLED(ch) && (GRAPTYPE(ch) == 1 || GRAPTYPE(ch) == 4))) {
    send_to_char(ch, "You are too busy grappling!%s\r\n", GRAPPLED(ch) != NULL ? " Try 'escape'!" : "");
    return 0;
   } else if (GRAPPLING(ch) && GRAPPLING(ch) != vict) {
    send_to_char(ch, "You can't reach that far in your current position!\r\n");
    return 0;
   } else if (GRAPPLED(ch) && GRAPPLED(ch) != vict) {
    send_to_char(ch, "You can't reach that far in your current position!\r\n");
    return 0;
   } else if (!IS_NPC(ch) && !IS_NPC(vict) && AFF_FLAGGED(ch, AFF_SPIRIT) && (!is_sparring(ch) || !is_sparring(vict)) && num != 2) {
    send_to_char(ch, "You can not fight other players in AL/Hell.\r\n");
    return 0;
   } else if (GET_LEVEL(vict) <= 8 && !IS_NPC(ch) && !IS_NPC(vict) && (!is_sparring(ch) || !is_sparring(vict))) {
    send_to_char(ch, "Newbie Shield Protects them!\r\n");
    return 0;
   } else if (GET_LEVEL(ch) <= 8 && !IS_NPC(ch) && !IS_NPC(vict) && (!is_sparring(ch) || !is_sparring(vict))) {
    send_to_char(ch, "Newbie Shield Protects you until level 8.\r\n");
    return 0;
   } else if (PLR_FLAGGED(vict, PLR_SPIRAL) && num != 3) {
    send_to_char(ch, "Due to the nature of their current technique anything less than a Tier 4 or AOE attack will not work on them.\r\n");
    return 0;
   } else if (ABSORBING(ch)) {
    send_to_char(ch, "You are too busy absorbing %s!\r\n", GET_NAME(ABSORBING(ch)));
    return 0;
   } else if (ABSORBBY(ch)) {
    send_to_char(ch, "You are too busy being absorbed by %s!\r\n", GET_NAME(ABSORBBY(ch)));
    return 0;
   } else if ((GET_ALT(vict) -1 > GET_ALT(ch) || GET_ALT(vict) < GET_ALT(ch) -1) && IS_NAMEK(ch)) {
     act("@GYou stretch your limbs toward @g$N@G in an attempt to hit $M!@n", TRUE, ch, 0, vict, TO_CHAR);
     act("@g$n@G stretches $s limbs toward @RYOU@G in an attempt to land a hit!@n", TRUE, ch, 0, vict, TO_VICT);
     act("@g$n@G stretches $s limbs toward @g$N@G in an attempt to hit $M!@n", TRUE, ch, 0, vict, TO_NOTVICT);
     return 1;
   } else if (AFF_FLAGGED(ch, AFF_FLYING) && !AFF_FLAGGED(vict, AFF_FLYING) && num == 0) {
    send_to_char(ch, "You are too far above them.\r\n");
    return 0;
   } else if (!AFF_FLAGGED(ch, AFF_FLYING) && AFF_FLAGGED(vict, AFF_FLYING) && num == 0) {
    send_to_char(ch, "They are too far above you.\r\n");
    return 0;
   } else if (!IS_NPC(ch) && GET_ALT(ch) > GET_ALT(vict) && !IS_NPC(vict) && num == 0) {
    if (GET_ALT(vict) < 0) {
     GET_ALT(vict) = GET_ALT(ch);
     return 1;
    } else {
     send_to_char(ch, "You are too far above them.\r\n");
     return 0;
    }
   } else if (!IS_NPC(ch) && GET_ALT(ch) < GET_ALT(vict) && !IS_NPC(vict)&& num == 0) {
    if (GET_ALT(vict) > 2) {
     GET_ALT(vict) = GET_ALT(ch);
     return 1;
    } else {
     send_to_char(ch, "They are too far above you.\r\n");
     return 0;
    }
   } else {
    return 1;
   }   
  }
  if (obj) {
   if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_PEACEFUL)) {
    send_to_char(ch, "This room just has such a peaceful, easy feeling...\r\n");
    return 0;
   } else if (OBJ_FLAGGED(obj, ITEM_UNBREAKABLE) && GET_OBJ_VNUM(obj) != 87 && GET_OBJ_VNUM(obj) != 80 && GET_OBJ_VNUM(obj) != 81 && GET_OBJ_VNUM(obj) != 82 && GET_OBJ_VNUM(obj) != 83) {
    send_to_char(ch, "You can't hit that, it is protected by the immortals!\r\n");
    return 0;
   } else if (AFF_FLAGGED(ch, AFF_FLYING)) {
    send_to_char(ch, "You are too far above it.\r\n");
    return 0;
   } else if (OBJ_FLAGGED(obj, ITEM_BROKEN)) {
    send_to_char(ch, "It is already broken!\r\n");
    return 0;
   } else {
    return 1;
   }
  } else {
   send_to_char(ch, "Error: Report to imm.");
   return 0;
  }
}

/* Whether they know the skill they are trying to use */
int check_skill(struct char_data *ch, int skill)
{
 if (!know_skill(ch, skill) && !IS_NPC(ch)) {
  return 0;
 }
 else {
  return 1;
 }
}

/* Whether they have enough stamina or charged ki to preform the skill */
int check_points(struct char_data *ch, int64_t ki, int64_t st)
{

 if (GET_PREFERENCE(ch) == PREFERENCE_H2H && GET_CHARGE(ch) >= GET_MAX_MANA(ch) * 0.1) {
  st -= st * 0.5;
 }

 int fail = FALSE;
 if (IS_NPC(ch)) {
  if ((getCurKI(ch)) < ki) {
   send_to_char(ch, "You do not have enough ki!\r\n");
   fail = TRUE;
  }
  if ((getCurST(ch)) < st) {
   send_to_char(ch, "You do not have enough stamina!\r\n");
   fail = TRUE;
  }
 }
 else {
  if (!calcGravCost(ch, st) && ki <= 0) {
    send_to_char(ch, "You do not have enough stamina to perform it in this gravity!\r\n");
    return 0;
  }
  if (GET_CHARGE(ch) < ki) {
   send_to_char(ch, "You do not have enough ki charged.\r\n");
   int64_t perc = GET_MAX_MANA(ch) * 0.01;
   if (ki >= perc * 49) {
    send_to_char(ch, "You need at least 50 percent charged.\r\n");
   }
   else if (ki >= perc * 44) {
    send_to_char(ch, "You need at least 45 percent charged.\r\n");
   }
   else if (ki >= perc * 39) {
    send_to_char(ch, "You need at least 40 percent charged.\r\n");
   }
   else if (ki >= perc * 34) {
    send_to_char(ch, "You need at least 35 percent charged.\r\n");
   }
   else if (ki >= perc * 29) {
    send_to_char(ch, "You need at least 30 percent charged.\r\n");
   }
   else if (ki >= perc * 24) {
    send_to_char(ch, "You need at least 25 percent charged.\r\n");
   }
   else if (ki >= perc * 19) {
    send_to_char(ch, "You need at least 20 percent charged.\r\n");
   }
   else if (ki >= perc * 14) {
    send_to_char(ch, "You need at least 15 percent charged.\r\n");
   }
   else if (ki >= perc * 9) {
    send_to_char(ch, "You need at least 10 percent charged.\r\n");
   }
   else if (ki >= perc * 4) {
    send_to_char(ch, "You need at least 5 percent charged.\r\n");
   }
   else if (ki >= 1) {
    send_to_char(ch, "You need at least 1 percent charged.\r\n");
   }
   fail = TRUE;
  }
  if (IS_NONPTRANS(ch)) {
   if (PLR_FLAGGED(ch, PLR_TRANS1)) {
    st -= st * 0.2;
   } else if (PLR_FLAGGED(ch, PLR_TRANS2)) {
    st -= st * 0.4;
   } else if (PLR_FLAGGED(ch, PLR_TRANS3)) {
    st -= st * 0.6;
   } else if (PLR_FLAGGED(ch, PLR_TRANS4)) {
    st -= st * 0.8;
   }
  }
  if ((getCurST(ch)) < st) {
   send_to_char(ch, "You do not have enough stamina.\r\n@C%s@n needed.\r\n", add_commas(st));
   fail = TRUE;
  } 
 }
 if (fail == TRUE) {
  return 0;
 }
 else {
  return 1;
 }
}

/* Subtract the stamina or ki required */
void pcost(struct char_data *ch, double ki, int64_t st)
{
 int before = 0;
 if (GET_LEVEL(ch) > 1 && !IS_NPC(ch)) {
  if (ki == 0) {
   before = (getCurST(ch));
   if (calcGravCost(ch, 0)) {
    if (before > (getCurST(ch))) {
     send_to_char(ch, "You exert more stamina in this gravity.\r\n");
    }
   }
  }
  if (GET_CHARGE(ch) <= (GET_MAX_MANA(ch) * ki)) {
   GET_CHARGE(ch) = 0;
  }
  if (GET_CHARGE(ch) > (GET_MAX_MANA(ch) * ki)) {
   GET_CHARGE(ch) -= (GET_MAX_MANA(ch) * ki);
  }
  if (GET_CHARGE(ch) < 0) {
   GET_CHARGE(ch) = 0;
  }
  if (GET_KAIOKEN(ch) > 0) {
   st += (st / 20) * GET_KAIOKEN(ch);
  }
  if (AFF_FLAGGED(ch, AFF_HASS)) {
   st += st * .3;
  }
  if (!IS_NPC(ch) && GET_BONUS(ch, BONUS_HARDWORKER) > 0) {
   st -= st * .25;
  } else if (!IS_NPC(ch) && GET_BONUS(ch, BONUS_SLACKER) > 0) {
   st += st * .25;
  }
   if (IS_ICER(ch)) {
    if (PLR_FLAGGED(ch, PLR_TRANS1)) {
     st = st * 1.05;
    } else if (PLR_FLAGGED(ch, PLR_TRANS2)) {
     st = st * 1.1;
    } else if (PLR_FLAGGED(ch, PLR_TRANS3)) {
     st = st * 1.15;
    } else if (PLR_FLAGGED(ch, PLR_TRANS4)) {
     st = st * 1.20;
    }
   }
   if (GET_PREFERENCE(ch) == PREFERENCE_H2H && GET_CHARGE(ch) >= GET_MAX_MANA(ch) * 0.1) {
    st -= st * 0.5;
    GET_CHARGE(ch) -= st;
    if (GET_CHARGE(ch) < 0)
     GET_CHARGE(ch) = 0;
   }
  if (IS_NONPTRANS(ch)) {
   if (PLR_FLAGGED(ch, PLR_TRANS1)) {
    st -= st * 0.2;
   } else if (PLR_FLAGGED(ch, PLR_TRANS2)) {
    st -= st * 0.4;
   } else if (PLR_FLAGGED(ch, PLR_TRANS3)) {
    st -= st * 0.6;
   } else if (PLR_FLAGGED(ch, PLR_TRANS4)) {
    st -= st * 0.8;
   }
  }
  decCurST(ch, st);
 }
 if (IS_NPC(ch)) {
     decCurKI(ch, ki);
     decCurST(ch, st);
 }
}


/* For checking if they have enough free limbs to preform the technique. */
int limb_ok(struct char_data *ch, int type) {
 if (IS_NPC(ch)) {
  if (AFF_FLAGGED(ch, AFF_ENSNARED) && rand_number(1, 100) <= 90) {
   return FALSE;
  }
  return TRUE;
 }
 if (GRAPPLING(ch) && GRAPTYPE(ch) != 3) {
  send_to_char(ch, "You are too busy grappling!\r\n");
  return FALSE;
 }
 if (GRAPPLED(ch) && (GRAPTYPE(ch) == 1 || GRAPTYPE(ch) == 4)) {
  send_to_char(ch, "You are unable to move while in this hold! Try using 'escape' to get out of it!\r\n");
  return FALSE;
 }
 if (GET_SONG(ch) > 0) {
  send_to_char(ch, "You are currently playing a song! Enter the song command in order to stop!\r\n");
  return FALSE;
 }
 if (type == 0) {
  if (!HAS_ARMS(ch)) {
   send_to_char(ch, "You have no available arms!\r\n");
   return FALSE;
  }
  if (AFF_FLAGGED(ch, AFF_ENSNARED) && rand_number(1, 100) <= 90) {
   send_to_char(ch, "You are unable to move your arms while bound by this strong silk!\r\n");
   WAIT_STATE(ch, PULSE_1SEC);
   return FALSE;
  } else if (AFF_FLAGGED(ch, AFF_ENSNARED)) {
   act("You manage to break the silk ensnaring your arms!", TRUE, ch, 0, 0, TO_CHAR);
   act("$n manages to break the silk ensnaring $s arms!", TRUE, ch, 0, 0, TO_ROOM);
   REMOVE_BIT_AR(AFF_FLAGS(ch), AFF_ENSNARED);
  }
  if (GET_EQ(ch, WEAR_WIELD1) && GET_EQ(ch, WEAR_WIELD2)) {
   send_to_char(ch, "Your hands are full!\r\n");
   return FALSE;
  }
 } // Arms
 else if (type > 0) {
  if (!HAS_LEGS(ch)) {
   send_to_char(ch, "You have no working legs!\r\n");
   return FALSE;
  }
 } // Legs
 return TRUE;
}


size_t send_to_char(struct char_data *ch, const char *messg, ...)
{
  if (ch->desc && messg && *messg) {
    size_t left;
    va_list args;

    va_start(args, messg);
    left = vwrite_to_output(ch->desc, messg, args);
    va_end(args);
    return left;
  }
  return 0;
}



int init_skill(struct char_data *ch, int snum) {
 int skill = 0;

 if (!IS_NPC(ch)) {
  skill = GET_SKILL(ch, snum);

  if (PLR_FLAGGED(ch, PLR_TRANSMISSION)) {
   skill += 4;
  }
 
  if (skill > 118)
   skill = 118;

  return (skill);
 }

 if (IS_NPC(ch) && GET_LEVEL(ch) <= 10) {
  skill = rand_number(30, 50);
 } else if (IS_NPC(ch) && GET_LEVEL(ch) <= 20) {
  skill = rand_number(45, 65);
 } else if (IS_NPC(ch) && GET_LEVEL(ch) <= 30) {
  skill = rand_number(55, 70);
 } else if (IS_NPC(ch) && GET_LEVEL(ch) <= 50) {
  skill = rand_number(65, 80);
 } else if (IS_NPC(ch) && GET_LEVEL(ch) <= 70) {
  skill = rand_number(75, 90);
 } else if (IS_NPC(ch) && GET_LEVEL(ch) <= 80) {
  skill = rand_number(85, 100);
 } else if (IS_NPC(ch) && GET_LEVEL(ch) <= 90) {
  skill = rand_number(90, 100);
 } else if (IS_NPC(ch) && GET_LEVEL(ch) <= 100) {
  skill = rand_number(95, 100);
 } else if (IS_NPC(ch) && GET_LEVEL(ch) <= 110) {
  skill = rand_number(95, 105);
 } else {
  skill = rand_number(100, 110);
 }

 return (skill);
}

/* For calculating the difficulty the player has to hit with the skill. */
int chance_to_hit(struct char_data *ch)
{

 int num = axion_dice(0);

 if (IS_NPC(ch))
  return (num);

 if (GET_COND(ch, DRUNK) > 4) {
  num += GET_COND(ch, DRUNK);
 }

 return (num);
}

/* For calculating the speed of the attacker and defender */
int handle_speed(struct char_data *ch, struct char_data *vict)
{

  if (ch == NULL || vict == NULL) { /* Ruh roh*/
   return (0);
  }

  if (GET_SPEEDI(ch) > GET_SPEEDI(vict) * 4) {
   return (15);
  } else if (GET_SPEEDI(ch) > GET_SPEEDI(vict) * 2) {
   return (10);
  } else if (GET_SPEEDI(ch) > GET_SPEEDI(vict)) {
   return (5);
  } else if (GET_SPEEDI(ch) * 4 < GET_SPEEDI(vict)) {
   return (-15);
  } else if (GET_SPEEDI(ch) * 2 < GET_SPEEDI(vict)) {
   return (-10);
  } else if (GET_SPEEDI(ch) < GET_SPEEDI(vict)) {
   return (-5);
  }

  return (0);
}


bool race_has_tail(int r_id) {
    switch(r_id) {
        case RACE_ICER:
        case RACE_BIO:
        case RACE_SAIYAN:
        case RACE_HALFBREED:
            return true;
        default:
            return false;
    }
}

void char_lose_tail(char_data *ch) {
    if(!char_has_tail(ch)) return;
    switch(ch->race) {
        case RACE_ICER:
        case RACE_BIO:
            REMOVE_BIT_AR(PLR_FLAGS(ch), PLR_TAIL);
            remove_limb(ch, 6);
            GET_TGROWTH(ch) = 0;
            break;
        case RACE_SAIYAN:
        case RACE_HALFBREED:
            REMOVE_BIT_AR(PLR_FLAGS(ch), PLR_STAIL);
            remove_limb(ch, 5);
            if(PLR_FLAGGED(ch, PLR_OOZARU)) {
                oozaru_revert(ch);
            }
            GET_TGROWTH(ch) = 0;
            break;
    }
}


bool char_has_tail(char_data *ch) {
    if(!race_has_tail(ch->race))
        return false;
    switch(ch->race) {
        case RACE_ICER:
        case RACE_BIO:
            return PLR_FLAGGED(ch, PLR_TAIL);
        case RACE_SAIYAN:
        case RACE_HALFBREED:
            return PLR_FLAGGED(ch, PLR_STAIL);
        default:
            return false;
    }
}

void char_gain_tail(char_data *ch, bool announce) {
    if(char_has_tail(ch)) return;
    switch(ch->race) {
        case RACE_ICER:
        case RACE_BIO:
            SET_BIT_AR(PLR_FLAGS(ch), PLR_TAIL);
            break;
        case RACE_SAIYAN:
        case RACE_HALFBREED:
            SET_BIT_AR(PLR_FLAGS(ch), PLR_STAIL);
            if(MOON_OK(ch)) {
                oozaru_transform(ch);
            }
            break;
    }
}


room_vnum sensei_location_id(int s_id)  {
    switch(s_id) {
        case CLASS_ROSHI:
            return 1131;
        case CLASS_KABITO:
            return 12098;
        case CLASS_NAIL:
            return 11683;
        case CLASS_BARDOCK:
            return 2267;
        case CLASS_KRANE:
            return 13012;
        case CLASS_TAPION:
            return 8233;
        case CLASS_PICCOLO:
            return 1662;
        case CLASS_ANDSIX:
            return 1714;
        case CLASS_DABURA:
            return 6487;
        case CLASS_FRIEZA:
            return 4283;
        case CLASS_GINYU:
            return 4290;
        case CLASS_JINTO:
            return 3499;
        case CLASS_KURZAK:
            return 16100;
        case CLASS_TSUNA:
            return 15009;
        default:
            return 300;
    }
}

room_vnum sensei_start_room(int s_id) {
    switch(s_id) {
        case CLASS_ROSHI:
            return 1130;
        case CLASS_KABITO:
            return 12098;
        case CLASS_NAIL:
            return 11683;
        case CLASS_BARDOCK:
            return 2268;
        case CLASS_KRANE:
            return 13009;
        case CLASS_TAPION:
            return 8231;
        case CLASS_PICCOLO:
            return 1659;
        case CLASS_ANDSIX:
            return 1713;
        case CLASS_DABURA:
            return 6486;
        case CLASS_FRIEZA:
            return 4282;
        case CLASS_GINYU:
            return 4289;
        case CLASS_JINTO:
            return 3499;
        case CLASS_KURZAK:
            return 16100;
        case CLASS_TSUNA:
            return 15009;
        default:
            return 300;
    }
}

int sensei_grav_tolerance(int s_id) {
    switch(s_id) {
        case CLASS_BARDOCK:
            return 10;
        default:
            return 0;
    }
}

int sensei_rpp_cost(int s_id, int r_id) {
    switch(s_id) {
        case CLASS_KABITO:
            if(r_id != RACE_KAI) {
                return 10;
            } else {
                return 0;
            }
        default:
            return 0;
    }
}

int race_get_size(int r_id) {
    switch(r_id) {
        case RACE_ANIMAL:
             return SIZE_FINE;
        case RACE_SAIBA:
        case RACE_OGRE:
            return SIZE_LARGE;
        case RACE_SPIRIT:
            return SIZE_TINY;
        case RACE_TRUFFLE:
            return SIZE_SMALL;
        default:
            return SIZE_MEDIUM;
    }
}

int race_is_playable(int r_id) {
    switch(r_id) {
        case RACE_ANIMAL:
        case RACE_SAIBA:
        case RACE_SERPENT:
        case RACE_OGRE:
        case RACE_YARDRATIAN:
        case RACE_DRAGON:
        case RACE_MECHANICAL:
        case RACE_SPIRIT:
            return false;
        default:
            return true;
    }
}

int race_is_people(int r_id) {
    switch(r_id) {
        case RACE_ANIMAL:
        case RACE_SAIBA:
        case RACE_MECHANICAL:
        case RACE_SPIRIT:
            return false;
        default:
            return true;
    }
}