/**************************************************************************
*  File: dg_variables.c                                                   *
*  Usage: contains the functions dealing with variable substitution.      *
*                                                                         *
*                                                                         *
*  $Author: Mark A. Heilpern/egreen/Welcor $                              *
*  $Date: 2004/10/11 12:07:00 $                                           *
*  $Revision: 1.0.14 $                                                    *
**************************************************************************/

#include "dbat/structs.h"
#include "dbat/dg_scripts.h"
#include "dbat/utils.h"
#include "dbat/comm.h"
#include "dbat/interpreter.h"
#include "dbat/handler.h"
#include "dbat/dg_event.h"
#include "dbat/db.h"
#include "dbat/screen.h"
#include "dbat/constants.h"
#include "dbat/spells.h"
#include "dbat/oasis.h"
#include "dbat/class.h"
#include "dbat/races.h"

/* Utility functions */


/* perhaps not the best place for this, but I didn't want a new file */
char *skill_percent(struct char_data *ch, char *skill) {
    static char retval[16];
    int skillnum;

    skillnum = find_skill_num(skill, SKTYPE_SKILL);
    if (skillnum <= 0) return ("unknown skill");

    snprintf(retval, sizeof(retval), "%d", GET_SKILL(ch, skillnum));
    return retval;
}

/*
   search through all the persons items, including containers
   and 0 if it doesnt exist, and greater then 0 if it does!
   Jamie Nelson (mordecai@timespace.co.nz)
   MUD -- 4dimensions.org:6000

   Now also searches by vnum -- Welcor
   Now returns the number of matching objects -- Welcor 02/04
*/

int item_in_list(char *item, obj_data *list) {
    obj_data *i;
    int count = 0;

    if (!item || !*item)
        return 0;

    if (*item == UID_CHAR) {
        std::optional<DgUID> result;
        result = resolveUID(item);
        auto uidResult = result;
        if(!uidResult) return 0;
        if(uidResult->index() != 1) return 0;
        auto obj = std::get<1>(*uidResult);

        for (i = list; i; i = i->next_content) {
            if (i == obj)
                count++;
            if (GET_OBJ_TYPE(i) == ITEM_CONTAINER)
                count += item_in_list(item, i->contents);
        }
    } else if (is_number(item) > -1) { /* check for vnum */
        obj_vnum ovnum = atof(item);

        for (i = list; i; i = i->next_content) {
            if (GET_OBJ_VNUM(i) == ovnum)
                count++;
            if (GET_OBJ_TYPE(i) == ITEM_CONTAINER)
                count += item_in_list(item, i->contents);
        }
    } else {
        for (i = list; i; i = i->next_content) {
            if (isname(item, i->name))
                count++;
            if (GET_OBJ_TYPE(i) == ITEM_CONTAINER)
                count += item_in_list(item, i->contents);
        }
    }
    return count;
}

/*
   BOOLEAN return, just check if a player or mob
   has an item of any sort, searched for by name
   or id.
   searching equipment as well as inventory,
   and containers.
   Jamie Nelson (mordecai@timespace.co.nz)
   MUD -- 4dimensions.org:6000
*/

int char_has_item(char *item, struct char_data *ch) {

    /* If this works, no more searching needed */
    if (get_object_in_equip(ch, item) != nullptr)
        return 1;

    if (item_in_list(item, ch->contents) == 0)
        return 0;
    else
        return 1;
}


std::size_t matching_percent(const std::string& line, std::size_t start) {
    int depth = 0;

    for (auto i = start+1; i < line.size(); i++) {
        auto p = line[i];
        if (p == '(')
            depth++;
        else if (p == ')')
            depth--;
        else if (p == '%' && depth == 0)
            return i;
    }

    return line.size();
}


std::string scriptTimeHolder(trig_data *trig, const std::string& field, const std::string& args) {
    if(iequals(field, "hour")) {
        return fmt::format("{}", time_info.hours);
    } else if(iequals(field, "minute")) {
        return fmt::format("{}", time_info.minutes);
    } else if(iequals(field, "second")) {
        return fmt::format("{}", time_info.seconds);
    } else if(iequals(field, "day")) {
        return fmt::format("{}", time_info.day + 1);
    } else if(iequals(field, "month")) {
        return fmt::format("{}", time_info.month + 1);
    } else if(iequals(field, "year")) {
        return fmt::format("{}", time_info.year);
    }
    return "";
}

std::optional<std::size_t> findDot(const std::string& line) {
    int depth = 0;
    bool escaped = false;
    for(std::size_t i = 0; i < line.size(); i++) {
        if(escaped) {
            escaped = false;
            continue;
        }
        switch(line[i]) {
            case '(':
                depth++;
                break;
            case ')':
                depth--;
                break;
            case '\\':
                escaped = true;
                break;
            case '.':
                if(depth == 0) return i;
                break;
        }
    }
    return {};
}

std::optional<std::size_t> findEndParen(const std::string& line, std::size_t start) {
    int depth = 0;
    bool escaped = false;
    for(std::size_t i = start; i < line.size(); i++) {
        if(escaped) {
            escaped = false;
            continue;
        }
        switch(line[i]) {
            case '(':
                depth++;
                break;
            case ')':
                depth--;
                if(depth == 0) return i;
                break;
            case '\\':
                escaped = true;
                break;
        }
    }
    return {};
}

DgResults scriptFindMob(trig_data *trig, const std::string& field, const std::string& args) {
    if(args.empty() || field.empty()) return "0";
    room_rnum rrnum = real_room(atof(field.c_str()));
    mob_vnum mvnum = atof(args.c_str());
    if (rrnum == NOWHERE) {
        script_log("findmob.vnum(ovnum): No room with vnum %d", atof(field.c_str()));
        return "0";
    }
    auto i = 0;
    for (auto ch = world[rrnum].people; ch; ch = ch->next_in_room)
        if (GET_MOB_VNUM(ch) == mvnum)
            i++;
    return fmt::format("{}", i);

}

DgResults scriptFindObj(trig_data *trig, const std::string& field, const std::string& args) {
    if(args.empty() || field.empty()) return "0";
    room_rnum rrnum = real_room(atof(field.c_str()));

    if (rrnum == NOWHERE) {
        script_log("findobj.vnum(ovnum): No room with vnum %d", atof(field.c_str()));
        return "0";
    }
     /* item_in_list looks within containers as well. */
    return fmt::format("{}", item_in_list((char*)args.c_str(), world[rrnum].contents));
}

DgResults scriptGlobal(trig_data *trig, const std::string& field, const std::string& args) {
    // TODO: not implemented yet!
    return "";
}

DgResults scriptRandom(trig_data *trig, const std::string& field, const std::string& args) {
    auto type = trig->parent->attach_type;
    struct char_data *enactor;
    room_data *r;
    switch(type) {
            case MOB_TRIGGER:
                enactor = (struct char_data*)trig->sc->owner;
                r = enactor->getRoom();
                break;
            case OBJ_TRIGGER:
                r = ((struct obj_data*)trig->sc->owner)->getRoom();
                break;
            case WLD_TRIGGER:
                r = (struct room_data*)trig->sc->owner;
                break;
        }

    if(iequals(field, "char")) {
        if(!r) return "";
        std::vector<struct char_data*> candidates;
        for(auto c = r->people; c; c = c->next_in_room) {
            if(type == MOB_TRIGGER && !CAN_SEE(enactor, c)) continue;
            if(!valid_dg_target(c, DG_ALLOW_GODS)) continue;
            candidates.push_back(c);
        }
        if(candidates.empty()) return "0";
        auto can = Random::get(candidates);
        return (*can);
    } else if(iequals(field, "dir")) {
        if(!r) return "";
        std::vector<int> available;
        for (auto i = 0; i < NUM_OF_DIRS; i++)
            if (R_EXIT(r, i))
                available.push_back(i);

        if (available.empty()) {
            return "";
        } else {
            auto dir = Random::get(available);
            return dirs[*dir];
        }
    } else {
        if(auto num = atoi(field.c_str()); num >= 1) {
            return fmt::format("{}", Random::get(1, num));
        }
    }

    return "";
}

std::string scriptTextProcess(trig_data *trig, const std::string& text, const std::string& field, const std::string& args) {
        char *p, *p2;
    char tmpvar[MAX_STRING_LENGTH];

    if (iequals(field, "strlen")) {                     /* strlen    */
        return fmt::format("{}", text.size());
    } else if (iequals(field, "trim")) {                /* trim      */
        std::string clone(text);
        trim(clone);
        return clone;
    } else if (iequals(field, "contains")) {            /* contains  */
        return str_str((char*)text.c_str(), (char*)args.c_str()) ? "1" : "0";
    } else if (iequals(field, "car")) {
        auto sp = split(text, ' ');
        if(!sp.empty()) return sp[0];
        return "";
    } else if (iequals(field, "cdr")) {                 /* cdr       */
        auto cdr = text.c_str();
        while (*cdr && !isspace(*cdr)) cdr++; /* skip 1st field */
        while (*cdr && isspace(*cdr)) cdr++;  /* skip to next */
        return cdr;
    } else if (iequals(field, "charat")) {              /* CharAt    */
        size_t len = text.size(), dgindex = atoi(args.c_str());
        if (dgindex > len || dgindex < 1)
            return "";
        else
            return fmt::format("{}", text[dgindex - 1]);
    } else if (iequals(field, "mudcommand")) {
        /* find the mud command returned from this text */
/* NOTE: you may need to replace "cmd_info" with "complete_cmd_info", */
/* depending on what patches you've got applied.                      */

/* on older source bases:    extern struct command_info *cmd_info; */
        int length, cmd;
        for (length = text.size(), cmd = 0;
             *cmd_info[cmd].command != '\n'; cmd++)
            if (!strncmp(cmd_info[cmd].command, text.c_str(), length))
                break;

        if (*cmd_info[cmd].command == '\n')
            return "";
        else
            return fmt::format("{}", cmd_info[cmd].command);
    }

    return {};
}

DgResults checkForID(const std::string& text) {
    auto uidCheck = resolveUID(text);
    if(uidCheck) {
        auto uid = *uidCheck;
        switch(uid.index()) {
            case 0: {
                return std::get<0>(uid);
            }
            case 1: {
                return std::get<1>(uid);
            }
            case 2: {
                return std::get<2>(uid);
            }
        }
    }
    return text;
}


void trig_data::handleSubst(std::vector<DgHolder> &current, const std::string& field, const std::string& args) {
    auto type = parent->attach_type;
    
    if(current.empty()) {
        // it's the first run.
        if(iequals(field, "self")) {
            current.emplace_back(sc->owner);
        }
        else if(iequals(field, "time")) {
            current.emplace_back(std::function(scriptTimeHolder));
        }
        else if (iequals(field, "global")) {
            current.emplace_back(std::function(scriptGlobal));
        }
        else if(iequals(field, "people")) {
            int num = 0;
            auto f = fmt::format("{}", ((num = atoi(args.c_str())) > 0) ? trgvar_in_room(num) : 0);
            current.emplace_back(f);
        }
        else if(iequals(field, "findmob")) {
            current.emplace_back(std::function(scriptFindMob));
        }
        else if(iequals(field, "findobj")) {
            current.emplace_back(std::function(scriptFindObj));
        }
        else if(iequals(field, "random")) {
            current.emplace_back(std::function(scriptRandom));
        }
        else if (iequals(field, "ctime"))
            current.emplace_back(fmt::format("{}", time(nullptr)));
        else if (iequals(field, "door"))
            current.emplace_back(door[type]);
        else if (iequals(field, "force"))
            current.emplace_back(force[type]);
        else if (iequals(field, "load"))
            current.emplace_back(load[type]);
        else if (iequals(field, "purge"))
            current.emplace_back(purge[type]);
        else if (iequals(field, "teleport"))
            current.emplace_back(teleport[type]);
        else if (iequals(field, "damage"))
            current.emplace_back(xdamage[type]);
        else if (iequals(field, "send"))
            current.emplace_back(send_cmd[type]);
        else if (iequals(field, "echo"))
            current.emplace_back(echo_cmd[type]);
        else if (iequals(field, "echoaround"))
            current.emplace_back(echoaround_cmd[type]);
        else if (iequals(field, "zoneecho"))
            current.emplace_back(zoneecho[type]);
        else if (iequals(field, "asound"))
            current.emplace_back(asound[type]);
        else if (iequals(field, "at"))
            current.emplace_back(at[type]);
        else if (iequals(field, "transform"))
            current.emplace_back(transform[type]);
        else if (iequals(field, "recho"))
            current.emplace_back(recho[type]);
        else if(hasVar(field)) {
            auto res = getVar(field);
            if(res.index() == 0) {
                current.emplace_back(std::get<0>(res));
            } else {
                current.emplace_back(std::get<1>(res));
            }
        } else {
            current.emplace_back("");
        }
        return;
    }

    auto back = current.back();
    switch(back.index()) {
        case 0: {
            // Strings. strings will invoke the string manipulation funcs.
            auto s = std::get<0>(back);
            current.emplace_back(scriptTextProcess(this, s, field, args));
            }
            break;
        case 1: {
            // a unit_data*.
            auto u = std::get<1>(back);
            // here we'll call the dgCallMember method and set the result into current...
            auto res = u->dgCallMember(this, field, args);
            if(res.index() == 0) {
                    current.emplace_back(std::get<0>(res));
                } else {
                    current.emplace_back(std::get<1>(res));
                }
            }
            break;
        case 2: {
                // a function.
                auto f = std::get<2>(back);
                // here we'll call the function and set the result into current...
                auto res = f(this, field, args);
                if(res.index() == 0) {
                    current.emplace_back(std::get<0>(res));
                } else {
                    current.emplace_back(std::get<1>(res));
                }
            }
            break;
    }
}

std::string trig_data::innerSubst(std::vector<DgHolder>& current, const std::string& expr) {
    // This function will only be called on a string where the first character is right past a %  
    std::string chopped = expr;

    std::string field, args;
    bool finished = false;

    while(!chopped.empty()) {
        // if the first character is a %, we must recurse.
        if(chopped[0] == '%') {
            chopped = evalExpr(chopped);
            continue;
        }
        // After this SHOULD BE a field (no spaces allowed), which ends in a either a dot, open paren, or %.
        // First, let's find the end of the field.
        std::size_t end = 0;
        char found = -1;
        for(std::size_t i = 0; i < chopped.size(); i++) {
            if(chopped[i] == '.' || chopped[i] == '(' || chopped[i] == '%') {
                end = i;
                found = chopped[i];
                break;
            }
        }
        field = chopped.substr(0, end);
        switch(found) {
            case -1:
                // we reached the very end and found no %.
                throw DgScriptException("Unexpected end of string in innerSubst. No % found.");
                break;
            case '.': {
                // we found a dot. Capture the word up to but not including end.
                // be careful to not go past the end of the string. the . might be the end.
                if(end+1 < chopped.size()) {
                    chopped = chopped.substr(end+1);
                } else {
                    throw DgScriptException("Unexpected end of string in innerSubst. Dot with no following field.");
                }
            }
                break;
            case '(': {
                // An opening parentheses. We need to find the matching closing parentheses.
                if(auto endParen = findEndParen(chopped, end); endParen) {
                    args = chopped.substr(end+1, endParen.value()-end-1);
                    args = evalExpr(args);
                    // Be careful to not go past the end of the string. The closing paren might be the end.
                    if(endParen.value()+1 < chopped.size()) {
                        chopped = chopped.substr(endParen.value()+1);
                    } else {
                        chopped = "";
                    }
                    break;
                case 'l':
                    if (!strcasecmp(field, "level"))
                        snprintf(str, slen, "%d", GET_OBJ_LEVEL(o));
                    break;

                case 'n':
                    if (!strcasecmp(field, "name")) {
                        if (!subfield || !*subfield)
                            snprintf(str, slen, "%s", o->name);
                        else {
                            char blah[500];
                            sprintf(blah, "%s %s", o->name, subfield);
                            o->name = strdup(blah);
                        }
                    } else if (!strcasecmp(field, "next_in_list")) {
                        if (o->next_content)
                            snprintf(str, slen, "%s", ((o->next_content)->getUID(false).c_str()));
                        else
                            *str = '\0';
                    }
                    break;
                case 'r':
                    if (!strcasecmp(field, "room")) {
                        if (auto roomFound = world.find(obj_room(o)); roomFound != world.end())
                            snprintf(str, slen, "%s", roomFound->second.getUID(false).c_str());
                        else
                            *str = '\0';
                    }
                    break;
                case 's':
                    if (!strcasecmp(field, "shortdesc")) {
                        if (!subfield || !*subfield)
                            snprintf(str, slen, "%s", o->short_description);
                        else {
                            char blah[500];
                            sprintf(blah, "%s @wnicknamed @D(@C%s@D)@n", o->short_description, subfield);
                            o->short_description = strdup(blah);
                        }
                    } else if (!strcasecmp(field, "setaffects")) {
                        if (subfield && *subfield) {
                            int ns;
                            if ((ns = check_flags_by_name_ar(GET_OBJ_PERM(o), NUM_AFF_FLAGS, subfield, affected_bits)) >
                                0) {
                                o->bitvector.flip(ns);
                                snprintf(str, slen, "1");
                            }
                        }
                    } else if (!strcasecmp(field, "setextra")) {
                        if (subfield && *subfield) {
                            int ns;
                            if ((ns = check_flags_by_name_ar(GET_OBJ_EXTRA(o), NUM_ITEM_FLAGS, subfield, extra_bits)) >
                                0) {
                                o->extra_flags.flip(ns);
                                snprintf(str, slen, "1");
                            }
                        }
                    } else if (!strcasecmp(field, "size")) {
                        if (subfield && *subfield) {
                            int ns;
                            if ((ns = search_block(subfield, size_names, false)) > -1) {
                                (o)->size = ns;
                            }
                        }
                        sprinttype(GET_OBJ_SIZE(o), size_names, str, slen);
                    }
                    break;
                case 't':
                    if (!strcasecmp(field, "type"))
                        sprinttype(GET_OBJ_TYPE(o), item_types, str, slen);

                    else if (!strcasecmp(field, "timer"))
                        snprintf(str, slen, "%d", GET_OBJ_TIMER(o));
                    break;
                case 'v':
                    if (!strcasecmp(field, "vnum"))
                        if (subfield && *subfield) {
                            snprintf(str, slen, "%d", (int) (GET_OBJ_VNUM(o) == atof(subfield)));
                        } else {
                            snprintf(str, slen, "%d", GET_OBJ_VNUM(o));
                        }
                    else if (!strcasecmp(field, "val0"))
                        snprintf(str, slen, "%d", GET_OBJ_VAL(o, 0));

                    else if (!strcasecmp(field, "val1"))
                        snprintf(str, slen, "%d", GET_OBJ_VAL(o, 1));

                    else if (!strcasecmp(field, "val2"))
                        snprintf(str, slen, "%d", GET_OBJ_VAL(o, 2));

                    else if (!strcasecmp(field, "val3"))
                        snprintf(str, slen, "%d", GET_OBJ_VAL(o, 3));

                    else if (!strcasecmp(field, "val4"))
                        snprintf(str, slen, "%d", GET_OBJ_VAL(o, 4));

                    else if (!strcasecmp(field, "val5"))
                        snprintf(str, slen, "%d", GET_OBJ_VAL(o, 5));

                    else if (!strcasecmp(field, "val6"))
                        snprintf(str, slen, "%d", GET_OBJ_VAL(o, 6));

                    else if (!strcasecmp(field, "val7"))
                        snprintf(str, slen, "%d", GET_OBJ_VAL(o, 7));
                    break;
                case 'w':
                    if (!strcasecmp(field, "weight")) {
                        if (subfield && *subfield) {
                            auto addition = atof(subfield);
                            if (addition < 0 || addition > 0) {
                                GET_OBJ_WEIGHT(o) = std::max<double>(0, addition + GET_OBJ_WEIGHT(o));
                            } else {
                                GET_OBJ_WEIGHT(o) = 0;
                            }
                        }
                        snprintf(str, slen, "%s", fmt::format("{}", GET_OBJ_WEIGHT(o)).c_str());
                    } else if (!strcasecmp(field, "worn_by")) {
                        if (o->worn_by)
                            snprintf(str, slen, "%s", ((o->worn_by)->getUID(false).c_str()));
                        else
                            *str = '\0';
                    }
                    break;
            } /* switch *field */


            if (*str == '\x1') { /* no match in switch */
                if (SCRIPT(o)) { /* check for global var */
                    for (vd = (SCRIPT(o))->global_vars; vd; vd = vd->next)
                        if (!strcasecmp(vd->name, field))
                            break;
                    if (vd)
                        snprintf(str, slen, "%s", vd->value);
                    else {
                        *str = '\0';
                        if (strcasecmp(GET_TRIG_NAME(trig), "Rename Object")) {
                            script_log("Trigger: %s, VNum %d, type: %d. unknown object field: '%s'",
                                       GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), type, field);
                        }
                    }
                } else {
                    *str = '\0';
                    if (strcasecmp(GET_TRIG_NAME(trig), "Rename Object")) {
                        script_log("Trigger: %s, VNum %d, type: %d. unknown object field: '%s'",
                                   GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), type, field);
                    }
                }
            }
        } /* if (o) ... */

        else if (r) {

            if(auto result = r->dgCallMember(field, subfield ? subfield : ""); result) {
                snprintf(str, slen, "%s", result->c_str());
                return;
            }

            /* special handling of the void, as it stores all 'full global' variables */
            if (r->vn == 0) {
                if (!SCRIPT(r)) {
                    *str = '\0';
                    script_log(
                            "Trigger: %s, Vnum %d, type %d. Trying to access Global var list of void. Apparently this has not been set up!",
                            GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), type);
                } else {
                    for (vd = (SCRIPT(r))->global_vars; vd; vd = vd->next)
                        if (!strcasecmp(vd->name, field))
                            break;
                    if (vd)
                        snprintf(str, slen, "%s", vd->value);
                    else
                        *str = '\0';
                }
            } else if (!strcasecmp(field, "name"))
                snprintf(str, slen, "%s", r->name);

            else if (!strcasecmp(field, "sector"))
                sprinttype(r->sector_type, sector_types, str, slen);

            else if (!strcasecmp(field, "gravity"))
                snprintf(str, slen, "%d", (int)r->getGravity());

            else if (!strcasecmp(field, "vnum")) {
                if (subfield && *subfield) {
                    snprintf(str, slen, "%d", (int) (r->vn == atof(subfield)));
                } else {
                    snprintf(str, slen, "%d", r->vn);
                }
            } else if (!strcasecmp(field, "contents")) {
                if (subfield && *subfield) {
                    for (obj = r->contents; obj; obj = obj->next_content) {
                        if (GET_OBJ_VNUM(obj) == atof(subfield)) {
                            /* arg given, found */
                            snprintf(str, slen, "%s", ((obj)->getUID(false).c_str()));
                            return;
                        }
                    }
                    if (!obj)
                        *str = '\0'; /* arg given, not found */
                } else { /* no arg given */
                    if (r->contents) {
                        snprintf(str, slen, "%s", ((r->contents)->getUID(false).c_str()));
                    } else {
                        *str = '\0';
                    }
                }
            } else if (!strcasecmp(field, "people")) {
                if (r->people)
                    snprintf(str, slen, "%s", ((r->people)->getUID(false).c_str()));
                else
                    *str = '\0';
            } else if (!strcasecmp(field, "id")) {
                if (r->vn != NOWHERE)
                    snprintf(str, slen, "%s", r->getUID(false).c_str());
                else
                    *str = '\0';
            } else if (!strcasecmp(field, "weather")) {
                const char *sky_look[] = {
                        "sunny",
                        "cloudy",
                        "rainy",
                        "lightning"
                };

                if (!r->room_flags.test(ROOM_INDOORS))
                    snprintf(str, slen, "%s", sky_look[weather_info.sky]);
                else
                    *str = '\0';
            } else if (!strcasecmp(field, "fishing")) {
                if (ROOM_FLAGGED(r, ROOM_FISHING))
                    snprintf(str, slen, "1");
                else
                    snprintf(str, slen, "0");
            } else if (!strcasecmp(field, "zonenumber"))
                snprintf(str, slen, "%d", zone_table[r->zone].number);
            else if (!strcasecmp(field, "zonename"))
                snprintf(str, slen, "%s", zone_table[r->zone].name);
            else if (!strcasecmp(field, "roomflag")) {
                if (subfield && *subfield) {
                    if (check_flags_by_name_ar(r->room_flags, NUM_ROOM_FLAGS, subfield, room_bits) > 0)
                        snprintf(str, slen, "1");
                    else
                        snprintf(str, slen, "0");
                } else
                    snprintf(str, slen, "0");
            } else {
                if (SCRIPT(r)) { /* check for global var */
                    for (vd = (SCRIPT(r))->global_vars; vd; vd = vd->next)
                        if (!strcasecmp(vd->name, field))
                            break;
                    if (vd)
                        snprintf(str, slen, "%s", vd->value);
                    else {
                        *str = '\0';
                        script_log("Trigger: %s, VNum %d, type: %d. unknown room field: '%s'",
                                   GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), type, field);
                    }
                } else {
                    *str = '\0';
                    script_log("Trigger: %s, VNum %d, type: %d. unknown room field: '%s'",
                               GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), type, field);
                }
            }
        } /* if (r).. */
    }
}

/*
 * Now automatically checks if the variable has more then one field
 * in it. And if the field returns a name or a script UID or the like
 * it can recurse.
 * If you supply a value like, %actor.int.str% it wont blow up on you
 * either.
 * - Jamie Nelson 31st Oct 2003 01:03
 *
 * Now also lets subfields have variables parsed inside of them
 * so that:
 * %echo% %actor.gold(%actor.gold%)%
 * will double the actors gold every time its called.  etc...
 * - Jamie Nelson 31st Oct 2003 01:24
 */

/* substitutes any variables into line and returns it as buf */
void var_subst(void *go, struct script_data *sc, trig_data *trig,
               int type, char *line, char *buf) {
    char tmp[MAX_INPUT_LENGTH], repl_str[MAX_INPUT_LENGTH];
    char *var = nullptr, *field = nullptr, *p = nullptr;
    char tmp2[MAX_INPUT_LENGTH];
    char *subfield_p, subfield[MAX_INPUT_LENGTH];
    int left, len;
    int paren_count = 0;
    int dots = 0;

    if(boost::istarts_with(line, "eval loss 500 + %500000.random%")) {
        script_log("barf!");
    }

    /* skip out if no %'s */
    if (!strchr(line, '%')) {
        strcpy(buf, line);
        return;
    }
    /*lets just empty these to start with*/
    *repl_str = *tmp = *tmp2 = '\0';

    p = strcpy(tmp, line);
    subfield_p = subfield;

    left = MAX_INPUT_LENGTH - 1;

    while (*p && (left > 0)) {


        /* copy until we find the first % */
        while (*p && (*p != '%') && (left > 0)) {
            *(buf++) = *(p++);
            left--;
        }

        *buf = '\0';

        /* double % */
        if (*p && (*(++p) == '%') && (left > 0)) {
            *(buf++) = *(p++);
            *buf = '\0';
            left--;
            continue;
        }

            /* so it wasn't double %'s */
        else if (*p && (left > 0)) {

            /* search until end of var or beginning of field */
            for (var = p; *p && (*p != '%') && (*p != '.'); p++);

            field = p;
            if (*p == '.') {
                *(p++) = '\0';
                dots = 0;
                for (field = p; *p && ((*p != '%') || (paren_count > 0) || (dots)); p++) {
                    if (dots > 0) {
                        *subfield_p = '\0';
                        find_replacement(go, sc, trig, type, var, field, subfield, repl_str, sizeof(repl_str));
                        if (*repl_str) {
                            snprintf(tmp2, sizeof(tmp2), "eval tmpvr %s", repl_str); //temp var
                            process_eval(go, sc, trig, type, tmp2);
                            strcpy(var, "tmpvr");
                            field = p;
                            dots = 0;
                            continue;
                        }
                        dots = 0;
                    } else if (*p == '(') {
                        *p = '\0';
                        paren_count++;
                    } else if (*p == ')') {
                        *p = '\0';
                        paren_count--;
                    } else if (paren_count > 0) {
                        *subfield_p++ = *p;
                    } else if (*p == '.') {
                        *p = '\0';
                        dots++;
                    }
                } /* for (field.. */
            } /* if *p == '.' */

            *(p++) = '\0';
            *subfield_p = '\0';

            if (*subfield) {
                var_subst(go, sc, trig, type, subfield, tmp2);
                strcpy(subfield, tmp2);
            }

            find_replacement(go, sc, trig, type, var, field, subfield, repl_str, sizeof(repl_str) - 1);

            strncat(buf, repl_str, left);
            len = strlen(repl_str);
            buf += len;
            left -= len;
        } /* else if *p .. */
    } /* while *p .. */
}
