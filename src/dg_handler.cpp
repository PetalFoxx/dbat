/**************************************************************************
*  File: dg_handler.c                                                     *
*                                                                         *
*  Usage: contains functions to handle memory for scripts.                *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
*                                                                         *
*  $Author: Mark A. Heilpern/egreen/Welcor $                              *
*  $Date: 2004/10/11 12:07:00$                                            *
*  $Revision: 1.0.14 $                                                    *
***************************************************************************/

#include "dbat/structs.h"
#include "dbat/dg_scripts.h"
#include "dbat/utils.h"
#include "dbat/db.h"
#include "dbat/handler.h"
#include "dbat/dg_event.h"



/* remove all triggers from a mob/obj/room */
void extract_script(void *thing, int type) {
    char_data *mob;
    obj_data *obj;
    room_data *room;

    switch (type) {
        case MOB_TRIGGER:
            mob = (struct char_data *) thing;
            mob->script->purged = true;
            mob->script->removeAll();
            break;
        case OBJ_TRIGGER:
            obj = (struct obj_data *) thing;
            obj->script->purged = true;
            obj->script->removeAll();
            break;
        case WLD_TRIGGER:
            room = (struct room_data *) thing;
            room->script->purged = true;
            room->script->removeAll();
            break;
    }
}

/* erase the script memory of a mob */
void extract_script_mem(struct script_memory *sc) {
    struct script_memory *next;
    while (sc) {
        next = sc->next;
        if (sc->cmd) free(sc->cmd);
        free(sc);
        sc = next;
    }
}

void free_proto_script(struct unit_data *thing, int type) {
    thing->proto_script.clear();
}

void copy_proto_script(struct unit_data *source, struct unit_data *dest, int type) {
    dest->proto_script = source->proto_script;
}

