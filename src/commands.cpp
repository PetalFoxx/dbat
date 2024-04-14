#include "dbat/commands.h"
#include "dbat/structs.h"
#include "dbat/act.movement.h"
#include "dbat/act.informative.h"
#include "dbat/area.h"
#include "dbat/tedit.h"


/* prototypes for all do_x functions. */
ACMD(do_lag);

ACMD(do_runic);

ACMD(do_refuel);

ACMD(do_extract);

ACMD(do_combine);

ACMD(do_channel);

ACMD(do_teach);

ACMD(do_action);

ACMD(do_scry);

ACMD(do_spiritcontrol);

ACMD(do_song);

ACMD(do_shell);

ACMD(do_moondust);

ACMD(do_multiform);

ACMD(do_amnisiac);

ACMD(do_arena);

ACMD(do_approve);

ACMD(do_ensnare);

ACMD(do_absorb);

ACMD(do_ingest);

ACMD(do_instill);

ACMD(do_infuse);

ACMD(do_implant);

ACMD(do_interest);

ACMD(do_appraise);

ACMD(do_forgery);

ACMD(do_fish);

ACMD(do_fireshield);

ACMD(do_feed);

ACMD(do_future);

ACMD(do_fury);

ACMD(do_aid);

ACMD(do_advance);

ACMD(do_adrenaline);

ACMD(do_aedit);

ACMD(do_alias);

ACMD(do_assemble);

ACMD(do_ashcloud);

ACMD(do_assedit);

ACMD(do_assist);

ACMD(do_rescue);

ACMD(do_resize);

ACMD(do_rpp);

ACMD(do_rip);

ACMD(do_intro);

ACMD(do_astat);

ACMD(do_at);

ACMD(do_hydromancy);

ACMD(do_liquefy);

ACMD(do_lightgrenade);

ACMD(do_attack);

ACMD(do_attack2);

ACMD(do_auction);

ACMD(do_autoexit);

ACMD(do_ban);

ACMD(do_bash);

ACMD(do_head);

ACMD(do_healglow);

ACMD(do_hayasa);

ACMD(do_nova);

ACMD(do_beacon);

ACMD(do_breaker);

ACMD(do_block);

ACMD(do_barrier);

ACMD(do_beam);

ACMD(do_tsuihidan);

ACMD(do_table);

ACMD(do_play);

ACMD(do_preference);

ACMD(do_energize);

ACMD(do_evolve);

ACMD(do_post);

ACMD(do_hand);

ACMD(do_trip);

ACMD(do_train);

ACMD(do_taisha);

ACMD(do_twohand);

ACMD(do_tailwhip);

ACMD(do_shogekiha);

ACMD(do_shimmer);

ACMD(do_sunder);

ACMD(do_seishou);

ACMD(do_spiral);

ACMD(do_spoil);

ACMD(do_selfd);

ACMD(do_shuffle);

ACMD(do_bid);

ACMD(do_boom);

ACMD(do_break);

ACMD(do_disguise);

ACMD(do_defend);

ACMD(do_deploy);

ACMD(do_pack);

ACMD(do_dimizu);

ACMD(do_obstruct);

ACMD(do_draw);

ACMD(do_diagnose);

ACMD(do_drag);

ACMD(do_carry);

ACMD(do_cook);

ACMD(do_stop);

ACMD(do_silk);

ACMD(do_focus);

ACMD(do_charge);

ACMD(do_candy);

ACMD(do_form);

ACMD(do_checkloadstatus);

ACMD(do_finger);

ACMD(do_chown);

ACMD(do_clan);

ACMD(do_color);

ACMD(do_compare);

ACMD(do_copyover);

ACMD(do_combo);

ACMD(do_commands);

ACMD(do_consider);

ACMD(do_credits);

ACMD(do_date);

ACMD(do_dc);

ACMD(do_disable);

ACMD(do_dig);

ACMD(do_bury);

ACMD(do_disarm);

ACMD(do_display);

ACMD(do_drink);

ACMD(do_drive);

ACMD(do_drop);

ACMD(do_eat);

ACMD(do_commune);

ACMD(do_rptrans);

ACMD(do_aura);

ACMD(do_escape);

ACMD(do_eavesdrop);

ACMD(do_eyec);

ACMD(do_solar);

ACMD(do_spit);

ACMD(do_echo);

ACMD(do_enter);

ACMD(do_equipment);

ACMD(do_examine);

ACMD(do_exit);

ACMD(do_exits);

ACMD(do_feats);

ACMD(do_file);

ACMD(do_finddoor);

ACMD(do_findkey);

ACMD(do_fix);

ACMD(do_srepair);

ACMD(do_recharge);

ACMD(do_renzo);

ACMD(do_flee);

ACMD(do_follow);

ACMD(do_force);

ACMD(do_gecho);

ACMD(do_garden);

ACMD(do_grapple);

ACMD(do_ginfo);

ACMD(do_gen_comm);

ACMD(do_gen_door);

ACMD(do_gen_ps);

ACMD(do_gen_tog);

ACMD(do_gen_write);

ACMD(do_get);

ACMD(do_give);

ACMD(do_gold);

ACMD(do_goto);

ACMD(do_grab);

ACMD(do_group);

ACMD(do_gsay);

ACMD(do_hell);

ACMD(do_hspiral);

ACMD(do_heal);

ACMD(do_help);

ACMD(do_hindex);

ACMD(do_history);

ACMD(do_helpcheck);

ACMD(do_hide);

ACMD(do_pushup);

ACMD(do_paralyze);

ACMD(do_perf);

ACMD(do_permission);

ACMD(do_reward);

ACMD(do_plant);

ACMD(do_meditate);

ACMD(do_malice);

ACMD(do_mimic);

ACMD(do_masenko);

ACMD(do_dodonpa);

ACMD(do_deathball);

ACMD(do_spiritball);

ACMD(do_genki);

ACMD(do_geno);

ACMD(do_deathbeam);

ACMD(do_dualbeam);

ACMD(do_pose);

ACMD(do_rogafufuken);

ACMD(do_baku);

ACMD(do_kienzan);

ACMD(do_kanso);

ACMD(do_koteiru);

ACMD(do_kyodaika);

ACMD(do_kousengan);

ACMD(do_kura);

ACMD(do_tribeam);

ACMD(do_sbc);

ACMD(do_final);

ACMD(do_crusher);

ACMD(do_ddslash);

ACMD(do_pbarrage);

ACMD(do_hellflash);

ACMD(do_hellspear);

ACMD(do_scatter);

ACMD(do_bigbang);

ACMD(do_pslash);

ACMD(do_kakusanha);

ACMD(do_hass);

ACMD(do_handout);

ACMD(do_eraser);

ACMD(do_tslash);

ACMD(do_psyblast);

ACMD(do_honoo);

ACMD(do_galikgun);

ACMD(do_majinize);

ACMD(do_potential);

ACMD(do_map);

ACMD(do_situp);

ACMD(do_iedit);

ACMD(do_instant);

ACMD(do_insult);

ACMD(do_inventory);

ACMD(do_invis);

ACMD(do_kill);

ACMD(do_kamehameha);

ACMD(do_kiball);

ACMD(do_kiblast);

ACMD(do_kick);

ACMD(do_elbow);

ACMD(do_knee);

ACMD(do_languages);

ACMD(do_lifeforce);

ACMD(do_land);

ACMD(do_last);

ACMD(do_leave);

ACMD(do_levels);

ACMD(do_load);

ACMD(do_look);

ACMD(do_balefire);

ACMD(do_blessedhammer);

/* ACMD(do_move); -- interpreter.h */
ACMD(do_not_here);

ACMD(do_news);

ACMD(do_newsedit);

ACMD(do_nickname);

ACMD(do_oasis_copy);

ACMD(do_oasis);

ACMD(do_olc);

ACMD(do_page);

ACMD(do_powerup);

ACMD(do_suppress);

ACMD(do_punch);

ACMD(do_bite);

ACMD(do_roundhouse);

ACMD(do_regenerate);

ACMD(do_uppercut);

ACMD(do_upgrade);

ACMD(do_peace);

ACMD(do_plist);

ACMD(do_poofset);

ACMD(do_pour);

ACMD(do_practice);

ACMD(do_pgrant);

ACMD(do_eratime);

ACMD(do_rpreward);

ACMD(do_purge);

ACMD(do_put);

ACMD(do_qcomm);

ACMD(do_quit);

ACMD(do_raise);

ACMD(do_radar);

ACMD(do_sradar);

ACMD(do_recall);

ACMD(do_reboot);

ACMD(do_remove);

ACMD(do_reply);

ACMD(do_report);

ACMD(do_respond);

ACMD(do_rest);

ACMD(do_restore);

ACMD(do_resurrect);

ACMD(do_return);

ACMD(do_room_copy);

ACMD(do_sac);

ACMD(do_spar);

ACMD(do_slam);

ACMD(do_heeldrop);

ACMD(do_scouter);

ACMD(do_snet);

ACMD(do_save);

ACMD(do_skills);

ACMD(do_saveall);

ACMD(do_say);

ACMD(do_osay);

ACMD(do_scan);

ACMD(do_score);

ACMD(do_status);

ACMD(do_scribe);

ACMD(do_send);

ACMD(do_set);

ACMD(do_show);

ACMD(do_showoff);

ACMD(do_show_save_list);

ACMD(do_shutdown);

ACMD(do_sit);

ACMD(do_rcopy);

ACMD(do_skillset);

ACMD(do_sleep);

ACMD(do_sneak);

ACMD(do_snoop);

ACMD(do_spec_comm);

ACMD(do_spells);

ACMD(do_gen);

ACMD(do_split);

ACMD(do_stand);

ACMD(do_fly);

ACMD(do_stat);

ACMD(do_steal);

ACMD(do_switch);

ACMD(do_syslog);

ACMD(do_telepathy);

ACMD(do_think);

ACMD(do_transform);

ACMD(do_transobj);

ACMD(do_throw);

ACMD(do_teleport);

ACMD(do_tell);

ACMD(do_time);

ACMD(do_title);

ACMD(do_toggle);

ACMD(do_toplist);

ACMD(do_track);

ACMD(do_trans);

ACMD(do_unban);

ACMD(do_ungroup);

ACMD(do_use);

ACMD(do_users);

ACMD(do_visible);

ACMD(do_varstat);

ACMD(do_voice);

ACMD(do_vnum);

ACMD(do_vstat);

ACMD(do_wake);

ACMD(do_warp);

ACMD(do_warppool);

ACMD(do_razor);

ACMD(do_spike);

ACMD(do_willpower);

ACMD(do_summon);

ACMD(do_wear);

ACMD(do_weather);

ACMD(do_where);

ACMD(do_who);

ACMD(do_whois);

ACMD(do_wield);

ACMD(do_value);

ACMD(do_wizlock);

ACMD(do_wiznet);

ACMD(do_wizupdate);

ACMD(do_wizutil);

ACMD(do_write);

ACMD(do_zcheck);

ACMD(do_zen);

ACMD(do_zanzoken);

ACMD(do_zreset);

ACMD(do_zpurge);

ACMD(do_tailhide);

ACMD(do_nogrow);

ACMD(do_restring);

/* DG Script ACMD's */
ACMD(do_attach);

ACMD(do_detach);

ACMD(do_tlist);

ACMD(do_tstat);

ACMD(do_masound);

ACMD(do_mkill);

ACMD(do_mheal);

ACMD(do_mjunk);

ACMD(do_mdoor);

ACMD(do_mechoaround);

ACMD(do_msend);

ACMD(do_mecho);

ACMD(do_mload);

ACMD(do_mpurge);

ACMD(do_mgoto);

ACMD(do_mat);

ACMD(do_mdamage);

ACMD(do_mteleport);

ACMD(do_mforce);

ACMD(do_mremember);

ACMD(do_mforget);

ACMD(do_mtransform);

ACMD(do_mzoneecho);

ACMD(do_vdelete);

ACMD(do_mfollow);

ACMD(do_dig);

ACMD(do_rdisplay);

ACMD(do_maddtransform);

struct command_info *complete_cmd_info;

const struct command_info cmd_info[] = {
        {"RESERVED",      "",        0,                 nullptr,            0,  ADMLVL_NONE,    0,      0},     /* this must be first -- for specprocs */

        /* directions must come before other commands but after RESERVED */
        {"north",         "n",            POS_RESTING,  do_move,            0,  ADMLVL_NONE,    SCMD_NORTH,      1},
        {"east",          "e",            POS_RESTING,  do_move,            0,  ADMLVL_NONE,    SCMD_EAST,      1},
        {"south",         "s",            POS_RESTING,  do_move,            0,  ADMLVL_NONE,    SCMD_SOUTH,      1},
        {"west",          "w",            POS_RESTING,  do_move,            0,  ADMLVL_NONE,    SCMD_WEST,      1},
        {"up",            "u",            POS_RESTING,  do_move,            0,  ADMLVL_NONE,    SCMD_UP,      1},
        {"down",          "d",            POS_RESTING,  do_move,            0,  ADMLVL_NONE,    SCMD_DOWN,      1},
        {"northwest",     "northw",       POS_RESTING,  do_move,            0,  ADMLVL_NONE,    SCMD_NW,      1},
        {"nw",            "nw",           POS_RESTING,  do_move,            0,  ADMLVL_NONE,    SCMD_NW,      1},
        {"northeast",     "northe",       POS_RESTING,  do_move,            0,  ADMLVL_NONE,    SCMD_NE,      1},
        {"ne",            "ne",           POS_RESTING,  do_move,            0,  ADMLVL_NONE,    SCMD_NE,      1},
        {"southeast",     "southe",       POS_RESTING,  do_move,            0,  ADMLVL_NONE,    SCMD_SE,      1},
        {"se",            "se",           POS_RESTING,  do_move,            0,  ADMLVL_NONE,    SCMD_SE,      1},
        {"southwest",     "southw",       POS_RESTING,  do_move,            0,  ADMLVL_NONE,    SCMD_SW,      1},
        {"sw",            "sw",           POS_RESTING,  do_move,            0,  ADMLVL_NONE,    SCMD_SW,      1},
        {"i",             "i",            POS_DEAD,     do_inventory,       0,  ADMLVL_NONE,    0,      0},
        {"inside",        "in",           POS_RESTING,  do_move,            0,  ADMLVL_NONE,    SCMD_IN,      1},
        {"outside",       "out",          POS_RESTING,  do_move,            0,  ADMLVL_NONE,    SCMD_OUT,      1},

        /* now, the main list */
        {"absorb",        "absor",        POS_FIGHTING, do_absorb,          0,  ADMLVL_NONE,    0,      1},
        {"at",            "at",           POS_DEAD,     do_at,              0,  ADMLVL_BUILDER, 0,      0},
        {"adrenaline",    "adrenalin",    POS_DEAD,     do_adrenaline,      0,  ADMLVL_NONE,    0,      1},
        {"advance",       "adv",          POS_DEAD,     do_advance,         0,  ADMLVL_GOD,    0,      1},
        {"aedit",         "aed",          POS_DEAD,     do_oasis,           0,  ADMLVL_BUILDER, SCMD_OASIS_AEDIT,      0},
        {"alias",         "ali",          POS_DEAD,     do_alias,           0,  ADMLVL_NONE,    0,      0},
        {"afk",           "afk",          POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_AFK,      0},
        {"aid",           "aid",          POS_STANDING, do_aid,             0,  ADMLVL_NONE,    0,      1},
        {"amnesiac",      "amnesia",      POS_STANDING, do_amnisiac,        0,  ADMLVL_NONE,    0,      0},
        {"appraise",      "apprais",      POS_STANDING, do_appraise,        0,  ADMLVL_NONE,    0,      1},
        {"approve",       "approve",      POS_STANDING, do_approve,         0,  ADMLVL_IMMORT,  0,      0},
        {"arena",         "aren",         POS_RESTING,  do_arena,           0,  ADMLVL_NONE,    0,      0},
        {"arlist",         "arl",         POS_DEAD,     do_arlist,          0,  ADMLVL_BUILDER, 0,      0},
        {"ashcloud",      "ashclou",      POS_RESTING,  do_ashcloud,        0,  ADMLVL_NONE,    0,      1},
        {"assedit",       "assed",        POS_STANDING, do_assedit,         0,  ADMLVL_GOD,     0,      0},
        {"assist",        "assis",        POS_STANDING, do_assist,          0,  ADMLVL_NONE,    0,      0},
        {"astat",         "ast",          POS_DEAD,     do_astat,           0,  ADMLVL_GOD,     0,      0},
        {"ask",           "ask",          POS_RESTING,  do_spec_comm,       0,  ADMLVL_NONE,    SCMD_ASK,      0},
        {"attack",        "attack",       POS_FIGHTING, do_attack,          0, 0,               0,      1},
        {"auction",       "auctio",       POS_RESTING,  do_not_here,        0, 0,               0,      0},
        {"augment",       "augmen",       POS_SITTING,  do_not_here,        1,  ADMLVL_NONE,    0,      1},
        {"aura",          "aura",         POS_RESTING,  do_aura,            0,  ADMLVL_NONE,    0,      0},
        {"autoexit",      "autoex",       POS_DEAD,     do_autoexit,        0,  ADMLVL_NONE,    0,      0},
        {"autogold",      "autogo",       POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_AUTOGOLD,      0},
        {"autoloot",      "autolo",       POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_AUTOLOOT,      0},
        {"autosplit",     "autosp",       POS_DEAD,     do_gen_tog,         0,  ADMLVL_IMMORT,  SCMD_AUTOSPLIT,      0},

        {"bakuhatsuha",   "baku",         POS_FIGHTING, do_baku,            0, 0,               0,      1},
        {"ban",           "ban",          POS_DEAD,     do_ban,             0,  ADMLVL_VICE,    0,      0},
        {"balance",       "bal",          POS_STANDING, do_not_here,        1,  ADMLVL_NONE,    0,      1},
        {"balefire",      "balef",        POS_FIGHTING, do_balefire,        0,  ADMLVL_NONE,    0,      1},
        {"barrage",       "barrage",      POS_FIGHTING, do_pbarrage,        0,  ADMLVL_NONE,    0,      1},
        {"barrier",       "barri",        POS_FIGHTING, do_barrier,         0,  ADMLVL_NONE,    0,      1},
        {"bash",          "bas",          POS_FIGHTING, do_bash,            0,  ADMLVL_NONE,    0,      1},
        {"beam",          "bea",          POS_FIGHTING, do_beam,            0,  ADMLVL_NONE,    0,      1},
        {"bid",           "bi",           POS_RESTING,  do_bid,             0, 0,               0,      0},
        {"bigbang",       "bigban",       POS_FIGHTING, do_bigbang,         0, 0,               0,      1},
        {"bite",          "bit",          POS_FIGHTING, do_bite,            0, 0,               0,      1},
        {"blessedhammer", "bham",         POS_FIGHTING, do_blessedhammer,   0,  ADMLVL_NONE,    0,      1},
        {"block",         "block",        POS_FIGHTING, do_block,           0, 0,               0,      1},
        {"book",          "boo",          POS_SLEEPING, do_gen_ps,          0,  ADMLVL_IMMORT,  SCMD_INFO,      0},
        {"break",         "break",        POS_STANDING, do_break,           0,  ADMLVL_IMMORT,  0,      1},
        {"brief",         "br",           POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_BRIEF,      0},
        {"build",         "bui",          POS_SITTING,  do_assemble,        0,  ADMLVL_NONE,    SCMD_BREW,      1},
        {"buildwalk",     "buildwalk",    POS_STANDING, do_gen_tog,         0,  ADMLVL_IMMORT,  SCMD_BUILDWALK,      0},
        {"buy",           "bu",           POS_STANDING, do_not_here,        0,  ADMLVL_NONE,    0,      0},
        {"bug",           "bug",          POS_DEAD,     do_gen_write,       0,  ADMLVL_NONE,    SCMD_BUG,      0},

        {"cancel",        "cance",        POS_RESTING,  do_not_here,        0, 0,               0,      0},
        {"candy",         "cand",         POS_FIGHTING, do_candy,           0, 0,               0,      1},
        {"carry",         "carr",         POS_STANDING, do_carry,           0, 0,               0,      0},
        {"carve",         "carv",         POS_SLEEPING, do_gen_tog,         0, 0,               SCMD_CARVE,      0},
        {"cedit",         "cedit",        POS_DEAD,     do_oasis,           0,  ADMLVL_IMPL,    SCMD_OASIS_CEDIT,      0},
        {"channel",       "channe",       POS_FIGHTING, do_channel,         0, 0,               0,      0},
        {"charge",        "char",         POS_FIGHTING, do_charge,          0, 0,               0,      1},
        {"check",         "ch",           POS_STANDING, do_not_here,        1,  ADMLVL_NONE,    0,      0},
        {"checkload",     "checkl",       POS_DEAD,     do_checkloadstatus, 0,  ADMLVL_GOD,     0,      0},
        {"chown",         "cho",          POS_DEAD,     do_chown,           1,  ADMLVL_IMPL,    0,      0},
        {"clan",          "cla",          POS_DEAD,     do_clan,            0,  ADMLVL_NONE,    0,      0},
        {"clear",         "cle",          POS_DEAD,     do_gen_ps,          0,  ADMLVL_NONE,    SCMD_CLEAR,      0},
        {"close",         "cl",           POS_SITTING,  do_gen_door,        0,  ADMLVL_NONE,    SCMD_CLOSE,      0},
        {"closeeyes",     "closeey",      POS_RESTING,  do_eyec,            0,  ADMLVL_NONE,    0,      1},
        {"cls",           "cls",          POS_DEAD,     do_gen_ps,          0,  ADMLVL_NONE,    SCMD_CLEAR,      0},
        {"clsolc",        "clsolc",       POS_DEAD,     do_gen_tog,         0,  ADMLVL_BUILDER, SCMD_CLS,      0},
        {"consider",      "con",          POS_RESTING,  do_consider,        0,  ADMLVL_NONE,    0,      0},
        {"color",         "col",          POS_DEAD,     do_color,           0,  ADMLVL_NONE,    0,      0},
        {"combo",         "combo",        POS_RESTING,  do_combo,           0,  ADMLVL_NONE,    0,      1},
        {"combine",       "comb",         POS_RESTING,  do_combine,         0,  ADMLVL_NONE,    0,      1},
        {"compare",       "comp",         POS_RESTING,  do_compare,         0,  ADMLVL_NONE,    0,      0},
        {"commands",      "com",          POS_DEAD,     do_commands,        0,  ADMLVL_NONE,    SCMD_COMMANDS,      0},
        {"commune",       "comm",         POS_DEAD,     do_commune,         0,  ADMLVL_NONE,    0,      1},
        {"compact",       "compact",      POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_COMPACT,      0},
        {"cook",          "coo",          POS_RESTING,  do_cook,            0,  ADMLVL_NONE,    0,      0},
        {"copyover",      "copyover",     POS_DEAD,     do_copyover,        0,  ADMLVL_GOD,     0,      0},
        {"create",        "crea",         POS_STANDING, do_form,            0,  ADMLVL_NONE,    0,      1},
        {"credits",       "cred",         POS_DEAD,     do_gen_ps,          0,  ADMLVL_NONE,    SCMD_CREDITS,      0},
        {"crusher",       "crushe",       POS_FIGHTING, do_crusher,         0, 0,               0,      1},

        {"date",          "da",           POS_DEAD,     do_date,            0,  ADMLVL_IMMORT,  SCMD_DATE,      0},
        {"darkness",      "darknes",      POS_FIGHTING, do_ddslash,         0,  ADMLVL_NONE,    0,      1},
        {"dc",            "dc",           POS_DEAD,     do_dc,              0,  ADMLVL_GOD,     0,      0},
        {"deathball",     "deathbal",     POS_FIGHTING, do_deathball,       0,  ADMLVL_NONE,    0,      1},
        {"deathbeam",     "deathbea",     POS_FIGHTING, do_deathbeam,       0,  ADMLVL_NONE,    0,      1},
        {"decapitate",    "decapit",      POS_STANDING, do_spoil,           0,  ADMLVL_NONE,    0,      1},
        {"defend",        "defen",        POS_STANDING, do_defend,          0,  ADMLVL_NONE,    0,      1},
        {"deploy",        "deplo",        POS_STANDING, do_deploy,          0,  ADMLVL_NONE,    0,      0},
        {"describe",      "desc",         POS_DEAD,     do_desc,            0,  ADMLVL_NONE,    0,      0},
        {"dualbeam",      "dualbea",      POS_FIGHTING, do_dualbeam,        0,  ADMLVL_NONE,    0,      1},
        {"deposit",       "depo",         POS_STANDING, do_not_here,        1,  ADMLVL_NONE,    0,      0},
        {"diagnose",      "diagnos",      POS_RESTING,  do_diagnose,        0,  ADMLVL_NONE,    0,      1},
        {"dimizu",        "dimizu",       POS_STANDING, do_dimizu,          0, 0,               0,      0},
        {"disable",       "disa",         POS_DEAD,     do_disable,         0,  ADMLVL_VICE,    0,      0},
        {"disguise",      "disguis",      POS_DEAD,     do_disguise,        0, 0,               0,      1},
        {"dig",           "dig",          POS_DEAD,     do_bury,            0,  ADMLVL_NONE,    0,      0},
        {"display",       "disp",         POS_DEAD,     do_display,         0,  ADMLVL_NONE,    0,      0},
        {"dodonpa",       "dodon",        POS_FIGHTING, do_dodonpa,         0,  ADMLVL_NONE,    0,      1},
        {"donate",        "don",          POS_RESTING,  do_drop,            0,  ADMLVL_NONE,    SCMD_DONATE,      0},
        {"drag",          "dra",          POS_STANDING, do_drag,            0,  ADMLVL_NONE,    0,      0},
        {"draw",          "dra",          POS_SITTING,  do_draw,            0,  ADMLVL_NONE,    0,      0},
        {"drink",         "dri",          POS_RESTING,  do_drink,           0,  ADMLVL_NONE,    SCMD_DRINK,      1},
        {"drop",          "dro",          POS_RESTING,  do_drop,            0,  ADMLVL_NONE,    SCMD_DROP,      0},
        {"dub",           "du",           POS_STANDING, do_intro,           0,  ADMLVL_NONE,    0,      0},

        {"eat",           "ea",           POS_RESTING,  do_eat,             0,  ADMLVL_NONE,    SCMD_EAT,      1},
        {"eavesdrop",     "eaves",        POS_RESTING,  do_eavesdrop,       0,  ADMLVL_NONE,    0,      0},
        {"echo",          "ec",           POS_SLEEPING, do_echo,            0,  ADMLVL_IMMORT,  SCMD_ECHO,      0},
        {"elbow",         "elb",          POS_FIGHTING, do_elbow,           0,  ADMLVL_NONE,    0,      1},
        {"emote",         "em",           POS_RESTING,  do_echo,            1,  ADMLVL_NONE,    SCMD_EMOTE,      0},
        {"energize",      "energiz",      POS_RESTING,  do_energize,        1,  ADMLVL_NONE,    0,      1},
        {":",             ":",            POS_RESTING,  do_echo,            1,  ADMLVL_NONE,    SCMD_EMOTE,      0},
        {"ensnare",       "ensnar",       POS_FIGHTING, do_ensnare,         0,  ADMLVL_NONE,    0,      1},
        {"enter",         "ent",          POS_STANDING, do_enter,           0,  ADMLVL_NONE,    0,      0},
        {"equipment",     "eq",           POS_SLEEPING, do_equipment,       0,  ADMLVL_NONE,    0,      0},
        {"eraser",        "eras",         POS_FIGHTING, do_eraser,          0,  ADMLVL_NONE,    0,      1},
        {"escape",        "esca",         POS_RESTING,  do_escape,          0,  ADMLVL_NONE,    0,      1},
        {"evolve",        "evolv",        POS_RESTING,  do_evolve,          0,  ADMLVL_NONE,    0,      0},
        {"exchange",      "exchan",       POS_RESTING,  do_rptrans,         0,  ADMLVL_NONE,    0,      0},
        {"exits",         "ex",           POS_RESTING,  do_exits,           0,  ADMLVL_NONE,    0,      0},
        {"examine",       "exa",          POS_SITTING,  do_examine,         0,  ADMLVL_NONE,    0,      0},
        {"extract",       "extrac",       POS_STANDING, do_extract,         0,  ADMLVL_NONE,    0,      1},

        {"feed",          "fee",          POS_STANDING, do_feed,            0,  ADMLVL_NONE,    0,      1},
        {"fill",          "fil",          POS_STANDING, do_pour,            0,  ADMLVL_NONE,    SCMD_FILL,      0},
        {"file",          "fi",           POS_SLEEPING, do_file,            0,  ADMLVL_IMMORT,  0,      0},
        {"finalflash",    "finalflash",   POS_FIGHTING, do_final,           0,  ADMLVL_NONE,    0,      1},
        {"finddoor",      "findd",        POS_SLEEPING, do_finddoor,        0,  ADMLVL_IMMORT,  0,      0},
        {"findkey",       "findk",        POS_SLEEPING, do_findkey,         0,  ADMLVL_IMMORT,  0,      0},
        {"finger",        "finge",        POS_SLEEPING, do_finger,          0,  ADMLVL_NONE,    0,      0},
        {"fireshield",    "firesh",       POS_STANDING, do_fireshield,      0,  ADMLVL_NONE,    0,      0},
        {"fish",          "fis",          POS_STANDING, do_fish,            0,  ADMLVL_NONE,    0,      1},
        {"fix",           "fix",          POS_STANDING, do_fix,             0,  ADMLVL_NONE,    0,      0},
        {"flee",          "fl",           POS_FIGHTING, do_flee,            1,  ADMLVL_NONE,    0,      0},
        {"fly",           "fly",          POS_RESTING,  do_fly,             0,  ADMLVL_NONE,    0,      0},
        {"focus",         "foc",          POS_STANDING, do_focus,           0,  ADMLVL_NONE,    0,      0},
        {"follow",        "fol",          POS_RESTING,  do_follow,          0,  ADMLVL_NONE,    0,      0},
        {"force",         "force",        POS_SLEEPING, do_force,           0,  ADMLVL_IMMORT,  0,      0},
        {"forgery",       "forg",         POS_RESTING,  do_forgery,         0,  ADMLVL_NONE,    0,      0},
        {"forget",        "forg",         POS_RESTING,  do_not_here,        0,  ADMLVL_NONE,    0,      0},
        {"freeze",        "freeze",       POS_DEAD,     do_wizutil,         0,  ADMLVL_IMMORT,  SCMD_FREEZE,      0},
        {"fury",          "fury",         POS_FIGHTING, do_fury,            0,  ADMLVL_NONE,    0,      0},
        {"future",        "futu",         POS_STANDING, do_future,          0,  ADMLVL_NONE,    0,      0},

        {"gain",          "ga",           POS_RESTING,  do_not_here,        0,  ADMLVL_NONE,    0,      0},
        {"galikgun",      "galik",        POS_FIGHTING, do_galikgun,        0,  ADMLVL_NONE,    0,      1},
        {"game",          "gam",          POS_RESTING,  do_show,            0,  ADMLVL_IMMORT,  0,      0},
        {"garden",        "garde",        POS_STANDING, do_garden,          0,  ADMLVL_NONE,    0,      0},
        {"genkidama",     "genkidam",     POS_FIGHTING, do_genki,           0,  ADMLVL_NONE,    0,      1},
        {"genocide",      "genocid",      POS_FIGHTING, do_geno,            0,  ADMLVL_NONE,    0,      1},
        {"get",           "get",          POS_RESTING,  do_get,             0,  ADMLVL_NONE,    0,      0},
        {"gecho",         "gecho",        POS_DEAD,     do_gecho,           0,  ADMLVL_BUILDER, 0,      0},
        {"gedit",         "gedit",        POS_DEAD,     do_oasis,           0,  ADMLVL_BUILDER, SCMD_OASIS_GEDIT,      0},
        {"gemote",        "gem",          POS_SLEEPING, do_gen_comm,        0,  ADMLVL_NONE,    SCMD_GEMOTE,      0},
        {"generator",     "genr",         POS_STANDING, do_not_here,        1,  ADMLVL_NONE,    0,      0},
        {"glist",         "glist",        POS_SLEEPING, do_oasis,           0,  ADMLVL_BUILDER, SCMD_OASIS_GLIST,      0},
        {"give",          "giv",          POS_RESTING,  do_give,            0,  ADMLVL_NONE,    0,      0},
        {"goto",          "go",           POS_SLEEPING, do_goto,            0,  ADMLVL_IMMORT,  0,      0},
        {"gold",          "gol",          POS_RESTING,  do_gold,            0,  ADMLVL_NONE,    0,      0},
        {"group",         "gro",          POS_RESTING,  do_group,           1,  ADMLVL_NONE,    0,      0},
        {"grab",          "grab",         POS_RESTING,  do_grab,            0,  ADMLVL_NONE,    0,      0},
        {"grand",         "gran",         POS_RESTING,  do_not_here,        0,  ADMLVL_NONE,    0,      0},
        {"grapple",       "grapp",        POS_FIGHTING, do_grapple,         0,  ADMLVL_NONE,    0,      1},
        {"grats",         "grat",         POS_SLEEPING, do_gen_comm,        0,  ADMLVL_NONE,    SCMD_GRATZ,      0},
        {"gravity",       "grav",         POS_STANDING, do_not_here,        1,  ADMLVL_NONE,    0,      0},
        {"gsay",          "gsay",         POS_SLEEPING, do_gsay,            0,  ADMLVL_NONE,    0,      0},
        {"gtell",         "gt",           POS_SLEEPING, do_gsay,            0,  ADMLVL_NONE,    0,      0},

        {"hand",          "han",          POS_SITTING,  do_hand,            0,  ADMLVL_NONE,    0,      0},
        {"handout",       "hand",         POS_STANDING, do_handout,         0,  ADMLVL_GOD,     0,      0},
        {"hasshuken",     "hasshuke",     POS_STANDING, do_hass,            0,  ADMLVL_NONE,    0,      1},
        {"hayasa",        "hayas",        POS_STANDING, do_hayasa,          0,  ADMLVL_NONE,    0,      1},
        {"headbutt",      "headbut",      POS_FIGHTING, do_head,            0,  ADMLVL_NONE,    0,      1},
        {"heal",          "hea",          POS_STANDING, do_heal,            0,  ADMLVL_NONE,    0,      0},
        {"health",        "hea",          POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_GHEALTH,      0},
        {"healingglow",   "healing",      POS_STANDING, do_healglow,        0,  ADMLVL_NONE,    0,      1},
        {"heeldrop",      "heeldr",       POS_FIGHTING, do_heeldrop,        0,  ADMLVL_NONE,    0,      1},
        {"hellflash",     "hellflas",     POS_FIGHTING, do_hellflash,       0,  ADMLVL_NONE,    0,      1},
        {"hellspear",     "hellspea",     POS_FIGHTING, do_hellspear,       0,  ADMLVL_NONE,    0,      1},
        {"help",          "h",            POS_DEAD,     do_help,            0,  ADMLVL_NONE,    0,      0},
        {"hedit",         "hedit",        POS_DEAD,     do_oasis,           0,  ADMLVL_IMMORT,  SCMD_OASIS_HEDIT,      0},
        {"hindex",        "hind",         POS_DEAD,     do_hindex,          0,  ADMLVL_NONE,    0,      0},
        {"helpcheck",     "helpch",       POS_DEAD,     do_helpcheck,       0,  ADMLVL_NONE,    0,      0},
        {"handbook",      "handb",        POS_DEAD,     do_gen_ps,          0,  ADMLVL_IMMORT,  SCMD_HANDBOOK,      0},
        {"hide",          "hide",         POS_RESTING,  do_gen_tog,         1,  ADMLVL_NONE,    SCMD_HIDE,      0},
        {"hints",         "hints",        POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_HINTS,      0},
        {"history",       "hist",         POS_DEAD,     do_history,         0,  ADMLVL_NONE,    0,      0},
        {"hold",          "hold",         POS_RESTING,  do_grab,            1,  ADMLVL_NONE,    0,      0},
        {"holylight",     "holy",         POS_DEAD,     do_gen_tog,         0,  ADMLVL_IMMORT,  SCMD_HOLYLIGHT,      0},
        {"honoo",         "hono",         POS_FIGHTING, do_honoo,           0,  ADMLVL_NONE,    0,      1},
        {"hsedit",        "hsedit",       POS_DEAD,     do_oasis,           0,  ADMLVL_BUILDER, SCMD_OASIS_HSEDIT,      0},
        {"hspiral",       "hspira",       POS_FIGHTING, do_hspiral,         0,  ADMLVL_NONE,    0,      1},
        {"htank",         "htan",         POS_STANDING, do_not_here,        1,  ADMLVL_NONE,    0,      0},
        {"hydromancy",    "hydrom",       POS_STANDING, do_hydromancy,      0,  ADMLVL_NONE,    0,      1},
        {"hyoga",         "hyoga",        POS_STANDING, do_obstruct,        0,  ADMLVL_NONE,    0,      1},

        {"ihealth",       "ihea",         POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_IHEALTH,      0},
        {"info",          "info",         POS_DEAD,     do_ginfo,           0,  ADMLVL_IMMORT,  0,      0},
        {"infuse",        "infus",        POS_STANDING, do_infuse,          0,  ADMLVL_NONE,    0,      1},
        {"ingest",        "inges",        POS_STANDING, do_ingest,          0,  ADMLVL_NONE,    0,      0},
        {"imotd",         "imotd",        POS_DEAD,     do_gen_ps,          0,  ADMLVL_IMMORT,  SCMD_IMOTD,      0},
        {"immlist",       "imm",          POS_DEAD,     do_gen_ps,          0,  ADMLVL_NONE,    SCMD_WIZLIST,      0},
        {"implant",       "implan",       POS_RESTING,  do_implant,         0,  ADMLVL_NONE,    0,      0},
        {"instant",       "insta",        POS_STANDING, do_instant,         0,  ADMLVL_NONE,    0,      0},
        {"instill",       "instil",       POS_STANDING, do_instill,         0,  ADMLVL_NONE,    0,      0},
        {"instruct",      "instruc",      POS_STANDING, do_gen_tog,         0, 0,               SCMD_INSTRUCT,      0},
        /*{ "insult"   , "insult"	, POS_RESTING , do_insult   , 0, ADMLVL_NONE	, 0 },*/
        {"inventory",     "inv",          POS_DEAD,     do_inventory,       0,  ADMLVL_NONE,    0,      0},
        {"interest",      "inter",        POS_DEAD,     do_interest,        0,  ADMLVL_IMPL,    0,      0},
        {"iedit",         "ie",           POS_DEAD,     do_iedit,           0,  ADMLVL_IMPL,    0,      0},
        {"invis",         "invi",         POS_DEAD,     do_invis,           0,  ADMLVL_IMMORT,  0,      0},
        {"iwarp",         "iwarp",        POS_RESTING,  do_warp,            0,  ADMLVL_NONE,    0,      0},

        {"junk",          "junk",         POS_RESTING,  do_drop,            0,  ADMLVL_NONE,    SCMD_JUNK,      0},

        {"kakusanha",     "kakusan",      POS_FIGHTING, do_kakusanha,       0,  ADMLVL_NONE,    0,      1},
        {"kamehameha",    "kame",         POS_FIGHTING, do_kamehameha,      0,  ADMLVL_NONE,    0,      1},
        {"kanso",         "kans",         POS_FIGHTING, do_kanso,           0,  ADMLVL_NONE,    0,      1},
        {"kiball",        "kibal",        POS_FIGHTING, do_kiball,          0,  ADMLVL_NONE,    0,      1},
        {"kiblast",       "kiblas",       POS_FIGHTING, do_kiblast,         0,  ADMLVL_NONE,    0,      1},
        {"kienzan",       "kienza",       POS_FIGHTING, do_kienzan,         0,  ADMLVL_NONE,    0,      1},
        {"kill",          "kil",          POS_FIGHTING, do_kill,            0,  ADMLVL_IMMORT,  0,      1},
        {"kick",          "kic",          POS_FIGHTING, do_kick,            0,  ADMLVL_NONE,    0,      1},
        {"knee",          "kne",          POS_FIGHTING, do_knee,            0,  ADMLVL_NONE,    0,      1},
        {"koteiru",       "koteiru",      POS_FIGHTING, do_koteiru,         0,  ADMLVL_NONE,    0,      1},
        {"kousengan",     "kousengan",    POS_FIGHTING, do_kousengan,       0,  ADMLVL_NONE,    0,      1},
        {"kuraiiro",      "kuraiir",      POS_FIGHTING, do_kura,            0,  ADMLVL_NONE,    0,      1},
        {"kyodaika",      "kyodaik",      POS_STANDING, do_kyodaika,        0,  ADMLVL_NONE,    0,      1},

        {"look",          "lo",           POS_RESTING,  do_look,            0,  ADMLVL_NONE,    SCMD_LOOK,      0},
        {"lag",           "la",           POS_RESTING,  do_lag,             0, 5,               0,      0},
        {"land",          "lan",          POS_RESTING,  do_land,            0,  ADMLVL_NONE,    0,      0},
        {"languages",     "lang",         POS_RESTING,  do_languages,       0,  ADMLVL_NONE,    0,      0},
        {"last",          "last",         POS_DEAD,     do_last,            0,  ADMLVL_GOD,     0,      0},
        {"learn",         "lear",         POS_RESTING,  do_not_here,        0,  ADMLVL_NONE,    0,      0},
        {"leave",         "lea",          POS_STANDING, do_leave,           0,  ADMLVL_NONE,    0,      0},
        {"levels",        "lev",          POS_DEAD,     do_levels,          0,  ADMLVL_NONE,    0,      0},
        {"light",         "ligh",         POS_STANDING, do_lightgrenade,    0,  ADMLVL_NONE,    0,      0},
        {"list",          "lis",          POS_STANDING, do_not_here,        0,  ADMLVL_NONE,    0,      0},
        {"life",          "lif",          POS_SLEEPING, do_lifeforce,       0,  ADMLVL_NONE,    0,      0},
        {"links",         "lin",          POS_DEAD,     do_oasis,           0,  ADMLVL_BUILDER, SCMD_OASIS_LINKS,      0},
        {"liquefy",       "liquef",       POS_SLEEPING, do_liquefy,         0,  ADMLVL_NONE,    0,      1},
        {"lkeep",         "lkee",         POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_LKEEP,      0},
        {"lock",          "loc",          POS_SITTING,  do_gen_door,        0,  ADMLVL_NONE,    SCMD_LOCK,      0},
        {"lockout",       "lock",         POS_STANDING, do_hell,            0,  ADMLVL_IMMORT,  0,      0},
        {"load",          "load",         POS_DEAD,     do_load,            0,  ADMLVL_IMMORT,  0,      0},

        {"majinize",      "majini",       POS_STANDING, do_majinize,        0,  ADMLVL_NONE,    0,      1},
        {"malice",        "malic",        POS_FIGHTING, do_malice,          0,  ADMLVL_NONE,    0,      1},
        {"masenko",       "masenk",       POS_FIGHTING, do_masenko,         0,  ADMLVL_NONE,    0,      1},
        {"motd",          "motd",         POS_DEAD,     do_gen_ps,          0,  ADMLVL_NONE,    SCMD_MOTD,      0},
        {"mail",          "mail",         POS_STANDING, do_not_here,        2,  ADMLVL_NONE,    0,      0},
        {"map",           "map",          POS_STANDING, do_map,             0,  ADMLVL_NONE,    0,      0},
        /*{ "mcopy"    , "mcopy"	, POS_DEAD    , do_oasis_copy, 0, ADMLVL_GOD	, SCMD_MEDIT },*/
        {"medit",         "medit",        POS_DEAD,     do_oasis,           0,  ADMLVL_IMMORT,  SCMD_OASIS_MEDIT,      0},
        {"meditate",      "medita",       POS_SITTING,  do_meditate,        0,  ADMLVL_NONE,    0,      1},
        {"mimic",         "mimi",         POS_STANDING, do_mimic,           0,  ADMLVL_NONE,    0,      0},
        {"mlist",         "mlist",        POS_DEAD,     do_oasis,           0,  ADMLVL_IMMORT,  SCMD_OASIS_MLIST,      0},
        {"moondust",      "moondus",      POS_STANDING, do_moondust,        0,  ADMLVL_NONE,    0,      1},
        {"multiform",     "multifor",     POS_STANDING, do_multiform,       0,  ADMLVL_NONE,    0,      1},
        {"mute",          "mute",         POS_DEAD,     do_wizutil,         0,  ADMLVL_IMMORT,  SCMD_SQUELCH,      0},
        {"music",         "musi",         POS_RESTING,  do_gen_comm,        1,  ADMLVL_NONE,    SCMD_HOLLER,      0},

        {"newbie",        "newbie",       POS_SLEEPING, do_gen_comm,        0,  ADMLVL_NONE,    SCMD_AUCTION,      0},
        {"news",          "news",         POS_SLEEPING, do_news,            0,  ADMLVL_NONE,    0,      0},
        {"newsedit",      "newsedi",      POS_SLEEPING, do_newsedit,        0,  ADMLVL_IMMORT,  0,      0},
        {"nickname",      "nicknam",      POS_RESTING,  do_nickname,        0,  ADMLVL_NONE,    0,      0},
        {"nocompress",    "nocompress",   POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_NOCOMPRESS,      0},
        {"noeq",          "noeq",         POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_NOEQSEE,      0},
        {"nolin",         "nolin",        POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_NODEC,      0},
        {"nomusic",       "nomusi",       POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_NOMUSIC,      0},
        {"noooc",         "noooc",        POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_NOGOSSIP,      0},
        {"nogive",        "nogiv",        POS_DEAD,     do_gen_tog,         0, 0,               SCMD_NOGIVE,      0},
        {"nograts",       "nograts",      POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_NOGRATZ,      0},
        {"nogrow",        "nogro",        POS_DEAD,     do_nogrow,          0,  ADMLVL_NONE,    0,      0},
        {"nohassle",      "nohassle",     POS_DEAD,     do_gen_tog,         0,  ADMLVL_IMMORT,  SCMD_NOHASSLE,      0},
        {"nomail",        "nomail",       POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_NMWARN,      0},
        {"nonewbie",      "nonewbie",     POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_NOAUCTION,      0},
        {"noparry",       "noparr",       POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_NOPARRY,      0},
        {"norepeat",      "norepeat",     POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_NOREPEAT,      0},
        {"noshout",       "noshout",      POS_SLEEPING, do_gen_tog,         1,  ADMLVL_NONE,    SCMD_DEAF,      0},
        {"nosummon",      "nosummon",     POS_DEAD,     do_gen_tog,         1,  ADMLVL_NONE,    SCMD_NOSUMMON,      0},
        {"notell",        "notell",       POS_DEAD,     do_gen_tog,         1,  ADMLVL_NONE,    SCMD_NOTELL,      0},
        {"notitle",       "notitle",      POS_DEAD,     do_wizutil,         0,  ADMLVL_GOD,     SCMD_NOTITLE,      0},
        {"nova",          "nov",          POS_STANDING, do_nova,            0,  ADMLVL_NONE,    0,      1},
        {"nowiz",         "nowiz",        POS_DEAD,     do_gen_tog,         0,  ADMLVL_IMMORT,  SCMD_NOWIZ,      0},
        {"oaffects",          "oaf",          POS_DEAD,     do_oaffects,            0,  ADMLVL_BUILDER, 0,      0},
        /*{ "ocopy"    , "ocopy"	, POS_DEAD    , do_oasis_copy, 0, ADMLVL_GOD	, SCMD_OEDIT },*/
        {"ooc",           "ooc",          POS_SLEEPING, do_gen_comm,        0,  ADMLVL_NONE,    SCMD_GOSSIP,      0},
        {"offer",         "off",          POS_STANDING, do_not_here,        1,  ADMLVL_NONE,    0,      0},
        {"open",          "ope",          POS_SITTING,  do_gen_door,        0,  ADMLVL_NONE,    SCMD_OPEN,      0},
        {"olc",           "olc",          POS_DEAD,     do_show_save_list,  0,  ADMLVL_IMMORT,  0,      0},
        {"olist",         "olist",        POS_DEAD,     do_oasis,           0,  ADMLVL_IMMORT,  SCMD_OASIS_OLIST,      0},
        {"oedit",         "oedit",        POS_DEAD,     do_oasis,           0,  ADMLVL_IMMORT,  SCMD_OASIS_OEDIT,      0},
        {"osay",          "osay",         POS_RESTING,  do_osay,            0,  ADMLVL_NONE,    0,      0},

        {"pack",          "pac",          POS_STANDING, do_pack,            0, 0,               0,      0},
        {"page",          "pag",          POS_DEAD,     do_page,            0,  ADMLVL_BUILDER, 0,      0},
        {"paralyze",      "paralyz",      POS_FIGHTING, do_paralyze,        0,  ADMLVL_NONE,    0,      0},
        {"peace",         "pea",          POS_DEAD,     do_peace,           0,  ADMLVL_BUILDER, 0,      0},
        {"perfect",       "perfec",       POS_DEAD,     do_perf,            0,  ADMLVL_NONE,    0,      0},
        {"permission",    "permiss",      POS_DEAD,     do_permission,      0,  ADMLVL_IMMORT,  0,      0},
        {"phoenix",       "phoeni",       POS_FIGHTING, do_pslash,          0,  ADMLVL_NONE,    0,      1},
        {"pick",          "pi",           POS_STANDING, do_gen_door,        1,  ADMLVL_NONE,    SCMD_PICK,      0},
        {"pickup",        "picku",        POS_RESTING,  do_not_here,        0, 0,               0,      0},
        {"pilot",         "pilot",        POS_SITTING,  do_drive,           0,  ADMLVL_NONE,    0,      0},
        {"plant",         "plan",         POS_STANDING, do_plant,           0,  ADMLVL_NONE,    0,      0},
        {"play",          "pla",          POS_SITTING,  do_play,            0,  ADMLVL_NONE,    0,      0},
        {"players",       "play",         POS_DEAD,     do_plist,           0,  ADMLVL_IMPL,    0,      0},
        {"poofin",        "poofi",        POS_DEAD,     do_poofset,         0,  ADMLVL_IMMORT,  SCMD_POOFIN,      0},
        {"poofout",       "poofo",        POS_DEAD,     do_poofset,         0,  ADMLVL_IMMORT,  SCMD_POOFOUT,      0},
        {"pose",          "pos",          POS_STANDING, do_pose,            0,  ADMLVL_NONE,    0,      0},
        {"post",          "pos",          POS_STANDING, do_post,            0,  ADMLVL_NONE,    0,      0},
        {"potential",     "poten",        POS_STANDING, do_potential,       0,  ADMLVL_NONE,    0,      1},
        {"pour",          "pour",         POS_STANDING, do_pour,            0,  ADMLVL_NONE,    SCMD_POUR,      0},
        {"powerup",       "poweru",       POS_FIGHTING, do_powerup,         0,  ADMLVL_NONE,    0,      1},
        {"preference",    "preferenc",    POS_DEAD,     do_preference,      0,  ADMLVL_NONE,    0,      0},
        {"program",       "progra",       POS_DEAD,     do_oasis,           0,  ADMLVL_NONE,    SCMD_OASIS_REDIT,      0},
        {"prompt",        "pro",          POS_DEAD,     do_display,         0,  ADMLVL_NONE,    0,      0},
        {"practice",      "pra",          POS_RESTING,  do_practice,        1,  ADMLVL_NONE,    0,      0},
        {"psychic",       "psychi",       POS_FIGHTING, do_psyblast,        0,  ADMLVL_NONE,    0,      1},
        {"punch",         "punc",         POS_FIGHTING, do_punch,           0,  ADMLVL_NONE,    0,      1},
        {"pushup",        "pushu",        POS_STANDING, do_pushup,          0,  ADMLVL_NONE,    0,      1},
        {"put",           "put",          POS_RESTING,  do_put,             0,  ADMLVL_NONE,    0,      0},
        {"pgrant",        "pgrant",       POS_DEAD,     do_pgrant,          0,  ADMLVL_IMMORT,  0,      0},
        {"rpreward",      "rpreward",     POS_DEAD,     do_rpreward,        0,  ADMLVL_IMMORT,  0,      0},
        {"eratime",       "eratime",      POS_DEAD,     do_eratime,         0,  ADMLVL_IMMORT,  0,      0},
        {"purge",         "purge",        POS_DEAD,     do_purge,           0,  ADMLVL_BUILDER, 0,      0},

        {"qui",           "qui",          POS_DEAD,     do_quit,            0,  ADMLVL_NONE,    0,      0},
        {"quit",          "quit",         POS_DEAD,     do_quit,            0,  ADMLVL_NONE,    SCMD_QUIT,      0},

        {"radar",         "rada",         POS_RESTING,  do_sradar,          0,  ADMLVL_NONE,    0,      0},
        {"raise",         "rai",          POS_DEAD,     do_raise,           0,  ADMLVL_NONE,    0,      0},
        {"refuel",        "refue",        POS_SITTING,  do_refuel,          0,  ADMLVL_NONE,    0,      0},
        {"resize",        "resiz",        POS_STANDING, do_resize,          0,  ADMLVL_NONE,    0,      0},
        {"rescue",        "rescu",        POS_STANDING, do_rescue,          0,  ADMLVL_NONE,    0,      0},
        {"rest",          "re",           POS_RESTING,  do_rest,            0,  ADMLVL_NONE,    0,      1},
        {"restring",      "restring",     POS_STANDING, do_restring,        0,  ADMLVL_NONE,    0,      0},
        {"rclone",        "rclon",        POS_DEAD,     do_rcopy,           0,  ADMLVL_BUILDER, 0,      0},
        {"rcopy",         "rcopy",        POS_DEAD,     do_rcopy,           0,  ADMLVL_BUILDER, 0,      0},
        {"roomdisplay",   "roomdisplay",  POS_RESTING,  do_rdisplay,        0,  ADMLVL_NONE,    0,      0},
        {"read",          "rea",          POS_RESTING,  do_look,            0,  ADMLVL_NONE,    SCMD_READ,      0},
        {"recall",        "reca",         POS_STANDING, do_recall,          0,  ADMLVL_IMMORT,  0,      0},
        {"recharge",      "rechar",       POS_STANDING, do_recharge,        0,  ADMLVL_NONE,    0,      0},
        {"regenerate",    "regen",        POS_RESTING,  do_regenerate,      0,  ADMLVL_NONE,    0,      1},
        {"renzokou",      "renzo",        POS_FIGHTING, do_renzo,           0,  ADMLVL_NONE,    0,      1},
        {"repair",        "repai",        POS_STANDING, do_srepair,         0,  ADMLVL_NONE,    0,      0},
        {"reply",         "rep",          POS_SLEEPING, do_reply,           0,  ADMLVL_NONE,    0,      0},
        {"reward",        "rewar",        POS_RESTING,  do_reward,          0,  ADMLVL_IMMORT,  0,      0},
        {"reload",        "reload",       POS_DEAD,     do_reboot,          0, 5,               0,      0},
        {"receive",       "rece",         POS_STANDING, do_not_here,        1,  ADMLVL_NONE,    0,      0},
        {"remove",        "rem",          POS_RESTING,  do_remove,          0,  ADMLVL_NONE,    0,      0},
        {"rent",          "rent",         POS_STANDING, do_not_here,        1,  ADMLVL_NONE,    0,      0},
        {"report",        "repor",        POS_DEAD,     do_gen_write,       0,  ADMLVL_NONE,    SCMD_IDEA,      0},
        {"reroll",        "rero",         POS_DEAD,     do_wizutil,         0,  ADMLVL_IMPL,    SCMD_REROLL,      0},
        {"respond",       "resp",         POS_RESTING,  do_respond,         1,  ADMLVL_NONE,    0,      0},
        {"restore",       "resto",        POS_DEAD,     do_restore,         0,  ADMLVL_GOD,     0,      0},
        {"return",        "retu",         POS_DEAD,     do_return,          0,  ADMLVL_NONE,    0,      0},
        {"redit",         "redit",        POS_DEAD,     do_oasis,           0,  ADMLVL_IMMORT,  SCMD_OASIS_REDIT,      0},
        {"rip",           "ri",           POS_DEAD,     do_rip,             0,  ADMLVL_NONE,    0,      1},
        {"rlist",         "rlist",        POS_DEAD,     do_oasis,           0,  ADMLVL_IMMORT,  SCMD_OASIS_RLIST,      0},
        {"rogafufuken",   "rogafu",       POS_FIGHTING, do_rogafufuken,     0,  ADMLVL_NONE,    0,      1},
        {"roomflags",     "roomf",        POS_DEAD,     do_gen_tog,         0,  ADMLVL_IMMORT,  SCMD_ROOMFLAGS,      0},
        {"roundhouse",    "roundhou",     POS_FIGHTING, do_roundhouse,      0,  ADMLVL_NONE,    0,      1},
        {"rpp",           "rpp",          POS_SLEEPING, do_rpp,             0,  ADMLVL_NONE,    0,      0},
        {"runic",         "runi",         POS_STANDING, do_runic,           0,  ADMLVL_NONE,    0,      0},

        {"say",           "say",          POS_RESTING,  do_say,             0,  ADMLVL_NONE,    0,      0},
        {"'",             "'",            POS_RESTING,  do_say,             0,  ADMLVL_NONE,    0,      0},
        {"save",          "sav",          POS_SLEEPING, do_save,            0,  ADMLVL_NONE,    0,      0},
        {"saveall",       "saveall",      POS_DEAD,     do_saveall,         0,  ADMLVL_BUILDER, 0,      0},
        {"sbc",           "sbc",          POS_FIGHTING, do_sbc,             0,  ADMLVL_NONE,    0,      0},
        {"scan",          "sca",          POS_FIGHTING, do_scan,            0,  ADMLVL_NONE,    0,      0},
        {"scatter",       "scatte",       POS_FIGHTING, do_scatter,         0,  ADMLVL_NONE,    0,      0},
        {"score",         "sc",           POS_DEAD,     do_score,           0,  ADMLVL_NONE,    0,      0},
        /*{ "scopy"    , "scopy"  	, POS_DEAD    , do_oasis_copy, 0, ADMLVL_GOD	, CON_SEDIT },*/
        {"scouter",       "scou",         POS_RESTING,  do_scouter,         0,  ADMLVL_NONE,    0,      0},
        {"scry",          "scr",          POS_STANDING, do_scry,            0,  ADMLVL_NONE,    0,      0},
        {"seishou",       "seisho",       POS_FIGHTING, do_seishou,         0,  ADMLVL_NONE,    0,      1},
        {"shell",         "she",          POS_STANDING, do_shell,           0,  ADMLVL_NONE,    0,      1},
        {"shimmer",       "shimme",       POS_STANDING, do_shimmer,         0,  ADMLVL_NONE,    0,      1},
        {"shogekiha",     "shog",         POS_STANDING, do_shogekiha,       0,  ADMLVL_NONE,    0,      1},
        {"shuffle",       "shuff",        POS_SITTING,  do_shuffle,         0,  ADMLVL_NONE,    0,      0},
        {"snet",          "snet",         POS_RESTING,  do_snet,            0,  ADMLVL_NONE,    0,      0},
        {"search",        "sea",          POS_STANDING, do_look,            0,  ADMLVL_NONE,    SCMD_SEARCH,      0},
        {"sell",          "sell",         POS_STANDING, do_not_here,        0,  ADMLVL_NONE,    0,      0},
        {"selfdestruct",  "selfdest",     POS_STANDING, do_selfd,           0,  ADMLVL_NONE,    0,      1},
        {"sedit",         "sedit",        POS_DEAD,     do_oasis,           0,  ADMLVL_IMMORT,  SCMD_OASIS_SEDIT,      0},
        {"send",          "send",         POS_SLEEPING, do_send,            0,  ADMLVL_GOD,     0,      0},
        {"sense",         "sense",        POS_RESTING,  do_track,           0,  ADMLVL_NONE,    0,      0},
        {"set",           "set",          POS_DEAD,     do_set,             0,  ADMLVL_IMMORT,  0,      0},
        {"shout",         "sho",          POS_RESTING,  do_gen_comm,        0,  ADMLVL_NONE,    SCMD_SHOUT,      0},
        {"show",          "show",         POS_DEAD,     do_showoff,         0,  ADMLVL_NONE,    0,      0},
        {"shutdow",       "shutdow",      POS_DEAD,     do_shutdown,        0,  ADMLVL_IMPL,    0,      0},
        {"shutdown",      "shutdown",     POS_DEAD,     do_shutdown,        0,  ADMLVL_IMPL,    SCMD_SHUTDOWN,      0},
        {"silk",          "sil",          POS_RESTING,  do_silk,            0,  ADMLVL_NONE,    0,      0},
        {"sip",           "sip",          POS_RESTING,  do_drink,           0,  ADMLVL_NONE,    SCMD_SIP,      0},
        {"sit",           "sit",          POS_RESTING,  do_sit,             0,  ADMLVL_NONE,    0,      1},
        {"situp",         "situp",        POS_STANDING, do_situp,           0,  ADMLVL_NONE,    0,      1},
        {"skills",        "skills",       POS_SLEEPING, do_skills,          0,  ADMLVL_NONE,    0,      0},
        {"skillset",      "skillset",     POS_SLEEPING, do_skillset,        0, 5,               0,      0},
        {"slam",          "sla",          POS_FIGHTING, do_slam,            0,  ADMLVL_NONE,    0,      1},
        {"sleep",         "sl",           POS_SLEEPING, do_sleep,           0,  ADMLVL_NONE,    0,      1},
        {"slist",         "slist",        POS_SLEEPING, do_oasis,           0,  ADMLVL_IMMORT,  SCMD_OASIS_SLIST,      0},
        {"slowns",        "slowns",       POS_DEAD,     do_gen_tog,         0,  ADMLVL_IMPL,    SCMD_SLOWNS,      0},
        {"smote",         "sm",           POS_RESTING,  do_echo,            1,  ADMLVL_NONE,    SCMD_SMOTE,      0},
        {"sneak",         "sneak",        POS_STANDING, do_gen_tog,         1,  ADMLVL_NONE,    SCMD_SNEAK,      1},
        {"snoop",         "snoop",        POS_DEAD,     do_snoop,           0,  ADMLVL_IMMORT,  0,      0},
        {"song",          "son",          POS_RESTING,  do_song,            0, 0,               0,      1},
        {"spiral",        "spiral",       POS_STANDING, do_spiral,          0,  ADMLVL_NONE,    0,      1},
        {"socials",       "socials",      POS_DEAD,     do_commands,        0,  ADMLVL_NONE,    SCMD_SOCIALS,      0},
        {"solarflare",    "solarflare",   POS_FIGHTING, do_solar,           0,  ADMLVL_NONE,    0,      1},
        {"spar",          "spa",          POS_FIGHTING, do_spar,            0,  ADMLVL_NONE,    0,      1},
        {"spit",          "spi",          POS_STANDING, do_spit,            0,  ADMLVL_NONE,    0,      0},
        {"spiritball",    "spiritball",   POS_FIGHTING, do_spiritball,      0,  ADMLVL_NONE,    0,      1},
        {"spiritcontrol", "spiritcontro", POS_RESTING,  do_spiritcontrol,   0,  ADMLVL_NONE,    0,      1},
        {"split",         "split",        POS_SITTING,  do_split,           1,  ADMLVL_IMMORT,  0,      1},
        {"speak",         "spe",          POS_RESTING,  do_languages,       0,  ADMLVL_NONE,    0,      0},
        {"spells",        "spel",         POS_RESTING,  do_spells,          0,  ADMLVL_IMMORT,  0,      0},
        {"chargen",       "chargen",      POS_RESTING,  do_gen,             0,  ADMLVL_NONE,    0,      0},
        {"stand",         "st",           POS_RESTING,  do_stand,           0,  ADMLVL_NONE,    0,      1},
        {"starbreaker",   "starbr",       POS_FIGHTING, do_breaker,         0,  ADMLVL_NONE,    0,      1},
        {"stake",         "stak",         POS_SLEEPING, do_beacon,          0, 0,               0,      1},
        {"stat",          "stat",         POS_DEAD,     do_stat,            0,  ADMLVL_IMMORT,  0,      0},
        {"status",        "statu",        POS_DEAD,     do_status,          0, 0,               0,      0},
        {"steal",         "ste",          POS_STANDING, do_steal,           1,  ADMLVL_NONE,    0,      1},
        {"stone",         "ston",         POS_STANDING, do_spit,            0,  ADMLVL_NONE,    0,      0},
        {"stop",          "sto",          POS_STANDING, do_stop,            0,  ADMLVL_NONE,    0,      0},
        {"study",         "stu",          POS_RESTING,  do_not_here,        0,  ADMLVL_NONE,    0,      0},
        {"summon",        "summo",        POS_STANDING, do_summon,          0,  ADMLVL_NONE,    0,      0},
        {"sunder",        "sunde",        POS_STANDING, do_sunder,          0,  ADMLVL_NONE,    0,      1},
        {"suppress",      "suppres",      POS_STANDING, do_suppress,        0,  ADMLVL_NONE,    0,      1},
        {"swallow",       "swall",        POS_RESTING,  do_use,             0,  ADMLVL_NONE,    SCMD_QUAFF,      1},
        {"switch",        "switch",       POS_DEAD,     do_switch,          0,  ADMLVL_VICE,    0,      0},
        {"syslog",        "syslog",       POS_DEAD,     do_syslog,          0,  ADMLVL_IMMORT,  0,      0},

        /*{ "tcopy"    , "tcopy"  	, POS_DEAD    , do_oasis_copy, 0, ADMLVL_GOD	, SCMD_TEDIT },*/
        {"tailhide",      "tailh",        POS_RESTING,  do_tailhide,        0,  ADMLVL_NONE,    0,      1},
        {"table",         "tabl",         POS_SITTING,  do_table,           0,  ADMLVL_NONE,    0,      0},
        {"teach",         "teac",         POS_STANDING, do_teach,           0,  ADMLVL_NONE,    0,      0},
        {"tell",          "tel",          POS_DEAD,     do_tell,            0,  ADMLVL_NONE,    0,      0},
        {"take",          "tak",          POS_RESTING,  do_get,             0,  ADMLVL_NONE,    0,      0},
        {"tailwhip",      "tailw",        POS_FIGHTING, do_tailwhip,        0,  ADMLVL_NONE,    0,      1},
        {"taisha",        "taish",        POS_FIGHTING, do_taisha,          0,  ADMLVL_NONE,    0,      1},
        {"taste",         "tas",          POS_RESTING,  do_eat,             0,  ADMLVL_NONE,    SCMD_TASTE,      0},
        {"teleport",      "tele",         POS_DEAD,     do_teleport,        0,  ADMLVL_IMMORT,  0,      0},
        {"telepathy",     "telepa",       POS_DEAD,     do_telepathy,       0,  ADMLVL_NONE,    0,      0},
        {"tedit",         "tedit",        POS_DEAD,     do_tedit,           0,  ADMLVL_GRGOD,   0,      0},
        {"test",          "test",         POS_DEAD,     do_gen_tog,         0,  ADMLVL_BUILDER, SCMD_TEST,      0},
        {"thaw",          "thaw",         POS_DEAD,     do_wizutil,         0,  ADMLVL_IMMORT,  SCMD_THAW,      0},
        {"think",         "thin",         POS_DEAD,     do_think,           0,  ADMLVL_NONE,    0,      0},
        {"throw",         "thro",         POS_FIGHTING, do_throw,           0,  ADMLVL_NONE,    0,      1},
        {"title",         "title",        POS_DEAD,     do_title,           0,  ADMLVL_NONE,    0,      0},
        {"time",          "time",         POS_DEAD,     do_time,            0,  ADMLVL_NONE,    0,      0},
        {"toggle",        "toggle",       POS_DEAD,     do_toggle,          0,  ADMLVL_NONE,    0,      0},
        {"toplist",       "toplis",       POS_DEAD,     do_toplist,         0,  ADMLVL_NONE,    0,      0},
        {"trackthru",     "trackthru",    POS_DEAD,     do_gen_tog,         0,  ADMLVL_IMPL,    SCMD_TRACK,      0},
        {"train",         "train",        POS_STANDING, do_train,           0,  ADMLVL_NONE,    0,      0},
        {"transfer",      "transfer",     POS_SLEEPING, do_trans,           0,  ADMLVL_IMMORT,  0,      0},
        {"transform",     "transform",    POS_FIGHTING, do_transform,       0,  ADMLVL_NONE,    0,      1},
        {"transo",        "trans",        POS_STANDING, do_transobj,        0, 5,               0,      0},
        {"tribeam",       "tribe",        POS_FIGHTING, do_tribeam,         0,  ADMLVL_NONE,    0,      1},
        {"trigedit",      "trigedit",     POS_DEAD,     do_oasis,           0,  ADMLVL_IMMORT,  SCMD_OASIS_TRIGEDIT,      0},
        {"trip",          "trip",         POS_FIGHTING, do_trip,            0,  ADMLVL_NONE,    0,      1},
        {"tsuihidan",     "tsuihida",     POS_FIGHTING, do_tsuihidan,       0,  ADMLVL_NONE,    0,      1},
        {"tunnel",        "tunne",        POS_DEAD,     do_dig,             0,  ADMLVL_IMMORT,  0,      0},
        {"twinslash",     "twins",        POS_FIGHTING, do_tslash,          0,  ADMLVL_NONE,    0,      1},
        {"twohand",       "twohand",      POS_DEAD,     do_twohand,         0,  ADMLVL_NONE,    0,      1},
        {"typo",          "typo",         POS_DEAD,     do_gen_write,       0,  ADMLVL_NONE,    SCMD_TYPO,      0},

        {"unlock",        "unlock",       POS_SITTING,  do_gen_door,        0,  ADMLVL_NONE,    SCMD_UNLOCK,      0},
        {"ungroup",       "ungroup",      POS_DEAD,     do_ungroup,         0,  ADMLVL_NONE,    0,      0},
        {"unban",         "unban",        POS_DEAD,     do_unban,           0,  ADMLVL_GRGOD,   0,      0},
        {"unaffect",      "unaffect",     POS_DEAD,     do_wizutil,         0,  ADMLVL_GOD,     SCMD_UNAFFECT,      0},
        {"uppercut",      "upperc",       POS_FIGHTING, do_uppercut,        0,  ADMLVL_NONE,    0,      1},
        {"upgrade",       "upgrad",       POS_RESTING,  do_upgrade,         0,  ADMLVL_NONE,    0,      0},
        {"uptime",        "uptime",       POS_DEAD,     do_date,            0,  ADMLVL_IMMORT,  SCMD_UPTIME,      0},
        {"use",           "use",          POS_SITTING,  do_use,             1,  ADMLVL_NONE,    SCMD_USE,      0},
        {"users",         "users",        POS_DEAD,     do_users,           0,  ADMLVL_IMMORT,  0,      0},

        {"value",         "val",          POS_STANDING, do_not_here,        0,  ADMLVL_NONE,    0,      0},
        {"varstat",       "varst",        POS_DEAD,     do_varstat,         0,  ADMLVL_IMMORT,  0,      0},
        {"version",       "ver",          POS_DEAD,     do_gen_ps,          0,  ADMLVL_NONE,    SCMD_VERSION,      0},
        {"vieworder",     "view",         POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_VIEWORDER,      0},
        {"visible",       "vis",          POS_RESTING,  do_visible,         1,  ADMLVL_NONE,    0,      0},
        {"vnum",          "vnum",         POS_DEAD,     do_vnum,            0,  ADMLVL_IMMORT,  0,      0},
        {"voice",         "voic",         POS_RESTING,  do_voice,           0,  ADMLVL_NONE,    0,      0},
        {"vstat",         "vstat",        POS_DEAD,     do_vstat,           0,  ADMLVL_IMMORT,  0,      0},

        {"wake",          "wa",           POS_SLEEPING, do_wake,            0,  ADMLVL_NONE,    0,      1},
        {"warppool",      "warppoo",      POS_STANDING, do_warppool,        0,  ADMLVL_NONE,    0,      0},
        {"waterrazor",    "waterraz",     POS_STANDING, do_razor,           0,  ADMLVL_NONE,    0,      0},
        {"waterspikes",   "waterspik",    POS_STANDING, do_spike,           0,  ADMLVL_NONE,    0,      0},
        {"wear",          "wea",          POS_RESTING,  do_wear,            0,  ADMLVL_NONE,    0,      0},
        {"weather",       "weather",      POS_RESTING,  do_weather,         0,  ADMLVL_NONE,    0,      0},
        {"who",           "who",          POS_DEAD,     do_who,             0,  ADMLVL_NONE,    0,      0},
        {"whoami",        "whoami",       POS_DEAD,     do_gen_ps,          0,  ADMLVL_NONE,    SCMD_WHOAMI,      0},
        {"whohide",       "whohide",      POS_DEAD,     do_gen_tog,         0,  ADMLVL_NONE,    SCMD_WHOHIDE,      0},
        {"whois",         "whois",        POS_DEAD,     do_whois,           0,  ADMLVL_NONE,    0,      0},
        {"where",         "where",        POS_RESTING,  do_where,           1,  ADMLVL_IMMORT,  0,      0},
        {"whisper",       "whisper",      POS_RESTING,  do_spec_comm,       0,  ADMLVL_NONE,    SCMD_WHISPER,      0},
        {"wield",         "wie",          POS_RESTING,  do_wield,           0,  ADMLVL_NONE,    0,      1},
        {"will",          "wil",          POS_RESTING,  do_willpower,       0,  ADMLVL_NONE,    0,      0},
        {"wimpy",         "wimpy",        POS_DEAD,     do_value,           0,  ADMLVL_NONE,    SCMD_WIMPY,      0},
        {"withdraw",      "withdraw",     POS_STANDING, do_not_here,        1,  ADMLVL_NONE,    0,      0},
        {"wire",          "wir",          POS_STANDING, do_not_here,        1,  ADMLVL_NONE,    0,      0},
        {"wiznet",        "wiz",          POS_DEAD,     do_wiznet,          0,  ADMLVL_IMMORT,  0,      0},
        {";",             ";",            POS_DEAD,     do_wiznet,          0,  ADMLVL_IMMORT,  0,      0},
        {"wizhelp",       "wizhelp",      POS_SLEEPING, do_commands,        0,  ADMLVL_IMMORT,  SCMD_WIZHELP,      0},
        {"wizlist",       "wizlist",      POS_DEAD,     do_gen_ps,          0,  ADMLVL_NONE,    SCMD_WIZLIST,      0},
        {"wizlock",       "wizlock",      POS_DEAD,     do_wizlock,         0,  ADMLVL_IMMORT,  0,      0},
        {"wizupdate",     "wizupdate",    POS_DEAD,     do_wizupdate,       0,  ADMLVL_IMPL,    0,      0},
        {"write",         "write",        POS_STANDING, do_write,           1,  ADMLVL_NONE,    0,      0},


        {"zanzoken",      "zanzo",        POS_FIGHTING, do_zanzoken,        0,  ADMLVL_NONE,    0,      1},
        {"zen",           "ze",           POS_FIGHTING, do_zen,             0,  ADMLVL_NONE,    0,      1},
        {"zcheck",        "zcheck",       POS_DEAD,     do_zcheck,          0,  ADMLVL_GOD,     0,      0},
        {"zreset",        "zreset",       POS_DEAD,     do_zreset,          0,  ADMLVL_IMMORT,  0,      0},
        {"zedit",         "zedit",        POS_DEAD,     do_oasis,           0,  ADMLVL_IMMORT,  SCMD_OASIS_ZEDIT,      0},
        {"zlist",         "zlist",        POS_DEAD,     do_oasis,           0,  ADMLVL_IMMORT,  SCMD_OASIS_ZLIST,      0},
        {"zpurge",        "zpurge",       POS_DEAD,     do_zpurge,          0,  ADMLVL_GRGOD,   0,      0},

        /* DG trigger commands */
        {"attach",        "attach",       POS_DEAD,     do_attach,          0,  ADMLVL_BUILDER, 0,      0},
        {"detach",        "detach",       POS_DEAD,     do_detach,          0,  ADMLVL_BUILDER, 0,      0},
        {"detect",        "detec",        POS_STANDING, do_radar,           0,  ADMLVL_NONE,    0,      0},
        {"tlist",         "tlist",        POS_DEAD,     do_oasis,           0,  ADMLVL_IMMORT,  SCMD_OASIS_TLIST,      0},
        {"tstat",         "tstat",        POS_DEAD,     do_tstat,           0,  ADMLVL_IMMORT,  0,      0},
        {"masound",       "masound",      POS_DEAD,     do_masound,         -1, ADMLVL_NONE,    0,      0},
        {"mheal",         "mhea",         POS_SITTING,  do_mheal,           -1, ADMLVL_NONE,    0,      0},
        {"mkill",         "mkill",        POS_STANDING, do_mkill,           -1, ADMLVL_NONE,    0,      0},
        {"mjunk",         "mjunk",        POS_SITTING,  do_mjunk,           -1, ADMLVL_NONE,    0,      0},
        {"mdamage",       "mdamage",      POS_DEAD,     do_mdamage,         -1, ADMLVL_NONE,    0,      0},
        {"mdoor",         "mdoor",        POS_DEAD,     do_mdoor,           -1, ADMLVL_NONE,    0,      0},
        {"mecho",         "mecho",        POS_DEAD,     do_mecho,           -1, ADMLVL_NONE,    0,      0},
        {"mechoaround",   "mechoaround",  POS_DEAD,     do_mechoaround,     -1, ADMLVL_NONE,    0,      0},
        {"msend",         "msend",        POS_DEAD,     do_msend,           -1, ADMLVL_NONE,    0,      0},
        {"mload",         "mload",        POS_DEAD,     do_mload,           -1, ADMLVL_NONE,    0,      0},
        {"mpurge",        "mpurge",       POS_DEAD,     do_mpurge,          -1, ADMLVL_NONE,    0,      0},
        {"mgoto",         "mgoto",        POS_DEAD,     do_mgoto,           -1, ADMLVL_NONE,    0,      0},
        {"mat",           "mat",          POS_DEAD,     do_mat,             -1, ADMLVL_NONE,    0,      0},
        {"mteleport",     "mteleport",    POS_DEAD,     do_mteleport,       -1, ADMLVL_NONE,    0,      0},
        {"mforce",        "mforce",       POS_DEAD,     do_mforce,          -1, ADMLVL_NONE,    0,      0},
        {"mremember",     "mremember",    POS_DEAD,     do_mremember,       -1, ADMLVL_NONE,    0,      0},
        {"mforget",       "mforget",      POS_DEAD,     do_mforget,         -1, ADMLVL_NONE,    0,      0},
        {"mtransform",    "mtransform",   POS_DEAD,     do_mtransform,      -1, ADMLVL_NONE,    0,      0},
        {"mzoneecho",     "mzoneecho",    POS_DEAD,     do_mzoneecho,       -1, ADMLVL_NONE,    0,      0},
        {"vdelete",       "vdelete",      POS_DEAD,     do_vdelete,         0,  ADMLVL_BUILDER, 0,      0},
        {"mfollow",       "mfollow",      POS_DEAD,     do_mfollow,         -1, ADMLVL_NONE,    0,      0},
        {"maddtransform", "maddtransform",POS_DEAD,     do_maddtransform,   -1, ADMLVL_NONE,    0,      0},

        {"\n",            "zzzzzzz", 0,                 nullptr,            0,  ADMLVL_NONE,    0,      0}};    /* this must be last */
