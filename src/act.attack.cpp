/* ************************************************************************
*   File: act.attack.c                                  Part of DBAT      *
*  Usage: player-level commands of an offensive nature                    *
*         created because the size act.offensive.c was getting too large. *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
*                                                                         *
*  -While this is an original file and is credited to me it is basically  *
*   just act.offensive.c part 2. So all original credit is due to the     *
*   credits found in act.offensive.c except for the commands added in-    *
*                                                   ~~Iovan               *
************************************************************************ */
#include "dbat/act.attack.h"
#include "dbat/attack.h"
#include "dbat/fight.h"
#include "dbat/dg_comm.h"
#include "dbat/act.item.h"
#include "dbat/interpreter.h"
#include "dbat/utils.h"
#include "dbat/handler.h"
#include "dbat/comm.h"
#include "dbat/constants.h"
#include "dbat/combat.h"
#include "dbat/class.h"
#include "dbat/techniques.h"

ACMD(do_lightgrenade) {
    atk::LightGrenade a(ch, argument);
    a.execute();
}

ACMD(do_energize) {

    if (IS_NPC(ch))
        return;

    if (GET_PREFERENCE(ch) != PREFERENCE_THROWING) {
        send_to_char(ch, "You aren't dedicated to throwing!\r\n");
        return;
    }

    if (!GET_SKILL(ch, SKILL_ENERGIZE)) {
        if (GET_SKILL(ch, SKILL_FOCUS) >= 30) {
            int result = rand_number(10, 14);
            SET_SKILL(ch, SKILL_ENERGIZE, result);
            send_to_char(ch,
                         "You learn the basics for energizing thrown weapons! Now use the energize command again.\r\n");
            return;
        } else {
            send_to_char(ch, "You need a Focus skill level of 30 to figure out the basics of this technique.\r\n");
            return;
        }
    } else {
        ch->pref.flip(PRF_ENERGIZE);
        if (ch->pref.test(PRF_ENERGIZE)) {
            send_to_char(ch, "You start focusing your latent ki into your fingertips.\r\n");
        } else {
            send_to_char(ch, "You stop focusing ki into your fingertips.\r\n");
        }
    }
}

ACMD(do_breath) {
    atk::FireBreath a(ch, argument);
    a.execute();
}

ACMD(do_ram) {
    atk::Ram a(ch, argument);
    a.execute();
}

ACMD(do_strike) {
    atk::FangStrike a(ch, argument);
    a.execute();
}

/* End NPC skills */

ACMD(do_combine) {

    char arg[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
    struct char_data *vict;
    struct follow_type *f;
    int fire = false, temp = -1, temp2 = -1;

    two_arguments(argument, arg, arg2);

    if (!has_group(ch)) {
        send_to_char(ch, "You need to be in a group!\r\n");
        return;
    } else {
        if (!*arg || (!ch->master && !*arg2)) {
            send_to_char(ch, "Follower Syntax: combine (attack)\r\n");
            send_to_char(ch, "Leader Syntax: combine (attack) (target)\r\n");
            send_to_char(ch, "Cancel Syntax: combine stop\r\n");
        } else {
            if (!strcasecmp(arg, "stop") && ch->master) {
                if (GET_COMBINE(ch) == -1) {
                    send_to_char(ch, "You are not trying to combine any attacks...\r\n");
                    return;
                } else {
                    send_to_char(ch, "You stop your preparations to combine your attack with a group attack.\r\n");
                    send_to_char(ch->master, "@Y%s@C is no longer prepared to combine an attack with the group!@n\r\n",
                                 get_i_name(ch->master, ch));
                    for (f = ch->master->followers; f; f = f->next) {
                        if (ch != f->follower)
                            send_to_char(f->follower,
                                         "@Y%s@C is no longer prepared to combine an attack with the group!@n\r\n",
                                         get_i_name(f->follower, ch));
                    }
                    GET_COMBINE(ch) = -1;
                    return;
                }
            } else if (!strcasecmp(arg, "stop") && !ch->master) {
                send_to_char(ch, "You do not need to stop as you haven't prepared anything.\r\n");
                return;
            }
            int i = 0;
            for (i = 0; i < 14; i++) {
                if (strstr(arg, attack_names[i])) {
                    if (i == 5) {
                        if (!GET_EQ(ch, WEAR_WIELD1)) {
                            send_to_char(ch, "You need to wield a sword to use this technique.\r\n");
                            return;
                        }
                        if (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD1), VAL_WEAPON_DAMTYPE) != TYPE_SLASH - TYPE_HIT) {
                            send_to_char(ch, "You are not wielding a sword, you need one to use this technique.\r\n");
                            return;
                        } else {
                            temp = i;
                            i = 15;
                        }
                    }
                    temp = i;
                    i = 15;
                }
            }
            if (temp == -1) {
                send_to_char(ch, "Follower Syntax: combine (attack)\r\n");
                send_to_char(ch, "Leader Syntax: combine (attack) (target)\r\n");
                send_to_char(ch, "Follower Cancel Syntax: combine stop\r\n");
                return;
            } else if (!GET_SKILL(ch, attack_skills[temp])) {
                send_to_char(ch, "You do not know that skill.\r\n");
                return;
            } else if (attack_skills[temp] == 440 && !IS_NAIL(ch)) {
                send_to_char(ch, "Only students of Nail know how to combine that attack effectively.\r\n");
                return;
            } else if (GET_CHARGE(ch) < GET_MAX_MANA(ch) * 0.05) {
                send_to_char(ch, "You need to have the minimum of 5%s ki charged to combine.\r\n", "%");
            }
            if (!ch->master) {
                if (!(vict = get_char_vis(ch, arg2, nullptr, FIND_CHAR_ROOM))) {
                    send_to_char(ch, "Who will your combined attack be targeting?\r\n");
                    return;
                } else if (vict == ch) {
                    send_to_char(ch, "No targeting yourself...\r\n");
                    return;
                }
                GET_COMBINE(ch) = temp;
                for (f = ch->followers; f; f = f->next) {
                    if (!AFF_FLAGGED(f->follower, AFF_GROUP)) {
                        continue;
                    } else if (GET_COMBINE(f->follower) != -1 &&
                               GET_CHARGE(f->follower) >= GET_MAX_MANA(f->follower) * 0.05) {
                        fire = true;
                    }
                } /* End follow for statement */
                if (fire == true) {
                    combine_attacks(ch, vict);
                    return;
                } else {
                    send_to_char(ch,
                                 "You do not have any followers who have readied an attack to combine or they do not have enough ki anymore to combine said attack.\r\n");
                    return;
                }
            } else if (ch->master) {
                if (GET_CHARGE(ch) >= GET_MAX_MANA(ch) * 0.05) {
                    act("@C$n@c appears to be concentrating hard and focusing $s energy!@n\r\n", true, ch, nullptr,
                        nullptr, TO_ROOM);
                    send_to_char(ch->master,
                                 "@BCOMBINE@c: @Y%s@C has prepared to combine a @c'@G%s@c'@C with the next group attack!@n\r\n",
                                 get_i_name(ch->master, ch), attack_names[temp]);
                    for (f = ch->master->followers; f; f = f->next) {
                        if (ch != f->follower)
                            send_to_char(f->follower,
                                         "@BCOMBINE@c: @Y%s@C has prepared to combine a @c'@G%s@c'@C with the next group attack!@n\r\n",
                                         get_i_name(f->follower, ch), attack_names[temp]);
                    }
                    GET_COMBINE(ch) = temp;
                } else {
                    send_to_char(ch, "You do not have the minimum 5%s ki charged.\r\n", "%");
                    return;
                }
            }
        }
    }
} /* End */

ACMD(do_sunder) {
    atk::SunderingForce a(ch, argument);
    a.execute();
}

ACMD(do_zen) {
    atk::ZenBlade a(ch, argument);
    a.execute();
}

ACMD(do_malice) {
    atk::MaliceBreaker a(ch, argument);
    a.execute();
}

ACMD(do_nova) {
    atk::StarNova a(ch, argument);
    a.execute();
}

ACMD(do_head) {
    atk::Headbutt a(ch, argument);
    a.execute();
}

ACMD(do_bash) {
    atk::Bash a(ch, argument);
    a.execute();
}

ACMD(do_seishou) {
    atk::SeishouEnko a(ch, argument);
    a.execute();
}

ACMD(do_throw) {
    struct char_data *vict = nullptr, *tch = nullptr;
    struct obj_data *obj = nullptr;
    char arg[MAX_INPUT_LENGTH];
    char arg2[1000], chunk[2000], arg3[1000];
    int odam = 0, miss = true, perc = 0, prob = 0, perc2 = 0, grab = false;
    int64_t damage;

    half_chop(argument, arg, chunk);

    if (!*arg) {
        send_to_char(ch, "Throw what?\r\n");
        return;
    }

    if (is_sparring(ch)) {
        send_to_char(ch, "You can not spar with throw.\r\n");
        return;
    }

    if (*chunk) {
        two_arguments(chunk, arg2, arg3);
    }

    if (!(obj = get_obj_in_list_vis(ch, arg, nullptr, ch->contents))) {
        if (!(tch = get_char_vis(ch, arg, nullptr, FIND_CHAR_ROOM))) {
            send_to_char(ch, "You do not have that object or character to throw!\r\n");
            return;
        }
    }

    if (!(vict = get_char_vis(ch, arg2, nullptr, FIND_CHAR_ROOM))) {
        if (FIGHTING(ch) && IN_ROOM(FIGHTING(ch)) == IN_ROOM(ch)) {
            vict = FIGHTING(ch);
        } else {
            send_to_char(ch, "Who do you want to target?\r\n");
            return;
        }
    }

    if (GET_HIT(vict) <= 1) {
        return;
    }

    if (!can_kill(ch, vict, nullptr, 1)) {
        return;
    }

    if (handle_defender(vict, ch)) {
        struct char_data *def = GET_DEFENDER(vict);
        vict = def;
    }

    auto gravity = ch->currentGravity();

    /* We are throwing an object. */
    if (obj) {
        if (ch->throws == -1) {
            ch->throws = 0;
            return;
        }

        if ((ch->getCurST()) < ((GET_MAX_HIT(ch) / 200) + GET_OBJ_WEIGHT(obj))) {
            send_to_char(ch, "You do not have enough stamina to do it...\r\n");
            return;
        }
        if (OBJ_FLAGGED(obj, ITEM_BROKEN)) {
            send_to_char(ch, "That is broken and useless to throw!\r\n");
            return;
        }
        if (!ch->canCarryWeight(obj)) {
            send_to_char(ch, "The gravity has made that too heavy for you to throw!\r\n");
            return;
        } else {
            int penalty = 0, chance = axion_dice(0) + axion_dice(0), wtype = 0, wlvl = 1, multithrow = true;
            handle_cooldown(ch, 5);
            improve_skill(ch, SKILL_THROW, 0);
            damage = (GET_STR(ch) * (GET_CHA(ch) * 2) * GET_DEX(ch)) / 4;
            damage += (damage * 0.01) * (gravity / 4);

            if (GET_PREFERENCE(ch) == PREFERENCE_THROWING) {
                chance -= chance * 0.25;
            }

            if (OBJ_FLAGGED(obj, ITEM_WEAPLVL1)) {
                damage += damage * 0.1;
                wlvl = 1;
            } else if (OBJ_FLAGGED(obj, ITEM_WEAPLVL2)) {
                damage += damage * 0.2;
                wlvl = 2;
            } else if (OBJ_FLAGGED(obj, ITEM_WEAPLVL3)) {
                damage += damage * 0.3;
                wlvl = 3;
            } else if (OBJ_FLAGGED(obj, ITEM_WEAPLVL4)) {
                damage += damage * 0.4;
                wlvl = 4;
            } else if (OBJ_FLAGGED(obj, ITEM_WEAPLVL5)) {
                damage += damage * 0.5;
                wlvl = 5;
            }
            if (GET_OBJ_TYPE(obj) == ITEM_WEAPON) {
                if (GET_OBJ_VAL(obj, VAL_WEAPON_DAMTYPE) == TYPE_PIERCE - TYPE_HIT) {
                    wtype = 1;
                } else if (GET_OBJ_VAL(obj, VAL_WEAPON_DAMTYPE) == TYPE_SLASH - TYPE_HIT) {
                    wtype = 2;
                } else if (GET_OBJ_VAL(obj, VAL_WEAPON_DAMTYPE) == TYPE_CRUSH - TYPE_HIT) {
                    wtype = 3;
                } else if (GET_OBJ_VAL(obj, VAL_WEAPON_DAMTYPE) == TYPE_STAB - TYPE_HIT) {
                    wtype = 4;
                } else if (GET_OBJ_VAL(obj, VAL_WEAPON_DAMTYPE) == TYPE_BLAST - TYPE_HIT) {
                    wtype = 5;
                    damage = ((GET_OBJ_WEIGHT(obj) * GET_STR(ch)) * (GET_CHA(ch) / 3)) + (GET_MAX_HIT(ch) * 0.01);
                    damage += gravity * (gravity / 2);
                } else {
                    wtype = 6;
                }
            }
            if (GET_OBJ_VAL(obj, VAL_ALL_MATERIAL) == MATERIAL_STEEL) {
                odam = rand_number(5, 30);
            } else if (GET_OBJ_VAL(obj, VAL_ALL_MATERIAL) == MATERIAL_IRON) {
                odam = rand_number(18, 50);
            } else if (GET_OBJ_VAL(obj, VAL_ALL_MATERIAL) == MATERIAL_MITHRIL) {
                odam = rand_number(5, 15);
            } else if (GET_OBJ_VAL(obj, VAL_ALL_MATERIAL) == MATERIAL_KACHIN) {
                odam = rand_number(5, 15);
            } else if (GET_OBJ_VAL(obj, VAL_ALL_MATERIAL) == MATERIAL_STONE) {
                odam = rand_number(20, 50);
            } else if (GET_OBJ_VAL(obj, VAL_ALL_MATERIAL) == MATERIAL_DIAMOND) {
                odam = rand_number(5, 20);
            } else if (GET_OBJ_VAL(obj, VAL_ALL_MATERIAL) == MATERIAL_ENERGY) {
                if (rand_number(1, 2) == 2) {
                    odam = 0;
                } else {
                    odam = rand_number(1, 3);
                }
            } else {
                odam = rand_number(90, 100);
            }
            if (!OBJ_FLAGGED(obj, ITEM_THROW)) {
                penalty = 15;
                multithrow = false;
                damage = damage * 0.3;
            } else {
                odam = rand_number(0, 1);
                damage += GET_STR(ch) * GET_DEX(ch) + rand_number(1, 20);
                damage += wlvl * (damage * 0.1);
            }

            if (wlvl == 5) {
                damage *= 250 * GET_DEX(ch);
            } else if (wlvl == 4) {
                damage *= 160 * GET_DEX(ch);
            } else if (wlvl == 3) {
                damage *= 100 * GET_DEX(ch);
            } else if (wlvl == 2) {
                damage *= 50 * GET_DEX(ch);
            } else if (wlvl == 1) {
                damage *= 10 * GET_DEX(ch);
            }

            int hot = false;
            if (OBJ_FLAGGED(obj, ITEM_HOT)) {
                hot = true;
            }


            if (wtype > 0 && wtype != 5 && odam > 1) {
                odam = 1;
            }
            perc = init_skill(ch, SKILL_THROW);
            perc2 = init_skill(vict, SKILL_DODGE);
            prob = axion_dice(penalty);
            if (*arg3) {
                if (!strcasecmp(arg3, "1") || !strcasecmp(arg3, "single")) {
                    multithrow = false;
                } else {
                    send_to_char(ch,
                                 "Syntax: throw (obj | character) (target) <-- This will multithrow if able\nSyntax: throw (obj) (target) (1 | single) <-- This will not multi throw)\r\n");
                    return;
                }
            }

            if (!tech_handle_zanzoken(ch, vict, "$p")) {
                COMBO(ch) = -1;
                COMBHITS(ch) = 0;
                int stcost = ((GET_MAX_HIT(ch) / 200) + GET_OBJ_WEIGHT(obj));
                vict->affected_by.reset(AFF_ZANZOKEN);
                pcost(ch, 0, stcost / 2);
                pcost(vict, 0, GET_MAX_HIT(vict) / 200);
                obj_from_char(obj);
                obj_to_room(obj, IN_ROOM(vict));
                return;
            }

            if (perc - (perc2 / 10) < prob) {
                if (OBJ_FLAGGED(obj, ITEM_ICE) && IS_DEMON(vict)) {
                    act("You throw $p at $N@n, but it melts before touching $M!", true, ch, obj, vict, TO_CHAR);
                    act("$n@n throws $p at $N@n, but it melts before touching $M!", true, ch, obj, vict, TO_NOTVICT);
                    act("$n@n throws $p at you, but it melts before touching you!", true, ch, obj, vict, TO_VICT);
                    ch->decCurST(((GET_MAX_HIT(ch) / 100) + GET_OBJ_WEIGHT(obj)));
                    extract_obj(obj);
                    return;
                }
                if (perc2 > 0) {
                    act("You throw $p at $N@n, but $E manages to dodge it easily!", true, ch, obj, vict, TO_CHAR);
                    act("$n@n throws $p at $N@n, but $E manages to dodge it easily!", true, ch, obj, vict, TO_NOTVICT);
                    act("$n@n throws $p at you, but you easily dodge it.", true, ch, obj, vict, TO_VICT);
                } else if (perc2 <= 0) {
                    act("You throw $p at $N@n, but unfortunatly miss!", true, ch, obj, vict, TO_CHAR);
                    act("$n@n throws $p at $N@n, but unfortunatly misses!", true, ch, obj, vict, TO_NOTVICT);
                    act("$n@n throws $p at you, but thankfully misses you.", true, ch, obj, vict, TO_VICT);
                }
                ch->decCurST(((GET_MAX_HIT(ch) / 100) + GET_OBJ_WEIGHT(obj)));
                if (!OBJ_FLAGGED(obj, ITEM_UNBREAKABLE)) {
                    GET_OBJ_VAL(obj, VAL_ALL_HEALTH) -= odam / 2;
                }
                LASTATK(ch) = -50;
                hurt(0, 0, ch, vict, nullptr, 0, 0);
                obj_from_char(obj);
                obj_to_room(obj, IN_ROOM(vict));
                ch->decCurST(((GET_MAX_HIT(ch) / 200) + GET_OBJ_WEIGHT(obj)));
                if (!GET_EQ(ch, WEAR_WIELD1) && !GET_EQ(ch, WEAR_WIELD2))
                    perc += 20;
                if (perc + GET_CHA(ch) >= chance + penalty && multithrow == true && GET_HIT(vict) > 1 &&
                    ch->throws > 1) {
                    do_throw(ch, argument, 0, 0);
                    ch->throws -= 1;
                } else if (perc + GET_CHA(ch) >= chance + penalty && multithrow == true && GET_HIT(vict) > 1 &&
                           ch->throws == 1) {
                    do_throw(ch, argument, 0, 0);
                    ch->throws = -1;
                } else {
                    ch->throws = 0;
                }
                WAIT_STATE(ch, PULSE_3SEC);
                return;
            } else if (perc - (perc2 / 10) > prob) {
                miss = false;
            }
            if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_ENERGIZE) && (ch->getCurKI()) >= GET_MAX_MANA(ch) * 0.02) {
                damage += (damage * (0.0016 * GET_SKILL(ch, SKILL_ENERGIZE)));
                act("You charge $p with the energy in your fingertips! As it begins to @Yglow a bright hot @Rred@n you throw $p at $N@n full speed, and watch it smash into $M!",
                    true, ch, obj, vict, TO_CHAR);
                act("$n@n charges $p with the energy in $s fingertips! As it begins to @Yglow a bright hot @Rred@n $e throws $p at $N@n full speed, and watches it smash into $M!",
                    true, ch, obj, vict, TO_NOTVICT);
                act("$n@n charges $p with the energy in $s fingertips! As it begins to @Yglow a bright hot @Rred@n $e throws $p at YOU@n full speed, and watches it smash into YOU!!",
                    true, ch, obj, vict, TO_VICT);
                if (GET_MAX_MANA(ch) * 0.02 > 0) {
                    ch->decCurKI(ch->getMaxKI() * .02);
                } else {
                    ch->decCurKI(1);
                }
                improve_skill(ch, SKILL_ENERGIZE, 0);
            } else if (wtype == 0) {
                act("You throw $p at $N@n full speed, and watch it smash into $M!", true, ch, obj, vict, TO_CHAR);
                act("$n@n throws $p at $N@n full speed, and watches it smash into $M!", true, ch, obj, vict,
                    TO_NOTVICT);
                act("$n@n throws $p at you full speed. You reel as it smashes into your body!", true, ch, obj, vict,
                    TO_VICT);
            } else if (wtype == 1 || wtype == 2) {
                act("You pull out and throw $p at $N@n full speed, and watch it sink into $M!", true, ch, obj, vict,
                    TO_CHAR);
                act("$n@n pulls out and throws $p at $N@n full speed, and watches it sink into $M!", true, ch, obj,
                    vict, TO_NOTVICT);
                act("$n@n pulls out and throws $p at you full speed. You reel as it sink into your body!", true, ch,
                    obj, vict, TO_VICT);
            } else if (wtype == 3) {
                act("You swing $p overhead and throw it at $N@n full speed, and watch it slam into $M!", true, ch, obj,
                    vict, TO_CHAR);
                act("$n@n swings $p overhead and throws it at $N@n full speed, and watches it slam into $M!", true, ch,
                    obj, vict, TO_NOTVICT);
                act("$n@n swings $p overhead and throws it at you full speed. You reel as it slam into your body!",
                    true, ch, obj, vict, TO_VICT);
            } else if (wtype == 4) {
                act("You bring $p over your shoulder and throw it at $N@n full speed, and watch it sink into $M!", true,
                    ch, obj, vict, TO_CHAR);
                act("$n@n brings $p over $s shoulder and throws it at $N@n full speed, and watches it sink into $M!",
                    true, ch, obj, vict, TO_NOTVICT);
                act("$n@n brings $p over $s shoulder and throws $p at you full speed. You reel as it sink into your body!",
                    true, ch, obj, vict, TO_VICT);
            } else if (wtype == 5) {
                act("You pull out and throw $p at $N@n full speed, and watch it hit $M!", true, ch, obj, vict, TO_CHAR);
                act("$n@n pulls out and throws $p at $N@n full speed, and watches it hit $M!", true, ch, obj, vict,
                    TO_NOTVICT);
                act("$n@n pulls out and throws $p at you full speed. You reel as it hits your body!", true, ch, obj,
                    vict, TO_VICT);
            }
            if (!OBJ_FLAGGED(obj, ITEM_UNBREAKABLE)) {
                GET_OBJ_VAL(obj, VAL_ALL_HEALTH) -= odam;
            }
            LASTATK(ch) = -50;
            if ((GET_OBJ_VAL(obj, VAL_ALL_HEALTH) - odam) <= 0 && !OBJ_FLAGGED(obj, ITEM_UNBREAKABLE)) {
                act("You smile as $p breaks on $N's@n face!", true, ch, obj, vict, TO_CHAR);
                act("$n@n smiles as $p breaks on $N's@n face!", true, ch, obj, vict, TO_NOTVICT);
                act("$n@n smiles as $p breaks on your face!", true, ch, obj, vict, TO_VICT);
                obj->extra_flags.flip(ITEM_BROKEN);
            } else if (GET_DEX(ch) >= axion_dice(0)) {
                if (IS_ANDROID(vict) || IS_MECHANICAL(vict)) {
                    act("@RSome pieces of metal are sent flying!@n", true, ch, nullptr, vict, TO_CHAR);
                    act("@RSome pieces of metal are sent flying!@n", true, ch, nullptr, vict, TO_VICT);
                    act("@RSome pieces of metal are sent flying!@n", true, ch, nullptr, vict, TO_NOTVICT);
                } else if (IS_MAJIN(vict)) {
                    act("@RA wide hole is left in $S gooey flesh!@n", true, ch, nullptr, vict, TO_CHAR);
                    act("@RA wide hole is left is your gooey flesh!@n", true, ch, nullptr, vict, TO_VICT);
                    act("@RA wide hole is left in $N@R's gooey flesh@n", true, ch, nullptr, vict, TO_NOTVICT);
                } else {
                    act("@RBlood flies out from the impact!@n", true, ch, nullptr, vict, TO_CHAR);
                    act("@RBlood flies out from the impact!@n", true, ch, nullptr, vict, TO_VICT);
                    act("@RBlood flies out from the impact!@n", true, ch, nullptr, vict, TO_NOTVICT);
                }
                if (OBJ_FLAGGED(obj, ITEM_ICE)) {
                    if (!IS_ANDROID(vict) && !IS_ICER(vict)) {
                        vict->decCurST((vict->getMaxST() * .005) + GET_OBJ_WEIGHT(obj));
                        act("@mYou lose some stamina to the @ccold@m!@n", true, ch, nullptr, vict, TO_VICT);
                        act("@C$N@m loses some stamina to the @ccold@m!@n", true, ch, nullptr, vict, TO_CHAR);
                        act("@C$N@m loses some stamina to the @ccold@m!@n", true, ch, nullptr, vict, TO_NOTVICT);
                    }
                }
                damage *= calc_critical(ch, 0);
            }
            if (hot == true) {
                if (!IS_DEMON(vict) && !GET_BONUS(vict, BONUS_FIREPROOF)) {
                    act("@R$N@R is burned by it!@n", true, ch, nullptr, vict, TO_CHAR);
                    act("@RYou are burned by it!@n", true, ch, nullptr, vict, TO_VICT);
                    act("@R$N@R is burned by it!@n", true, ch, nullptr, vict, TO_NOTVICT);
                    vict->affected_by.set(AFF_BURNED);
                    damage += damage * 0.4;
                }
            }
            if (GET_PREFERENCE(ch) == PREFERENCE_KI) {
                damage -= damage * 0.20;
            }
            if (GET_OBJ_VNUM(obj) == 5899 || GET_OBJ_VNUM(obj) == 5898) {
                damage *= 0.35;
            }
            hurt(0, 0, ch, vict, nullptr, damage, 0);
            obj_from_char(obj);
            obj_to_room(obj, IN_ROOM(vict));

            ch->decCurST(((GET_MAX_HIT(ch) / 200) + GET_OBJ_WEIGHT(obj)));
            if (!GET_EQ(ch, WEAR_WIELD1) && !GET_EQ(ch, WEAR_WIELD2))
                perc += 12;
            if (perc + GET_CHA(ch) >= chance + penalty && multithrow == true && GET_HIT(vict) > 1 && ch->throws > 1) {
                do_throw(ch, argument, 0, 0);
                ch->throws -= 1;
            } else if (perc + GET_CHA(ch) >= chance + penalty && multithrow == true && GET_HIT(vict) > 1 &&
                       ch->throws == 1) {
                do_throw(ch, argument, 0, 0);
                ch->throws = -1;
            } else {
                ch->throws = 0;
            }
            WAIT_STATE(ch, PULSE_3SEC);
            return;
        }
    } /* End object section. */

    /* We are throwing a character at someone else. */
    if (tch) {
        if (tch == vict) {
            send_to_char(ch, "You can't throw someone at theirself.\r\n");
            return;
        }

        if (!can_kill(ch, tch, nullptr, 0)) {
            send_to_char(ch, "The one you are throwing can't be harmed.\r\n");
            return;
        }

        if (GET_SPEEDI(tch) < GET_SPEEDI(ch) && rand_number(1, 106) < GET_SKILL(ch, SKILL_THROW)) {
            grab = true;
        }

        if ((ch->getCurST()) < ((GET_MAX_HIT(ch) / 100) + tch->getTotalWeight())) {
            send_to_char(ch, "You do not have enough stamina to do it...\r\n");
            return;
        }
        if (!ch->canCarryWeight(tch)) {
            send_to_char(ch, "The gravity has made them too heavy for you to throw!\r\n");
            return;
        }
        if (grab == false) {
            act("@WYou try to grab @C$N@W and throw them, but they manage to dodge your attempt!@n", true, ch, nullptr,
                tch, TO_CHAR);
            act("@C$n@W tries to @RGRAB@W you and @RTHROW@W you, but you manage to dodge the attempt!@n", true, ch,
                nullptr, tch, TO_VICT);
            act("@C$n@W tries to @RGRAB@W @c$N@W and @RTHROW@W $M, but $E manages to dodge the attempt!@n", true, ch,
                nullptr, tch, TO_NOTVICT);
            hurt(0, 0, ch, tch, nullptr, 0, 0);
            handle_cooldown(ch, 5);
            ch->decCurST((GET_MAX_HIT(ch) / 200) + tch->getWeight());
            return;
        } else {
            handle_cooldown(ch, 5);
            improve_skill(ch, SKILL_THROW, 0);
            damage = ((tch->getWeight() * GET_STR(ch)) * (GET_CHA(ch) / 3)) + (GET_MAX_HIT(ch) / 100);
            damage += gravity * (gravity / 2);
            perc = init_skill(ch, SKILL_THROW);
            perc2 = init_skill(vict, SKILL_DODGE);
            prob = rand_number(1, 106);
            if (perc - (perc2 / 10) < prob) {
                if (perc2 > 0) {
                    act("@WYou grab @C$N@W and spinning around quickly you throw $M!@n", true, ch, nullptr, tch,
                        TO_CHAR);
                    act("@C$n@W grabs YOU and spinning around quickly $e throws you!@n", true, ch, nullptr, tch,
                        TO_VICT);
                    act("@C$n@W grabs @c$N@W and spinning around quickly $e throws $M!@n", true, ch, nullptr, tch,
                        TO_NOTVICT);
                    act("@WThrown through the air, YOU fly at @c$N@W, but $E manages to dodge and you manage recover your bearings a moment later!@n",
                        true, tch, nullptr, vict, TO_CHAR);
                    act("@WThrown through the air, @C$n@W flies at YOU, but you manage to dodge and @C$n@W recovers $s bearings a moment later!@n",
                        true, tch, nullptr, vict, TO_VICT);
                    act("@WThrown through the air, @C$n@W flies at @c$N@W, but $E manages to dodge and @C$n@W recovers $s bearingsa moment later!@n",
                        true, tch, nullptr, vict, TO_NOTVICT);
                } else if (perc2 <= 0) {
                    act("@WYou grab @C$N@W and spinning around quickly you throw $M!@n", true, ch, nullptr, tch,
                        TO_CHAR);
                    act("@C$n@W grabs YOU and spinning around quickly $e throws you!@n", true, ch, nullptr, tch,
                        TO_VICT);
                    act("@C$n@W grabs @c$N@W and spinning around quickly $e throws $M!@n", true, ch, nullptr, tch,
                        TO_NOTVICT);
                    act("@WThrown through the air, YOU fly at @c$N@W, but the throw is a miss! You manage recover your bearings a moment later!@n",
                        true, tch, nullptr, vict, TO_CHAR);
                    act("@WThrown through the air, @C$n@W flies at YOU, but the throw is a miss! @C$n@W recovers $s bearings a moment later!@n",
                        true, tch, nullptr, vict, TO_VICT);
                    act("@WThrown through the air, @C$n@W flies at @c$N@W, but the throw is a miss! @C$n@W recovers $s bearingsa moment later!@n",
                        true, tch, nullptr, vict, TO_NOTVICT);
                }
                ch->decCurST(((GET_MAX_HIT(ch) / 100) + tch->getWeight()));
                act("@W--@R$N@W--@n", true, ch, nullptr, vict, TO_CHAR);
                act("@W--@R$N@W--@n", true, tch, nullptr, vict, TO_CHAR);
                act("@W--@RYOU@W--@n", true, vict, nullptr, nullptr, TO_CHAR);
                hurt(0, 0, ch, vict, nullptr, 0, 0);
                act("@W--@R$N@W--@n", true, ch, nullptr, tch, TO_CHAR);
                act("@W--@R$N@W--@n", true, vict, nullptr, tch, TO_CHAR);
                act("@W--@RYOU@W--@n", true, tch, nullptr, nullptr, TO_CHAR);
                hurt(0, 0, ch, tch, nullptr, 0, 0);
                return;
            } else if (perc - (perc2 / 10) >= prob) {
                miss = false;
            }
            if (miss == false) {
                act("@WYou grab @C$N@W and spinning around quickly you throw $M!@n", true, ch, nullptr, tch, TO_CHAR);
                act("@C$n@W grabs YOU and spinning around quickly $e throws you!@n", true, ch, nullptr, tch, TO_VICT);
                act("@C$n@W grabs @c$N@W and spinning around quickly $e throws $M!@n", true, ch, nullptr, tch,
                    TO_NOTVICT);
                act("@WThrown through the air, YOU fly at @c$N@W and smash into $M!@n", true, tch, nullptr, vict,
                    TO_CHAR);
                act("@WThrown through the air, @C$n@W flies at YOU and smashes into YOU!@n", true, tch, nullptr, vict,
                    TO_VICT);
                act("@WThrown through the air, @C$n@W flies at @c$N@W and smashes into $M!@n", true, tch, nullptr, vict,
                    TO_NOTVICT);
                act("@W--@R$N@W--@n", true, ch, nullptr, vict, TO_CHAR);
                act("@W--@R$N@W--@n", true, tch, nullptr, vict, TO_CHAR);
                act("@W--@RYOU@W--@n", true, vict, nullptr, nullptr, TO_CHAR);
                hurt(0, 0, ch, vict, nullptr, damage, 0);
                act("@W--@R$N@W--@n", true, ch, nullptr, tch, TO_CHAR);
                if (vict) {
                    act("@W--@R$N@W--@n", true, vict, nullptr, tch, TO_CHAR);
                }
                act("@W--@RYOU@W--@n", true, tch, nullptr, nullptr, TO_CHAR);
                hurt(0, 0, ch, tch, nullptr, damage, 0);
            }
            ch->decCurST(((GET_MAX_HIT(ch) / 200) + tch->getWeight()));
            WAIT_STATE(ch, PULSE_3SEC);
        }
    } /* End throwing character. */

    /* Whoops!? */
    if (!obj && !tch) {
        send_to_imm("ERROR: Throw resolved without character or object.");
        return;
    }
}

ACMD(do_selfd) {

    struct char_data *tch = nullptr, *next_v = nullptr;
    int64_t dmg = 0;

    if (IS_NPC(ch))
        return;

    if (IN_ARENA(ch)) {
        send_to_char(ch, "You can not use self destruct in the arena.\r\n");
        return;
    }

    if (AFF_FLAGGED(ch, AFF_SPIRIT)) {
        send_to_char(ch, "You are already dead!\r\n");
        return;
    }

    if (GET_MAX_HIT(ch) <= 10000) {
        send_to_char(ch, "You can't self destruct while protected by the newbie shield!\r\n");
        return;
    }

    /*Andros Start*/
    if (!GET_SDCOOLDOWN(ch) <= 0) {
        send_to_char(ch, "Your body is still recovering from the last self destruct!\r\n");
        return;
    } /*Andros End*/


    if (!GET_SKILL(ch, SKILL_SELFD)) {
        int num = rand_number(10, 20);
        SET_SKILL(ch, SKILL_SELFD, num);
    }

    if (!PLR_FLAGGED(ch, PLR_SELFD)) {
        act("@RYour body starts to glow @wwhite@R and flash. The flashes start out slowly but steadilly increase in speed. Your aura begins to burn around your body at the same time in a violent fashion!@n",
            true, ch, nullptr, nullptr, TO_CHAR);
        act("@R$n's body starts to glow @wwhite@R and flash. The flashes start out slowly but steadilly increase in speed. $n's aura begins to burn around $s body at the same time in a violent fashion!@n",
            true, ch, nullptr, nullptr, TO_ROOM);
        ch->playerFlags.set(PLR_SELFD);
        return;
    } else if (!PLR_FLAGGED(ch, PLR_SELFD2)) {
        act("@wYour body slowly stops flashing. Steam rises from your skin as you slowly let off the energy you built up in a safe manner.@n",
            true, ch, nullptr, nullptr, TO_CHAR);
        act("@w$n's body slowly stops flashing. Steam rises from $s skin as $e slowly lets off the energy $e built up in a safe manner.@n",
            true, ch, nullptr, nullptr, TO_ROOM);
        ch->playerFlags.reset(PLR_SELFD);
        return;
    } else if (GRAPPLING(ch) != nullptr && !can_kill(ch, GRAPPLING(ch), nullptr, 3)) {
        act("@wYour body slowly stops flashing. Steam rises from your skin as you slowly let off the energy you built up in a safe manner.@n",
            true, ch, nullptr, nullptr, TO_CHAR);
        act("@w$n's body slowly stops flashing. Steam rises from $s skin as $e slowly lets off the energy $e built up in a safe manner.@n",
            true, ch, nullptr, nullptr, TO_ROOM);
        send_to_char(ch, "You can't kill them, the immortals won't allow it!\r\n");
        ch->playerFlags.reset(PLR_SELFD);
        return;
    } else if (GRAPPLING(ch) != nullptr) {
        tch = GRAPPLING(ch);
        dmg += GET_CHARGE(ch);
        GET_CHARGE(ch) = 0;
        dmg += (ch->getBasePL()) * 0.6;
        dmg += (ch->getBaseST());
        ch->decCurHealthPercent(1, 1);
        GET_SUPPRESS(ch) = 0;
        act("@RYou EXPLODE! The explosion concentrates on @r$N@R, engulfing $M in a sphere of deadly energy!@n", true,
            ch, nullptr, tch, TO_CHAR);
        act("@R$n EXPLODES! The explosion concentrates on YOU, engulfing your body in a sphere of deadly energy!@n",
            true, ch, nullptr, tch, TO_VICT);
        act("@R$n EXPLODES! The explosion concentrates on @r$N@R, engulfing $M in a sphere of deadly energy!@n", true,
            ch, nullptr, tch, TO_NOTVICT);
        hurt(0, 0, ch, tch, nullptr, dmg, 1);
        ch->playerFlags.reset(PLR_SELFD);
        ch->playerFlags.reset(PLR_SELFD2);
        if (PLR_FLAGGED(ch, PLR_IMMORTAL)) {
            GET_SDCOOLDOWN(ch) = 600;
        }
        if ((IS_MAJIN(ch) || IS_BIO(ch)) && ch->getCurLFPercent() > 0.5) {
            ch->decCurLFPercent(2, -1);
            ch->playerFlags.set(PLR_GOOP);
            ch->gooptime = 70;
        } else {
            die(ch, nullptr);
        }
        int num = rand_number(10, 20) + GET_SKILL(ch, SKILL_SELFD);
        if (GET_SKILL(ch, SKILL_SELFD) + num <= 100) {
            SET_SKILL(ch, SKILL_SELFD, num);
        } else {
            SET_SKILL(ch, SKILL_SELFD, 100);
        }
        return;
    } else {
        dmg += GET_CHARGE(ch);
        GET_CHARGE(ch) = 0;
        dmg += (ch->getBasePL()) * 0.6;
        dmg += (ch->getBaseST());
        dmg *= 1.5;
        ch->decCurHealthPercent(1, 1);
        GET_SUPPRESS(ch) = 0;
        act("@RYou EXPLODE! The explosion expands outward burning up all surroundings for a large distance. The explosion takes on the shape of a large energy dome with you at its center!@n",
            true, ch, nullptr, nullptr, TO_CHAR);
        act("@R$n EXPLODES! The explosion expands outward burning up all surroundings for a large distance. The explosion takes on the shape of a large energy dome with $n at its center!@n",
            true, ch, nullptr, nullptr, TO_ROOM);
        for (tch = ch->getRoom()->people; tch; tch = next_v) {
            next_v = tch->next_in_room;
            if (tch == ch) {
                continue;
            }
            if (!can_kill(ch, tch, nullptr, 3)) {
                continue;
            }
            if (MOB_FLAGGED(tch, MOB_NOKILL)) {
                continue;
            } else {
                act("@r$N@R is caught in the explosion!@n", true, ch, nullptr, tch, TO_CHAR);
                act("@RYou are caught in the explosion!@n", true, ch, nullptr, tch, TO_VICT);
                act("@r$N@R is caught in the explosion!@n", true, ch, nullptr, tch, TO_NOTVICT);
                hurt(0, 0, ch, tch, nullptr, dmg, 1);
            }
        }
        if (PLR_FLAGGED(ch, PLR_IMMORTAL)) {
            GET_SDCOOLDOWN(ch) = 600;
        }
        die(ch, nullptr);
        ch->playerFlags.reset(PLR_SELFD);
        ch->playerFlags.reset(PLR_SELFD2);
        int num = rand_number(10, 20) + GET_SKILL(ch, SKILL_SELFD);
        if (GET_SKILL(ch, SKILL_SELFD) + num <= 100) {
            SET_SKILL(ch, SKILL_SELFD, num);
        } else {
            SET_SKILL(ch, SKILL_SELFD, 100);
        }
        return;
    }
}

ACMD(do_razor) {
    atk::WaterRazor a(ch, argument);
    a.execute();
}

ACMD(do_spike) {
    atk::WaterSpike a(ch, argument);
    a.execute();
}

ACMD(do_koteiru) {
    atk::KoteiruBakuha a(ch, argument);
    a.execute();
}

ACMD(do_hspiral) {
    atk::HellSpiral a(ch, argument);
    a.execute();
}

ACMD(do_spiral) {
    int skill;
    struct char_data *vict;
    char arg[MAX_INPUT_LENGTH];

    one_argument(argument, arg);

    /* Can they do the technique? */

    if (!can_grav(ch)) {
        return;
    }

    if (!check_skill(ch, SKILL_SPIRAL)) {
        return;
    }

    if (!limb_ok(ch, 0)) {
        return;
    }

    if (!*arg && !FIGHTING(ch)) {
        send_to_char(ch, "Direct it at who?\r\n");
        return;
    }

    if (!check_points(ch, GET_MAX_MANA(ch) * .5, 0)) {
        return;
    }

    /* Passed sanity checks for doing the technique */

    skill = init_skill(ch, SKILL_SPIRAL); /* Set skill value */

    if (!(vict = get_char_vis(ch, arg, nullptr, FIND_CHAR_ROOM))) {
        if (FIGHTING(ch)) {
            vict = FIGHTING(ch);
        } else {
            send_to_char(ch, "Nothing around here by that name.");
            return;
        }
    }

    /* There is a player/mob targeted */
    if (!can_kill(ch, vict, nullptr, 3)) {
        return;
    }

    if (handle_defender(vict, ch)) {
        struct char_data *def = GET_DEFENDER(vict);
        vict = def;
    }

    ch->playerFlags.set(PLR_SPIRAL);
    improve_skill(ch, SKILL_SPIRAL, 0);
    act("@mFlying to a spot above your intended target you begin to move so fast all that can be seen of you are trails of color. You focus your movements into a vortex and prepare to attack!@n",
        true, ch, nullptr, nullptr, TO_CHAR);
    act("@w$n@m flies to a spot above and begins to move so fast all that can be seen of $m are trails of color. Suddenly $e focuses $s movements into a spinning vortex and you lose track of $s movements entirely!@n",
        true, ch, nullptr, nullptr, TO_ROOM);
    handle_spiral(ch, vict, skill, true);
    handle_cooldown(ch, 8);
}

ACMD(do_breaker) {
    atk::StarBreaker a(ch, argument);
    a.execute();
}
