/**************************************************************************
*  File: dg_olc.c                                                         *
*                                                                         *
*  Usage: this source file is used in extending Oasis style OLC for       *
*  dg-scripts onto a CircleMUD that already has dg-scripts (as released   *
*  by Mark Heilpern on 1/1/98) implemented.                               *
*                                                                         *
*  Parts of this file by Chris Jacobson of _Aliens vs Predator: The MUD_  *
*                                                                         *
*  $Author: Chris Jacobsen/Mark A. Heilpern/egreen/Welcor $               *
*  $Date: 2004/10/11 12:07:00$                                            *
*  $Revision: 1.0.14 $                                                    *
**************************************************************************/

#include "dbat/dg_olc.h"
#include "dbat/utils.h"
#include "dbat/comm.h"
#include "dbat/db.h"
#include "dbat/genolc.h"
#include "dbat/interpreter.h"
#include "dbat/oasis.h"
#include "dbat/dg_event.h"
#include "dbat/constants.h"
#include "dbat/act.wizard.h"
#include "dbat/modify.h"

/* local functions */
static void trigedit_disp_menu(struct descriptor_data *d);

static void trigedit_disp_types(struct descriptor_data *d);


/* ***********************************************************************
 * trigedit
 * ***********************************************************************/

ACMD(do_oasis_trigedit) {
    
}

/* called when a mob or object is being saved to disk, so its script can */
/* be saved */
void script_save_to_disk(FILE *fp, struct unit_data *item, int type) {

}

void trigedit_setup_new(struct descriptor_data *d) {

}

void trigedit_setup_existing(struct descriptor_data *d, int rtrg_num) {

}


static void trigedit_disp_menu(struct descriptor_data *d) {

}

static void trigedit_disp_types(struct descriptor_data *d) {


}

void trigedit_parse(struct descriptor_data *d, char *arg) {

}


/* save the zone's triggers to internal memory and to disk */
void trigedit_save(struct descriptor_data *d) {
    
}

void dg_olc_script_copy(struct descriptor_data *d) {

}

void dg_script_menu(struct descriptor_data *d) {

}

int dg_script_edit_parse(struct descriptor_data *d, char *arg) {
    return 0;
}

void trigedit_string_cleanup(struct descriptor_data *d, int terminator) {

}

int format_script(struct descriptor_data *d) {
    char nsc[MAX_CMD_LENGTH], *t, line[READ_SIZE];
    char *sc;
    size_t len = 0, nlen = 0, llen = 0;
    int indent = 0, indent_next = false, found_case = false, i, line_num = 0;

    if (!d->str || !*d->str)
        return false;

    sc = strdup(*d->str); /* we work on a copy, because of strtok() */
    t = strtok(sc, "\r\n");
    *nsc = '\0';

    while (t) {
        line_num++;
        skip_spaces(&t);
        if (!strncasecmp(t, "if ", 3) ||
            !strncasecmp(t, "switch ", 7)) {
            indent_next = true;
        } else if (!strncasecmp(t, "while ", 6)) {
            found_case = true;  /* so you can 'break' a loop without complains */
            indent_next = true;
        } else if (!strncasecmp(t, "end", 3) ||
                   !strncasecmp(t, "done", 4)) {
            if (!indent) {
                write_to_output(d, "Unmatched 'end' or 'done' (line %d)!\r\n", line_num);
                free(sc);
                return false;
            }
            indent--;
            indent_next = false;
        } else if (!strncasecmp(t, "else", 4)) {
            if (!indent) {
                write_to_output(d, "Unmatched 'else' (line %d)!\r\n", line_num);
                free(sc);
                return false;
            }
            indent--;
            indent_next = true;
        } else if (!strncasecmp(t, "case", 4) ||
                   !strncasecmp(t, "default", 7)) {
            if (!indent) {
                write_to_output(d, "Case/default outside switch (line %d)!\r\n", line_num);
                free(sc);
                return false;
            }
            if (!found_case) /* so we don't indent multiple case statements without a break */
                indent_next = true;
            found_case = true;
        } else if (!strncasecmp(t, "break", 5)) {
            if (!found_case || !indent) {
                write_to_output(d, "Break not in case (line %d)!\r\n", line_num);
                free(sc);
                return false;
            }
            found_case = false;
            indent--;
        }

        *line = '\0';
        for (nlen = 0, i = 0; i < indent; i++) {
            strncat(line, "  ", sizeof(line) - 1);
            nlen += 2;
        }
        llen = snprintf(line + nlen, sizeof(line) - nlen, "%s\r\n", t);
        if (llen < 0 || llen + nlen + len > d->max_str - 1) {
            write_to_output(d, "String too long, formatting aborted\r\n");
            free(sc);
            return false;
        }
        len = len + nlen + llen;
        strcat(nsc, line);  /* strcat OK, size checked above */

        if (indent_next) {
            indent++;
            indent_next = false;
        }
        t = strtok(nullptr, "\r\n");
    }

    if (indent)
        write_to_output(d, "Unmatched if, while or switch ignored.\r\n");

    free(*d->str);
    *d->str = strdup(nsc);
    free(sc);

    return true;
}
