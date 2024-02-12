/**************************************************************************
 *  File: dg_triggers.c                                                    *
 *                                                                         *
 *  Usage: contains all the trigger functions for scripts.                 *
 *                                                                         *
 *  All rights reserved.  See license.doc for complete information.        *
 *                                                                         *
 *  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 *                                                                         *
 *  $Author: galion/Mark A. Heilpern/egreen/Welcor $                       *
 *  $Date: 2004/10/11 12:07:00$                                            *
 *  $Revision: 1.0.14 $                                                    *
 **************************************************************************/

#include "dbat/structs.h"
#include "dbat/dg_scripts.h"
#include "dbat/utils.h"
#include "dbat/comm.h"
#include "dbat/interpreter.h"
#include "dbat/handler.h"
#include "dbat/db.h"
#include "dbat/oasis.h"
#include "dbat/constants.h"
#include "dbat/spell_parser.h"
#include "dbat/act.movement.h"
#include "dbat/spells.h"

/*
 *  General functions used by several triggers
 */

/*
 * Copy first phrase into first_arg, returns rest of string
 */
char *one_phrase(char *arg, char *first_arg)
{
    skip_spaces(&arg);

    if (!*arg)
        *first_arg = '\0';

    else if (*arg == '"')
    {
        char *p, c;

        p = matching_quote(arg);
        c = *p;
        *p = '\0';
        strcpy(first_arg, arg + 1);
        if (c == '\0')
            return p;
        else
            return p + 1;
    }
    else
    {
        char *s, *p;

        s = first_arg;
        p = arg;

        while (*p && !isspace(*p) && *p != '"')
            *s++ = *p++;

        *s = '\0';
        return p;
    }

    return arg;
}

int is_substring(char *sub, char *string)
{
    char *s;

    if ((s = str_str(string, sub)))
    {
        int len = strlen(string);
        int sublen = strlen(sub);

        /* check front */
        if ((s == string || isspace(*(s - 1)) || ispunct(*(s - 1))) &&

            /* check end */
            ((s + sublen == string + len) || isspace(s[sublen]) ||
             ispunct(s[sublen])))
            return 1;
    }

    return 0;
}

/*
 * return 1 if str contains a word or phrase from wordlist.
 * phrases are in double quotes (").
 * if wrdlist is nullptr, then return 1, if str is nullptr, return 0.
 */
int word_check(char *str, char *wordlist)
{
    char words[MAX_INPUT_LENGTH], phrase[MAX_INPUT_LENGTH], *s;

    if (*wordlist == '*')
        return 1;

    strcpy(words, wordlist);

    for (s = one_phrase(words, phrase); *phrase; s = one_phrase(s, phrase))
        if (is_substring(phrase, str))
            return 1;

    return 0;
}

/*
 *  mob triggers
 */

void random_mtrigger(char_data *ch)
{
    /*
     * This trigger is only called if a char is in the zone without nohassle.
     */

    if (!SCRIPT_CHECK(ch, MTRIG_RANDOM) || AFF_FLAGGED(ch, AFF_CHARM))
        return;

    for (auto t : ch->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, MTRIG_RANDOM) && (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            t->execute();
            break;
        }
    }
}

void bribe_mtrigger(char_data *ch, char_data *actor, int amount)
{
    if (!SCRIPT_CHECK(ch, MTRIG_BRIBE) || AFF_FLAGGED(ch, AFF_CHARM))
        return;

    char buf[MAX_INPUT_LENGTH];
    snprintf(buf, sizeof(buf), "%d", amount);

    for (auto t : ch->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, MTRIG_BRIBE) && (amount >= GET_TRIG_NARG(t)))
        {
            t->addVar("amount", buf);
            t->addVar("actor", actor);
            t->execute();
            break;
        }
    }
}

void greet_memory_mtrigger(char_data *actor)
{
    char_data *ch;
    struct script_memory *mem;
    char buf[MAX_INPUT_LENGTH];
    int command_performed = 0;

    if (!valid_dg_target(actor, DG_ALLOW_GODS))
        return;

    for (ch = actor->getRoom()->people; ch; ch = ch->next_in_room)
    {
        if (!SCRIPT_MEM(ch) || !AWAKE(ch) || FIGHTING(ch) || (ch == actor) ||
            AFF_FLAGGED(ch, AFF_CHARM))
            continue;
        /* find memory line with command only */
        for (mem = SCRIPT_MEM(ch); mem && SCRIPT_MEM(ch); mem = mem->next)
        {
            if (((actor)->id) != mem->id)
                continue;
            if (mem->cmd)
            {
                command_interpreter(ch, mem->cmd); /* no script */
                command_performed = 1;
                break;
            }
            /* if a command was not performed execute the memory script */
            if (mem && !command_performed)
            {
                for (auto t : ch->script->dgScripts)
                {
                    if (IS_SET(GET_TRIG_TYPE(t), MTRIG_MEMORY) &&
                        CAN_SEE(ch, actor) && GET_TRIG_DORMANT(t) &&
                        rand_number(1, 100) <= GET_TRIG_NARG(t))
                    {
                        t->addVar("actor", actor);
                        t->execute();
                        break;
                    }
                }
            }
            /* delete the memory */
            if (mem)
            {
                if (SCRIPT_MEM(ch) == mem)
                {
                    SCRIPT_MEM(ch) = mem->next;
                }
                else
                {
                    struct script_memory *prev;
                    prev = SCRIPT_MEM(ch);
                    while (prev->next != mem)
                        prev = prev->next;
                    prev->next = mem->next;
                }
                if (mem->cmd)
                    free(mem->cmd);
                free(mem);
            }
        }
    }
}

int greet_mtrigger(char_data *actor, int dir)
{
    char_data *ch;
    char buf[MAX_INPUT_LENGTH];
    int intermediate, final = true;

    if (!valid_dg_target(actor, DG_ALLOW_GODS))
        return true;

    for (ch = actor->getRoom()->people; ch; ch = ch->next_in_room)
    {
        if (!SCRIPT_CHECK(ch, MTRIG_GREET | MTRIG_GREET_ALL) ||
            !AWAKE(ch) || FIGHTING(ch) || (ch == actor) ||
            AFF_FLAGGED(ch, AFF_CHARM))
            continue;
        if (!SCRIPT_CHECK(ch, MTRIG_GREET_ALL) && IS_NPC(actor))
            continue;

        for (auto t : ch->script->dgScripts)
        {
            if (((IS_SET(GET_TRIG_TYPE(t), MTRIG_GREET) && CAN_SEE(ch, actor)) ||
                 IS_SET(GET_TRIG_TYPE(t), MTRIG_GREET_ALL)) &&
                GET_TRIG_DORMANT(t) && (rand_number(1, 100) <= GET_TRIG_NARG(t)))
            {
                if (dir >= 0 && dir < NUM_OF_DIRS)
                    t->addVar("direction", dirs[rev_dir[dir]]);
                else
                    t->addVar("direction", "none");
                t->addVar("actor", actor);
                intermediate = t->execute();
                if (!intermediate)
                    final = false;
                continue;
            }
        }
    }
    return final;
}

void entry_memory_mtrigger(char_data *ch)
{
    char_data *actor;
    struct script_memory *mem;
    char buf[MAX_INPUT_LENGTH];

    if (!SCRIPT_MEM(ch) || AFF_FLAGGED(ch, AFF_CHARM))
        return;

    for (actor = ch->getRoom()->people; actor && SCRIPT_MEM(ch);
         actor = actor->next_in_room)
    {
        if (actor != ch && SCRIPT_MEM(ch))
        {
            for (mem = SCRIPT_MEM(ch); mem && SCRIPT_MEM(ch); mem = mem->next)
            {
                if (((actor)->id) == mem->id)
                {
                    struct script_memory *prev;
                    if (mem->cmd)
                        command_interpreter(ch, mem->cmd);
                    else
                    {
                        for (auto t : ch->script->dgScripts)
                        {
                            if (TRIGGER_CHECK(t, MTRIG_MEMORY) && (rand_number(1, 100) <=
                                                                   GET_TRIG_NARG(t)))
                            {
                                t->addVar("actor", actor);
                                t->execute();
                                break;
                            }
                        }
                    }
                    /* delete the memory */
                    if (SCRIPT_MEM(ch) == mem)
                    {
                        SCRIPT_MEM(ch) = mem->next;
                    }
                    else
                    {
                        prev = SCRIPT_MEM(ch);
                        while (prev->next != mem)
                            prev = prev->next;
                        prev->next = mem->next;
                    }
                    if (mem->cmd)
                        free(mem->cmd);
                    free(mem);
                }
            } /* for (mem =..... */
        }
    }
}

int entry_mtrigger(char_data *ch)
{
    if (!SCRIPT_CHECK(ch, MTRIG_ENTRY) || AFF_FLAGGED(ch, AFF_CHARM))
        return 1;

    for (auto t : ch->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, MTRIG_ENTRY) && (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            return t->execute();
            break;
        }
    }

    return 1;
}

int command_mtrigger(char_data *actor, char *cmd, char *argument)
{
    char_data *ch, *ch_next;
    char buf[MAX_INPUT_LENGTH];

    /* prevent people we like from becoming trapped :P */
    if (!valid_dg_target(actor, 0))
        return 0;

    for (ch = actor->getRoom()->people; ch; ch = ch_next)
    {
        ch_next = ch->next_in_room;

        if (SCRIPT_CHECK(ch, MTRIG_COMMAND) && !AFF_FLAGGED(ch, AFF_CHARM) &&
            (actor != ch))
        {
            for (auto t : ch->script->dgScripts)
            {
                if (!TRIGGER_CHECK(t, MTRIG_COMMAND))
                    continue;

                if (t->parent->arglist.empty())
                {
                    mudlog(NRM, ADMLVL_BUILDER, true, "SYSERR: Command Trigger #%d has no text argument!",
                           GET_TRIG_VNUM(t));
                    continue;
                }

                if (t->parent->arglist == "*" || !strncasecmp(t->parent->arglist.c_str(), cmd, t->parent->arglist.size()))
                {
                    t->addVar("actor", actor);
                    skip_spaces(&argument);
                    t->addVar("arg", argument);
                    skip_spaces(&cmd);
                    t->addVar("cmd", cmd);

                    if (t->execute())
                        return 1;
                }
            }
        }
    }

    return 0;
}

void speech_mtrigger(char_data *actor, char *str)
{
    char_data *ch, *ch_next;
    char buf[MAX_INPUT_LENGTH];

    for (ch = actor->getRoom()->people; ch; ch = ch_next)
    {
        ch_next = ch->next_in_room;

        if (SCRIPT_CHECK(ch, MTRIG_SPEECH) && AWAKE(ch) &&
            !AFF_FLAGGED(ch, AFF_CHARM) && (actor != ch))
            for (auto t : ch->script->dgScripts)
            {
                if (!TRIGGER_CHECK(t, MTRIG_SPEECH))
                    continue;

                if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t))
                {
                    mudlog(NRM, ADMLVL_BUILDER, true, "SYSERR: Speech Trigger #%d has no text argument!",
                           GET_TRIG_VNUM(t));
                    continue;
                }

                if (((GET_TRIG_NARG(t) && word_check(str, GET_TRIG_ARG(t))) ||
                     (!GET_TRIG_NARG(t) && is_substring(GET_TRIG_ARG(t), str))))
                {
                    t->addVar("actor", actor);
                    t->addVar("speech", str);
                    t->execute();
                    break;
                }
            }
    }
}

void act_mtrigger(char_data *ch, char *str, char_data *actor, char_data *victim, obj_data *object,
                  obj_data *target, char *arg)
{
    char buf[MAX_INPUT_LENGTH];

    if (SCRIPT_CHECK(ch, MTRIG_ACT) && !AFF_FLAGGED(ch, AFF_CHARM) &&
        (actor != ch))
        for (auto t : ch->script->dgScripts)
        {
            if (!TRIGGER_CHECK(t, MTRIG_ACT))
                continue;

            if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t))
            {
                mudlog(NRM, ADMLVL_BUILDER, true, "SYSERR: Act Trigger #%d has no text argument!",
                       GET_TRIG_VNUM(t));
                continue;
            }

            if (((GET_TRIG_NARG(t) && word_check(str, GET_TRIG_ARG(t))) ||
                 (!GET_TRIG_NARG(t) && is_substring(GET_TRIG_ARG(t), str))))
            {
                if (actor)
                    t->addVar("actor", actor);
                if (victim)
                    t->addVar("victim", victim);
                if (object)
                    t->addVar("object", object);
                if (target)
                    t->addVar("target", target);
                if (str)
                {
                    /* we're guaranteed to have a string ending with \r\n\0 */
                    char *nstr = strdup(str), *fstr = nstr, *p = strchr(nstr, '\r');
                    skip_spaces(&nstr);
                    *p = '\0';
                    t->addVar("arg", nstr);
                    free(fstr);
                }
                t->execute();
                break;
            }
        }
}

void fight_mtrigger(char_data *ch)
{
    struct char_data *actor;
    char buf[MAX_INPUT_LENGTH];

    if (!SCRIPT_CHECK(ch, MTRIG_FIGHT) || !FIGHTING(ch) ||
        AFF_FLAGGED(ch, AFF_CHARM))
        return;

    for (auto t : ch->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, MTRIG_FIGHT) &&
            (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            actor = FIGHTING(ch);
            if (actor)
                t->addVar("actor", actor);
            else
                t->addVar("actor", "nobody");

            t->execute();
            break;
        }
    }
}

void hitprcnt_mtrigger(char_data *ch)
{
    struct char_data *actor;
    char buf[MAX_INPUT_LENGTH];

    if (!SCRIPT_CHECK(ch, MTRIG_HITPRCNT) || !FIGHTING(ch) ||
        AFF_FLAGGED(ch, AFF_CHARM))
        return;

    for (auto t : ch->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, MTRIG_HITPRCNT) && GET_MAX_HIT(ch) &&
            (GET_HIT(ch) <= (GET_MAX_HIT(ch) / 100) * GET_TRIG_NARG(t)))
        {

            actor = FIGHTING(ch);
            t->addVar("actor", actor);
            t->execute();
            break;
        }
    }
}

int receive_mtrigger(char_data *ch, char_data *actor, obj_data *obj)
{
    char buf[MAX_INPUT_LENGTH];
    int ret_val;

    if (!SCRIPT_CHECK(ch, MTRIG_RECEIVE) || AFF_FLAGGED(ch, AFF_CHARM))
        return 1;

    for (auto t : ch->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, MTRIG_RECEIVE) &&
            (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            t->addVar("actor", actor);
            t->addVar("object", obj);
            ret_val = t->execute();
            if (DEAD(actor) || DEAD(ch) || obj->carried_by != actor)
                return 0;
            else
                return ret_val;
        }
    }

    return 1;
}

int death_mtrigger(char_data *ch, char_data *actor)
{
    char buf[MAX_INPUT_LENGTH];

    if (!SCRIPT_CHECK(ch, MTRIG_DEATH) || AFF_FLAGGED(ch, AFF_CHARM))
        return 1;

    for (auto t : ch->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, MTRIG_DEATH) &&
            (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            if (actor)
                t->addVar("actor", actor);
            return t->execute();
        }
    }

    return 1;
}

void load_mtrigger(char_data *ch)
{
    int result = 0;

    if (!SCRIPT_CHECK(ch, MTRIG_LOAD))
        return;

    for (auto t : ch->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, MTRIG_LOAD) && (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            result = t->execute();
            break;
        }
    }
    if (result == SCRIPT_ERROR_CODE)
    {
        /* we have recursed beyond reasonable depth */
        /* make sure this mob is the last one in the load chain */
        if (GET_MOB_RNUM(ch) != NOBODY)
        {
            free_proto_script(&mob_proto[GET_MOB_RNUM(ch)], MOB_TRIGGER);
        }
    }
}

int cast_mtrigger(char_data *actor, char_data *ch, int spellnum)
{
    char buf[MAX_INPUT_LENGTH];

    if (ch == nullptr)
        return 1;

    if (!SCRIPT_CHECK(ch, MTRIG_CAST) || AFF_FLAGGED(ch, AFF_CHARM))
        return 1;

    for (auto t : ch->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, MTRIG_CAST) && (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            t->addVar("actor", actor);
            sprintf(buf, "%d", spellnum);
            t->addVar("spell", buf);
            t->addVar("spellname", skill_name(spellnum));
            return t->execute();
        }
    }

    return 1;
}

int leave_mtrigger(char_data *actor, int dir)
{
    char_data *ch;
    char buf[MAX_INPUT_LENGTH];

    if (!valid_dg_target(actor, DG_ALLOW_GODS))
        return 1;

    for (ch = actor->getRoom()->people; ch; ch = ch->next_in_room)
    {
        if (!SCRIPT_CHECK(ch, MTRIG_LEAVE) ||
            !AWAKE(ch) || FIGHTING(ch) || (ch == actor) ||
            AFF_FLAGGED(ch, AFF_CHARM))
            continue;

        for (auto t : ch->script->dgScripts)
        {
            if ((IS_SET(GET_TRIG_TYPE(t), MTRIG_LEAVE)) &&
                GET_TRIG_DORMANT(t) && (rand_number(1, 100) <= GET_TRIG_NARG(t)))
            {
                if (dir >= 0 && dir < NUM_OF_DIRS)
                    t->addVar("direction", dirs[dir]);
                else
                    t->addVar("direction", "none");
                t->addVar("actor", actor);
                return t->execute();
            }
        }
    }
    return 1;
}

int door_mtrigger(char_data *actor, int subcmd, int dir)
{
    char_data *ch;
    char buf[MAX_INPUT_LENGTH];

    for (ch = actor->getRoom()->people; ch; ch = ch->next_in_room)
    {
        if (!SCRIPT_CHECK(ch, MTRIG_DOOR) ||
            !AWAKE(ch) || FIGHTING(ch) || (ch == actor) ||
            AFF_FLAGGED(ch, AFF_CHARM))
            continue;

        for (auto t : ch->script->dgScripts)
        {
            if (IS_SET(GET_TRIG_TYPE(t), MTRIG_DOOR) && CAN_SEE(ch, actor) &&
                GET_TRIG_DORMANT(t) && (rand_number(1, 100) <= GET_TRIG_NARG(t)))
            {
                t->addVar("cmd", cmd_door[subcmd]);
                if (dir >= 0 && dir < NUM_OF_DIRS)
                    t->addVar("direction", dirs[dir]);
                else
                    t->addVar("direction", "none");
                t->addVar("actor", actor);
                return t->execute();
            }
        }
    }
    return 1;
}

void time_mtrigger(char_data *ch)
{
    /*
     * This trigger is called if the hour is the same as specified in Narg.
     */

    if (!SCRIPT_CHECK(ch, MTRIG_TIME) || AFF_FLAGGED(ch, AFF_CHARM))
        return;

    for (auto t : ch->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, MTRIG_TIME) &&
            (time_info.hours == GET_TRIG_NARG(t)))
        {
            t->execute();
            break;
        }
    }
}

void interval_mtrigger(char_data *ch, int trigFlag)
{
    /*
     * This trigger is called if the hour is the same as specified in Narg.
     */

    if (!SCRIPT_CHECK(ch, trigFlag) || AFF_FLAGGED(ch, AFF_CHARM))
        return;

    for (auto t : ch->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, trigFlag))
        {
            t->execute();
            break;
        }
    }
}

/*
 *  object triggers
 */

void random_otrigger(obj_data *obj)
{
    if (!SCRIPT_CHECK(obj, OTRIG_RANDOM))
        return;

    for (auto t : obj->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, OTRIG_RANDOM) &&
            (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            t->execute();
            break;
        }
    }
}

void timer_otrigger(struct obj_data *obj)
{

    if (!SCRIPT_CHECK(obj, OTRIG_TIMER))
        return;

    for (auto t : obj->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, OTRIG_TIMER))
        {
            t->execute();
        }
    }

    return;
}

int get_otrigger(obj_data *obj, char_data *actor)
{
    char buf[MAX_INPUT_LENGTH];
    int ret_val;
    if (!SCRIPT_CHECK(obj, OTRIG_GET))
        return 1;

    for (auto t : obj->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, OTRIG_GET) && (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            t->addVar("actor", actor);
            ret_val = t->execute();
            /* don't allow a get to take place, if
             * a) the actor is killed (the mud would choke on obj_to_char).
             * b) the object is purged.
             */
            if (DEAD(actor) || !obj)
                return 0;
            else
                return ret_val;
        }
    }

    return 1;
}

/* checks for command trigger on specific object. assumes obj has cmd trig */
int cmd_otrig(obj_data *obj, char_data *actor, char *cmd,
              char *argument, int type)
{
    char buf[MAX_INPUT_LENGTH];

    if (obj && SCRIPT_CHECK(obj, OTRIG_COMMAND))
        for (auto t : obj->script->dgScripts)
        {
            if (!TRIGGER_CHECK(t, OTRIG_COMMAND))
                continue;

            if (IS_SET(GET_TRIG_NARG(t), type) &&
                (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t)))
            {
                mudlog(NRM, ADMLVL_BUILDER, true, "SYSERR: O-Command Trigger #%d has no text argument!",
                       GET_TRIG_VNUM(t));
                continue;
            }

            if (IS_SET(GET_TRIG_NARG(t), type) &&
                (*GET_TRIG_ARG(t) == '*' ||
                 !strncasecmp(GET_TRIG_ARG(t), cmd, strlen(GET_TRIG_ARG(t)))))
            {

                t->addVar("actor", actor);
                skip_spaces(&argument);
                t->addVar("arg", argument);
                skip_spaces(&cmd);
                t->addVar("cmd", cmd);

                if (t->execute())
                    return 1;
            }
        }

    return 0;
}

int command_otrigger(char_data *actor, char *cmd, char *argument)
{
    obj_data *obj;
    int i;

    /* prevent people we like from becoming trapped :P */
    if (!valid_dg_target(actor, 0))
        return 0;

    for (i = 0; i < NUM_WEARS; i++)
        if (GET_EQ(actor, i))
            if (cmd_otrig(GET_EQ(actor, i), actor, cmd, argument, OCMD_EQUIP) &&
                !OBJ_FLAGGED(GET_EQ(actor, i), ITEM_FORGED))
                return 1;

    for (obj = actor->contents; obj; obj = obj->next_content)
        if (cmd_otrig(obj, actor, cmd, argument, OCMD_INVEN) && !OBJ_FLAGGED(obj, ITEM_FORGED))
            return 1;

    for (obj = actor->getRoom()->contents; obj; obj = obj->next_content)
        if (cmd_otrig(obj, actor, cmd, argument, OCMD_ROOM) && !OBJ_FLAGGED(obj, ITEM_FORGED))
            return 1;

    return 0;
}

int wear_otrigger(obj_data *obj, char_data *actor, int where)
{
    char buf[MAX_INPUT_LENGTH];
    int ret_val;

    if (!SCRIPT_CHECK(obj, OTRIG_WEAR))
        return 1;

    for (auto t : obj->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, OTRIG_WEAR))
        {
            t->addVar("actor", actor);
            ret_val = t->execute();
            /* don't allow a wear to take place, if
             * the object is purged.
             */
            if (!obj)
                return 0;
            else
                return ret_val;
        }
    }

    return 1;
}

int remove_otrigger(obj_data *obj, char_data *actor)
{
    char buf[MAX_INPUT_LENGTH];
    int ret_val;

    if (!SCRIPT_CHECK(obj, OTRIG_REMOVE))
        return 1;

    if (!valid_dg_target(actor, 0))
        return 1;

    for (auto t : obj->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, OTRIG_REMOVE))
        {
            t->addVar("actor", actor);
            ret_val = t->execute();
            /* don't allow a remove to take place, if
             * the object is purged.
             */
            if (!obj)
                return 0;
            else
                return ret_val;
        }
    }

    return 1;
}

int drop_otrigger(obj_data *obj, char_data *actor)
{
    trig_data *t;
    char buf[MAX_INPUT_LENGTH];
    int ret_val;

    if (!SCRIPT_CHECK(obj, OTRIG_DROP))
        return 1;

    for (auto t : obj->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, OTRIG_DROP) && (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            t->addVar("actor", actor);
            ret_val = t->execute();
            /* don't allow a drop to take place, if
             * the object is purged (nothing to drop).
             */
            if (!obj)
                return 0;
            else
                return ret_val;
        }
    }

    return 1;
}

int give_otrigger(obj_data *obj, char_data *actor, char_data *victim)
{
    trig_data *t;
    char buf[MAX_INPUT_LENGTH];
    int ret_val;

    if (!SCRIPT_CHECK(obj, OTRIG_GIVE))
        return 1;

    for (auto t : obj->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, OTRIG_GIVE) && (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            t->addVar("actor", actor);
            t->addVar("victim", victim);
            ret_val = t->execute();
            /* don't allow a give to take place, if
             * a) the object is purged.
             * b) the object is not carried by the giver.
             */
            if (!obj || obj->carried_by != actor)
                return 0;
            else
                return ret_val;
        }
    }

    return 1;
}

void load_otrigger(obj_data *obj)
{
    trig_data *t;
    int result = 0;

    if (!SCRIPT_CHECK(obj, OTRIG_LOAD))
        return;

    for (auto t : obj->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, OTRIG_LOAD) &&
            (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            result = t->execute();
            break;
        }
    }
    if (result == SCRIPT_ERROR_CODE)
    {
        /* we have recursed beyond reasonable depth */
        /* make sure this mob is the last one in the load chain */
        if (GET_OBJ_RNUM(obj) != NOTHING)
        {
            free_proto_script(&obj_proto[GET_OBJ_RNUM(obj)], OBJ_TRIGGER);
        }
    }
}

int cast_otrigger(char_data *actor, obj_data *obj, int spellnum)
{
    trig_data *t;
    char buf[MAX_INPUT_LENGTH];

    if (obj == nullptr)
        return 1;

    if (!SCRIPT_CHECK(obj, OTRIG_CAST))
        return 1;

    for (auto t : obj->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, OTRIG_CAST) &&
            (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            t->addVar("actor", actor);
            sprintf(buf, "%d", spellnum);
            t->addVar("spell", buf);
            t->addVar("spellname", skill_name(spellnum));
            return t->execute();
        }
    }

    return 1;
}

int leave_otrigger(room_data *room, char_data *actor, int dir)
{
    trig_data *t;
    char buf[MAX_INPUT_LENGTH];
    int temp, final = 1;
    obj_data *obj, *obj_next;

    if (!valid_dg_target(actor, DG_ALLOW_GODS))
        return 1;

    for (obj = room->contents; obj; obj = obj_next)
    {
        obj_next = obj->next_content;
        if (!SCRIPT_CHECK(obj, OTRIG_LEAVE))
            continue;

        for (auto t : obj->script->dgScripts)
        {
            if (TRIGGER_CHECK(t, OTRIG_LEAVE) &&
                (rand_number(1, 100) <= GET_TRIG_NARG(t)))
            {
                if (dir >= 0 && dir < NUM_OF_DIRS)
                    t->addVar("direction", dirs[dir]);
                else
                    t->addVar("direction", "none");
                t->addVar("actor", actor);
                temp = t->execute();
                if (temp == 0)
                    final = 0;
            }
        }
    }

    return final;
}

int consume_otrigger(obj_data *obj, char_data *actor, int cmd)
{
    trig_data *t;
    char buf[MAX_INPUT_LENGTH];
    int ret_val;

    if (!SCRIPT_CHECK(obj, OTRIG_CONSUME))
        return 1;

    for (auto t : obj->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, OTRIG_CONSUME))
        {
            t->addVar("actor", actor);
            switch (cmd)
            {
            case OCMD_EAT:
                t->addVar("command", "eat");
                break;
            case OCMD_DRINK:
                t->addVar("command", "drink");
                break;
            case OCMD_QUAFF:
                t->addVar("command", "quaff");
                break;
            }
            ret_val = t->execute();
            /* don't allow a wear to take place, if
             * the object is purged.
             */
            if (!obj)
                return 0;
            else
                return ret_val;
        }
    }

    return 1;
}

void time_otrigger(obj_data *obj)
{
    trig_data *t;
    char buf[MAX_INPUT_LENGTH];

    if (!SCRIPT_CHECK(obj, OTRIG_TIME))
        return;

    for (auto t : obj->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, OTRIG_TIME) &&
            (time_info.hours == GET_TRIG_NARG(t)))
        {
            t->addVar("time", fmt::format("{}", time_info.hours));
            t->execute();
            break;
        }
    }
}

void interval_otrigger(obj_data *obj, int trigFlag)
{
    if (!SCRIPT_CHECK(obj, trigFlag))
        return;

    for (auto t : obj->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, trigFlag))
        {
            t->execute();
            break;
        }
    }
}

/*
 *  world triggers
 */

void reset_wtrigger(struct room_data *room)
{
    if (!SCRIPT_CHECK(room, WTRIG_RESET))
        return;

    for (auto t : room->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, WTRIG_RESET) &&
            (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            t->execute();
            break;
        }
    }
}

void random_wtrigger(struct room_data *room)
{
    if (!SCRIPT_CHECK(room, WTRIG_RANDOM))
        return;

    for (auto t : room->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, WTRIG_RANDOM) &&
            (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            t->execute();
            break;
        }
    }
}

int enter_wtrigger(struct room_data *room, char_data *actor, int dir)
{
    char buf[MAX_INPUT_LENGTH];

    if (!SCRIPT_CHECK(room, WTRIG_ENTER))
        return 1;

    for (auto t : room->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, WTRIG_ENTER) &&
            (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            if (dir >= 0 && dir < NUM_OF_DIRS)
                t->addVar("direction", dirs[rev_dir[dir]]);
            else
                t->addVar("direction", "none");
            t->addVar("actor", actor);
            return t->execute();
        }
    }

    return 1;
}

int command_wtrigger(char_data *actor, char *cmd, char *argument)
{
    struct room_data *room;
    char buf[MAX_INPUT_LENGTH];

    if (!actor || !SCRIPT_CHECK(actor->getRoom(), WTRIG_COMMAND))
        return 0;

    /* prevent people we like from becoming trapped :P */
    if (!valid_dg_target(actor, 0))
        return 0;

    room = actor->getRoom();
    for (auto t : room->script->dgScripts)
    {
        if (!TRIGGER_CHECK(t, WTRIG_COMMAND))
            continue;

        if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t))
        {
            mudlog(NRM, ADMLVL_BUILDER, true, "SYSERR: W-Command Trigger #%d has no text argument!",
                   GET_TRIG_VNUM(t));
            continue;
        }

        if (*GET_TRIG_ARG(t) == '*' ||
            !strncasecmp(GET_TRIG_ARG(t), cmd, strlen(GET_TRIG_ARG(t))))
        {
            t->addVar("actor", actor);
            skip_spaces(&argument);
            t->addVar("arg", argument);
            skip_spaces(&cmd);
            t->addVar("cmd", cmd);

            return t->execute();
        }
    }

    return 0;
}

void speech_wtrigger(char_data *actor, char *str)
{
    struct room_data *room;
    char buf[MAX_INPUT_LENGTH];

    if (!actor || !SCRIPT_CHECK(actor->getRoom(), WTRIG_SPEECH))
        return;

    room = actor->getRoom();
    for (auto t : room->script->dgScripts)
    {
        if (!TRIGGER_CHECK(t, WTRIG_SPEECH))
            continue;

        if (!GET_TRIG_ARG(t) || !*GET_TRIG_ARG(t))
        {
            mudlog(NRM, ADMLVL_BUILDER, true, "SYSERR: W-Speech Trigger #%d has no text argument!",
                   GET_TRIG_VNUM(t));
            continue;
        }

        if (*GET_TRIG_ARG(t) == '*' ||
            (GET_TRIG_NARG(t) && word_check(str, GET_TRIG_ARG(t))) ||
            (!GET_TRIG_NARG(t) && is_substring(GET_TRIG_ARG(t), str)))
        {
            t->addVar("actor", actor);
            t->addVar("speech", str);
            t->execute();
            break;
        }
    }
}

int drop_wtrigger(obj_data *obj, char_data *actor)
{
    struct room_data *room;
    char buf[MAX_INPUT_LENGTH];
    int ret_val;

    if (!actor || !SCRIPT_CHECK(actor->getRoom(), WTRIG_DROP))
        return 1;

    room = actor->getRoom();
    for (auto t : room->script->dgScripts)
        if (TRIGGER_CHECK(t, WTRIG_DROP) &&
            (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            t->addVar("actor", actor);
            t->addVar("object", obj);
            ret_val = t->execute();
            if (obj->carried_by != actor)
                return 0;
            else
                return ret_val;
            break;
        }

    return 1;
}

int cast_wtrigger(char_data *actor, char_data *vict, obj_data *obj, int spellnum)
{
    room_data *room;
    char buf[MAX_INPUT_LENGTH];

    if (!actor || !SCRIPT_CHECK(actor->getRoom(), WTRIG_CAST))
        return 1;

    room = actor->getRoom();
    for (auto t : room->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, WTRIG_CAST) &&
            (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {

            t->addVar("actor", actor);
            if (vict)
                t->addVar("victim", vict);
            if (obj)
                t->addVar("object", obj);
            sprintf(buf, "%d", spellnum);
            t->addVar("spell", buf);
            t->addVar("spellname", skill_name(spellnum));
            return t->execute();
        }
    }

    return 1;
}

int leave_wtrigger(struct room_data *room, char_data *actor, int dir)
{
    char buf[MAX_INPUT_LENGTH];

    if (!valid_dg_target(actor, DG_ALLOW_GODS))
        return 1;

    if (!SCRIPT_CHECK(room, WTRIG_LEAVE))
        return 1;

    for (auto t : room->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, WTRIG_LEAVE) &&
            (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            if (dir >= 0 && dir < NUM_OF_DIRS)
                t->addVar("direction",dirs[dir]);
            else
                t->addVar("direction", "none");
            t->addVar("actor", actor);
            return t->execute();
        }
    }

    return 1;
}

int door_wtrigger(char_data *actor, int subcmd, int dir)
{
    room_data *room;
    char buf[MAX_INPUT_LENGTH];

    if (!actor || !SCRIPT_CHECK(actor->getRoom(), WTRIG_DOOR))
        return 1;

    room = actor->getRoom();
    for (auto t : room->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, WTRIG_DOOR) &&
            (rand_number(1, 100) <= GET_TRIG_NARG(t)))
        {
            t->addVar("cmd", cmd_door[subcmd]);
            if (dir >= 0 && dir < NUM_OF_DIRS)
                t->addVar("direction", dirs[dir]);
            else
                t->addVar("direction", "none");
            t->addVar("actor", actor);
            return t->execute();
        }
    }

    return 1;
}

void time_wtrigger(struct room_data *room)
{
    char buf[MAX_INPUT_LENGTH];

    if (!SCRIPT_CHECK(room, WTRIG_TIME))
        return;

    for (auto t : room->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, WTRIG_TIME) &&
            (time_info.hours == GET_TRIG_NARG(t)))
        {
            sprintf(buf, "%d", time_info.hours);
            t->addVar("time", buf);
            t->execute();
            break;
        }
    }
}

void interval_wtrigger(struct room_data *room, int trigFlag)
{
    if (!SCRIPT_CHECK(room, WTRIG_TIME))
        return;

    for (auto t : room->script->dgScripts)
    {
        if (TRIGGER_CHECK(t, WTRIG_TIME))
        {
            t->execute();
            break;
        }
    }
}
