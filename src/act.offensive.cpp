/* ************************************************************************
*   File: act.offensive.c                               Part of CircleMUD *
*  Usage: player-level commands of an offensive nature                    *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "dbat/act.offensive.h"
#include "dbat/act.attack.h"
#include "dbat/interpreter.h"
#include "dbat/combat.h"
#include "dbat/comm.h"
#include "dbat/spells.h"
#include "dbat/utils.h"
#include "dbat/handler.h"
#include "dbat/constants.h"
#include "dbat/fight.h"
#include "dbat/guild.h"
#include "dbat/class.h"
#include "dbat/techniques.h"
#include "dbat/attack.h"

/* Combat commands below this line */

ACMD(do_galikgun) {
    atk::GalikGun a(ch, argument);
    a.execute();
}

ACMD(do_honoo) {
    atk::Honoo a(ch, argument);
    a.execute();
}

ACMD(do_psyblast) {
    atk::PsychicBlast a(ch, argument);
    a.execute();
}

ACMD(do_tslash) {
    atk::TwinSlash a(ch, argument);
    a.execute();
}

ACMD(do_eraser) {
    atk::EraserCannon a(ch, argument);
    a.execute();
}

ACMD(do_pbarrage) {
    atk::PsychicBarrage a(ch, argument);
    a.execute();
}

ACMD(do_combo) {
    switch (COMBO(ch)) {
        case 0:
            do_punch(ch, argument, 0, 0);
            break;
        case 1:
            do_kick(ch, argument, 0, 0);
            break;
        case 2:
            do_elbow(ch, argument, 0, 0);
            break;
        case 3:
            do_knee(ch, argument, 0, 0);
            break;
        case 4:
            do_roundhouse(ch, argument, 0, 0);
            break;
        case 5:
            do_uppercut(ch, argument, 0, 0);
            break;
        case 6:
            do_slam(ch, argument, 0, 0);
            break;
        case 8:
            do_heeldrop(ch, argument, 0, 0);
            break;
        case 51:
            do_bash(ch, argument, 0, 0);
            break;
        case 52:
            do_head(ch, argument, 0, 0);
            break;
        case 56:
            do_tailwhip(ch, argument, 0, 0);
            break;
        default:
            do_punch(ch, argument, 0, 0);
            break;

    }
    
}

ACMD(do_geno) {

    int perc, prob;
    double attperc = 0.5, minimum = .4;
    struct char_data *vict = nullptr;
    char arg[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];

    two_arguments(argument, arg, arg2);
    /* Can they do the technique? */

    if (!can_grav(ch)) {
        return;
    }

    if (!check_skill(ch, SKILL_GENOCIDE)) {
        return;
    }

    if (!limb_ok(ch, 0)) {
        return;
    }

    if (!*arg && !FIGHTING(ch)) {
        send_to_char(ch, "Direct it at who?\r\n");
        return;
    }

    if (!tech_handle_charge(ch, arg2, minimum, &attperc)) return;

    if (GET_MAX_MANA(ch) * attperc > GET_CHARGE(ch)) {
        attperc = (long double) (GET_CHARGE(ch)) / (long double) (GET_MAX_MANA(ch));
    }

    if (!check_points(ch, GET_MAX_MANA(ch) * minimum, 0)) {
        return;
    }

    vict = nullptr;
    if (!*arg || !(vict = get_char_vis(ch, arg, nullptr, FIND_CHAR_ROOM))) {
        if (FIGHTING(ch) && IN_ROOM(FIGHTING(ch)) == IN_ROOM(ch)) {
            vict = FIGHTING(ch);
        } else {
            send_to_char(ch, "No one around here by that name.\r\n");
            return;
        }
    }

    if (!can_kill(ch, vict, nullptr, 3)) {
        return;
    }

    if (handle_defender(vict, ch)) {
        struct char_data *def = GET_DEFENDER(vict);
        vict = def;
    }

    prob = init_skill(ch, SKILL_GENOCIDE); /* Set skill value */
    perc = rand_number(1, 115);

    if (prob < perc - 20) {
        act("@WYou raise one arm above your head and pour your charged ki there. A large swirling pink ball of energy begins to form above your raised hand. You lose concentration and the ball of energy dissipates!@n",
            true, ch, nullptr, nullptr, TO_CHAR);
        act("@C$n@W raises one arm above $s head and pours $s charged ki there. A large swirling pink ball of energy begins to form above $s raised hand. @C$n@W loses concentration and the ball of energy dissipates!@n",
            true, ch, nullptr, nullptr, TO_ROOM);
        hurt(0, 0, ch, vict, nullptr, 0, 1);
        pcost(ch, attperc, 0);

        improve_skill(ch, SKILL_GENOCIDE, 2);
        return;
    }

    struct obj_data *obj;
    int dista = 15 - (GET_INT(ch) * 0.1);

    if (GET_SKILL(ch, SKILL_GENOCIDE) >= 100) {
        dista -= 3;
    } else if (GET_SKILL(ch, SKILL_GENOCIDE) >= 60) {
        dista -= 2;
    } else if (GET_SKILL(ch, SKILL_GENOCIDE) >= 40) {
        dista -= 1;
    }

    obj = read_object(83, VIRTUAL);
    obj_to_room(obj, IN_ROOM(vict));

    GET_CHARGE(ch) += GET_MAX_HIT(ch) / 10;
    TARGET(obj) = vict;
    KICHARGE(obj) = damtype(ch, 41, prob, attperc);
    KITYPE(obj) = SKILL_GENOCIDE;
    USER(obj) = ch;
    KIDIST(obj) = dista;
    pcost(ch, attperc, 0);
    act("@WYou raise one arm above your head and pour your charged ki there. A large swirling pink ball of energy begins to form above your raised hand. You grin viciously as the @mG@Me@wn@mo@Mc@wi@md@Me@W attack is complete and you toss it at @c$N@W!@n",
        true, ch, nullptr, vict, TO_CHAR);
    act("@C$n@W raises one arm above $s head and pours $s charged ki there. A large swirling pink ball of energy begins to form above $s raised hand. @C$n@W grins viciously as the @mG@Me@wn@mo@Mc@wi@md@Me@W attack is complete and $e tosses it at YOU!@n",
        true, ch, nullptr, vict, TO_VICT);
    act("@C$n@W raises one arm above $s head and pours $s charged ki there. A large swirling pink ball of energy begins to form above $s raised hand. @C$n@W grins viciously as the @mG@Me@wn@mo@Mc@wi@md@Me@W attack is complete and $e tosses it at @c$N@W!@n",
        true, ch, nullptr, vict, TO_NOTVICT);

    improve_skill(ch, SKILL_GENOCIDE, 2);
}

ACMD(do_genki) {

    int perc, prob;
    double attperc = .5, minimum = .4;
    struct char_data *friend_char = nullptr, *vict = nullptr, *next_v = nullptr;
    char arg[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];

    two_arguments(argument, arg, arg2);
    /* Can they do the technique? */

    if (!can_grav(ch)) {
        return;
    }

    if (!check_skill(ch, SKILL_GENKIDAMA)) {
        return;
    }

    if (!limb_ok(ch, 0)) {
        return;
    }

    if (!*arg && !FIGHTING(ch)) {
        send_to_char(ch, "Direct it at who?\r\n");
        return;
    }

    if (!tech_handle_charge(ch, arg2, minimum, &attperc)) return;

    if (GET_MAX_MANA(ch) * attperc > GET_CHARGE(ch)) {
        attperc = (long double) (GET_CHARGE(ch)) / (long double) (GET_MAX_MANA(ch));
    }

    if (!check_points(ch, GET_MAX_MANA(ch) * minimum, 0)) {
        return;
    }

    vict = nullptr;
    if (!*arg || !(vict = get_char_vis(ch, arg, nullptr, FIND_CHAR_ROOM))) {
        if (FIGHTING(ch) && IN_ROOM(FIGHTING(ch)) == IN_ROOM(ch)) {
            vict = FIGHTING(ch);
        } else {
            send_to_char(ch, "No one around here by that name.\r\n");
            return;
        }
    }

    if (!can_kill(ch, vict, nullptr, 3)) {
        return;
    }

    if (handle_defender(vict, ch)) {
        struct char_data *def = GET_DEFENDER(vict);
        vict = def;
    }

    prob = init_skill(ch, SKILL_GENKIDAMA); /* Set skill value */
    perc = rand_number(1, 115);

    if (prob < perc - 20) {
        act("@WYou raise both your arms upwards and begin to pool your charged ki there. You also start calling on the ki of all living beings in the vicinity who are willing to help. Your concentration wavers though and you waste the energy you have!@n",
            true, ch, nullptr, nullptr, TO_CHAR);
        act("@C$n@W raises both $s arms upwards and begin to pool $s charged ki there. @C$n@W also starts calling on the ki of all living beings in the vicinity who are willing to help. @C$n@W's concentration wavers though and $e wastes the energy $e has!@n",
            true, ch, nullptr, nullptr, TO_ROOM);
        hurt(0, 0, ch, vict, nullptr, 0, 1);
        pcost(ch, attperc, 0);
        handle_cooldown(ch, 10);
        improve_skill(ch, SKILL_GENKIDAMA, 2);
        return;
    }

    for (friend_char = ch->getRoom()->people; friend_char; friend_char = next_v) {
        next_v = friend_char->next_in_room;
        if (friend_char == ch) {
            continue;
        }
        if (AFF_FLAGGED(friend_char, AFF_GROUP) &&
            (friend_char->master == ch || ch->master == friend_char || friend_char->master == ch->master)) {
            GET_CHARGE(ch) += (ch->getCurKI()) / 10;
            ch->decCurKI(ch->getCurKI() / 20);
        }
    }

    int dista = 15 - (GET_INT(ch) * 0.1);

    if (GET_SKILL(ch, SKILL_GENKIDAMA) >= 100) {
        dista -= 3;
    } else if (GET_SKILL(ch, SKILL_GENKIDAMA) >= 60) {
        dista -= 2;
    } else if (GET_SKILL(ch, SKILL_GENKIDAMA) >= 40) {
        dista -= 1;
    }

    struct obj_data *obj;

    obj = read_object(82, VIRTUAL);
    obj_to_room(obj, IN_ROOM(vict));

    TARGET(obj) = vict;
    KICHARGE(obj) = damtype(ch, 40, prob, attperc);
    KITYPE(obj) = SKILL_GENKIDAMA;
    USER(obj) = ch;
    KIDIST(obj) = dista;
    pcost(ch, attperc, 0);
    act("@WYou raise both your arms upwards and begin to pool your charged ki there. You also start calling on the ki of all living beings in the vicinity who are willing to help. A large @cS@Cp@wi@cr@Ci@wt @cB@Co@wm@cb@W forms above your hands, when it is finished you lob it toward @c$N@W!@n",
        true, ch, nullptr, vict, TO_CHAR);
    act("@C$n@W raises both $s arms upwards and begin to pool $s charged ki there. @C$n@W also starts calling on the ki of all living beings in the vicinity who are willing to help. A large @cS@Cp@wi@cr@Ci@wt @cB@Co@wm@cb@W forms above $s hands, when it is finished $e lobs it toward YOU!@n",
        true, ch, nullptr, vict, TO_VICT);
    act("@C$n@W raises both $s arms upwards and begin to pool $s charged ki there. @C$n@W also starts calling on the ki of all living beings in the vicinity who are willing to help. A large @cS@Cp@wi@cr@Ci@wt @cB@Co@wm@cb@W forms above $s hands, when it is finished $e lobs it toward @c$N@W!@n",
        true, ch, nullptr, vict, TO_NOTVICT);
    handle_cooldown(ch, 10);
    improve_skill(ch, SKILL_GENKIDAMA, 2);
}

ACMD(do_spiritball) {
    atk::Spiritball a(ch, argument);
    a.execute();
}

ACMD(do_deathball) {
    atk::Deathball a(ch, argument);
    a.execute();
}

ACMD(do_pslash) {
    atk::PhoenixSlash a(ch, argument);
    a.execute();
}

ACMD(do_bigbang) {
    atk::BigBang a(ch, argument);
    a.execute();
}

ACMD(do_scatter) {
    atk::ScatterShot a(ch, argument);
    a.execute();
}

ACMD(do_balefire) {
    atk::Balefire a(ch, argument);
    a.execute();
}
/* Rillao: End Balefire */

ACMD(do_kakusanha) {
    atk::Kakusanha a(ch, argument);
    a.execute();

}

ACMD(do_hellspear) {
    atk::Hellspear a(ch, argument);
    a.execute();
}

ACMD(do_hellflash) {
    atk::Hellflash a(ch, argument);
    a.execute();
}

ACMD(do_ddslash) {
    atk::DarknessDragonSlash a(ch, argument);
    a.execute();
}

ACMD(do_crusher) {
    atk::CrusherBall a(ch, argument);
    a.execute();
}

ACMD(do_final) {
    atk::FinalFlash a(ch, argument);
    a.execute();
}

ACMD(do_sbc) {
    atk::SpecialBeamCannon a(ch, argument);
    a.execute();
}

ACMD(do_tribeam) {
    atk::Tribeam a(ch, argument);
    a.execute();
}

ACMD(do_kienzan) {
    atk::Kienzan a(ch, argument);
    a.execute();
}

ACMD(do_baku) {
    atk::Bakuhatsuha a(ch, argument);
    a.execute();
}

ACMD(do_rogafufuken) {
    atk::Rogafufuken a(ch, argument);
    a.execute();
}

ACMD(do_dualbeam) {
    atk::DualBeam a(ch, argument);
    a.execute();
}

/* Chimera */
ACMD(do_blessedhammer) {
    int prob, perc, avo, index, pry = 2, dge = 2, blk = 2, skill;
    int64_t dmg;
    double attperc = .05, minimum = .01;
    struct char_data *vict;
    struct obj_data *obj;
    char arg[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];

    two_arguments(argument, arg, arg2);

    /* Can they do the technique? */

    if (!can_grav(ch)) {
        return;
    }

    if (!check_skill(ch, SKILL_BLESSEDHAMMER)) {
        return;
    }

    if (!*arg && !FIGHTING(ch)) {
        send_to_char(ch, "Direct it at who?\r\n");
        return;
    }

    if (!tech_handle_charge(ch, arg2, minimum, &attperc)) return;

    if (GET_MAX_MANA(ch) * attperc > GET_CHARGE(ch)) {
        attperc = (long double) (GET_CHARGE(ch)) / (long double) (GET_MAX_MANA(ch));
    }

    if (!check_points(ch, GET_MAX_MANA(ch) * minimum, 0)) {
        return;
    }

    /* Passed sanity checks for doing the technique */

    skill = init_skill(ch, SKILL_BLESSEDHAMMER); /* Set skill value */

    if (!tech_handle_targeting(ch, arg, &vict, &obj)) return;

    /* There is a player/mob targeted */
    handle_cooldown(ch, 5);
    if (vict) {
        if (!can_kill(ch, vict, nullptr, 1)) {
            return;
        }
        if (handle_defender(vict, ch)) {
            struct char_data *def = GET_DEFENDER(vict);
            vict = def;
        }
        improve_skill(ch, SKILL_BLESSEDHAMMER, 0);

        index = check_def(vict); /* Check parry/block/dodge of vict */

        prob = roll_accuracy(ch, skill, true);
        perc = chance_to_hit(ch);

        index -= handle_speed(ch, vict);

        avo = index / 4;

        handle_defense(vict, &pry, &blk, &dge);

        prob += 15; /* kousengan bonus */

        prob -= avo;
        tech_handle_posmodifier(vict, pry, blk, dge, prob);

        if (!tech_handle_zanzoken(ch, vict, "@WB@Dl@We@Ds@Ws@De@Wd @DH@Wa@Dm@Wm@De@Wr@C")) {
            pcost(ch, attperc, 0);
            pcost(vict, 0, GET_MAX_HIT(vict) / 200);
            return;
        }

        if (prob < perc - 20) {
            if ((vict->getCurST()) > 0) {
                if (blk > axion_dice(10)) {
                    act("@C$N@W moves quickly and blocks your @WB@Dl@We@Ds@Ws@De@Wd @DH@Wa@Dm@Wm@De@Wr@n!@n", false, ch,
                        nullptr, vict, TO_CHAR);
                    act("@WYou move quickly and block @C$n's@W @WB@Dl@We@Ds@Ws@De@Wd @DH@Wa@Dm@Wm@De@Wr@n!@n", false,
                        ch, nullptr, vict, TO_VICT);
                    act("@C$N@W moves quickly and blocks @c$n's@W @WB@Dl@We@Ds@Ws@De@Wd @DH@Wa@Dm@Wm@De@Wr@n!@n", false,
                        ch, nullptr, vict, TO_NOTVICT);
                    pcost(ch, attperc, 0);
                    pcost(vict, 0, GET_MAX_HIT(vict) / 500);
                    dmg = damtype(ch, 42, skill, attperc);
                    dmg /= 4;
                    hurt(0, 0, ch, vict, nullptr, dmg, 1);

                    return;
                } else if (dge > axion_dice(10)) {
                    act("@C$N@W manages to dodge your @WB@Dl@We@Ds@Ws@De@Wd @DH@Wa@Dm@Wm@De@Wr@W, letting it slam into the surroundings!@n",
                        false, ch, nullptr, vict, TO_CHAR);
                    act("@WYou dodge @C$n's@W @WB@Dl@We@Ds@Ws@De@Wd @DH@Wa@Dm@Wm@De@Wr@W, letting it slam into the surroundings!@n",
                        false, ch, nullptr, vict, TO_VICT);
                    act("@C$N@W manages to dodge @c$n's@W @WB@Dl@We@Ds@Ws@De@Wd @DH@Wa@Dm@Wm@De@Wr@W, letting it slam into the surroundings!@n",
                        false, ch, nullptr, vict, TO_NOTVICT);
                    send_to_room(IN_ROOM(vict), "@wA bright explosion erupts from the impact!\r\n");

                    dodge_ki(ch, vict, 0, 17, skill, SKILL_BLESSEDHAMMER); /* Effects on the room from dodging a ki attack
                               Num 1: [ 0 for non-homing, 1 for homing ki attacks, 2 for guided ]
                               Num 2: [ Number of attack for damtype ]*/

                    ch->getRoom()->modDamage(5);
                    improve_skill(vict, SKILL_DODGE, 0);

                    pcost(ch, attperc, 0);
                    hurt(0, 0, ch, vict, nullptr, 0, 1);

                    return;
                } else {
                    act("@WYou can't believe it but your @WB@Dl@We@Ds@Ws@De@Wd @DH@Wa@Dm@Wm@De@Wr@W misses, flying through the air harmlessly!@n",
                        false, ch, nullptr, vict, TO_CHAR);
                    act("@C$n@W fires a @WB@Dl@We@Ds@Ws@De@Wd @DH@Wa@Dm@Wm@De@Wr@W at you, but misses!@n ", false, ch,
                        nullptr, vict, TO_VICT);
                    act("@c$n@W fires a @WB@Dl@We@Ds@Ws@De@Wd @DH@Wa@Dm@Wm@De@Wr@W at @C$N@W, but somehow misses!@n ",
                        false, ch, nullptr, vict, TO_NOTVICT);

                    pcost(ch, attperc, 0);
                    hurt(0, 0, ch, vict, nullptr, 0, 1);

                    return;
                }
            } else {
                act("@WYou can't believe it but your @WB@Dl@We@Ds@Ws@De@Wd @DH@Wa@Dm@Wm@De@Wr@W misses, flying through the air harmlessly!@n",
                    false, ch, nullptr, vict, TO_CHAR);
                act("@C$n@W fires a @WB@Dl@We@Ds@Ws@De@Wd @DH@Wa@Dm@Wm@De@Wr@W at you, but misses!@n", false, ch,
                    nullptr,
                    vict, TO_VICT);
                act("@c$n@W fires a @WB@Dl@We@Ds@Ws@De@Wd @DH@Wa@Dm@Wm@De@Wr@W at @C$N@W, but somehow misses!@n", false,
                    ch, nullptr, vict, TO_NOTVICT);
                pcost(ch, attperc, 0);

            }
            hurt(0, 0, ch, vict, nullptr, 0, 1);
            return;
        } else {
            dmg = damtype(ch, 42, skill, attperc);
            if (AFF_FLAGGED(vict, AFF_SANCTUARY)) {
                dmg *= calc_critical(ch, 1);
            }
            int hitspot = 1;
            hitspot = roll_hitloc(ch, vict, skill);
            switch (hitspot) {
                case 1:
                    act("@WFocusing your attention on @R$N@W, you reach back and form an @ne@Dt@nh@De@nr@De@na@Dl @Whammer before hurling it with all your might into their chest!@n",
                        true, ch, nullptr, vict, TO_CHAR);
                    act("@C$n@W narrows $s eyes and focuses $s energy. They reach back to form an @ne@Dt@nh@De@nr@De@na@Dl @Whammer which they hurl at you with all their @Rmight@W into YOUR chest!@n",
                        true, ch, nullptr, vict, TO_VICT);
                    act("@C$n@W narrows their eyes at @c$N@W. $n reaches back to form an @ne@Dt@nh@De@nr@De@na@Dl @Whammer which they hurl at @c$N@W with all their might into @c$N@W's chest!@n",
                        true, ch, nullptr, vict, TO_NOTVICT);
                    if (GET_BONUS(ch, BONUS_SOFT)) {
                        dmg *= calc_critical(ch, 2);
                    }
                    hurt(0, 0, ch, vict, nullptr, dmg, 1);
                    dam_eq_loc(vict, 4);
                    /* dam_eq_loc: 1 Arms, 2 legs, 3 head, and 4 body. */
                    break;
                case 2: /* Critical */
                    if (!GET_BONUS(ch, BONUS_POWERHIT)) {
                        dmg *= 3;
                    } else {
                        dmg *= 5;
                    }
                    act("@WFocusing your attention on @R$N@W, you reach back and form an @ne@Dt@nh@De@nr@De@na@Dl @Whammer before hurling it with all your might into their face!@n",
                        true, ch, nullptr, vict, TO_CHAR);
                    act("@C$n@W narrows $s eyes and focuses $s energy. They reach back to form an @ne@Dt@nh@De@nr@De@na@Dl @Whammer which they hurl at you with all their @Rmight@W into YOUR face!@n",
                        true, ch, nullptr, vict, TO_VICT);
                    act("@C$n@W narrows their eyes at @c$N@W. $n reaches back to form an @ne@Dt@nh@De@nr@De@na@Dl @Whammer which they hurl at @c$N@W with all their might into @c$N@W's face!@n",
                        true, ch, nullptr, vict, TO_NOTVICT);
                    hurt(0, 0, ch, vict, nullptr, dmg, 1);
                    dam_eq_loc(vict, 3);
                    /* dam_eq_loc: 1 Arms, 2 legs, 3 head, and 4 body. */
                    break;
                case 3:
                    act("@WFocusing your attention on @R$N@W, you reach back and form an @ne@Dt@nh@De@nr@De@na@Dl @Whammer before hurling it with all your might into their gut!@n",
                        true, ch, nullptr, vict, TO_CHAR);
                    act("@C$n@W narrows $s eyes and focuses $s energy. They reach back to form an @ne@Dt@nh@De@nr@De@na@Dl @Whammer which they hurl at you with all their @Rmight@W into YOUR gut!@n",
                        true, ch, nullptr, vict, TO_VICT);
                    act("@C$n@W narrows their eyes at @c$N@W. $n reaches back to form an @ne@Dt@nh@De@nr@De@na@Dl @Whammer which they hurl at @c$N@W with all their might into @c$N@W's gut!@n",
                        true, ch, nullptr, vict, TO_NOTVICT);
                    if (GET_BONUS(ch, BONUS_SOFT)) {
                        dmg *= calc_critical(ch, 2);
                    }
                    hurt(0, 0, ch, vict, nullptr, dmg, 1);
                    dam_eq_loc(vict, 4);
                    /* dam_eq_loc: 1 Arms, 2 legs, 3 head, and 4 body. */
                    break;
                case 4: /* Weak */
                    act("@WFocusing your attention on @R$N@W, you reach back and form an @ne@Dt@nh@De@nr@De@na@Dl @Whammer before hurling it with all your might into their arm!@n",
                        true, ch, nullptr, vict, TO_CHAR);
                    act("@C$n@W narrows $s eyes and focuses $s energy. They reach back to form an @ne@Dt@nh@De@nr@De@na@Dl @Whammer which they hurl at you with all their @Rmight@W into YOUR arm!@n",
                        true, ch, nullptr, vict, TO_VICT);
                    act("@C$n@W narrows their eyes at @c$N@W. $n reaches back to form an @ne@Dt@nh@De@nr@De@na@Dl @Whammer which they hurl at @c$N@W with all their might into @c$N@W's arm!@n",
                        true, ch, nullptr, vict, TO_NOTVICT);
                    dmg *= calc_critical(ch, 1);
                    hurt(0, 190, ch, vict, nullptr, dmg, 1);
                    dam_eq_loc(vict, 1);
                    /* dam_eq_loc: 1 Arms, 2 legs, 3 head, and 4 body. */
                    break;
                case 5: /* Weak 2 */
                    act("@WFocusing your attention on @R$N@W, you reach back and form an @ne@Dt@nh@De@nr@De@na@Dl @Whammer before hurling it with all your might into their leg!@n",
                        true, ch, nullptr, vict, TO_CHAR);
                    act("@C$n@W narrows $s eyes and focuses $s energy. They reach back to form an @ne@Dt@nh@De@nr@De@na@Dl @Whammer which they hurl at you with all their @Rmight@W into YOUR leg!@n",
                        true, ch, nullptr, vict, TO_VICT);
                    act("@C$n@W narrows their eyes at @c$N@W. $n reaches back to form an @ne@Dt@nh@De@nr@De@na@Dl @Whammer which they hurl at @c$N@W with all their might into @c$N@W's leg!@n",
                        true, ch, nullptr, vict, TO_NOTVICT);
                    dmg *= calc_critical(ch, 1);
                    hurt(1, 190, ch, vict, nullptr, dmg, 1);
                    dam_eq_loc(vict, 2);
                    /* dam_eq_loc: 1 Arms, 2 legs, 3 head, and 4 body. */
                    break;
            }
            if (!AFF_FLAGGED(vict, AFF_BURNED) && rand_number(1, 4) == 3 && !IS_DEMON(vict)) {
                send_to_char(vict, "@RYou are burned by the attack!@n\r\n");
                send_to_char(ch, "@RThey are burned by the attack!@n\r\n");
                vict->affected_by.set(AFF_BURNED);
            }
            pcost(ch, attperc, 0);

            return;
        }
    } else if (obj) {
        if (!can_kill(ch, nullptr, obj, 1)) {
            return;
        }
        if (OBJ_FLAGGED(obj, ITEM_BROKEN)) {
            send_to_char(ch, "It is broken already!\r\n");
            return;
        }
        dmg = damtype(ch, 42, skill, attperc);
        dmg /= 10;
        act("@WYou fire a @WB@Dl@We@Ds@Ws@De@Wd @DH@Wa@Dm@Wm@De@Wr at $p@W!@n", false, ch, obj, nullptr, TO_CHAR);
        act("@C$n@W fires a @WB@Dl@We@Ds@Ws@De@Wd @DH@Wa@Dm@Wm@De@Wr at $p@W!@n", false, ch, obj, nullptr, TO_ROOM);
        hurt(0, 0, ch, nullptr, obj, dmg, 0);
        pcost(ch, attperc, 0);

    } else {
        send_to_char(ch, "Error! Please report.\r\n");
        return;
    }
}

ACMD(do_kousengan) {
    atk::Kousengan a(ch, argument);
    a.execute();
}

ACMD(do_deathbeam) {
    atk::DeathBeam a(ch, argument);
    a.execute();
}

ACMD(do_dodonpa) {
    atk::Dodonpa a(ch, argument);
    a.execute();
}

ACMD(do_masenko) {
    atk::Masenko a(ch, argument);
    a.execute();
}

ACMD(do_kamehameha) {
    atk::Kamehameha a(ch, argument);
    a.execute();
}

ACMD(do_renzo) {
    atk::Renzo a(ch, argument);
    a.execute();
}

ACMD(do_heeldrop) {
    atk::Heeldrop a(ch, argument);
    a.execute();
}

ACMD(do_attack) {
    if (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD2), VAL_WEAPON_DAMTYPE) == TYPE_PIERCE - TYPE_HIT) {
        atk::Stab a(ch, argument);
        a.execute();
    } else if (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD2), VAL_WEAPON_DAMTYPE) == TYPE_SLASH - TYPE_HIT) {
        atk::Slash a(ch, argument);
        a.execute();
    } else if (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD2), VAL_WEAPON_DAMTYPE) == TYPE_CRUSH - TYPE_HIT) {
        atk::Crush a(ch, argument);
        a.execute();
    } else if (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD2), VAL_WEAPON_DAMTYPE) == TYPE_STAB - TYPE_HIT) {
        atk::Impale a(ch, argument);
        a.execute();
    } else if (GET_OBJ_VAL(GET_EQ(ch, WEAR_WIELD2), VAL_WEAPON_DAMTYPE) == TYPE_BLAST - TYPE_HIT) {
        atk::Shoot a(ch, argument);
        a.execute();
    } else {
        atk::Smash a(ch, argument);
        a.execute();
    }
}

ACMD(do_shogekiha) {
    atk::Shogekiha a(ch, argument);
    a.execute();
}

ACMD(do_tsuihidan) {
    atk::Tsuihidan a(ch, argument);
    a.execute();
}

ACMD(do_bite) {
    atk::Bite a(ch, argument);
    a.execute();
}

ACMD(do_kiball) {
    atk::KiBall a(ch, argument);
    a.execute();
}

ACMD(do_beam) {
    atk::Beam a(ch, argument);
    a.execute();
}

ACMD(do_kiblast) {
    atk::KiBlast a(ch, argument);
    a.execute();
}

ACMD(do_slam) {
    atk::Slam a(ch, argument);
    a.execute();
}

ACMD(do_uppercut) {
    atk::Uppercut a(ch, argument);
    a.execute();
}

ACMD(do_roundhouse) {
    atk::Roundhouse a(ch, argument);
    a.execute();
}

ACMD(do_elbow) {
    atk::Elbow a(ch, argument);
    a.execute();
}

ACMD(do_kick) {
    atk::Kick a(ch, argument);
    a.execute();
}

ACMD(do_knee) {
    atk::Knee a(ch, argument);
    a.execute();
}

ACMD(do_punch) {
    atk::Punch a(ch, argument);
    a.execute();
}

ACMD(do_tailwhip) {
    atk::Tailwhip a(ch, argument);
    a.execute();
}

/* This handles charging for energy attacks */
ACMD(do_charge) {

    char arg[MAX_INPUT_LENGTH];
    int amt;

    one_argument(argument, arg);

    if (PLR_FLAGGED(ch, PLR_AURALIGHT)) {
        send_to_char(ch, "@WYou are concentrating too much on your aura to be able to charge.");
        return;
    }
    if (PLR_FLAGGED(ch, PLR_HEALT)) {
        send_to_char(ch, "You are inside a healing tank!\r\n");
        return;
    }
    if (AFF_FLAGGED(ch, AFF_MBREAK)) {
        send_to_char(ch, "Your mind is still strained from psychic attacks...\r\n");
        return;
    }
    if (AFF_FLAGGED(ch, AFF_POISON)) {
        send_to_char(ch, "You feel too sick from the poison to concentrate.\r\n");
        return;
    }
    if (!*arg) {
        send_to_char(ch, "Charge, yes. How much percent though?\r\n");
        send_to_char(ch, "[ 1 - 100 | cancel | release]\r\n");
        return;
    } else if (!strcasecmp("release", arg) && (PLR_FLAGGED(ch, PLR_CHARGE) && GET_CHARGE(ch) <= 0)) {
        send_to_char(ch, "Try cancel instead, you have nothing charged up yet!\r\n");
        return;
    } else if (!strcasecmp("release", arg) && (PLR_FLAGGED(ch, PLR_CHARGE) && GET_CHARGE(ch) > 0)) {
        send_to_char(ch, "You stop charging and release your pent up energy.\r\n");
        switch (rand_number(1, 3)) {
            case 1:
                act("$n@w's aura disappears.@n", true, ch, nullptr, nullptr, TO_ROOM);
                break;
            case 2:
                act("$n@w's aura fades.@n", true, ch, nullptr, nullptr, TO_ROOM);
                break;
            case 3:
                act("$n@w's aura flickers brightly before disappearing.@n", true, ch, nullptr, nullptr, TO_ROOM);
                break;
            default:
                act("$n@w's aura disappears.@n", true, ch, nullptr, nullptr, TO_ROOM);
                break;
        }
        ch->incCurKI(GET_CHARGE(ch));
        GET_CHARGE(ch) = 0;
        GET_CHARGETO(ch) = 0;
        ch->playerFlags.reset(PLR_CHARGE);
        return;
    } else if (!strcasecmp("release", arg) && GET_CHARGE(ch) > 0) {
        send_to_char(ch, "You release your pent up energy.\r\n");
        switch (rand_number(1, 3)) {
            case 1:
                act("$n@w's aura disappears.@n", true, ch, nullptr, nullptr, TO_ROOM);
                break;
            case 2:
                act("$n@w's aura fades.@n", true, ch, nullptr, nullptr, TO_ROOM);
                break;
            case 3:
                act("$n@w's aura flickers brightly before disappearing.@n", true, ch, nullptr, nullptr, TO_ROOM);
                break;
            default:
                act("$n@w's aura disappears.@n", true, ch, nullptr, nullptr, TO_ROOM);
                break;
        }
        ch->incCurKI(GET_CHARGE(ch));
        GET_CHARGE(ch) = 0;
        GET_CHARGETO(ch) = 0;
        return;
    } else if (!strcasecmp("cancel", arg) && PLR_FLAGGED(ch, PLR_CHARGE)) {
        send_to_char(ch, "You stop charging.\r\n");
        switch (rand_number(1, 3)) {
            case 1:
                act("$n@w's aura disappears.@n", true, ch, nullptr, nullptr, TO_ROOM);
                break;
            case 2:
                act("$n@w's aura fades.@n", true, ch, nullptr, nullptr, TO_ROOM);
                break;
            case 3:
                act("$n@w's aura flickers brightly before disappearing.@n", true, ch, nullptr, nullptr, TO_ROOM);
                break;
            default:
                act("$n@w's aura disappears.@n", true, ch, nullptr, nullptr, TO_ROOM);
                break;
        }
        ch->playerFlags.reset(PLR_CHARGE);
        GET_CHARGETO(ch) = 0;
        return;
    } else if (!strcasecmp("cancel", arg) && !PLR_FLAGGED(ch, PLR_CHARGE)) {
        send_to_char(ch, "You are not even charging!\r\n");
        return;
    } else if ((ch->getCurKI()) < GET_MAX_MANA(ch) / 100) {
        send_to_char(ch, "You don't even have enough ki!\r\n");
        return;
    } else if ((amt = atoi(arg)) > 0) {
        if (amt >= 101) {
            send_to_char(ch, "You have set it too high!\r\n");
            return;
        } else if (AFF_FLAGGED(ch, AFF_SPIRITCONTROL)) {
            int64_t diff = 0;
            if ((ch->getCurKI()) < ((GET_MAX_MANA(ch) * 0.01) * amt) + 1) {
                diff = (((GET_MAX_MANA(ch) * 0.01) * amt) + 1) - (ch->getCurKI());
            }
            int chance = 15;
            chance -= GET_SKILL(ch, SKILL_SPIRITCONTROL) / 10;
            if (chance < 10)
                chance = 10;
            else if (chance > 15)
                chance = 15;

            if (chance > rand_number(1, 100)) {
                send_to_char(ch,
                             "The rush of ki that you try to pool temporarily overwhelms you and you lose control!\r\n");
                null_affect(ch, AFF_SPIRITCONTROL);
                return;
            } else {
                int64_t spiritcost = GET_MAX_MANA(ch) * 0.05;
                if (GET_SKILL(ch, SKILL_SPIRITCONTROL) >= 100) {
                    spiritcost = GET_MAX_MANA(ch) * 0.01;
                }
                reveal_hiding(ch, 0);
                send_to_char(ch,
                             "Your %s colored aura flashes up around you as you instantly take control of the ki you needed!\r\n",
                             aura_types[GET_AURA(ch)]);
                send_to_char(ch, "@D[@RCost@D: @r%s@D]@n\r\n", add_commas(spiritcost));
                char bloom[MAX_INPUT_LENGTH];
                sprintf(bloom, "@wA %s aura flashes up brightly around $n@w!@n", aura_types[GET_AURA(ch)]);
                act(bloom, true, ch, nullptr, nullptr, TO_ROOM);
                GET_CHARGE(ch) = (((GET_MAX_MANA(ch) * 0.01) * amt) + 1) - diff;
                ch->decCurKI((((GET_MAX_MANA(ch) * 0.01) * amt) + 1) - diff + spiritcost);
            }
        } else {
            reveal_hiding(ch, 0);
            send_to_char(ch, "You begin to charge some energy, as a %s aura begins to burn around you!\r\n",
                         aura_types[GET_AURA(ch)]);
            char bloom[MAX_INPUT_LENGTH];
            sprintf(bloom, "@wA %s aura flashes up brightly around $n@w!@n", aura_types[GET_AURA(ch)]);
            act(bloom, true, ch, nullptr, nullptr, TO_ROOM);
            GET_CHARGETO(ch) = (((GET_MAX_MANA(ch) * 0.01) * amt) + 1);
            GET_CHARGE(ch) += 1;
            ch->playerFlags.set(PLR_CHARGE);
        }
    } else if (amt < 1 && GET_ROOM_VNUM(IN_ROOM(ch)) != 1562) {
        send_to_char(ch, "You have set it too low!\r\n");
        return;
    } else {
        send_to_char(ch, "That is an invalid argument.\r\n");
    }
}

ACMD(do_powerup) {
    if (IS_NPC(ch)) {
        ch->mobFlags.set(MOB_POWERUP);
        if (GET_MAX_HIT(ch) < 50000) {
            act("@RYou begin to powerup, and air billows outward around you!@n", true, ch, nullptr, nullptr, TO_CHAR);
            act("@R$n begins to powerup, and air billows outward around $m!@n", true, ch, nullptr, nullptr, TO_ROOM);
        } else if (GET_MAX_HIT(ch) < 500000) {
            act("@RYou begin to powerup, and loose objects are lifted into the air!@n", true, ch, nullptr, nullptr,
                TO_CHAR);
            act("@R$n begins to powerup, and loose objects are lifted into the air!@n", true, ch, nullptr, nullptr,
                TO_ROOM);
        } else if (GET_MAX_HIT(ch) < 5000000) {
            act("@RYou begin to powerup, and torrents of energy crackle around you!@n", true, ch, nullptr, nullptr,
                TO_CHAR);
            act("@R$n begins to powerup, and torrents of energy crackle around $m!@n", true, ch, nullptr, nullptr,
                TO_ROOM);
        } else if (GET_MAX_HIT(ch) < 50000000) {
            act("@RYou begin to powerup, and the entire area begins to shudder!@n", true, ch, nullptr, nullptr,
                TO_CHAR);
            act("@R$n begins to powerup, and the entire area begins to shudder!@n", true, ch, nullptr, nullptr,
                TO_ROOM);
        } else if (GET_MAX_HIT(ch) < 100000000) {
            act("@RYou begin to powerup, and massive cracks begin to form beneath you!@n", true, ch, nullptr, nullptr,
                TO_CHAR);
            act("@R$n begins to powerup, and massive cracks begin to form beneath $m!@n", true, ch, nullptr, nullptr,
                TO_ROOM);
        } else if (GET_MAX_HIT(ch) < 300000000) {
            act("@RYou begin to powerup, and everything around you shudders from the power!@n", true, ch, nullptr,
                nullptr,
                TO_CHAR);
            act("@R$n begins to powerup, and everything around $m shudders from the power!@n", true, ch, nullptr,
                nullptr, TO_ROOM);
        } else {
            act("@RYou begin to powerup, and the very air around you begins to burn!@n", true, ch, nullptr, nullptr,
                TO_CHAR);
            act("@R$n begins to powerup, and the very air around $m begins to burn!@n", true, ch, nullptr, nullptr,
                TO_ROOM);
        }
        return;
    }
    if (PLR_FLAGGED(ch, PLR_AURALIGHT)) {
        send_to_char(ch, "@WYou are concentrating too much on your aura to be able to power up.");
        return;
    }
    if (IS_ANDROID(ch)) {
        send_to_char(ch, "@WYou are an android, you do not powerup.@n");
        return;
    }
    if (GET_SUPPRESS(ch) > 0) {
        send_to_char(ch, "@WYou currently have your powerlevel suppressed to %" I64T " percent.@n", GET_SUPPRESS(ch));
        return;
    }
    if (PLR_FLAGGED(ch, PLR_POWERUP)) {
        send_to_char(ch, "@WYou stop powering up.@n");
        ch->playerFlags.reset(PLR_POWERUP);
        return;
    }
    if (GET_HIT(ch) >= GET_MAX_HIT(ch)) {
        send_to_char(ch, "@WYou are already at max!@n");
        return;
    }
    if ((ch->getCurKI()) < GET_MAX_MANA(ch) / 20) {
        send_to_char(ch, "@WYou do not have enough ki to powerup!@n");
        return;
    } else {
        reveal_hiding(ch, 0);
        if (GET_MAX_HIT(ch) < 50000) {
            act("@RYou begin to powerup, and air billows outward around you!@n", true, ch, nullptr, nullptr, TO_CHAR);
            act("@R$n begins to powerup, and air billows outward around $m!@n", true, ch, nullptr, nullptr, TO_ROOM);
        } else if (GET_MAX_HIT(ch) < 500000) {
            act("@RYou begin to powerup, and loose objects are lifted into the air!@n", true, ch, nullptr, nullptr,
                TO_CHAR);
            act("@R$n begins to powerup, and loose objects are lifted into the air!@n", true, ch, nullptr, nullptr,
                TO_ROOM);
        } else if (GET_MAX_HIT(ch) < 5000000) {
            act("@RYou begin to powerup, and torrents of energy crackle around you!@n", true, ch, nullptr, nullptr,
                TO_CHAR);
            act("@R$n begins to powerup, and torrents of energy crackle around $m!@n", true, ch, nullptr, nullptr,
                TO_ROOM);
        } else if (GET_MAX_HIT(ch) < 50000000) {
            act("@RYou begin to powerup, and the entire area begins to shudder!@n", true, ch, nullptr, nullptr,
                TO_CHAR);
            act("@R$n begins to powerup, and the entire area begins to shudder!@n", true, ch, nullptr, nullptr,
                TO_ROOM);
        } else if (GET_MAX_HIT(ch) < 100000000) {
            act("@RYou begin to powerup, and massive cracks begin to form beneath you!@n", true, ch, nullptr, nullptr,
                TO_CHAR);
            act("@R$n begins to powerup, and massive cracks begin to form beneath $m!@n", true, ch, nullptr, nullptr,
                TO_ROOM);
        } else if (GET_MAX_HIT(ch) < 300000000) {
            act("@RYou begin to powerup, and everything around you shudders from the power!@n", true, ch, nullptr,
                nullptr,
                TO_CHAR);
            act("@R$n begins to powerup, and everything around $m shudders from the power!@n", true, ch, nullptr,
                nullptr, TO_ROOM);
        } else {
            act("@RYou begin to powerup, and the very air around you begins to burn!@n", true, ch, nullptr, nullptr,
                TO_CHAR);
            act("@R$n begins to powerup, and the very air around $m begins to burn!@n", true, ch, nullptr, nullptr,
                TO_ROOM);
        }
        ch->playerFlags.set(PLR_POWERUP);
        return;
    }
}

ACMD(do_rescue) {

    char arg[100];
    struct char_data *helpee, *opponent;

    one_argument(argument, arg);

    if (!*arg)
        send_to_char(ch, "Whom do you wish to rescue?\r\n");
    else if (!(helpee = get_char_vis(ch, arg, nullptr, FIND_CHAR_ROOM)))
        send_to_char(ch, "%s", CONFIG_NOPERSON);
    else if (helpee == ch)
        send_to_char(ch, "You can't help yourself any more than this!\r\n");
    else if (!FIGHTING(helpee))
        send_to_char(ch, "They are not fighting anyone!\r\n");
    else if (FIGHTING(ch) && !IS_NPC(ch))
        send_to_char(ch, "You are a little too busy fighting for yourself!\r\n");
    else {
        opponent = FIGHTING(helpee);
        int mobbonus = 0;

        if (IS_NPC(ch)) {
            mobbonus = GET_SPEEDI(ch) * 0.2;
        }
        if (GET_SPEEDI(ch) + mobbonus < GET_SPEEDI(opponent) && rand_number(1, 3) != 3) {
            act("@GYou leap towards @g$N@G and try to rescue $M but are too slow!@n", true, ch, nullptr, helpee,
                TO_CHAR);
            act("@g$n@G leaps towards you! $n is too slow and fails to rescue you!@n", true, ch, nullptr, helpee,
                TO_VICT);
            act("@g$n@G leaps towards @g$N@G and tries to rescue $M but is too slow!@n", true, ch, nullptr, helpee,
                TO_NOTVICT);
            return;
        }
        act("@GYou leap in front of @g$N@G and rescue $M!@n", true, ch, nullptr, helpee, TO_CHAR);
        act("@g$n@G leaps in front of you! You are rescued!@n", true, ch, nullptr, helpee, TO_VICT);
        act("@g$n@G leaps in front of @g$N@G and rescues $M!@n", true, ch, nullptr, helpee, TO_NOTVICT);
        stop_fighting(opponent);
        hurt(0, 0, ch, opponent, nullptr, rand_number(1, GET_STR(ch)), 1);
        hurt(0, 0, opponent, ch, nullptr, rand_number(1, GET_STR(ch)), 1);
        return;
    }
}

ACMD(do_assist) {
    char arg[MAX_INPUT_LENGTH];
    struct char_data *helpee, *opponent;

    if (FIGHTING(ch)) {
        send_to_char(ch, "You're already fighting!  How can you assist someone else?\r\n");
        return;
    }
    one_argument(argument, arg);

    if (!*arg)
        send_to_char(ch, "Whom do you wish to assist?\r\n");
    else if (!(helpee = get_char_vis(ch, arg, nullptr, FIND_CHAR_ROOM)))
        send_to_char(ch, "%s", CONFIG_NOPERSON);
    else if (helpee == ch)
        send_to_char(ch, "You can't help yourself any more than this!\r\n");
    else {
        /*
     * Hit the same enemy the person you're helping is.
     */
        if (FIGHTING(helpee))
            opponent = FIGHTING(helpee);
        else
            for (opponent = ch->getRoom()->people;
                 opponent && (FIGHTING(opponent) != helpee);
                 opponent = opponent->next_in_room);

        if (!opponent)
            act("But nobody is fighting $M!", true, ch, nullptr, helpee, TO_CHAR);
        else if (!CAN_SEE(ch, opponent))
            act("You can't see who is fighting $M!", true, ch, nullptr, helpee, TO_CHAR);
            /* prevent accidental pkill */
        else {
            reveal_hiding(ch, 0);
            send_to_char(ch, "You join the fight!\r\n");
            act("$N assists you!", 0, helpee, nullptr, ch, TO_CHAR);
            act("$n assists $N.", true, ch, nullptr, helpee, TO_NOTVICT);
            if (!FIGHTING(ch)) {
                set_fighting(ch, opponent);
            }
            if (!FIGHTING(opponent)) {
                set_fighting(opponent, ch);
            }
        }
    }
}

ACMD(do_kill) {
    char arg[MAX_INPUT_LENGTH];
    struct char_data *vict;

    if (IS_NPC(ch) || !ADM_FLAGGED(ch, ADM_INSTANTKILL)) {
        return;
    }
    one_argument(argument, arg);

    if (!*arg) {
        send_to_char(ch, "Kill who?\r\n");
    } else {
        if (!(vict = get_char_vis(ch, arg, nullptr, FIND_CHAR_ROOM)))
            send_to_char(ch, "They aren't here.\r\n");
        else if (ch == vict)
            send_to_char(ch, "Your mother would be so sad.. :(\r\n");
        else {
            act("You chop $M to pieces!  Ah!  The blood!", true, ch, nullptr, vict, TO_CHAR);
            act("$N chops you to pieces!", true, vict, nullptr, ch, TO_CHAR);
            act("$n brutally slays $N!", true, ch, nullptr, vict, TO_NOTVICT);
            raw_kill(vict, ch);
        }
    }
}

ACMD(do_flee) {
    int i, attempt;
    struct char_data *was_fighting;
    char arg[MAX_INPUT_LENGTH];

    one_argument(argument, arg);

    if (GET_POS(ch) < POS_RESTING) {
        send_to_char(ch, "You are in pretty bad shape, unable to flee!\r\n");
        return;
    }

    if (GRAPPLING(ch)) {
        send_to_char(ch, "You are grappling with someone!\r\n");
        return;
    }

    if (GRAPPLED(ch)) {
        send_to_char(ch, "You are grappling with someone!\r\n");
        return;
    }

    if (ABSORBING(ch)) {
        send_to_char(ch, "You are absorbing from someone!\r\n");
        return;
    }

    if (ABSORBBY(ch)) {
        send_to_char(ch, "You are being absorbed from by someone!\r\n");
        return;
    }

    if (!IS_NPC(ch)) {
        int fail = false;
        auto isMyAttack = [&](const auto&o) {return KICHARGE(o) > 0 && USER(o) == ch;};
        if (auto obj = ch->getRoom()->findObject(isMyAttack); obj) {
            send_to_char(ch, "You are too busy controlling your attack!\r\n");
            return;
        }
    }

    for (i = 0; i < 12; i++) {
        if (*arg) {
            if ((attempt = search_block(arg, dirs, false) > -1)) {
                attempt = search_block(arg, dirs, false);
            } else if ((attempt = search_block(arg, abbr_dirs, false) > -1)) {
                attempt = search_block(arg, abbr_dirs, false);
            } else {
                attempt = rand_number(0, NUM_OF_DIRS - 1);  /* Select a random direction */
            }
        }
        if (!*arg) {
            attempt = rand_number(0, NUM_OF_DIRS - 1);    /* Select a random direction */
        }
        if (CAN_GO(ch, attempt) &&
            !ROOM_FLAGGED(EXIT(ch, attempt)->to_room, ROOM_DEATH)) {
            act("$n panics, and attempts to flee!", true, ch, nullptr, nullptr, TO_ROOM);
            if (IS_NPC(ch) && ROOM_FLAGGED(EXIT(ch, attempt)->to_room, ROOM_NOMOB)) {
                return;
            }
            was_fighting = FIGHTING(ch);

            auto isWall = [&](const auto&o) {return o->vn == 79 && GET_OBJ_COST(o) == attempt;};
            if(auto wall = ch->getRoom()->findObject(isWall); wall) {
                send_to_char(ch, "That direction has a glacial wall blocking it.\r\n");
                return;
            }

            if (!block_calc(ch)) {
                return;
            }

            if (ABSORBING(ch)) {
                send_to_char(ch, "You are busy absorbing from %s!\r\n", GET_NAME(ABSORBING(ch)));
                return;
            }
            if (ABSORBBY(ch)) {
                if (axion_dice(0) < GET_SKILL(ABSORBING(ch), SKILL_ABSORB)) {
                    send_to_char(ch, "You are being held by %s, they are absorbing you!\r\n", GET_NAME(ABSORBBY(ch)));
                    send_to_char(ABSORBBY(ch), "%s struggles in your grasp!\r\n", GET_NAME(ch));
                    WAIT_STATE(ch, PULSE_2SEC);
                    return;
                } else {
                    act("@c$N@W manages to break loose of @C$n's@W hold!@n", true, ABSORBBY(ch), nullptr, ch,
                        TO_NOTVICT);
                    act("@WYou manage to break loose of @C$n's@W hold!@n", true, ABSORBBY(ch), nullptr, ch, TO_VICT);
                    act("@c$N@W manages to break loose of your hold!@n", true, ABSORBBY(ch), nullptr, ch, TO_CHAR);
                    ABSORBING(ABSORBBY(ch)) = nullptr;
                    ABSORBBY(ch) = nullptr;
                }
            }
            if (do_simple_move(ch, attempt, true)) {
                send_to_char(ch, "You flee head over heels.\r\n");
                WAIT_STATE(ch, PULSE_2SEC);
            } else {
                act("$n tries to flee, but can't!", true, ch, nullptr, nullptr, TO_ROOM);
                WAIT_STATE(ch, PULSE_2SEC);
            }
            return;
        }
    }
    send_to_char(ch, "PANIC!  You couldn't escape!\r\n");

}
