/**************************************************************************
*  File: dg_scripts.c                                                     *
*  Usage: contains the main script driver interface.                      *
*                                                                         *
*                                                                         *
*  $Author: Mark A. Heilpern/egreen/Welcor $                              *
*  $Date: 2004/10/11 12:07:00$                                            *
*  $Revision: 1.0.14 $                                                    *
**************************************************************************/

#include "dbat/dg_scripts.h"
#include "dbat/act.wizard.h"
#include "dbat/dg_event.h"
#include "dbat/utils.h"
#include "dbat/interpreter.h"
#include "dbat/handler.h"
#include "dbat/constants.h"
#include "dbat/comm.h"
#include "dbat/players.h"

#define PULSES_PER_MUD_HOUR     (SECS_PER_MUD_HOUR*PASSES_PER_SEC)

void obj_command_interpreter(obj_data *obj, char *argument);
void wld_command_interpreter(room_data *room, char *argument);

void trig_data::setState(DgScriptState st) {
    state = st;
    auto sh = shared_from_this();
    switch(state) {
        case DgScriptState::WAITING:
            triggers_waiting.push_back(sh);
            break;
        default:
            triggers_waiting.remove_if([sh](const std::weak_ptr<trig_data>& t) {
                return t.expired() || t.lock() == sh;
            });
            break;
    }
}

void trig_data::reset() {
    vars.clear();
    setState(DgScriptState::DORMANT);
    depth.clear();
    loops = 0;
    totalLoops = 0;
    waiting = 0.0;
}

int trig_data::execute() {
    try {
        switch(state) {
            case DgScriptState::RUNNING:
            case DgScriptState::ERROR:
            case DgScriptState::DONE:
            case DgScriptState::PURGED:
                throw DgScriptException(fmt::format("Script called in Invalid State: {}", (int)state));
            case DgScriptState::WAITING:
                // do a thing here..
                break;
        }
        if(parent->lines.empty()) {
            throw DgScriptException("Script has no lines to execute.");
        }
        setState(DgScriptState::RUNNING);
        auto results = executeBlock(lineNumber, parent->lines.size());
        if(state != DgScriptState::WAITING) reset();
        return results;
    }
    catch(const DgScriptException& err) {
        script_log("DgScript Error on %s: '%s' - DgScript ID %d, Name: %s. Exception: %s", 
            sc->owner->getUID(false).c_str(), sc->owner->name, GET_TRIG_VNUM(this), GET_TRIG_NAME(this), err.what());
        reset();
        return 0;
    }
}

std::string trig_data::getLine(std::size_t num) {
    std::string line = parent->lines[num];
    trim(line);
    return line;
}

int trig_data::executeBlock(std::size_t start, std::size_t end) {
    int ret_val = 1;

    lineNumber = start;

    while(lineNumber < end) {
        if(loops >= 500) {
            setState(DgScriptState::WAITING);
            waiting = 1.0;
            loops = 0;
            return ret_val;
        }

        loops++;
        totalLoops++;

        if(totalLoops >= 5000)
            throw DgScriptException("Runaway script halted!");

        auto line = getLine(lineNumber);
        auto l = line;
        to_lower(l);
        if(line.empty() || line.starts_with("*")) {
            // comment or empty...
        }

        // Handle the if control flow.
        else if(l.starts_with("if ")) {
            depth.emplace_back(NestType::IF, lineNumber);
            if(!processIf(line.substr(3, line.size()))) {
                lineNumber = findElseEnd();
                continue;
            }
        }
        else if(l.starts_with("elseif ")) {
            if(depth.empty() || depth.back().first != NestType::IF)
                throw DgScriptException("'elseif' outside of an if!");
            lineNumber = findEnd();
            continue;
        }
        else if(l.starts_with("else ") || l == "else") {
            if(depth.empty() || depth.back().first != NestType::IF)
                throw DgScriptException("'else' outside of an if!");
            lineNumber = findEnd();
            continue;
        }
        else if(l.starts_with("end ") || l == "end") {
            if(depth.empty() || depth.back().first != NestType::IF)
                throw DgScriptException("'end' outside of an if!");
            depth.pop_back();
        }

        // Handle the while control flow.
        else if(l.starts_with("while ")) {
            depth.emplace_back(NestType::WHILE, lineNumber);
            if(!processIf(line.substr(6))) {
                lineNumber = findDone() + 1;
                depth.pop_back();
                continue;
            }
        }

        // Handle the switch control flow...
        else if(l.starts_with("switch ")) {
            depth.emplace_back(NestType::SWITCH, lineNumber);
            lineNumber = findCase(line.substr(7));
            continue;
        }

        else if(l.starts_with("break ") || l == "break") {
            if(depth.empty() || depth.back().first != NestType::SWITCH)
                throw DgScriptException("'break' outside of a switch-case block!");
            lineNumber = findDone();
            continue;
        }

        else if(l.starts_with("case ")) {
            if(depth.empty() || depth.back().first != NestType::SWITCH)
                throw DgScriptException("'case' outside of a switch-case block!");
            // Fall through behavior mimicking C switch.
            lineNumber++;
            continue;
        }

        else if(l.starts_with("done ") || l == "done") {
            if(depth.empty() || !(depth.back().first == NestType::SWITCH || depth.back().first == NestType::WHILE))
                throw DgScriptException("done outside of switch-case or while block!");
            auto back = depth.back();
            switch(back.first) {
                case NestType::WHILE:
                    lineNumber = back.second;
                    depth.pop_back();
                    continue;
                    break;
                case NestType::SWITCH:
                    depth.pop_back();
                    break;
            }
        }

        // It's a normal line to process...
        else {
            auto cmdres = varSubst(line);
            trim(cmdres);
            auto cmd = (char*)cmdres.c_str();

            if (!strncasecmp(cmd, "eval ", 5))
                processEval(cmdres);

            else if (!strncasecmp(cmd, "nop ", 4)); /* nop: do nothing */

            else if (!strncasecmp(cmd, "extract ", 8))
                extractValue(cmdres);

            else if (!strncasecmp(cmd, "halt", 4))
                break;

            else if (!strncasecmp(cmd, "global ", 7))
                processGlobal(cmdres);

            else if (!strncasecmp(cmd, "context ", 8))
                processContext(cmdres);

            else if (!strncasecmp(cmd, "remote ", 7))
                processRemote(cmdres);

            else if (!strncasecmp(cmd, "rdelete ", 8))
                processRdelete(cmdres);

            else if (!strncasecmp(cmd, "return ", 7))
                ret_val = atoi(cmdres.c_str());

            else if (!strncasecmp(cmd, "set ", 4))
                processSet(cmdres);

            else if (!strncasecmp(cmd, "unset ", 6))
                processUnset(cmdres);

            else if (!strncasecmp(cmd, "wait ", 5)) {
                processWait(cmdres);
                loops--;
                lineNumber++;
                return ret_val;
            } else if (!strncasecmp(cmd, "attach ", 7))
                processAttach(cmdres);

            else if (!strncasecmp(cmd, "detach ", 7))
                processDetach(cmdres);

            else if (!strncasecmp(cmd, "version", 7))
                mudlog(NRM, ADMLVL_GOD, true, "%s", DG_SCRIPT_VERSION);

            else {
                switch (parent->attach_type) {
                    case MOB_TRIGGER:
                        command_interpreter((char_data *) sc->owner, cmd);
                        break;
                    case OBJ_TRIGGER:
                        obj_command_interpreter((obj_data *) sc->owner, cmd);
                        break;
                    case WLD_TRIGGER:
                        wld_command_interpreter((struct room_data *)sc->owner, cmd);
                        break;
                }
                if (sc->purged) {
                    loops--;
                    setState(DgScriptState::PURGED);
                    return ret_val;
                }
            }
        }
        lineNumber++;
    }
    return ret_val;

}

std::size_t trig_data::findElseEnd(bool matchElseIf, bool matchElse) {
    if(depth.empty() || depth.back().first != NestType::IF)
        throw DgScriptException("findElseEnd called outside of if!");

    auto i = depth.back().second + 1;
    auto total = parent->lines.size();

    while(i < total) {
        auto line = getLine(i);
        auto l = line;
        to_lower(l);

        if(l.empty() || l.starts_with("*")) {}
        else if(matchElseIf && (l.starts_with("elseif ") && processIf(line.substr(7)))) {
            return i+1;
        }
        else if(matchElse && (l.starts_with("else ") || l == "else")) {
            return i+1;
        }
        else if(l.starts_with("end") || l == "end") {
            return i;
        }
        else if(l.starts_with("if ")) {
            depth.emplace_back(NestType::IF, i);
            i = findEnd() + 1;
            depth.pop_back();
            continue;
        }
        else if(l.starts_with("switch ")) {
            depth.emplace_back(NestType::SWITCH, i);
            i = findDone() + 1;
            depth.pop_back();
            continue;
        }
        else if(l.starts_with("while ")) {
            depth.emplace_back(NestType::WHILE, i);
            i = findDone() + 1;
            depth.pop_back();
            continue;
        }
        else if(l.starts_with("default ") || l == "default") {
            throw DgScriptException("'default' outside of a switch-case block!");
        }
        else if(l.starts_with("done ") || l == "done") {
            throw DgScriptException("'done' outside of a switch-case or while block!");
        }
        else if(l.starts_with("case ") || l == "case") {
            throw DgScriptException("'case' outside of a switch-case block");
        }
        i++;
    }
    throw DgScriptException("'if' without corresponding end!");
}

std::size_t trig_data::findEnd() {
    return findElseEnd(false, false);
}

std::size_t trig_data::findDone() {
    if(depth.empty() || !(depth.back().first == NestType::SWITCH || depth.back().first == NestType::WHILE))
        throw DgScriptException("findDone called outside of switch-case or while block!");
    
    auto t = depth.back().first;
    auto i = depth.back().second + 1;
    auto total = parent->lines.size();

    while(i < total) {
        auto line = getLine(i);
        auto l = line;
        to_lower(l);

        if(l.empty() || l.starts_with("*")) {}
        else if(l.starts_with("if ")) {
            depth.emplace_back(NestType::IF, i);
            i = findEnd() + 1;
            depth.pop_back();
            continue;
        }
        else if(l.starts_with("switch ")) {
            depth.emplace_back(NestType::SWITCH, i);
            i = findDone() + 1;
            depth.pop_back();
            continue;
        }
        else if(l.starts_with("while ")) {
            depth.emplace_back(NestType::WHILE, i);
            i = findDone() + 1;
            depth.pop_back();
            continue;
        }
        else if(l.starts_with("elseif ")) {
            throw DgScriptException("'elseif' outside of an if block");
        }
        else if(l.starts_with("else ") || l == "else") {
            throw DgScriptException("'else' outside of an if block");
        }
        else if(l.starts_with("end ") || l == "end") {
            throw DgScriptException("'end' outside of an if block");
        }
        else if(t == NestType::WHILE && (l.starts_with("default ") || l == "default")) {
            throw DgScriptException("'default' outside of a switch-case block!");
        }
        else if(l.starts_with("done ") || l == "done") {
            return i;
        }

        i++;
    }
    throw DgScriptException("'switch/while' without corresponding end!");
}

std::size_t trig_data::findCase(const std::string& cond) {
    if(depth.empty() || depth.back().first != NestType::SWITCH)
        throw DgScriptException("findCase called outside of switch-case block!");

    auto res = evalExpr(cond);

    auto t = depth.back().first;
    auto i = depth.back().second + 1;
    auto total = parent->lines.size();

    while(i < total) {
        auto line = getLine(i);
        auto l = line;
        to_lower(l);
        if(l.empty() || l.starts_with("*")) {}
        if(l.starts_with("case ") && truthy(evalOp("==", res, line.substr(5)))) {
            return i + 1;
        }
        else if(l.starts_with("default ") || l == "default") {
            return i + 1;
        }
        else if(l.starts_with("done ") || l == "done") {
            return i;
        }
        else if(l.starts_with("if ")) {
            depth.emplace_back(NestType::IF, i);
            i = findEnd() + 1;
            depth.pop_back();
            continue;
        }
        else if(l.starts_with("switch ")) {
            depth.emplace_back(NestType::SWITCH, i);
            i = findDone() + 1;
            depth.pop_back();
            continue;
        }
        else if(l.starts_with("while ")) {
            depth.emplace_back(NestType::WHILE, i);
            i = findDone() + 1;
            depth.pop_back();
            continue;
        }
        else if(l.starts_with("elseif ")) {
            throw DgScriptException("'elseif' outside of an if block");
        }
        else if(l.starts_with("else ") || l == "else") {
            throw DgScriptException("'else' outside of an if block");
        }
        else if(l.starts_with("end ") || l == "end") {
            throw DgScriptException("'end' outside of an if block");
        }

        i++;
    }
    throw DgScriptException("'switch-case' without corresponding done!");
}


/* Local functions not used elsewhere */
void do_stat_trigger(struct char_data *ch, const std::shared_ptr<trig_data> &trig);

void script_stat(char_data *ch, const std::shared_ptr<script_data>& sc);

int remove_trigger(struct script_data *sc, char *name);

bool is_num(const std::string &arg);

char *matching_paren(char *p);

int fgetline(FILE *file, char *p);

ACMD(do_attach);

ACMD(do_detach);

ACMD(do_vdelete);

ACMD(do_tstat);

/* Return pointer to first occurrence of string ct in */
/* cs, or nullptr if not present.  Case insensitive */
char *str_str(char *cs, char *ct) {
    char *s, *t;

    if (!cs || !ct || !*ct)
        return nullptr;

    while (*cs) {
        t = ct;

        while (*cs && (LOWER(*cs) != LOWER(*t)))
            cs++;

        s = cs;

        while (*t && *cs && (LOWER(*cs) == LOWER(*t))) {
            t++;
            cs++;
        }

        if (!*t)
            return s;
    }

    return nullptr;
}


int trgvar_in_room(room_vnum vnum) {
    room_rnum rnum = real_room(vnum);
    int i = 0;
    char_data *ch;

    if (rnum == NOWHERE) {
        script_log("people.vnum: world[rnum] does not exist");
        return (-1);
    }

    for (ch = world[rnum].people; ch != nullptr; ch = ch->next_in_room)
        i++;

    return i;
}

obj_data *get_obj_in_list(char *name, obj_data *list) {
    obj_data *i;
    int32_t id;

    if (*name == UID_CHAR) {
        std::optional<DgUID> result;
        result = resolveUID(name);
        auto uidResult = result;
        if(!uidResult) return nullptr;
        if(uidResult->index() != 1) return nullptr;
        auto obj = std::get<1>(*uidResult);

        for (i = list; i; i = i->next_content)
            if(i == obj) return obj;
    } else {
        for (i = list; i; i = i->next_content)
            if (isname(name, i->name))
                return i;
    }

    return nullptr;
}

obj_data *get_object_in_equip(char_data *ch, char *name) {
    int j, n = 0, number;
    obj_data *obj;
    char tmpname[MAX_INPUT_LENGTH];
    char *tmp = tmpname;
    int32_t id;

    if (*name == UID_CHAR) {
        std::optional<DgUID> result;
        result = resolveUID(name);
        auto uidResult = result;
        if(!uidResult) return nullptr;
        if(uidResult->index() != 1) return nullptr;
        auto o = std::get<1>(*uidResult);

        for (j = 0; j < NUM_WEARS; j++)
            if ((obj = GET_EQ(ch, j)))
                if (o == obj) return obj;
    } else if (is_number(name)) {
        obj_vnum ovnum = atoi(name);
        for (j = 0; j < NUM_WEARS; j++)
            if ((obj = GET_EQ(ch, j)))
                if (GET_OBJ_VNUM(obj) == ovnum)
                    return (obj);
    } else {
        snprintf(tmpname, sizeof(tmpname), "%s", name);
        if (!(number = get_number(&tmp)))
            return nullptr;

        for (j = 0; (j < NUM_WEARS) && (n <= number); j++)
            if ((obj = GET_EQ(ch, j)))
                if (isname(tmp, obj->name))
                    if (++n == number)
                        return (obj);
    }

    return nullptr;
}

static struct eq_pos_list {
    const char *pos;
    int where;
} eq_pos[] = {
        {"hold",      WEAR_WIELD2},
        {"held",      WEAR_WIELD2},
        {"light",     WEAR_WIELD2},
        {"wield",     WEAR_WIELD1},
        {"rfinger",   WEAR_FINGER_R},
        {"lfinger",   WEAR_FINGER_L},
        {"neck1",     WEAR_NECK_1},
        {"neck2",     WEAR_NECK_2},
        {"body",      WEAR_BODY},
        {"head",      WEAR_HEAD},
        {"legs",      WEAR_LEGS},
        {"feet",      WEAR_FEET},
        {"hands",     WEAR_HANDS},
        {"arms",      WEAR_ARMS},
        {"shield",    WEAR_WIELD2},
        {"about",     WEAR_ABOUT},
        {"waist",     WEAR_WAIST},
        {"rwrist",    WEAR_WRIST_R},
        {"lwrist",    WEAR_WRIST_L},
        {"backpack",  WEAR_BACKPACK},
        {"rear",      WEAR_EAR_R},
        {"lear",      WEAR_EAR_L},
        {"shoulders", WEAR_SH},
        {"scouter",   WEAR_EYE},
        {"none", -1}
};

/* Handles 'held', 'light' and 'wield' positions - Welcor
   After idea from Byron Ellacott - bje@apnic.net */
int find_eq_pos_script(char *arg) {
    int i;

    if (is_number(arg) && (i = atoi(arg)) >= 0 && i < NUM_WEARS)
        return i;

    for (i = 0; eq_pos[i].where != -1; i++) {
        if (!strcasecmp(eq_pos[i].pos, arg))
            return eq_pos[i].where;
    }
    return (-1);
}

int can_wear_on_pos(struct obj_data *obj, int pos) {
    switch (pos) {
        case WEAR_WIELD1:
            return CAN_WEAR(obj, ITEM_WEAR_WIELD);
        case WEAR_WIELD2:
            return CAN_WEAR(obj, ITEM_WEAR_TAKE);
        case WEAR_FINGER_R:
        case WEAR_FINGER_L:
            return CAN_WEAR(obj, ITEM_WEAR_FINGER);
        case WEAR_NECK_1:
        case WEAR_NECK_2:
            return CAN_WEAR(obj, ITEM_WEAR_NECK);
        case WEAR_BODY:
            return CAN_WEAR(obj, ITEM_WEAR_BODY);
        case WEAR_HEAD:
            return CAN_WEAR(obj, ITEM_WEAR_HEAD);
        case WEAR_LEGS:
            return CAN_WEAR(obj, ITEM_WEAR_LEGS);
        case WEAR_FEET:
            return CAN_WEAR(obj, ITEM_WEAR_FEET);
        case WEAR_HANDS:
            return CAN_WEAR(obj, ITEM_WEAR_HANDS);
        case WEAR_ARMS:
            return CAN_WEAR(obj, ITEM_WEAR_ARMS);
        case WEAR_ABOUT:
            return CAN_WEAR(obj, ITEM_WEAR_ABOUT);
        case WEAR_WAIST:
            return CAN_WEAR(obj, ITEM_WEAR_WAIST);
        case WEAR_WRIST_R:
        case WEAR_WRIST_L:
            return CAN_WEAR(obj, ITEM_WEAR_WRIST);
        case WEAR_BACKPACK:
            return CAN_WEAR(obj, ITEM_WEAR_PACK);
        case WEAR_EAR_R:
        case WEAR_EAR_L:
            return CAN_WEAR(obj, ITEM_WEAR_EAR);
        case WEAR_SH:
            return CAN_WEAR(obj, ITEM_WEAR_SH);
        case WEAR_EYE:
            return CAN_WEAR(obj, ITEM_WEAR_EYE);
        default:
            return false;
    }
}

/************************************************************
 * generic searches based only on name
 ************************************************************/

/* search the entire world for a char, and return a pointer */
char_data *get_char(char *name) {
    char_data *i;

    if (*name == UID_CHAR) {
        std::optional<DgUID> result;
        result = resolveUID(name);
        auto uidResult = result;
        if(!uidResult) return nullptr;
        if(uidResult->index() != 2) return nullptr;
        i = std::get<2>(*uidResult);

        if (i && valid_dg_target(i, DG_ALLOW_GODS))
            return i;
    } else {
        for (i = character_list; i; i = i->next)
            if (isname(name, i->name) &&
                valid_dg_target(i, DG_ALLOW_GODS))
                return i;
    }

    return nullptr;
}

/*
 * Finds a char in the same room as the object with the name 'name'
 */
char_data *get_char_near_obj(obj_data *obj, char *name) {
    char_data *ch;

    if (*name == UID_CHAR) {
        std::optional<DgUID> result;
        result = resolveUID(name);
        auto uidResult = result;
        if(!uidResult) return nullptr;
        if(uidResult->index() != 2) return nullptr;
        ch = std::get<2>(*uidResult);

        if (ch && valid_dg_target(ch, DG_ALLOW_GODS))
            return ch;
    } else {
        room_rnum num;
        if ((num = obj_room(obj)) != NOWHERE)
            for (ch = world[num].people; ch; ch = ch->next_in_room)
                if (isname(name, ch->name) &&
                    valid_dg_target(ch, DG_ALLOW_GODS))
                    return ch;
    }

    return nullptr;
}


/*
 * returns a pointer to the first character in world by name name,
 * or nullptr if none found.  Starts searching in room room first
 */
char_data *get_char_in_room(room_data *room, char *name) {
    char_data *ch;

    if (*name == UID_CHAR) {
        std::optional<DgUID> result;
        result = resolveUID(name);
        auto uidResult = result;
        if(!uidResult) return nullptr;
        if(uidResult->index() != 2) return nullptr;
        ch = std::get<2>(*uidResult);

        if (ch && valid_dg_target(ch, DG_ALLOW_GODS))
            return ch;
    } else {
        for (ch = room->people; ch; ch = ch->next_in_room)
            if (isname(name, ch->name) &&
                valid_dg_target(ch, DG_ALLOW_GODS))
                return ch;
    }

    return nullptr;
}

/* searches the room with the object for an object with name 'name'*/

obj_data *get_obj_near_obj(obj_data *obj, char *name) {
    obj_data *i = nullptr;
    char_data *ch;
    room_vnum rm;
    int32_t id;

    if (!strcasecmp(name, "self") || !strcasecmp(name, "me"))
        return obj;

    /* is it inside ? */
    if (obj->contents && (i = get_obj_in_list(name, obj->contents)))
        return i;

    /* or outside ? */
    if (obj->in_obj) {
        if (*name == UID_CHAR) {
            std::optional<DgUID> result;
            result = resolveUID(name);
            auto uidResult = result;
            if(!uidResult) return nullptr;
            if(uidResult->index() != 1) return nullptr;
            auto o = std::get<1>(*uidResult);
            if(o == obj->in_obj) return o;
        } else if (isname(name, obj->in_obj->name))
            return obj->in_obj;
    }
        /* or worn ?*/
    else if (obj->worn_by && (i = get_object_in_equip(obj->worn_by, name)))
        return i;
        /* or carried ? */
    else if (obj->carried_by &&
             (i = get_obj_in_list(name, obj->carried_by->contents)))
        return i;
    else if ((rm = obj_room(obj)) != NOWHERE) {
        /* check the floor */
        if ((i = get_obj_in_list(name, world[rm].contents)))
            return i;

        /* check peoples' inventory */
        for (ch = world[rm].people; ch; ch = ch->next_in_room)
            if ((i = get_object_in_equip(ch, name)))
                return i;
    }
    return nullptr;
}

/* returns the object in the world with name name, or nullptr if not found */
obj_data *get_obj(char *name) {
    obj_data *obj;

    if (*name == UID_CHAR) {
        std::optional<DgUID> result;
        result = resolveUID(name);
        auto uidResult = result;
        if(!uidResult) return nullptr;
        if(uidResult->index() != 1) return nullptr;
        return std::get<1>(*uidResult);
    }
    else {
        for (obj = object_list; obj; obj = obj->next)
            if (isname(name, obj->name))
                return obj;
    }

    return nullptr;
}


/* finds room by id or vnum.  returns nullptr if not found */
room_data *get_room(char *name) {
    room_rnum nr;

    if (*name == UID_CHAR) {
        std::optional<DgUID> result;
        result = resolveUID(name);
        auto uidResult = result;
        if(!uidResult) return nullptr;
        if(uidResult->index() != 0) return nullptr;
        auto r = std::get<0>(*uidResult);
        return r;
    }
    else if ((nr = real_room(atoi(name))) == NOWHERE)
        return nullptr;
    else
        return &world[nr];
}


/*
 * returns a pointer to the first character in world by name name,
 * or nullptr if none found.  Starts searching with the person owing the object
 */
char_data *get_char_by_obj(obj_data *obj, char *name) {
    char_data *ch;

    if (*name == UID_CHAR) {
        std::optional<DgUID> result;
        result = resolveUID(name);
        auto uidResult = result;
        if(!uidResult) return nullptr;
        if(uidResult->index() != 2) return nullptr;
        ch = std::get<2>(*uidResult);

        if (ch && valid_dg_target(ch, DG_ALLOW_GODS))
            return ch;
    } else {
        if (obj->carried_by &&
            isname(name, obj->carried_by->name) &&
            valid_dg_target(obj->carried_by, DG_ALLOW_GODS))
            return obj->carried_by;

        if (obj->worn_by &&
            isname(name, obj->worn_by->name) &&
            valid_dg_target(obj->worn_by, DG_ALLOW_GODS))
            return obj->worn_by;

        for (ch = character_list; ch; ch = ch->next)
            if (isname(name, ch->name) &&
                valid_dg_target(ch, DG_ALLOW_GODS))
                return ch;
    }

    return nullptr;
}


/*
 * returns a pointer to the first character in world by name name,
 * or nullptr if none found.  Starts searching in room room first
 */
char_data *get_char_by_room(room_data *room, char *name) {
    char_data *ch;

    if (*name == UID_CHAR) {
        std::optional<DgUID> result;
        result = resolveUID(name);
        auto uidResult = result;
        if(!uidResult) return nullptr;
        if(uidResult->index() != 2) return nullptr;
        ch = std::get<2>(*uidResult);

        if (ch && valid_dg_target(ch, DG_ALLOW_GODS))
            return ch;
    } else {
        for (ch = room->people; ch; ch = ch->next_in_room)
            if (isname(name, ch->name) &&
                valid_dg_target(ch, DG_ALLOW_GODS))
                return ch;

        for (ch = character_list; ch; ch = ch->next)
            if (isname(name, ch->name) &&
                valid_dg_target(ch, DG_ALLOW_GODS))
                return ch;
    }

    return nullptr;
}


/*
 * returns the object in the world with name name, or nullptr if not found
 * search based on obj
 */
obj_data *get_obj_by_obj(obj_data *obj, char *name) {
    obj_data *i = nullptr;
    room_vnum rm;

    if (*name == UID_CHAR) {
        std::optional<DgUID> result;
        result = resolveUID(name);
        auto uidResult = result;
        if(!uidResult) return nullptr;
        if(uidResult->index() != 1) return nullptr;
        return std::get<1>(*uidResult);
    }

    if (!strcasecmp(name, "self") || !strcasecmp(name, "me"))
        return obj;

    if (obj->contents && (i = get_obj_in_list(name, obj->contents)))
        return i;

    if (obj->in_obj && isname(name, obj->in_obj->name))
        return obj->in_obj;

    if (obj->worn_by && (i = get_object_in_equip(obj->worn_by, name)))
        return i;

    if (obj->carried_by &&
        (i = get_obj_in_list(name, obj->carried_by->contents)))
        return i;

    if (((rm = obj_room(obj)) != NOWHERE) &&
        (i = get_obj_in_list(name, world[rm].contents)))
        return i;

    return get_obj(name);
}

/* only searches the room */
obj_data *get_obj_in_room(room_data *room, char *name) {
    obj_data *obj;
    int32_t id;

    if (*name == UID_CHAR) {
        std::optional<DgUID> result;
        result = resolveUID(name);
        auto uidResult = result;
        if(!uidResult) return nullptr;
        if(uidResult->index() != 1) return nullptr;
        auto o = std::get<1>(*uidResult);
        for (obj = room->contents; obj; obj = obj->next_content)
            if (o == obj)
                return obj;
    } else {
        for (obj = room->contents; obj; obj = obj->next_content)
            if (isname(name, obj->name))
                return obj;
    }

    return nullptr;
}

/* returns obj with name - searches room, then world */
obj_data *get_obj_by_room(room_data *room, char *name) {
    obj_data *obj;

    if (*name == UID_CHAR) {
        std::optional<DgUID> result;
        result = resolveUID(name);
        auto uidResult = result;
        if(!uidResult) return nullptr;
        if(uidResult->index() != 1) return nullptr;
        return std::get<1>(*uidResult);
    }

    for (obj = room->contents; obj; obj = obj->next_content)
        if (isname(name, obj->name))
            return obj;

    for (obj = object_list; obj; obj = obj->next)
        if (isname(name, obj->name))
            return obj;

    return nullptr;
}

/* checks every PULSE_SCRIPT for random triggers */
void script_trigger_check(uint64_t heartPulse, double deltaTime) {
    char_data *ch;
    obj_data *obj;
    struct room_data *room = nullptr;
    int nr;

    for (ch = character_list; ch; ch = ch->next) {
        auto sc = SCRIPT(ch);
        if (IS_SET(SCRIPT_TYPES(sc), WTRIG_RANDOM) &&
            (!is_empty(ch->getRoom()->zone) ||
                IS_SET(SCRIPT_TYPES(sc), WTRIG_GLOBAL)))
            random_mtrigger(ch);
    }

    for (obj = object_list; obj; obj = obj->next) {
        auto sc = SCRIPT(obj);
        if (IS_SET(SCRIPT_TYPES(sc), OTRIG_RANDOM))
                random_otrigger(obj);
    }

    for (auto &[vn, r] : world) {
        auto sc = SCRIPT(&r); 
        if (IS_SET(SCRIPT_TYPES(sc), WTRIG_RANDOM) &&
            (!is_empty(r.zone) ||
                IS_SET(SCRIPT_TYPES(sc), WTRIG_GLOBAL)))
            random_wtrigger(&r);
    }
}

void check_time_triggers() {
    char_data *ch;
    obj_data *obj;
    struct room_data *room = nullptr;
    int nr;

    for (ch = character_list; ch; ch = ch->next) {
        auto sc = SCRIPT(ch);

        if (IS_SET(SCRIPT_TYPES(sc), MTRIG_TIME) && (!is_empty(ch->getRoom()->zone) || IS_SET(SCRIPT_TYPES(sc), MTRIG_GLOBAL)))
            time_mtrigger(ch);
    }

    for (obj = object_list; obj; obj = obj->next) {
        auto sc = SCRIPT(obj);
        if (IS_SET(SCRIPT_TYPES(sc), OTRIG_TIME))
                time_otrigger(obj);
    }

    for (auto &[vn, r] : world) {
        auto sc = SCRIPT(&r);
        if (IS_SET(SCRIPT_TYPES(sc), WTRIG_TIME) &&
                (!is_empty(r.zone) ||
                 IS_SET(SCRIPT_TYPES(sc), WTRIG_GLOBAL)))
                time_wtrigger(&r);
    }
}

void check_interval_triggers(int trigFlag) {

    for (auto ch = character_list; ch; ch = ch->next) {
        auto sc = SCRIPT(ch);
        if (IS_SET(SCRIPT_TYPES(sc), trigFlag) &&
                (!is_empty(ch->getRoom()->zone) ||
                 IS_SET(SCRIPT_TYPES(sc), MTRIG_GLOBAL)))
                interval_mtrigger(ch, trigFlag);
    }

    for (auto obj = object_list; obj; obj = obj->next) {
        auto sc = SCRIPT(obj);
        if (IS_SET(SCRIPT_TYPES(sc), trigFlag))
                interval_otrigger(obj, trigFlag);
    }

    for (auto &[vn, r] : world) {
        auto sc = SCRIPT(&r);
        if (IS_SET(SCRIPT_TYPES(sc), trigFlag) &&
            (!is_empty(r.zone) ||
                IS_SET(SCRIPT_TYPES(sc), WTRIG_GLOBAL)))
            interval_wtrigger(&r, trigFlag);
    }
}


void do_stat_trigger(struct char_data *ch, const std::shared_ptr<trig_data>& trig) {
    char sb[MAX_STRING_LENGTH], buf[MAX_STRING_LENGTH];
    int len = 0;

    if (!trig) {
        basic_mud_log("SYSERR: nullptr trigger passed to do_stat_trigger.");
        return;
    }

    len += snprintf(sb, sizeof(sb), "Name: '@y%s@n',  VNum: [@g%5d@n], RNum: [%5d]\r\n",
                    GET_TRIG_NAME(trig), GET_TRIG_VNUM(trig), GET_TRIG_RNUM(trig));

    if (trig->parent->attach_type == OBJ_TRIGGER) {
        len += snprintf(sb + len, sizeof(sb) - len, "Trigger Intended Assignment: Objects\r\n");
        sprintbit(GET_TRIG_TYPE(trig), otrig_types, buf, sizeof(buf));
    } else if (trig->parent->attach_type == WLD_TRIGGER) {
        len += snprintf(sb + len, sizeof(sb) - len, "Trigger Intended Assignment: Rooms\r\n");
        sprintbit(GET_TRIG_TYPE(trig), wtrig_types, buf, sizeof(buf));
    } else {
        len += snprintf(sb + len, sizeof(sb) - len, "Trigger Intended Assignment: Mobiles\r\n");
        sprintbit(GET_TRIG_TYPE(trig), trig_types, buf, sizeof(buf));
    }

    len += snprintf(sb + len, sizeof(sb) - len, "Trigger Type: %s, Numeric Arg: %d, Arg list: %s\r\n",
                    buf, GET_TRIG_NARG(trig),
                    GET_TRIG_ARG(trig));

    len += snprintf(sb + len, sizeof(sb) - len, "Commands:\r\n");

    for (const auto &line : trig->parent->lines) {
        if (!line.empty())
            len += snprintf(sb + len, sizeof(sb) - len, "%s\r\n", line.c_str());

        if (len > MAX_STRING_LENGTH - 80) {
            len += snprintf(sb + len, sizeof(sb) - len, "*** Overflow - script too long! ***\r\n");
            break;
        }
    }

    ch->desc->sendText(sb);
}


/* find the name of what the uid points to */
void find_uid_name(char *uid, char *name, size_t nlen) {
    char_data *ch;
    obj_data *obj;

    if ((ch = get_char(uid)))
        snprintf(name, nlen, "%s", ch->name);
    else if ((obj = get_obj(uid)))
        snprintf(name, nlen, "%s", obj->name);
    else
        snprintf(name, nlen, "uid = %s, (not found)", uid + 1);
}


/* general function to display stats on script sc */
void script_stat(char_data *ch, const std::shared_ptr<script_data>& sc) {
    send_to_char(ch, "Global Variables: %d\r\n", sc->vars.size());
    char buf1[MAX_STRING_LENGTH];

    for (auto &[name, value] : sc->vars) {
        if (!value.empty() && value[0] == UID_CHAR) {
            std::optional<DgUID> result;
            result = resolveUID(value);
            auto uidResult = result;
            if(uidResult) {
				auto idx = uidResult->index();
                std::string n;
                if(idx == 0) {
                    // Room.
                    auto thing = std::get<0>(*uidResult);
                    n = thing->name;
                } else if(idx == 1) {
                    // object
                    auto thing = std::get<1>(*uidResult);
                    n = thing->name;
                } else if(idx == 2) {
                    // character or player...
                    auto thing = std::get<2>(*uidResult);
                    n = thing->name;
                }
                send_to_char(ch, "    %15s:  %s\r\n", name, n.c_str());
            } else {
                send_to_char(ch, "   -BAD UID: %s", value);
            }
        } else
            send_to_char(ch, "    %15s:  %s\r\n", name, value);
    }

    for (auto t : sc->dgScripts) {
        send_to_char(ch, "\r\n  Trigger: @y%s@n, VNum: [@y%5d@n], RNum: [%5d]\r\n",
                     GET_TRIG_NAME(t), GET_TRIG_VNUM(t), GET_TRIG_RNUM(t));

        if (t->parent->attach_type == OBJ_TRIGGER) {
            send_to_char(ch, "  Trigger Intended Assignment: Objects\r\n");
            sprintbit(GET_TRIG_TYPE(t), otrig_types, buf1, sizeof(buf1));
        } else if (t->parent->attach_type == WLD_TRIGGER) {
            send_to_char(ch, "  Trigger Intended Assignment: Rooms\r\n");
            sprintbit(GET_TRIG_TYPE(t), wtrig_types, buf1, sizeof(buf1));
        } else {
            send_to_char(ch, "  Trigger Intended Assignment: Mobiles\r\n");
            sprintbit(GET_TRIG_TYPE(t), trig_types, buf1, sizeof(buf1));
        }

        send_to_char(ch, "  Trigger Type: %s, Numeric Arg: %d, Arg list: %s\r\n",
                     buf1, GET_TRIG_NARG(t),
                     GET_TRIG_ARG(t));

        if (t->waiting != 0.0) {
            send_to_char(ch, "    Wait: %f seconds, Current line: %s\r\n",
                         t->waiting,
                         t->parent->lines[t->lineNumber]);
            send_to_char(ch, "  Variables: %d\r\n", t->vars.size());

            for (auto &[name, value] : t->vars) {
                if (!value.empty() && value[0] == UID_CHAR) {
                    std::optional<DgUID> result;
                    result = resolveUID(value);
                    auto uidResult = result;
                    if(uidResult) {
                        auto idx = uidResult->index();
                        std::string n;
                        if(idx == 0) {
                            // Room.
                            auto thing = std::get<0>(*uidResult);
                            n = thing->name;
                        } else if(idx == 1) {
                            // object
                            auto thing = std::get<1>(*uidResult);
                            n = thing->name;
                        } else if(idx == 2) {
                            // character or player...
                            auto thing = std::get<2>(*uidResult);
                            n = thing->name;
                        }
                        send_to_char(ch, "    %15s:  %s\r\n", name, n.c_str());
                    } else {
                        send_to_char(ch, "   -BAD UID: %s", value);
                    }
                } else {
                    send_to_char(ch, "    %15s:  %s\r\n", name, value);
                }

            }
        }
    }
}


void do_sstat(struct char_data *ch, struct unit_data *ud) {
    send_to_char(ch, "Triggers:\r\n");
    if (!SCRIPT(ud)) {
        send_to_char(ch, "  None.\r\n");
        return;
    }

    script_stat(ch, SCRIPT(ud));
}

/*
 * adds the trigger t to script sc in in location loc.  loc = -1 means
 * add to the end, loc = 0 means add before all other triggers.
 */
void script_data::addTrigger(const std::shared_ptr<trig_data> t, int loc) {
    t->sc = this;

    // Now insert t in the right spot...
    if(loc == -1) {
        dgScripts.push_back(t);
    } else if(loc == 0) {
        dgScripts.push_front(t);
    } else {
        auto it = dgScripts.begin();
        std::advance(it, loc);
        dgScripts.insert(it, t);
    }

    SCRIPT_TYPES(this) |= GET_TRIG_TYPE(t);
    t->activate();
}


ACMD(do_attach) {
    char_data *victim;
    obj_data *object;
    room_data *room;
    trig_data *trig;
    char targ_name[MAX_INPUT_LENGTH], trig_name[MAX_INPUT_LENGTH];
    char loc_name[MAX_INPUT_LENGTH], arg[MAX_INPUT_LENGTH];
    int loc, tn, rn, num_arg;
    room_rnum rnum;

    argument = two_arguments(argument, arg, trig_name);
    two_arguments(argument, targ_name, loc_name);

    if (!*arg || !*targ_name || !*trig_name) {
        send_to_char(ch, "Usage: attach { mob | obj | room } { trigger } { name } [ location ]\r\n");
        return;
    }

    num_arg = atoi(targ_name);
    tn = atoi(trig_name);
    loc = (*loc_name) ? atoi(loc_name) : -1;

    if (is_abbrev(arg, "mobile") || is_abbrev(arg, "mtr")) {
        victim = get_char_vis(ch, targ_name, nullptr, FIND_CHAR_WORLD);
        if (!victim) { /* search room for one with this vnum */
            for (victim = ch->getRoom()->people; victim; victim = victim->next_in_room)
                if (GET_MOB_VNUM(victim) == num_arg)
                    break;

            if (!victim) {
                send_to_char(ch, "That mob does not exist.\r\n");
                return;
            }
        }
        if (!IS_NPC(victim)) {
            send_to_char(ch, "Players can't have scripts.\r\n");
            return;
        }
        if (!can_edit_zone(ch, ch->getRoom()->zone)) {
            send_to_char(ch, "You can only attach triggers in your own zone\r\n");
            return;
        }
        /* have a valid mob, now get trigger */
        auto trig = read_trigger(tn);
        if (!trig) {
            send_to_char(ch, "That trigger does not exist.\r\n");
            return;
        }
        victim->script->addTrigger(trig, loc);

        send_to_char(ch, "Trigger %d (%s) attached to %s [%d].\r\n",
                     tn, GET_TRIG_NAME(trig), GET_SHORT(victim), GET_MOB_VNUM(victim));
    } else if (is_abbrev(arg, "object") || is_abbrev(arg, "otr")) {
        object = get_obj_vis(ch, targ_name, nullptr);
        if (!object) { /* search room for one with this vnum */
            for (object = ch->getRoom()->contents; object; object = object->next_content)
                if (GET_OBJ_VNUM(object) == num_arg)
                    break;

            if (!object) { /* search inventory for one with this vnum */
                for (object = ch->contents; object; object = object->next_content)
                    if (GET_OBJ_VNUM(object) == num_arg)
                        break;

                if (!object) {
                    send_to_char(ch, "That object does not exist.\r\n");
                    return;
                }
            }
        }

        if (!can_edit_zone(ch, ch->getRoom()->zone)) {
            send_to_char(ch, "You can only attach triggers in your own zone\r\n");
            return;
        }
        /* have a valid obj, now get trigger */
        auto trig = read_trigger(tn);
        if (!trig) {
            send_to_char(ch, "That trigger does not exist.\r\n");
            return;
        }

        object->script->addTrigger(trig, loc);

        send_to_char(ch, "Trigger %d (%s) attached to %s [%d].\r\n",
                     tn, GET_TRIG_NAME(trig),
                     (object->short_description ?
                      object->short_description : object->name),
                     GET_OBJ_VNUM(object));
    } else if (is_abbrev(arg, "room") || is_abbrev(arg, "wtr")) {
        if (strchr(targ_name, '.'))
            rnum = IN_ROOM(ch);
        else if (isdigit(*targ_name))
            rnum = find_target_room(ch, targ_name);
        else
            rnum = NOWHERE;

        if (rnum == NOWHERE) {
            send_to_char(ch, "You need to supply a room number or . for current room.\r\n");
            return;
        }

        if (!can_edit_zone(ch, world[rnum].zone)) {
            send_to_char(ch, "You can only attach triggers in your own zone\r\n");
            return;
        }
        /* have a valid room, now get trigger */
        auto trig = read_trigger(tn);
        if (!trig) {
            send_to_char(ch, "That trigger does not exist.\r\n");
            return;
        }

        room = &world[rnum];
        room->script->addTrigger(trig, loc);

        send_to_char(ch, "Trigger %d (%s) attached to room %d.\r\n",
                     tn, GET_TRIG_NAME(trig), world[rnum].vn);
    } else
        send_to_char(ch, "Please specify 'mob', 'obj', or 'room'.\r\n");
}


/*
 *  removes the trigger specified by name, and the script of o if
 *  it removes the last trigger.  name can either be a number, or
 *  a 'silly' name for the trigger, including things like 2.beggar-death.
 *  returns 0 if did not find the trigger, otherwise 1.  If it matters,
 *  you might need to check to see if all the triggers were removed after
 *  this function returns, in order to remove the script.
 */
int remove_trigger(struct script_data *sc, char *name) {
    return;
}

ACMD(do_detach) {
    return;
}

/*
 *  Logs any errors caused by scripts to the system log.
 *  Will eventually allow on-line view of script errors.
 */
void script_vlog(const char *format, va_list args) {
    char output[MAX_STRING_LENGTH];
    struct descriptor_data *i;

    snprintf(output, sizeof(output), "SCRIPT ERR: %s", format);

    // wrap up basic_mud_vlog in a va copy...
    va_list args_copy;
    va_copy(args_copy, args);
    basic_mud_vlog(output, args_copy);
    va_end(args_copy);

    /* the rest is mostly a rip from basic_mud_log() */
    strcpy(output, "[ ");            /* strcpy: OK */
    vsnprintf(output + 2, sizeof(output) - 6, format, args);
    strcat(output, " ]\r\n");        /* strcat: OK */

    for (i = descriptor_list; i; i = i->next) {
        if (STATE(i) != CON_PLAYING || IS_NPC(i->character)) /* switch */
            continue;
        if (GET_ADMLEVEL(i->character) < ADMLVL_BUILDER)
            continue;
        if (PLR_FLAGGED(i->character, PLR_WRITING))
            continue;
        if (NRM > (PRF_FLAGGED(i->character, PRF_LOG1) ? 1 : 0) + (PRF_FLAGGED(i->character, PRF_LOG2) ? 2 : 0))
            continue;

        send_to_char(i->character, "@g%s@n", output);
    }
}


void script_log(const char *format, ...) {
    va_list args;

    va_start(args, format);
    script_vlog(format, args);
    va_end(args);
}

bool is_num(const std::string& arg) {
    if (arg.empty()) return false;

    bool decimal_point_found = false;
    for (char c : arg) {
        if (c == '.') {
            if (decimal_point_found) {
                // More than one decimal point found, not a number
                return false;
            }
            decimal_point_found = true;
        } else if (!std::isdigit(c)) {
            // Non-digit character found, not a number
            return false;
        }
    }
    return true;
}

std::string trig_data::evalNumericOp(const std::string& op, const std::string &lhs, const std::string &rhs) {
    auto l = atof(lhs.c_str());
    auto r = atof(rhs.c_str());

    /* find the op, and figure out the value */
    if (iequals("||", op)) {
        return fmt::format("{}", (l || r) ? 1 : 0);
    } else if (iequals("&&", op)) {
        return fmt::format("{}", (l && r) ? 1 : 0);
    } else if (iequals("==", op)) {
       return fmt::format("{}", (l == r) ? 1 : 0);
    } else if (iequals("!=", op)) {
        return fmt::format("{}", (l != r) ? 1 : 0);
    } else if (iequals("<=", op)) {
        return fmt::format("{}", (l <= r) ? 1 : 0);
    } else if (iequals(">=", op)) {
        return fmt::format("{}", (l >= r) ? 1 : 0);
    } else if (iequals("<", op)) {
        return fmt::format("{}", (l < r) ? 1 : 0);
    } else if (iequals(">", op)) {
        return fmt::format("{}", (l > r) ? 1 : 0);
    }else if (iequals("*", op))
        return fmt::format("{}", l * r);
    else if (iequals("/", op))
        return fmt::format("{}", (int64_t)l / (int64_t)r);
    else if (iequals("+", op))
        return fmt::format("{}", l + r);
    else if (iequals("-", op))
        return fmt::format("{}", l - r);
    else if (iequals("!", op)) {
        return fmt::format("{}", (r != 0.0) ? 0 : 1);
    }
    return "0";
}

bool trig_data::truthy(const std::string &txt) {
    if(txt.empty()) return false;

    // Try to convert to a number and check if it's zero
    std::stringstream str(txt);
    double num;
    str >> num;
    if (!str.fail() && num == 0.0) return false;

    // If not null, not empty, and not zero, it's truthy
    return true;

}


/* evaluates 'lhs op rhs', and copies to result */
std::string trig_data::evalOp(const std::string& op, const std::string& lhs, const std::string &rhs) {

    std::string l = lhs;
    std::string r = rhs;
    trim(l);
    trim(r);

    if(is_num(l) && is_num(r)) {
        return evalNumericOp(op, l, r);
    }

    /* find the op, and figure out the value */
    if ("||" == op) {
        return (lhs == "0" && rhs == "0") ? "0" : "1";
    } else if ("&&" == op) {
        return (lhs == "0" || rhs == "0") ? "0" : "1";
    } else if ("==" == op) {
        return !strcasecmp(lhs.c_str(), rhs.c_str()) ? "1" : "0";
    } else if ("!=" == op) {
        return strcasecmp(lhs.c_str(), rhs.c_str()) ? "1" : "0";
    } else if ("<=" == op) {
        return (strcasecmp(lhs.c_str(), rhs.c_str()) <= 0) ? "1" : "0";
    } else if (">=" == op) {
        return (strcasecmp(lhs.c_str(), rhs.c_str()) <= 0) ? "1" : "0";
    } else if ("<" == op) {
        return (strcasecmp(lhs.c_str(), rhs.c_str()) < 0) ? "1" : "0";
    } else if (">" == op) {
        return (strcasecmp(lhs.c_str(), rhs.c_str()) > 0) ? "1" : "0";
    } else if ("/=" == op)
        return str_str((char*)lhs.c_str(), (char*)rhs.c_str()) ? "1" : "0";
    else if ("!" == op) {
        return !truthy(rhs) ? "1" : "0";
    }
    return "0";
}

char *matching_quote(char *p)
{
  for (p++; *p && (*p != '"'); p++) {
    if (*p == '\\')
      p++;
  }

  if (!*p)
    p--;

  return p;
}

/*
 * p points to the first quote, returns the matching
 * end quote, or the last non-null char in p.
*/
std::size_t matching_quote(const std::string& line, std::size_t start) {
    for(auto i = start; i < line.size(); i++) {
        auto p = line[i];
        if(p == '\\') continue;
        if(p == '"') return i;
    }
    return line.size();
}

/*
 * p points to the first paren.  returns a pointer to the
 * matching closing paren, or the last non-null char in p.
 */
std::size_t matching_paren(const std::string& line, std::size_t start) {
    int depth = 0;

    for (auto i = start; i < line.size(); i++) {
        auto p = line[i];
        if (p == '(')
            depth++;
        else if (p == ')')
            depth--;
            if(depth == 0) return i;
        else if (p == '"')
            i = matching_quote(line, i);
    }

    return line.size();
}


/* evaluates line, and returns answer in result */
std::string trig_data::evalExpr(const std::string& expr) {

    std::string l = expr;
    trim(l);

    if (l.starts_with('(')) {
        auto p = matching_paren(l, 0);
        return evalExpr(l.substr(1, p-1));
    } else if (auto result = evalLhsOpRhs(l); result) {
        return result.value();
    } else
        return varSubst(l);
}

static std::vector<std::string> ops = {
        "||",
        "&&",
        "==",
        "!=",
        "<=",
        ">=",
        "<",
        ">",
        "/=",
        "-",
        "+",
        "/",
        "*",
        "!",
};

/*
 * evaluates expr if it is in the form lhs op rhs, and copies
 * answer in result.  returns 1 if expr is evaluated, else 0
 */
std::optional<std::string> trig_data::evalLhsOpRhs(const std::string& expr) {
    for (const auto& op : ops) {
        auto idx = expr.find(op);
        if (idx == std::string::npos) continue;

        // Special case for unary operators
        if (idx == 0) {
            // For unary operators like "!" and potentially unary "+", handle them here
            if (op == "!" || op == "+" || op == "-") {
                auto right = expr.substr(op.size());
                auto rhr = evalExpr(right);
                if (op == "!") {
                    return !truthy(rhr) ? "1" : "0";
                } else if (op == "+") {
                    // Unary plus, simply return the evaluated right-hand side
                    return rhr;
                } else {
                    // Unary minus, negate the evaluated right-hand side
                    auto r = atof(rhr.c_str());
                    return fmt::format("{}", -r);
                }
            }
            continue; // If other unary operators are added, handle them before this line
        }

        // Binary operator cases
        auto left = expr.substr(0, idx-1);
        auto right = expr.substr(idx+1 + op.size());

        auto lhr = evalExpr(left);
        auto rhr = evalExpr(right);
        return evalOp(op, lhr, rhr);
    }

    return {};
}


/* returns 1 if cond is true, else 0 */
bool trig_data::processIf(const std::string& cond) {
    auto result = evalExpr(cond);
    return truthy(result);
}


/* processes any 'wait' commands in a trigger */
void trig_data::processWait(const std::string& cmd) {
    char buf[MAX_INPUT_LENGTH], *arg;
    long when, hr, min, ntime;
    char c;

    arg = any_one_arg((char*)cmd.c_str(), buf);
    skip_spaces(&arg);

    if (!*arg) {
        script_log("Trigger: %s, VNum %d. wait w/o an arg: '%d'",
                   GET_TRIG_NAME(this), GET_TRIG_VNUM(this), lineNumber);
        return;
    }

    if (!strncasecmp(arg, "until ", 6)) {

        /* valid forms of time are 14:30 and 1430 */
        if (sscanf(arg, "until %ld:%ld", &hr, &min) == 2)
            min += (hr * 60);
        else
            min = (hr % 100) + ((hr / 100) * 60);

        // Convert current MUD time to seconds since midnight
        double current_mud_seconds = time_info.seconds +
                                     (time_info.minutes * SECS_PER_MINUTE) +
                                     (time_info.hours * SECS_PER_HOUR);

        // Convert target MUD time to seconds since midnight
        double target_mud_seconds = min * SECS_PER_MINUTE;

        // Determine waiting time in MUD seconds
        double waiting_mud_seconds;
        if (current_mud_seconds >= target_mud_seconds) {
            // If the target time has already passed, wait until the next day
            waiting_mud_seconds = (SECS_PER_DAY - current_mud_seconds) + target_mud_seconds;
        } else {
            // If the target time is in the future, wait until that time
            waiting_mud_seconds = target_mud_seconds - current_mud_seconds;
        }

        // Convert waiting time to real seconds
        waiting = waiting_mud_seconds * SECS_PER_MUD_SECOND;

    } else {
        if (sscanf(arg, "%ld %c", &when, &c) == 2) {
            if (c == 't')
                when *= PULSES_PER_MUD_HOUR;
            else if (c == 's')
                when *= PASSES_PER_SEC;
        }
        // We need to convert 'when' into a double of seconds-to-wait by dividing by PASSES_PER_SEC.
        waiting = (double) when / (double) PASSES_PER_SEC;
    }

    // we're replacing the old wait_event_obj.
    setState(DgScriptState::WAITING);
}


/* processes a script set command */
void trig_data::processSet(const std::string& cmd) {

    char arg[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH], *value;
    value = two_arguments((char*)cmd.c_str(), arg, name);

    skip_spaces(&value);
    
    if (!*name) {
        throw DgScriptException(fmt::format("set w/o an arg: '{}'",cmd));
    }
    addVar(name, value);
}

/* processes a script eval command */
void trig_data::processEval(const std::string &cmd) {
    char arg[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH];
    char result[MAX_INPUT_LENGTH], *expr;

    expr = one_argument((char*)cmd.c_str(), arg); /* cut off 'eval' */
    expr = one_argument(expr, name); /* cut off name */

    skip_spaces(&expr);

    if (!*name) {
        throw DgScriptException(fmt::format("eval w/o an arg: '{}'",cmd));
    }

    if(isUID(expr)) {
        addVar(name, expr);
    } else {
        auto val = evalExpr(expr);
        addVar(name, val);
    }
}


/* script attaching a trigger to something */
void trig_data::processAttach(const std::string &cmd) {
    char arg[MAX_INPUT_LENGTH], trignum_s[MAX_INPUT_LENGTH];
    char result[MAX_INPUT_LENGTH], *id_p;
    trig_data *newtrig;
    char_data *c = nullptr;
    obj_data *o = nullptr;
    room_data *r = nullptr;
    long trignum, id;

    id_p = two_arguments((char*)cmd.c_str(), arg, trignum_s);
    skip_spaces(&id_p);

    if (!*trignum_s) {
        throw DgScriptException(fmt::format("attach w/o an arg: '{}'",cmd));
    }

    if(isUID(id_p)) {
        snprintf(result, sizeof(result), "%s", id_p);
    } else {
        /* parse and locate the id specified */
        auto res = evalExpr(id_p);
        snprintf(result, sizeof(result), "%s", res.c_str());
    }

    /* parse and locate the id specified */
    std::optional<DgUID> result1 = resolveUID(result);
    auto uidResult = result1;
    if(!uidResult) {
        throw DgScriptException(fmt::format("attach invalid id arg: '{}'",cmd));
    }

    struct unit_data *u;

    switch(uidResult->index()) {
        case 0:
            u = std::get<0>(*uidResult);
            break;
        case 1:
            u = std::get<1>(*uidResult);
            break;
        case 2:
            u = std::get<2>(*uidResult);
            break;
    }

    /* locate and load the trigger specified */
    trignum = real_trigger(atoi(trignum_s));
    auto trig = read_trigger(atoi(trignum_s));
    
    if (!trig) {
        throw DgScriptException(fmt::format("attach invalid trigger: '{}'",trignum_s));
    }

    u->script->addTrigger(trig, -1);
}


/* script detaching a trigger from something */
void trig_data::processDetach(const std::string& cmd) {
    char arg[MAX_INPUT_LENGTH], trignum_s[MAX_INPUT_LENGTH];
    char result[MAX_INPUT_LENGTH], *id_p;
    char_data *c = nullptr;
    obj_data *o = nullptr;
    room_data *r = nullptr;
    long id;

    id_p = two_arguments((char*)cmd.c_str(), arg, trignum_s);
    skip_spaces(&id_p);

    if (!*trignum_s) {
        throw DgScriptException(fmt::format("detach w/o an arg: '{}'",cmd));
    }

    if(isUID(id_p)) {
        snprintf(result, sizeof(result), "%s", id_p);
    } else {
        /* parse and locate the id specified */
        auto res = evalExpr(id_p);
        snprintf(result, sizeof(result), "%s", res.c_str());
    }

    /* parse and locate the id specified */
    std::optional<DgUID> result1 = resolveUID(result);

    auto uidResult = result1;
    if(!uidResult) {
        throw DgScriptException(fmt::format("detach invalid id arg: '{}'",cmd));
        return;
    }

    struct unit_data *u;

    switch(uidResult->index()) {
        case 0:
            u = std::get<0>(*uidResult);
            break;
        case 1:
            u = std::get<1>(*uidResult);
            break;
        case 2:
            u = std::get<2>(*uidResult);
            break;
    }

    if(iequals(trignum_s, "all")) {
        u->script->removeAll();
    } else {
        u->script->removeScript(trignum_s);
    }
}

struct room_data *dg_room_of_obj(struct obj_data *obj) {
    return obj->getAbsoluteRoom();
}


/*
 * removes a variable from the global vars of sc,
 * or the local vars of trig if not found in global list.
 */
void trig_data::processUnset(const std::string& cmd) {
    char arg[MAX_INPUT_LENGTH], *var;

    var = any_one_arg((char*)cmd.c_str(), arg);

    skip_spaces(&var);

    if (!*var) {
        throw DgScriptException(fmt::format("unset w/o an arg: '{}'",cmd));
    }

    if(sc->hasVar(var)) {
        sc->delVar(var);
    } else {
        delVar(var);
    }
}


/*
 * copy a locally owned variable to the globals of another script
 *     'remote <variable_name> <uid>'
 */
void trig_data::processRemote(const std::string& cmd) {
    auto args = split(cmd, ' ');

    if (args.size() != 3) {
        throw DgScriptException(fmt::format("remote: invalid arguments '{}'",cmd));
    }

    if(!hasVar(args[1])) {
        throw DgScriptException(fmt::format("remote: local var '{}' not found in remote call",args[1]));
    }

    auto loc = getRaw(args[1]);
    /* find the target script from the uid number */
    std::optional<DgUID> result;
    result = resolveUID(args[2]);
    auto uidResult = result;
    if(!uidResult) {
        throw DgScriptException(fmt::format("remote: illegal uid '{}'",args[2]));
    }

    struct unit_data *u;
    switch(uidResult->index()) {
        case 0:
            u = std::get<0>(*uidResult);
            break;
        case 1:
            u = std::get<1>(*uidResult);
            break;
        case 2:
            u = std::get<2>(*uidResult);
            break;
    }

    u->script->addVar(args[0], loc);
}


/*
 * command-line interface to rdelete
 * named vdelete so people didn't think it was to delete rooms
 */
ACMD(do_vdelete) {
    struct trig_var_data *vd, *vd_prev = nullptr;
    char *var, *uid_p;
    char buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH];
    long uid, context;
    room_data *room;
    char_data *mob;
    obj_data *obj;

    argument = two_arguments(argument, buf, buf2);
    var = buf;
    uid_p = buf2;
    skip_spaces(&var);
    skip_spaces(&uid_p);


    if (!*buf || !*buf2) {
        send_to_char(ch, "Usage: vdelete { <variablename> | * | all } <id>\r\n");
        return;
    }

    std::optional<DgUID> result;
    result = resolveUID(buf2);
    auto uidResult = result;
    if(!uidResult) {
        send_to_char(ch, "vdelete: illegal id specified.\r\n");
        return;
    }
    struct unit_data *u;
    switch(uidResult->index()) {
        case 0:
            u = std::get<0>(*uidResult);
            break;
        case 1:
            u = std::get<1>(*uidResult);
            break;
        case 2:
            u = std::get<2>(*uidResult);
            break;
    }

    auto sc_remote = u->script;

    if(!sc_remote) {
        send_to_char(ch, "vdelete: cannot resolve specified id.\r\n");
        return;
    }

    if (sc_remote->vars.empty()) {
        send_to_char(ch, "That id represents no global variables.(2)\r\n");
        return;
    }

    if (*var == '*' || is_abbrev(var, "all")) {
        sc_remote->vars.clear();
        send_to_char(ch, "All variables deleted from that id.\r\n");
        return;
    }

    /* find the global */
    if (sc_remote->delVar(var)) {
        send_to_char(ch, "That variable cannot be located.\r\n");
        return;
    }

    send_to_char(ch, "Deleted.\r\n");
}

/*
 * delete a variable from the globals of another script
 *     'rdelete <variable_name> <uid>'
 */
void trig_data::processRdelete(const std::string &cmd) {
    auto args = split(cmd, ' ');

    struct script_data *sc_remote = nullptr;

    if (args.size() != 2) {
        throw DgScriptException(fmt::format("rdelete: invalid arguments '{}'",cmd));
    }

    std::optional<DgUID> result;
    result = resolveUID(args[1]);
    auto uidResult = result;
    if(!uidResult) {
        throw DgScriptException(fmt::format("rdelete: illegal uid '{}'",args[1]));
    }

    struct unit_data *u;
    switch(uidResult->index()) {
        case 0:
            u = std::get<0>(*uidResult);
            break;
        case 1:
            u = std::get<1>(*uidResult);
            break;
        case 2:
            u = std::get<2>(*uidResult);
            break;
    }

    u->script->addVar(args[0], "");
}


void trig_data::processGlobal(const std::string& name) {
    if(hasVar(name)) {
        sc->addVar(name, getRaw(name));
        delVar(name);
    }
}


/* set the current context for a script */
void trig_data::processContext(const std::string& cmd) {

}

void trig_data::extractValue(const std::string& cmd) {
    char buf[MAX_INPUT_LENGTH], buf2[MAX_INPUT_LENGTH];
    char *buf3;
    char to[128];
    int num;

    buf3 = any_one_arg((char*)cmd.c_str(), buf);
    half_chop(buf3, buf2, buf);
    strcpy(to, buf2);

    num = atoi(buf);
    if (num < 1) {
        script_log("extract number < 1!");
        return;
    }

    half_chop(buf, buf3, buf2);

    while (num > 0) {
        half_chop(buf2, buf, buf2);
        num--;
    }

    addVar(to, buf);

}

/* returns the real number of the trigger with given virtual number */
trig_rnum real_trigger(trig_vnum vnum) {
    return trig_index.count(vnum) ? vnum : NOTHING;
}

ACMD(do_tstat) {
    int rnum;
    char str[MAX_INPUT_LENGTH];

    half_chop(argument, str, argument);
    if (*str) {
        rnum = real_trigger(atoi(str));
        if (rnum == NOTHING) {
            send_to_char(ch, "That vnum does not exist.\r\n");
            return;
        }
    } else
        send_to_char(ch, "Usage: tstat <vnum>\r\n");
}

/* read a line in from a file, return the number of chars read */
int fgetline(FILE *file, char *p) {
    int count = 0;

    do {
        *p = fgetc(file);
        if (*p != '\n' && !feof(file)) {
            p++;
            count++;
        }
    } while (*p != '\n' && !feof(file));

    if (*p == '\n') *p = '\0';

    return count;
}


/* load in a character's saved variables */
void read_saved_vars(struct char_data *ch) {
    FILE *file;
    long context;
    char fn[127];
    char input_line[1024], *temp, *p;
    char varname[32];
    char context_str[16];

    /*
     * If getting to the menu from inside the game, the vars aren't removed.
     * So let's not allocate them again.
     */
    if (SCRIPT(ch))
        return;

    /* create the space for the script structure which holds the vars */
    /* We need to do this first, because later calls to 'remote' will need */
    /* a script already assigned. */

    /* find the file that holds the saved variables and open it*/
    get_filename(fn, sizeof(fn), SCRIPT_VARS_FILE, GET_NAME(ch));
    file = fopen(fn, "r");

    /* if we failed to open the file, return */
    if (!file) {
        basic_mud_log("%s had no variable file", GET_NAME(ch));
        return;
    }
    /* walk through each line in the file parsing variables */
    do {
        if (get_line(file, input_line) > 0) {
            p = temp = strdup(input_line);
            temp = any_one_arg(temp, varname);
            temp = any_one_arg(temp, context_str);
            skip_spaces(&temp); /* temp now points to the rest of the line */

            ch->script->addVar(varname, temp);
            free(p); /* plug memory hole */
        }
    } while (!feof(file));

    /* close the file and return */
    fclose(file);
}

/* load in a character's saved variables from an ASCII pfile*/
void read_saved_vars_ascii(FILE *file, struct char_data *ch, int count) {
    long context;
    char input_line[1024], *temp, *p;
    char varname[READ_SIZE];
    char context_str[READ_SIZE];
    int i;

    /*
     * If getting to the menu from inside the game, the vars aren't removed.
     * So let's not allocate them again.
     */
    if (SCRIPT(ch))
        return;

    /* walk through each line in the file parsing variables */
    for (i = 0; i < count; i++) {
        if (get_line(file, input_line) > 0) {
            p = temp = strdup(input_line);
            temp = any_one_arg(temp, varname);
            temp = any_one_arg(temp, context_str);
            skip_spaces(&temp); /* temp now points to the rest of the line */

            context = atol(context_str);
            ch->script->addVar(varname, temp);
            free(p); /* plug memory hole */
        }
    }
}

/* find_char() helpers */

// Must be power of 2
#define BUCKET_COUNT 64
// to recognize an empty bucket
#define UID_OUT_OF_RANGE 1000000000


int check_flags_by_name_ar(bitvector_t *array, int numflags, char *search, const char *namelist[]) {
    int i, item = -1;

    for (i = 0; i < numflags && item < 0; i++)
        if (!strcmp(search, namelist[i]))
            item = i;

    if (item < 0)
        return false;

    if (IS_SET_AR(array, item))
        return item;

    return false;
}

nlohmann::json trig_proto::serialize() {
    auto j = nlohmann::json::object();
    if(vn != NOTHING) j["vn"] = vn;
    if(!name.empty()) j["name"] = name;
    if(attach_type) j["attach_type"] = attach_type;
    if(data_type) j["data_type"] = data_type;
    if(trigger_type) j["trigger_type"] = trigger_type;
    if(narg) j["narg"] = narg;
    if(!arglist.empty()) j["arglist"] = arglist;
    if(!lines.empty()) j["lines"] = lines;
    return j;
}

trig_proto::trig_proto(const nlohmann::json& j) {
    deserialize(j);
}

void trig_proto::deserialize(const nlohmann::json& j) {
    if(j.contains("vn")) vn = j["vn"].get<trig_vnum>();
    if(j.contains("name")) name = j["name"].get<std::string>();
    if(j.contains("attach_type")) attach_type = j["attach_type"].get<int>();
    if(j.contains("data_type")) data_type = j["data_type"].get<int>();
    if(j.contains("trigger_type")) trigger_type = j["trigger_type"].get<int>();
    if(j.contains("narg")) narg = j["narg"].get<int>();
    if(j.contains("arglist")) arglist = j["arglist"].get<std::string>();
    if(j.contains("lines")) lines = j["lines"].get<std::vector<std::string>>();

}

nlohmann::json trig_data::serialize() {
    auto j = nlohmann::json::object();
    for(auto &[nt, line] : depth) {
        j["depth"].push_back(std::make_pair(nt, line));
    }
    if(loops) j["loops"] = loops;
    if(totalLoops) j["totalLoops"] = totalLoops;
    if(waiting != 0.0) j["waiting"] = waiting;
    if(!vars.empty()) j["vars"] = vars;

    return j;
}

nlohmann::json HasVars::serializeVars() {
    nlohmann::json j = nlohmann::json::object();
    for(auto &[n, val] : vars) {
        j[n] = val;
    }
    return j;
}

nlohmann::json script_data::serialize() {
    nlohmann::json j = nlohmann::json::object();
    if(!vars.empty()) j["vars"] = serializeVars();
    for(auto t : dgScripts) {
        j["dgScripts"].push_back(std::make_pair(t->parent->vn, t->serialize()));
    }
    return j;
}

void script_data::deserialize(const nlohmann::json& j) {
    if(j.contains("vars")) vars = j["vars"];
    if(j.contains("dgScripts")) {
        for(auto &t : j["dgScripts"]) {
            auto proto = t[0].get<trig_vnum>();
            auto data = t[1];
            auto trig = read_trigger(proto);
            trig->deserialize(data);
            loadScript(trig);
        }
    }
}


void trig_data::deserialize(const nlohmann::json &j) {
    if(j.contains("waiting")) waiting = j["waiting"].get<double>();
    if(j.contains("depth")) {
        for(auto &d : j["depth"]) {
            depth.emplace_back(d[0].get<NestType>(), d[1].get<int>());
        }
    }
    if(j.contains("loops")) loops = j["loops"].get<int>();
    if(j.contains("totalLoops")) totalLoops = j["totalLoops"].get<int>();

    if(j.contains("vars")) vars = j["vars"];
}

std::string trig_data::serializeLocation() {
    return sc->owner->getUID(false);
}

// Note: Trigger instances are meant to be set all active or inactive on a per room/character/item basis,
// not individually.
void trig_data::activate() {
    if(active) {
        basic_mud_log("SYSERR: Attempt to activate already-active trigger %s / %ld", sc->owner->getUID(), parent->vn);
        return;
    }
    active = true;
    if(waiting != 0.0) triggers_waiting.push_back(shared_from_this());
}

void trig_data::deactivate() {
    if(!active) {
        basic_mud_log("SYSERR: Attempt to deactivate already-inactive trigger %s / %ld", sc->owner->getUID(), parent->vn);
        return;
    }
    active = false;
}


void script_data::activate() {
    for(auto t : dgScripts) {
        t->activate();
    }
}

void script_data::deactivate() {
    for(auto t : dgScripts) {
        t->deactivate();
    }
}

DgResults HasVars::getVar(const std::string& name) {
    if(name.empty()) return "";
    for(auto &[n, val] : vars) {
        if(iequals(name, n)) {
            auto uidResult = resolveUID(val);
            if(uidResult) {
                auto u = *uidResult;
                switch(u.index()) {
                    case 0:
                        return std::get<0>(u);
                    case 1:
                        return std::get<1>(u);
                    case 2:
                        return std::get<2>(u);
                }
            }
            return val;
        }
    }
    return "";
}

std::string HasVars::getRaw(const std::string& name) {
    if(name.empty()) return "";
    for(auto &[n, val] : vars) {
        if(iequals(name, n)) return val;
    }
    return "";
}

void HasVars::addVar(const std::string& name, const std::string& val) {
    std::string n = name;
    to_lower(n);
    trim(n);
    vars[n] = val;
}

void HasVars::addVar(const std::string& name, struct unit_data *u) {
    addVar(name, u->getUID(false));
}

bool HasVars::hasVar(const std::string& name) {
    if(name.empty()) return false;
    for(auto &[n, val] : vars) {
        if(iequals(name, n)) return true;
    }
    return false;
}

bool HasVars::delVar(const std::string& name) {
    if(name.empty()) return false;
    for(auto &[n, val] : vars) {
        if(iequals(name, n)) {
            vars.erase(n);
            return true;
        }
    }
    return false;
}

trig_data::trig_data(std::shared_ptr<trig_proto> parent) : parent(parent) {
    
}

void script_data::loadScript(std::shared_ptr<trig_data> t) {
    dgScripts.push_back(t);
    t->sc = this;
    types |= GET_TRIG_TYPE(t);
    t->activate();
}

trig_data::~trig_data() {
    if(active) deactivate();
}

void script_data::removeAll() {
    dgScripts.clear();
}

void script_data::removeScript(const std::string &name) {
    
}