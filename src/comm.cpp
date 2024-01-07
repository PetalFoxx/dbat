/*************************************************************************
*   File: comm.c                                        Part of CircleMUD *
*  Usage: Communication, socket handling, main(), central game loop       *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */
#include "dbat/comm.h"
#include "dbat/utils.h"
#include "dbat/config.h"
#include "dbat/maputils.h"
#include "dbat/ban.h"
#include "dbat/weather.h"
#include "dbat/act.wizard.h"
#include "dbat/act.misc.h"
#include "dbat/house.h"
#include "dbat/act.other.h"
#include "dbat/dg_comm.h"
#include "dbat/handler.h"
#include "dbat/dg_scripts.h"
#include "dbat/act.item.h"
#include "dbat/interpreter.h"
#include "dbat/random.h"
#include "dbat/act.informative.h"
#include "dbat/dg_event.h"
#include "dbat/mobact.h"
#include "dbat/magic.h"
#include "dbat/objsave.h"
#include "dbat/genolc.h"
#include "dbat/class.h"
#include "dbat/combat.h"
#include "dbat/fight.h"
#include "dbat/local_limits.h"
#include "dbat/clan.h"
#include "dbat/mail.h"
#include "dbat/constants.h"
#include "dbat/screen.h"
#include <fstream>
#include "sodium.h"
#include <thread>
#include "dbat/players.h"
#include "dbat/account.h"
#include "dbat/charmenu.h"
#include "dbat/puppet.h"
#include <mutex>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <locale>

/* local globals */
struct descriptor_data *descriptor_list = nullptr;        /* master desc list */
std::map<int64_t, struct descriptor_data*> sessions;
struct txt_block *bufpool = nullptr;    /* pool of large output buffers */
int buf_largecount = 0;        /* # of large buffers which exist */
int buf_overflows = 0;        /* # of overflows of output */
int buf_switches = 0;        /* # of switches from small to large buf */
int circle_shutdown = 0;    /* clean shutdown */
int circle_reboot = 0;        /* reboot the game after a shutdown */
int no_specials = 0;        /* Suppress ass. of special routines */
int tics_passed = 0;        /* for extern checkpointing */
int scheck = 0;            /* for syntax checking mode */
struct timeval null_time;    /* zero-valued time structure */
int8_t reread_wizlist;        /* signal: SIGUSR1 */
int8_t emergency_unban;        /* signal: SIGUSR2 */
FILE *logfile = nullptr;        /* Where to send the log messages. */
int dg_act_check;               /* toggle for act_trigger */
uint64_t pulse = 0;        /* number of pulses since game start */
bool fCopyOver;          /* Are we booting in copyover mode? */
uint16_t port;
socklen_t mother_desc;
char *last_act_message = nullptr;

/***********************************************************************
*  main game loop and related stuff                                    *
***********************************************************************/

void broadcast(const std::string& txt) {
    basic_mud_log("Broadcasting: %s", txt.c_str());
    for(auto &[cid, c] : net::connections) {
        c->sendText(txt);
    }
}

boost::asio::awaitable<void> signal_watcher() {
    while (!circle_shutdown) {
        try {
            // Wait for a signal to be received
            int signal_number = co_await net::signals->async_wait(boost::asio::use_awaitable);

            // Process the signal
            switch(signal_number) {
                case SIGUSR1:
                    circle_shutdown = 1;
                    circle_reboot = 1;
                    break;
                case SIGUSR2:
                    circle_shutdown = 1;
                    circle_reboot = 2;
                    break;
                default:
                    basic_mud_log("Unexpected signal: %d", signal_number);
            }

        } catch(const std::exception& e) {
            std::cerr << "Error in signal watcher: " << e.what() << '\n';
        }
    }
}


void copyover_recover_final() {
    struct descriptor_data *next_d;
    for(auto d = descriptor_list; d; d = next_d) {
        next_d = d->next;
        if(STATE(d) != CON_COPYOVER) continue;

		auto accID = d->obj_editval;
        auto playerID = d->id;
        room_vnum room = d->obj_type;

        d->obj_editflag = 0;
        d->obj_type = 0;

        auto accFind = accounts.find(accID);

        if(accFind == accounts.end()) {
            basic_mud_log("recoverConnection: user %d not found.", accID);
            close_socket(d);
            continue;
        }
        d->account = &accFind->second;
        d->account->descriptors.insert(d);
        for(auto &[cid, c] : d->conns) c->account = d->account;

        auto playFind = players.find(playerID);
        if(playFind == players.end()) {
            basic_mud_log("recoverConnection: character %d not found.", playerID);
            close_socket(d);
            continue;
        }
        auto c = d->character = playFind->second.character;
        c->desc = d;

		GET_LOADROOM(c) = room;
        REMOVE_BIT_AR(PLR_FLAGS(c), PLR_WRITING);
        REMOVE_BIT_AR(PLR_FLAGS(c), PLR_MAILING);
        REMOVE_BIT_AR(PLR_FLAGS(c), PLR_CRYO);
        auto conns = d->conns;
        for(auto &[cid, con] : conns) {
            d->account->connections.insert(con.get());
            con->setParser(new net::PuppetParser(con, c));
        }

        write_to_output(d, "@rThe world comes back into focus... has something changed?@n\r\n");

        if (AFF_FLAGGED(d->character, AFF_HAYASA)) {
            GET_SPEEDBOOST(d->character) = GET_SPEEDCALC(d->character) * 0.5;
        }
    }
}

boost::asio::awaitable<void> yield_for(std::chrono::milliseconds ms) {
    boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor, ms);
    timer.expires_after(ms);
    co_await timer.async_wait(boost::asio::use_awaitable);
    co_return;
}

/* Reload players after a copyover */
void copyover_recover() {
    basic_mud_log("Copyover recovery initiated");
    std::ifstream fp(COPYOVER_FILE);

    if(!fp.is_open()) {
        basic_mud_log("Copyover file not found. Exiting.\r\n");
        shutdown_game(1);
    }

    nlohmann::json j;
    fp >> j;
    fp.close();

    // erase the file.
    std::filesystem::remove(COPYOVER_FILE);

    if(j.contains("descriptors")) {
        for(const auto &jd : j["descriptors"]) {
            auto d = new descriptor_data();
            d->raw_input_queue = std::make_unique<net::Channel<std::string>>(*net::io, 200);
            d->obj_editval = jd["user"];
            d->id = jd["character"];
            d->connected = CON_COPYOVER;
            d->obj_type = NOWHERE;
            if(jd.contains("in_room")) d->obj_type = jd["in_room"].get<room_vnum>();
            if(jd.contains("connections")) {
                {
                    std::lock_guard<std::mutex> guard(net::connectionsMutex);
                    for(const auto& jc : jd["connections"]) {
                        auto c = std::make_shared<net::Connection>(jc.get<int64_t>());
                        c->desc = d;
                        d->conns[c->connId] = c;
                        net::connections[c->connId] = c;
                    }
                }
            }
            sessions[d->id] = d;

            d->next = descriptor_list;
            descriptor_list = d;
        }
    }
}


static boost::asio::awaitable<void> performReboot(int mode) {
    char buf[100], buf2[100];

    std::ofstream fp(COPYOVER_FILE);

    if (!fp.is_open()) {
        send_to_imm("Copyover file not writeable, aborted.\r\n");
        circle_reboot = 0;
        co_return;
    }

    broadcast("\t\x1B[1;31m \007\007\007The universe stops for a moment as space and time fold.\x1B[0;0m\r\n");
    save_mud_time(&time_info);

    nlohmann::json j;

    /* For each playing descriptor, save its state */
    for (auto &[cid, conn] : net::connections) {
        if(conn->desc) continue;
        conn->sendText("\r\nSorry, we are rebooting. Please wait warmly for a few seconds.\r\n");
    }

    // wait 200 milliseconds... that should be enough time to push out all of the data.
    auto hb = std::chrono::milliseconds(200);
    boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor, hb);
    co_await timer.async_wait(boost::asio::use_awaitable);

    /* For each descriptor/connection, halt them and save state. */
    for (auto &[cid, d] : sessions) {
        nlohmann::json jd;
        if(d->conns.empty()) continue;

        for(auto &[cid, c] : d->conns) {
            jd["connections"].push_back(c->connId);
        }
        auto och = d->character;

        jd["user"] = d->account->vn;
        jd["character"] = och->id;

        auto r = IN_ROOM(och);
        auto w = GET_WAS_IN(och);
        if(r > 1) {
            jd["in_room"] = r;
        } else if(r <= 1 && w > 1) {
        	jd["in_room"] = w;
        }

        j["descriptors"].push_back(jd);
    }

    fp << j.dump(4, ' ', false, nlohmann::json::error_handler_t::ignore) << std::endl;
    fp.close();

	co_return;

}

static std::vector<std::pair<std::string, double>> timings;

struct GameSystem {
    // In seconds.
    GameSystem(std::string name, double interval, std::function<void(uint64_t, double)> func) : name(std::move(name)), interval(interval), func(std::move(func)) {
        countdown = interval;
    }
    std::string name;
    double interval{0.0};
    std::function<void(uint64_t, double)> func;
    double countdown{0.0};
};

static void saveMudTimeWrapper(uint64_t heartBeat, double deltaTime) {
    save_mud_time(&time_info);
}

static void deathTrapWrapper(uint64_t heartBeat, double deltaTime) {
    timed_dt(nullptr);
}

static std::vector<GameSystem> gameSystems = {
        GameSystem("event_process", 0.0, event_process),
        GameSystem("script_trigger_check", 13.0, script_trigger_check),
        GameSystem("zone_update", 10.0, zone_update),
        GameSystem("dball_load", 1.0, dball_load),
        GameSystem("base_update", 2.0, base_update),
        GameSystem("fish_update", 2.0, fish_update),
        GameSystem("handle_songs", 15.0, handle_songs),
        GameSystem("wishSYS", 1.0, wishSYS),
        GameSystem("mobile_activity", 10.0, mobile_activity),
        GameSystem("check_auction", 15.0, check_auction),
        GameSystem("fight_stack", 15.0, fight_stack),
        GameSystem("homing_update", 2.0, homing_update),
        GameSystem("huge_update", 2.0, huge_update),
        GameSystem("broken_update", 2.0, broken_update),
        GameSystem("copyover_check", 1.0, copyover_check),
        GameSystem("affect_update_violence", 5.0, affect_update_violence),
        GameSystem("advanceClock", 0.0, advanceClock),
        GameSystem("check_time_triggers", 300.0, check_time_triggers),
        GameSystem("affect_update", 300.0, affect_update),
        GameSystem("point_update", 100.0, point_update),
        GameSystem("clan_update", 60.0, clan_update),
        GameSystem("Crash_save_all", 60.0, Crash_save_all),
        GameSystem("House_save_all", 60.0, House_save_all),
        GameSystem("record_usage", 5.0, record_usage),
        GameSystem("save_mud_time", 30.0, saveMudTimeWrapper),
        GameSystem("timed_dt", 30.0, deathTrapWrapper),
        GameSystem("extract_pending_chars", 0.0, extract_pending_chars),
};

boost::asio::awaitable<void> heartbeat(uint64_t heart_pulse, double deltaTime) {
    static int mins_since_crashsave = 0;
    timings.clear();

    for(auto &s : gameSystems) {
        s.countdown -= deltaTime;
        if(s.countdown <= 0.0) {
            auto start = std::chrono::high_resolution_clock::now();
            s.func(heart_pulse, deltaTime);
            auto end = std::chrono::high_resolution_clock::now();
            timings.emplace_back(s.name, std::chrono::duration<double>(end - start).count());
            s.countdown += s.interval;
        }
    }
    co_return;
}

boost::asio::awaitable<void> processConnections(double deltaTime) {
    // First, handle any disconnected connections.
    for(auto &[id, reason] : net::deadConnections) {
        auto it = net::connections.find(id);
        // This shouldn't happen, but whatever.
        if(it == net::connections.end()) continue;
        it->second->cleanup(reason);
    }
    for(auto &[id, reason] : net::deadConnections) {
        net::connections.erase(id);
    }
    net::deadConnections.clear();

    // Second, welcome any new connections!
    auto pending = net::pendingConnections;
    for(const auto& id : pending) {
        auto it = net::connections.find(id);
        if (it != net::connections.end()) {
            auto conn = it->second;
            // Need a proper welcoming later....
            conn->onWelcome();
            net::pendingConnections.erase(id);
        }
    }

    // Next, we must handle the heartbeat routine for each connection.
    for(auto& [id, c] : net::connections) {
        c->onHeartbeat(deltaTime);
    }

    co_return;
}

boost::asio::awaitable<void> runOneLoop(double deltaTime) {
    static bool sleeping = false;
    struct descriptor_data* next_d;

    co_await processConnections(deltaTime);

    if(sleeping && descriptor_list) {
        basic_mud_log("Waking up.");
        sleeping = false;
    }

    if(!descriptor_list) {
        if(!sleeping) {
            basic_mud_log("No connections.  Going to sleep.");
            sleeping = true;
        }
        co_return;
    }

    {
        std::set<struct descriptor_data*> toLook;
        auto start = std::chrono::high_resolution_clock::now();
        for(auto d = descriptor_list; d; d = next_d) {
            next_d = d->next;
            switch(STATE(d)) {
                case CON_LOGIN:
                    d->character->login();
                    break;
                case CON_COPYOVER:
                    enter_player_game(d);
                    d->connected = CON_PLAYING;
                    toLook.insert(d);
                    break;
                default:
                    break;
            }
        }
        for(auto d : toLook) look_at_room(IN_ROOM(d->character), d->character, 0);
        auto end = std::chrono::high_resolution_clock::now();
        timings.emplace_back("handle logins", std::chrono::duration<double>(end - start).count());
    }


    /* Process commands we just read from process_input */
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (auto d = descriptor_list; d; d = next_d) {
            next_d = d->next;
            if(d->character) d->character->time.played += deltaTime;
            d->handle_input();
        }
        auto end = std::chrono::high_resolution_clock::now();
        timings.emplace_back("handle input", std::chrono::duration<double>(end - start).count());
    }

    bool gameActive = false;
    // TODO: replace this with a smarter check...
    // to determine if the game is active, we need to check if there are any players in the game.
    // this will be the case if any descriptor has an attached character who's in a valid room.
    for(auto d = descriptor_list; d; d = next_d) {
        next_d = d->next;
        if(d->character && IN_ROOM(d->character) != NOWHERE) {
            gameActive = true;
            break;
        }
    }

    if(gameActive) {
        auto start = std::chrono::high_resolution_clock::now();
        co_await heartbeat(++pulse, deltaTime);
        auto end = std::chrono::high_resolution_clock::now();
        timings.emplace_back("heartbeat total", std::chrono::duration<double>(end - start).count());
    }

    /* Send queued output out to the operating system (ultimately to user). */
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (auto d = descriptor_list; d; d = next_d) {
            next_d = d->next;
            if (!d->output.empty()) {
                process_output(d);
                d->has_prompt = true;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        timings.emplace_back("process output", std::chrono::duration<double>(end - start).count());
    }

    /* Print prompts for other descriptors who had no other output */
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (auto d = descriptor_list; d; d = d->next) {
            if (!d->has_prompt) {
                write_to_output(d, "@n");
                process_output(d);
                d->has_prompt = true;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        timings.emplace_back("print prompts", std::chrono::duration<double>(end - start).count());
    }

    /* Kick out folks in the CON_CLOSE or CON_DISCONNECT state */
    {
        auto start = std::chrono::high_resolution_clock::now();
        for (auto d = descriptor_list; d; d = next_d) {
            next_d = d->next;
            if(d->conns.empty()) {
                d->timeoutCounter += deltaTime;
                if(d->timeoutCounter > 300) {
                    STATE(d) = CON_CLOSE;
                }
            }
            if (STATE(d) == CON_CLOSE || STATE(d) == CON_DISCONNECT || STATE(d) == CON_QUITGAME)
                close_socket(d);
        }
        auto end = std::chrono::high_resolution_clock::now();
        timings.emplace_back("close sockets", std::chrono::duration<double>(end - start).count());
    }

    /* Check for any signals we may have received. */
    if (reread_wizlist) {
        reread_wizlist = false;
        mudlog(CMP, ADMLVL_IMMORT, true, "Signal received - rereading wizlists.");
        reboot_wizlists();
    }
    if (emergency_unban) {
        emergency_unban = false;
        mudlog(BRF, ADMLVL_IMMORT, true, "Received SIGUSR2 - completely unrestricting game (emergent)");
        ban_list = nullptr;
        circle_restrict = 0;
    }

    tics_passed++;
	co_return;
}


/*
 * game_loop contains the main loop which drives the entire MUD.  It
 * cycles once every 0.10 seconds and is responsible for accepting new
 * new connections, polling existing connections for input, dequeueing
 * output and sending it out to players, and calling "heartbeat" functions
 * such as mobile_activity().
 */
boost::asio::awaitable<void> game_loop() {
    broadcast("The world seems to shimmer and waver as it comes into focus.\r\n");
    for (auto &[vn, z] : zone_table) {
        basic_mud_log("Resetting #%d: %s (rooms %d-%d).", vn,
            z.name, z.bot, z.top);
        reset_zone(vn);
    }

    boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor, config::heartbeatInterval);
    double saveTimer = 60.0 * 5.0;
    double deltaTimeInSeconds = 0.1;
    gameIsLoading = false;

    /* The Main Loop.  The Big Cheese.  The Top Dog.  The Head Honcho.  The.. */
    while (!circle_shutdown) {

        auto loopStart = boost::asio::steady_timer::clock_type::now();
        try {
            SQLite::Transaction transaction(*assetDb);
            co_await runOneLoop(deltaTimeInSeconds);
            if(circle_shutdown) saveAll = true;
            if(saveAll) {
                dirty_all();
            }
            {
                auto start = boost::asio::steady_timer::clock_type::now();
                process_dirty();
                auto end = boost::asio::steady_timer::clock_type::now();
                timings.emplace_back("process_dirty", std::chrono::duration<double>(end - start).count());
            }

            {
                auto start = boost::asio::steady_timer::clock_type::now();
                transaction.commit();
                auto end = boost::asio::steady_timer::clock_type::now();
                timings.emplace_back("transaction.commit", std::chrono::duration<double>(end - start).count());
            }

            saveTimer -= deltaTimeInSeconds;
            if(saveTimer <= 0 || saveAll) {
                saveTimer = 60.0 * 5.0;
                dump_state();
            }
            if(saveAll) saveAll = false;

        } catch(std::exception& e) {
            basic_mud_log("Exception in runOneLoop(): %s", e.what());
            shutdown_game(1);
        } catch(...) {
             basic_mud_log("Unknown exception in runOneLoop()");
            shutdown_game(1);
        }
        auto loopEnd = boost::asio::steady_timer::clock_type::now();

        auto loopDuration = loopEnd - loopStart;
        auto nextWait = config::heartbeatInterval - loopDuration;

        // If heartbeat takes more than 100ms, default to a very short wait
        if(nextWait.count() < 0) {
            if(config::logEgregiousTimings) {
                logger->warn("Heartbeat took {} too long, defaulting to short wait", abs(std::chrono::duration<double>(nextWait).count()));
                for(auto &t : timings) {
                    logger->warn("Timing {}: {}", t.first, std::chrono::duration<double>(t.second).count());
                }
            }
            timings.clear();
            nextWait = std::chrono::milliseconds(1);
        }

        timer.expires_from_now(nextWait);
        co_await timer.async_wait(boost::asio::use_awaitable);
        deltaTimeInSeconds = std::chrono::duration<double>(boost::asio::steady_timer::clock_type::now() - loopStart).count();
    }

    if(circle_reboot > 0) {
        // circle_reboot at 1 is copyover, 2 is a full reboot.
        co_await performReboot(circle_reboot);
    }
	net::io->stop();
    co_return;
}

std::function<boost::asio::awaitable<void>()> gameFunc;

static boost::asio::awaitable<void> runGame() {
	// instantiate db with a shared_ptr, the filename is dbat.sqlite3
    try {
        assetDb = std::make_shared<SQLite::Database>(fmt::format("{}.sqlite3", config::assetDbName), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
    } catch (std::exception &e) {
        basic_mud_log("Exception Opening Asset Database: %s", e.what());
        shutdown_game(1);
    }
    create_schema();

    circle_srandom(time(nullptr));

    if(fCopyOver) {
        copyover_recover();
    }
    try {
        co_await boot_db();
    } catch(std::exception& e) {
        basic_mud_log("Exception in boot_db(): %s", e.what());
        exit(1);
    }


    {
        broadcast("Loading Space Map. ");
        FILE *mapfile = fopen("../lib/surface.map", "r");
        int rowcounter, colcounter;
        int vnum_read;
        for (rowcounter = 0; rowcounter <= MAP_ROWS; rowcounter++) {
            for (colcounter = 0; colcounter <= MAP_COLS; colcounter++) {
                fscanf(mapfile, "%d", &vnum_read);
                mapnums[rowcounter][colcounter] = real_room(vnum_read);
            }
        }
        fclose(mapfile);
    }

    /* Load the toplist */
    topLoad();

    /* If we made it this far, we will be able to restart without problem. */
    remove(KILLSCRIPT_FILE);

    // bring anyone who's in the middle of a copyover back into the game.
    if(fCopyOver) {
        copyover_recover_final();
        logger->info("Copyover recover complete.");
    }

    // Finally, let's get the game cracking.
    try {
        if(gameFunc) co_await gameFunc();
        else co_await game_loop();
    } catch(std::exception& e) {
        logger->critical("Exception in game_loop(): %s", e.what());
        shutdown_game(1);
    } catch(...) {
        logger->critical("Unknown exception in game_loop()");
        shutdown_game(1);
    }

    co_return;
}

/* Init sockets, run game, and cleanup sockets */
void init_game() {
    /* We don't want to restart if we crash before we get up. */
    touch(KILLSCRIPT_FILE);

    std::locale::global(std::locale("en_US.UTF-8"));

    if (sodium_init() != 0) {
        basic_mud_log("Could not initialize libsodium!");
        shutdown_game(EXIT_FAILURE);
    }

    logger->info("Setting up executor...");
    if(!net::io) net::io = std::make_unique<boost::asio::io_context>();
    if(!net::linkChannel) net::linkChannel = std::make_unique<net::JsonChannel>(*net::io, 200);

    // Next, we need to create the config::thermiteEndpoint from config::thermiteAddress and config::thermitePort
    logger->info("Setting up thermite endpoint...");
    try {
        net::thermiteEndpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(config::thermiteAddress), config::thermitePort);
    } catch (const boost::system::system_error& ex) {
        logger->critical("Failed to create thermite endpoint: {}", ex.what());
        shutdown_game(EXIT_FAILURE);
    } catch(...) {
        logger->critical("Failed to create thermite endpoint: Unknown exception");
        shutdown_game(EXIT_FAILURE);
    }

    if(!gameFunc) {
        basic_mud_log("Signal trapping.");
        net::signals = std::make_unique<boost::asio::signal_set>(*net::io, SIGUSR1, SIGUSR2);

        boost::asio::co_spawn(boost::asio::make_strand(*net::io), signal_watcher(), boost::asio::detached);

        boost::asio::co_spawn(boost::asio::make_strand(*net::io), net::runLinkManager(), boost::asio::detached);
    }

    boost::asio::co_spawn(boost::asio::make_strand(*net::io), runGame(), boost::asio::detached);

    // Run the io_context
    logger->info("Entering main loop...");
    // This part is a little tricky. if config::enableMultithreading is true, want to
    // run the io_context on multiple threads. Otherwise, we want to run it on a single thread.
    // Additionally, if it's true, we need to check config::threadsCount to see how many threads
    // to run on. If it's <1, we want to run on the number of cores on the machine.
    // The current thread should, of course, be considered one of those threads, so we subtract 1.
    // The best way to do this is to simply create a vector of threads, decide how many it should contain,
    // and start them. Then, we run the io_context on the current thread.

    unsigned int threadCount = 0;
    if(config::enableMultithreading) {
        logger->info("Multithreading is enabled.");
        if(config::threadsCount < 1) {
            threadCount = std::thread::hardware_concurrency() - 1;
            logger->info("Using {} threads. (Automatic detection)", threadCount+1);
        } else {
            threadCount = config::threadsCount - 1;
            logger->info("Using {} threads. (Manual override)", threadCount+1);
        }
    } else {
        threadCount = 0;
    }

    std::vector<std::thread> threads;
    if(threadCount) {
        logger->info("Starting {} helper threads...", threadCount);
        config::usingMultithreading = true;
    }
    for (int i = 0; i < threadCount; ++i) {
        threads.emplace_back([&]() {
            net::io->run();
        });
    }
    logger->info("Main thread entering executor...");
    try {
        net::io->run();
    }
    catch (const std::exception& e) {
        logger->critical("Exception in main thread: {}", e.what());
        shutdown_game(EXIT_FAILURE);
    }
    catch (...) {
        logger->critical("Unknown exception in main thread.");
        shutdown_game(EXIT_FAILURE);
    }

    logger->info("Executor has shut down. Running cleanup.");

    if(threadCount) {
        // Join all threads.
        // This should happen without any incident, since the only thing they're doing is
        // the io_executor which has been stopped.
        logger->info("Joining threads...");
        for (auto &thread: threads) {
            thread.join();
        }
        logger->info("All threads joined.");
        threads.clear();
    }

    // Release the executor and acceptor.
    // ASIO maintains its own socket polling fd and killing the executor is the
    // only way to release it.

    net::linkChannel.reset();
    net::signals.reset();
    net::link.reset();

    net::io.reset();

    basic_mud_log("Saving current MUD time.");
    save_mud_time(&time_info);

    if (circle_reboot) {
        basic_mud_log("Rebooting.");
        //shutdown_game(52);            /* what's so great about HHGTTG, anyhow? */
        std::filesystem::current_path("..");
        execl ("bin/circle", "circle", "-C%d", "1280", (char *) NULL);
    }
    basic_mud_log("Normal termination of game.");
    shutdown_game(0);
}

/* ******************************************************************
*  general utility stuff (for local use)                            *
****************************************************************** */

/*
 *  new code to calculate time differences, which works on systems
 *  for which tv_usec is unsigned (and thus comparisons for something
 *  being < 0 fail).  Based on code submitted by ss@sirocco.cup.hp.com.
 */



void record_usage(uint64_t heartPulse, double deltaTime) {

}




char *make_prompt(struct descriptor_data *d) {
    static char prompt[MAX_PROMPT_LENGTH];
    struct obj_data *chair = nullptr;
    int flagged = false;

    /* Note, prompt is truncated at MAX_PROMPT_LENGTH chars (structs.h) */

    if (d->str) {
        if (STATE(d) == CON_EXDESC) {
            strcpy(prompt, "Enter Description(/h for editor help)> ");
        } else if (PLR_FLAGGED(d->character, PLR_WRITING) && !PLR_FLAGGED(d->character, PLR_MAILING)) {
            strcpy(prompt, "Enter Message(/h for editor help)> ");
        } else if (PLR_FLAGGED(d->character, PLR_MAILING)) {
            strcpy(prompt, "Enter Mail Message(/h for editor help)> ");
        } else {
            strcpy(prompt, "Enter Message> ");
        }
    } else if (STATE(d) == CON_PLAYING && !IS_NPC(d->character)) {
        int count;
        size_t len = 0;

        *prompt = '\0';

        if (GET_INVIS_LEV(d->character) && len < sizeof(prompt)) {
            count = snprintf(prompt + len, sizeof(prompt) - len, "i%d ", GET_INVIS_LEV(d->character));
            if (count >= 0)
                len += count;
        }
        /* show only when below 25% */
        if (PRF_FLAGGED(d->character, PRF_DISPAUTO) && GET_LEVEL(d->character) >= 500 && len < sizeof(prompt)) {
            struct char_data *ch = d->character;
            if (GET_HIT(ch) << 2 < GET_MAX_HIT(ch)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "PL: %" I64T " ", GET_HIT(ch));
                if (count >= 0)
                    len += count;
            }
            if ((ch->getCurST()) << 2 < GET_MAX_MOVE(ch) && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "STA: %" I64T " ", (ch->getCurST()));
                if (count >= 0)
                    len += count;
            }
            if (GET_KI(ch) << 2 < GET_MAX_MANA(ch) && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "KI: %" I64T " ", GET_KI(ch));
                if (count >= 0)
                    len += count;
            }
        } else { /* not auto prompt */
            if (len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "@w");
                if (count >= 0)
                    len += count;
            }
            if (PLR_FLAGGED(d->character, PLR_SELFD) && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@RSELF-D@r: @w%s@D]@n",
                                 PLR_FLAGGED(d->character, PLR_SELFD2) ? "READY" : "PREP");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (IS_HALFBREED(d->character) && !PLR_FLAGGED(d->character, PLR_FURY) &&
                PRF_FLAGGED(d->character, PRF_FURY)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@mFury@W: @r%d@D]@w", GET_FURY(d->character));
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (IS_HALFBREED(d->character) && PLR_FLAGGED(d->character, PLR_FURY) &&
                PRF_FLAGGED(d->character, PRF_FURY)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@mFury@W: @rENGAGED@D]@w");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (has_mail(GET_IDNUM(d->character)) && !PRF_FLAGGED(d->character, PRF_NMWARN) &&
                (GET_ADMLEVEL(d->character) > 0) && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "CHECK MAIL - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (GET_KAIOKEN(d->character) > 0 && GET_ADMLEVEL(d->character) > 0) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "KAIOKEN X%d - ", GET_KAIOKEN(d->character));
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (GET_SONG(d->character) > 0) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "%s - ", song_types[GET_SONG(d->character)]);
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (GET_KAIOKEN(d->character) > 0 && GET_ADMLEVEL(d->character) <= 0) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "KAIOKEN X%d - ", GET_KAIOKEN(d->character));
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (has_mail(GET_IDNUM(d->character)) && (GET_ADMLEVEL(d->character) <= 0) &&
                !PRF_FLAGGED(d->character, PRF_NMWARN) && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "CHECK MAIL - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (d->snooping && d->snooping->character != nullptr && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "Snooping: (%s) - ",
                                 GET_NAME(d->snooping->character));
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (DRAGGING(d->character) && DRAGGING(d->character) != nullptr && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "Dragging: (%s) - ",
                                 GET_NAME(DRAGGING(d->character)));
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (PRF_FLAGGED(d->character, PRF_BUILDWALK) && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "BUILDWALKING - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (AFF_FLAGGED(d->character, AFF_FLYING) && len < sizeof(prompt) &&
                !PRF_FLAGGED(d->character, PRF_NODEC)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "FLYING - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (AFF_FLAGGED(d->character, AFF_HIDE) && len < sizeof(prompt) && !PRF_FLAGGED(d->character, PRF_NODEC)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "HIDING - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (PLR_FLAGGED(d->character, PLR_SPAR) && len < sizeof(prompt) && !PRF_FLAGGED(d->character, PRF_NODEC)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "SPARRING - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (PLR_FLAGGED(d->character, PLR_NOSHOUT) && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "MUTED - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (COMBO(d->character) == 51 && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "Combo (Bash) - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (COMBO(d->character) == 52 && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "Combo (Headbutt) - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (COMBO(d->character) == 56 && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "Combo (Tailwhip) - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (COMBO(d->character) == 0 && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "Combo (Punch) - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (COMBO(d->character) == 1 && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "Combo (Kick) - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (COMBO(d->character) == 2 && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "Combo (Elbow) - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (COMBO(d->character) == 3 && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "Combo (Knee) - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (COMBO(d->character) == 4 && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "Combo (Roundhouse) - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (COMBO(d->character) == 5 && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "Combo (Uppercut) - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (COMBO(d->character) == 6 && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "Combo (Slam) - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (COMBO(d->character) == 8 && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "Combo (Heeldrop) - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (PRF_FLAGGED(d->character, PRF_AFK) && len < sizeof(prompt) && !PRF_FLAGGED(d->character, PRF_NODEC)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "AFK - ");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (PLR_FLAGGED(d->character, PLR_FISHING) && len < sizeof(prompt) &&
                !PRF_FLAGGED(d->character, PRF_NODEC)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "FISHING -");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (flagged == true && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "@n\n");
                if (count >= 0)
                    len += count;
            }
            if ((SITS(d->character) && PLR_FLAGGED(d->character, PLR_HEALT)) && len < sizeof(prompt) &&
                !PRF_FLAGGED(d->character, PRF_NODEC)) {
                chair = SITS(d->character);
                count = snprintf(prompt + len, sizeof(prompt) - len, "@c<@CFloating inside a healing tank@c>@n\r\n");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if ((SITS(d->character) && GET_POS(d->character) == POS_SITTING) && len < sizeof(prompt) &&
                !PRF_FLAGGED(d->character, PRF_NODEC)) {
                chair = SITS(d->character);
                count = snprintf(prompt + len, sizeof(prompt) - len, "Sitting on: %s\r\n", chair->short_description);
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if ((SITS(d->character) && GET_POS(d->character) == POS_RESTING) && len < sizeof(prompt) &&
                !PRF_FLAGGED(d->character, PRF_NODEC)) {
                chair = SITS(d->character);
                count = snprintf(prompt + len, sizeof(prompt) - len, "Resting on: %s\r\n", chair->short_description);
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if ((SITS(d->character) && GET_POS(d->character) == POS_SLEEPING) && len < sizeof(prompt) &&
                !PRF_FLAGGED(d->character, PRF_NODEC)) {
                chair = SITS(d->character);
                count = snprintf(prompt + len, sizeof(prompt) - len, "Sleeping on: %s\r\n", chair->short_description);
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (AFF_FLAGGED(d->character, AFF_POSITION) && len < sizeof(prompt) &&
                !PRF_FLAGGED(d->character, PRF_NODEC)) {
                chair = SITS(d->character);
                count = snprintf(prompt + len, sizeof(prompt) - len, "(Best Position)\r\n");
                flagged = true;
                if (count >= 0)
                    len += count;
            }
            if (GET_CHARGE(d->character) < GET_MAX_MANA(d->character) * .01 && GET_CHARGE(d->character) > 0) {
                GET_CHARGE(d->character) = 0;
            }
            if (GET_CHARGE(d->character) > 0) {
                int64_t charge = GET_CHARGE(d->character);
                if (!PRF_FLAGGED(d->character, PRF_NODEC) && !PRF_FLAGGED(d->character, PRF_DISPERC)) {
                    if (charge >= GET_MAX_MANA(d->character)) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G==@D<@RMAX@D>@G===@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .95) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G=========-@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .9) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G=========@g-@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .85) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G========-@g-@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .80) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G========@g--@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .75) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G=======-@g--@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .70) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G=======@g---@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .65) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G======-@g---@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .60) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G======@g----@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .55) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G=====-@g----@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .50) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G=====@g-----@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .45) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G====-@g-----@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .40) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G====@g------@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .35) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G===-@g------@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .30) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G===@g-------@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .25) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G==-@g-------@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .20) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G==@g--------@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .15) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G=-@g--------@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .10) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G=@g---------@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge >= GET_MAX_MANA(d->character) * .05) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@G-@g---------@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (charge < GET_MAX_MANA(d->character) * .05) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@g----------@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@CCharge @D[@g----------@D]@n\n");
                        if (count >= 0)
                            len += count;
                    }
                }
                if (PRF_FLAGGED(d->character, PRF_DISPERC) && !PRF_FLAGGED(d->character, PRF_NODEC)) {
                    if (GET_CHARGE(d->character) > 0) {
                        int64_t perc = (GET_CHARGE(d->character) * 100) / GET_MAX_MANA(d->character);
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@BCharge@Y: @C%" I64T "%s@D]@n\n",
                                         perc, "%");
                        if (count >= 0)
                            len += count;
                    }
                }
                if (PRF_FLAGGED(d->character, PRF_NODEC)) {
                    if (charge > 0) {
                        int64_t perc = (charge * 100) / GET_MAX_MANA(d->character);
                        count = snprintf(prompt + len, sizeof(prompt) - len, "Ki is charged to %" I64T " percent.\n",
                                         perc);
                        if (count >= 0)
                            len += count;
                    }
                }
            }
            if (AFF_FLAGGED(d->character, AFF_FIRESHIELD)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "@D(@rF@RI@YR@rE@RS@YH@rI@RE@YL@rD@D)@n\n");
                if (count >= 0)
                    len += count;
            }
            if (AFF_FLAGGED(d->character, AFF_SANCTUARY)) {
                if (PRF_FLAGGED(d->character, PRF_DISPERC) && !PRF_FLAGGED(d->character, PRF_NODEC)) {
                    if (GET_BARRIER(d->character) > 0) {
                        int64_t perc = (GET_BARRIER(d->character) * 100) / GET_MAX_MANA(d->character);
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@GBarrier@Y: @B%" I64T "%s@D]@n\n",
                                         perc, "%");
                        if (count >= 0)
                            len += count;
                    }
                }

                if (!PRF_FLAGGED(d->character, PRF_NODEC) && !PRF_FLAGGED(d->character, PRF_DISPERC)) {
                    if (GET_BARRIER(d->character) >= GET_MAX_MANA(d->character) * .75) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@BBarrier @D[@C==MAX==@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (GET_BARRIER(d->character) >= GET_MAX_MANA(d->character) * .70) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@BBarrier @D[@C=======@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (GET_BARRIER(d->character) >= GET_MAX_MANA(d->character) * .65) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@BBarrier @D[@C======-@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (GET_BARRIER(d->character) >= GET_MAX_MANA(d->character) * .60) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@BBarrier @D[@C======@c-@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (GET_BARRIER(d->character) >= GET_MAX_MANA(d->character) * .55) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@BBarrier @D[@C=====-@c-@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (GET_BARRIER(d->character) >= GET_MAX_MANA(d->character) * .50) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@BBarrier @D[@C=====@c--@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (GET_BARRIER(d->character) >= GET_MAX_MANA(d->character) * .45) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@BBarrier @D[@C====-@c--@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (GET_BARRIER(d->character) >= GET_MAX_MANA(d->character) * .40) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@BBarrier @D[@C====@c---@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (GET_BARRIER(d->character) >= GET_MAX_MANA(d->character) * .35) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@BBarrier @D[@C===-@c---@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (GET_BARRIER(d->character) >= GET_MAX_MANA(d->character) * .30) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@BBarrier @D[@C===@c----@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (GET_BARRIER(d->character) >= GET_MAX_MANA(d->character) * .25) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@BBarrier @D[@C==-@c----@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (GET_BARRIER(d->character) >= GET_MAX_MANA(d->character) * .20) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@BBarrier @D[@C==@c-----@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (GET_BARRIER(d->character) >= GET_MAX_MANA(d->character) * .15) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@BBarrier @D[@C=-@c-----@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (GET_BARRIER(d->character) >= GET_MAX_MANA(d->character) * .10) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@BBarrier @D[@C=@c------@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (GET_BARRIER(d->character) >= GET_MAX_MANA(d->character) * .05) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@BBarrier @D[@C-@c------@D]@n\n");
                        if (count >= 0)
                            len += count;
                    } else if (GET_BARRIER(d->character) < GET_MAX_MANA(d->character) * .05) {
                        count = snprintf(prompt + len, sizeof(prompt) - len, "@BBarrier @D[@C--Low-@D]@n\n");
                        if (count >= 0)
                            len += count;
                    }
                }
                if (PRF_FLAGGED(d->character, PRF_NODEC)) {
                    if (GET_BARRIER(d->character) > 0) {
                        int64_t perc = (GET_BARRIER(d->character) * 100) / GET_MAX_MANA(d->character);
                        count = snprintf(prompt + len, sizeof(prompt) - len,
                                         "A barrier charged to %" I64T " percent surrounds you.@n\n", perc);
                        if (count >= 0)
                            len += count;
                    }
                }
            }
            if (!PRF_FLAGGED(d->character, PRF_DISPERC)) {
                if (PRF_FLAGGED(d->character, PRF_DISPHP) && len < sizeof(prompt)) {
                    auto col = "n";
                    auto ch = d->character;
                    if (ch->getMaxPL() > ch->getMaxPLTrans())
                        col = "g";
                    else if (ch->isWeightedPL())
                        col = "m";
                    else if (ch->getCurHealthPercent() > .5)
                        col = "c";
                    else if (ch->getCurHealthPercent() > .1)
                        col = "y";
                    else if (ch->getCurHealthPercent() <= .1)
                        col = "r";

                    if ((count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@RPL@n@Y: @%s%s@D]@n", col,
                                          add_commas(ch->getCurPL()).c_str())) > 0)
                        len += count;
                }
            } else if (PRF_FLAGGED(d->character, PRF_DISPHP)) {

                auto ch = d->character;
                auto perc = ((double) ch->getCurHealth() / (double) ch->getMaxPLTrans()) * 100;
                auto col = "n";
                if (perc > 100)
                    col = "g";
                else if (perc >= 70)
                    col = "c";
                else if (perc >= 51)
                    col = "Y";
                else if (perc >= 20)
                    col = "y";
                else
                    col = "r";

                if ((count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@RPL@n@Y: @%s%d%s@D]@n", col, (int) perc,
                                      "@w%")) > 0)
                    len += count;
            }
            if (!PRF_FLAGGED(d->character, PRF_DISPERC)) {
                if (PRF_FLAGGED(d->character, PRF_DISPKI) && len < sizeof(prompt) &&
                    (d->character->getCurKI()) > GET_MAX_MANA(d->character) / 2) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@CKI@Y: @c%s@D]@n", add_commas(
                            (d->character->getCurKI())).c_str());
                    if (count >= 0)
                        len += count;
                } else if (PRF_FLAGGED(d->character, PRF_DISPKI) && len < sizeof(prompt) &&
                           (d->character->getCurKI()) > GET_MAX_MANA(d->character) / 10) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@CKI@Y: @y%s@D]@n", add_commas(
                            (d->character->getCurKI())).c_str());
                    if (count >= 0)
                        len += count;
                } else if (PRF_FLAGGED(d->character, PRF_DISPKI) && len < sizeof(prompt) &&
                           (d->character->getCurKI()) <= GET_MAX_MANA(d->character) / 10) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@CKI@Y: @r%s@D]@n", add_commas(
                            (d->character->getCurKI())).c_str());
                    if (count >= 0)
                        len += count;
                }
            } else if (PRF_FLAGGED(d->character, PRF_DISPKI)) {
                int64_t power = (d->character->getCurKI()), maxpower = GET_MAX_MANA(d->character);
                int perc = 0;
                if (power <= 0) {
                    power = 1;
                }
                if (maxpower <= 0) {
                    maxpower = 1;
                }
                perc = (power * 100) / maxpower;
                if (perc > 100) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@CKI@n@Y: @G%d%s@D]@n", perc, "@w%");
                    if (count >= 0)
                        len += count;
                } else if (perc >= 70) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@CKI@n@Y: @c%d%s@D]@n", perc, "@w%");
                    if (count >= 0)
                        len += count;
                } else if (perc >= 51) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@CKI@n@Y: @Y%d%s@D]@n", perc, "@w%");
                    if (count >= 0)
                        len += count;
                } else if (perc >= 20) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@CKI@n@Y: @y%d%s@D]@n", perc, "@w%");
                    if (count >= 0)
                        len += count;
                } else {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@CKI@n@Y: @r%d%s@D]@n", perc, "@w%");
                    if (count >= 0)
                        len += count;
                }
            }
            if (!PRF_FLAGGED(d->character, PRF_DISPERC)) {
                if (PRF_FLAGGED(d->character, PRF_DISPMOVE) && len < sizeof(prompt) &&
                    (d->character->getCurST()) > GET_MAX_MOVE(d->character) / 2) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@GSTA@Y: @c%s@D]@n", add_commas(
                            (d->character->getCurST())).c_str());
                    if (count >= 0)
                        len += count;
                } else if (PRF_FLAGGED(d->character, PRF_DISPMOVE) && len < sizeof(prompt) &&
                           (d->character->getCurST()) > GET_MAX_MOVE(d->character) / 10) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@GSTA@Y: @y%s@D]@n", add_commas(
                            (d->character->getCurST())).c_str());
                    if (count >= 0)
                        len += count;
                } else if (PRF_FLAGGED(d->character, PRF_DISPMOVE) && len < sizeof(prompt) &&
                           (d->character->getCurST()) <= GET_MAX_MOVE(d->character) / 10) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@GSTA@Y: @r%s@D]@n", add_commas(
                            (d->character->getCurST())).c_str());
                    if (count >= 0)
                        len += count;
                }
            } else if (PRF_FLAGGED(d->character, PRF_DISPMOVE)) {
                int64_t power = (d->character->getCurST()), maxpower = GET_MAX_MOVE(d->character);
                int perc = 0;
                if (power <= 0) {
                    power = 1;
                }
                if (maxpower <= 0) {
                    maxpower = 1;
                }
                perc = (power * 100) / maxpower;
                if (perc > 100) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@GSTA@n@Y: @G%d%s@D]@n", perc, "@w%");
                    if (count >= 0)
                        len += count;
                } else if (perc >= 70) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@GSTA@n@Y: @c%d%s@D]@n", perc, "@w%");
                    if (count >= 0)
                        len += count;
                } else if (perc >= 51) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@GSTA@n@Y: @Y%d%s@D]@n", perc, "@w%");
                    if (count >= 0)
                        len += count;
                } else if (perc >= 20) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@GSTA@n@Y: @y%d%s@D]@n", perc, "@w%");
                    if (count >= 0)
                        len += count;
                } else {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@GSTA@n@Y: @r%d%s@D]@n", perc, "@w%");
                    if (count >= 0)
                        len += count;
                }
            }
            if (PRF_FLAGGED(d->character, PRF_DISPTNL) && len < sizeof(prompt) && GET_LEVEL(d->character) < 100) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@yTNL@Y: @W%s@D]@n", add_commas(
                        level_exp(d->character, GET_LEVEL(d->character) + 1) - GET_EXP(d->character)).c_str());
                if (count >= 0)
                    len += count;
            }
            if (PRF_FLAGGED(d->character, PRF_DISTIME) && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@W%2d %s@D]@n",
                                 (time_info.hours % 12 == 0) ? 12 : (time_info.hours % 12),
                                 time_info.hours >= 12 ? "PM" : "AM");
                if (count >= 0)
                    len += count;
            }
            if (PRF_FLAGGED(d->character, PRF_DISGOLD) && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@YZen@y: @W%s@D]@n",
                                 add_commas(GET_GOLD(d->character)).c_str());
                if (count >= 0)
                    len += count;
            }
            if (PRF_FLAGGED(d->character, PRF_DISPRAC) && len < sizeof(prompt)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@mPS@y: @W%s@D]@n",
                                 add_commas(GET_PRACTICES(d->character)).c_str());
                if (count >= 0)
                    len += count;
            }
            if (PRF_FLAGGED(d->character, PRF_DISHUTH) && len < sizeof(prompt)) {
                int hun = GET_COND(d->character, HUNGER), thir = GET_COND(d->character, THIRST);
                count = snprintf(prompt + len, sizeof(prompt) - len, "\n@D[@mHung@y:");
                if (count >= 0)
                    len += count;
                if (hun >= 48) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, " @WFull@D]@n");
                } else if (hun >= 40) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, " @WAlmost Full@D]@n");
                } else if (hun >= 30) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, " @WNeed Snack@D]@n");
                } else if (hun >= 20) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, " @WHungry@D]@n");
                } else if (hun >= 20) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, " @WVery Hungry@D]@n");
                } else if (hun >= 10) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, " @WAlmost Starving@D]@n");
                } else if (hun >= 5) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, " @WNear Starving@D]@n");
                } else if (hun >= 0) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, " @WStarving@D]@n");
                } else {
                    count = snprintf(prompt + len, sizeof(prompt) - len, " @WN/A@D]@n");
                }
                if (count >= 0)
                    len += count;
                if (thir >= 48) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@mThir@y: @WQuenched@D]@n");
                } else if (thir >= 40) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@mThir@y: @WNeed Sip@D]@n");
                } else if (thir >= 30) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@mThir@y: @WNeed Drink@D]@n");
                } else if (thir >= 20) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@mThir@y: @WThirsty@D]@n");
                } else if (thir >= 20) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@mThir@y: @WVery Thirsty@D]@n");
                } else if (thir >= 10) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@mThir@y: @WAlmost Dehydrated@D]@n");
                } else if (thir >= 5) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@mThir@y: @WNear Dehydration@D]@n");
                } else if (thir >= 0) {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@mThir@y: @WDehydrated@D]@n");
                } else {
                    count = snprintf(prompt + len, sizeof(prompt) - len, "@D[@mThir@y: @WN/A@D]@n");
                }
                if (count >= 0)
                    len += count;
            }
            if (len < sizeof(prompt) && has_group(d->character) && !PRF_FLAGGED(d->character, PRF_GHEALTH)) {
                count = snprintf(prompt + len, sizeof(prompt) - len, "\n%s", report_party_health(d->character));
                if (d->character->temp_prompt)
                    free(d->character->temp_prompt);
                if (count >= 0)
                    len += count;
            }
            if (len < sizeof(prompt))
                count = snprintf(prompt + len, sizeof(prompt) - len, "\n"); /* strncat: OK */
        }
        if (len < sizeof(prompt) && len < 5)
            strncat(prompt, ">\n", sizeof(prompt) - len - 1);    /* strncat: OK */

    } else if (STATE(d) == CON_PLAYING && IS_NPC(d->character))
        snprintf(prompt, sizeof(prompt), "%s>\n", CAP(GET_NAME(d->character)));
    else
        *prompt = '\0';

    return (prompt);
}


/*
 * NOTE: 'txt' must be at most MAX_INPUT_LENGTH big.
 */
void write_to_q(const char *txt, struct txt_q *queue, int aliased) {
    struct txt_block *newt;

    CREATE(newt, struct txt_block, 1);
    newt->text = strdup(txt);
    newt->aliased = aliased;

    /* queue empty? */
    if (!queue->head) {
        newt->next = nullptr;
        queue->head = queue->tail = newt;
    } else {
        queue->tail->next = newt;
        queue->tail = newt;
        newt->next = nullptr;
    }
}



/* Add a new string to a player's output queue. For outside use. */
size_t write_to_output(struct descriptor_data *t, const char *txt, ...) {
    va_list args;
    size_t left;

    va_start(args, txt);
    left = vwrite_to_output(t, txt, args);
    va_end(args);

    return left;
}

#define COLOR_ON(ch) (COLOR_LEV(ch) > 0)

/* Color replacement arrays. Orig. Renx -- 011100, now modified */
char *ANSI[] = {
        "@",
        AA_NORMAL,
        AA_NORMAL ANSISEPSTR AF_BLACK,
        AA_NORMAL ANSISEPSTR AF_BLUE,
        AA_NORMAL ANSISEPSTR AF_GREEN,
        AA_NORMAL ANSISEPSTR AF_CYAN,
        AA_NORMAL ANSISEPSTR AF_RED,
        AA_NORMAL ANSISEPSTR AF_MAGENTA,
        AA_NORMAL ANSISEPSTR AF_YELLOW,
        AA_NORMAL ANSISEPSTR AF_WHITE,
        AA_BOLD ANSISEPSTR AF_BLACK,
        AA_BOLD ANSISEPSTR AF_BLUE,
        AA_BOLD ANSISEPSTR AF_GREEN,
        AA_BOLD ANSISEPSTR AF_CYAN,
        AA_BOLD ANSISEPSTR AF_RED,
        AA_BOLD ANSISEPSTR AF_MAGENTA,
        AA_BOLD ANSISEPSTR AF_YELLOW,
        AA_BOLD ANSISEPSTR AF_WHITE,
        AB_BLACK,
        AB_BLUE,
        AB_GREEN,
        AB_CYAN,
        AB_RED,
        AB_MAGENTA,
        AB_YELLOW,
        AB_WHITE,
        AA_BLINK,
        AA_UNDERLINE,
        AA_BOLD,
        AA_REVERSE,
        "!"
};

const char CCODE[] = "@ndbgcrmywDBGCRMYW01234567luoex!";
/*
  Codes are:      @n - normal
  @d - black      @D - gray           @0 - background black
  @b - blue       @B - bright blue    @1 - background blue
  @g - green      @G - bright green   @2 - background green
  @c - cyan       @C - bright cyan    @3 - background cyan
  @r - red        @R - bright red     @4 - background red
  @m - magneta    @M - bright magneta @5 - background magneta
  @y - yellow     @Y - bright yellow  @6 - background yellow
  @w - white      @W - bright white   @7 - background white
  @x - random
Extra codes:      @l - blink          @o - bold
  @u - underline  @e - reverse video  @@ - single @

  @[num] - user color choice num, [] are required
*/
const char RANDOM_COLORS[] = "bgcrmywBGCRMWY";

#define NEW_STRING_LENGTH (size_t)(dest_char-save_pos)

size_t proc_colors(char *txt, size_t maxlen, int parse, char **choices) {
    char *dest_char, *source_char, *color_char, *save_pos, *replacement = nullptr;
    int i, temp_color;
    size_t wanted;

    if (!txt || !strchr(txt, '@')) /* skip out if no color codes     */
        return strlen(txt);

    source_char = txt;
    CREATE(dest_char, char, maxlen);
    save_pos = dest_char;
    for (; *source_char && (NEW_STRING_LENGTH < maxlen);) {
        /* no color code - just copy */
        if (*source_char != '@') {
            *dest_char++ = *source_char++;
            continue;
        }

        /* if we get here we have a color code */

        source_char++; /* source_char now points to the code */

        /* look for a random color code picks a random number between 1 and 14 */
        if (*source_char == 'x') {
            temp_color = (rand() % 14);
            *source_char = RANDOM_COLORS[temp_color];
        }

        if (*source_char == '\0') { /* string was terminated with color code - just put it in */
            *dest_char++ = '@';
            /* source_char will now point to '\0' in the for() check */
            continue;
        }

        if (!parse) { /* not parsing, just skip the code, unless it's @@ */
            if (*source_char == '@') {
                *dest_char++ = '@';
            }
            if (*source_char == '[') { /* Multi-character code */
                source_char++;
                while (*source_char && isdigit(*source_char))
                    source_char++;
                if (!*source_char)
                    source_char--;
            }
            source_char++; /* skip to next (non-colorcode) char */
            continue;
        }

        /* parse the color code */
        if (*source_char == '[') { /* User configurable color */
            source_char++;
            if (*source_char) {
                i = atoi(source_char);
                if (i < 0 || i >= NUM_COLOR)
                    i = COLOR_NORMAL;
                replacement = default_color_choices[i];
                if (choices && choices[i])
                    replacement = choices[i];
                while (*source_char && isdigit(*source_char))
                    source_char++;
                if (!*source_char)
                    source_char--;
            }
        } else if (*source_char == 'n') {
            replacement = default_color_choices[COLOR_NORMAL];
            if (choices && choices[COLOR_NORMAL])
                replacement = choices[COLOR_NORMAL];
        } else {
            for (i = 0; CCODE[i] != '!'; i++) { /* do we find it ? */
                if ((*source_char) == CCODE[i]) {           /* if so :*/
                    replacement = ANSI[i];
                    break;
                }
            }
        }
        if (replacement) {
            if (NEW_STRING_LENGTH + strlen(replacement) + strlen(ANSISTART) + 1 <
                maxlen) { /* only substitute if there's room for the whole code */
                if (isdigit(replacement[0]))
                    for (color_char = ANSISTART; *color_char;)
                        *dest_char++ = *color_char++;
                for (color_char = replacement; *color_char;)
                    *dest_char++ = *color_char++;
                if (isdigit(replacement[0]))
                    *dest_char++ = ANSIEND;
            }
            replacement = nullptr;
        }
        /* If we couldn't find any correct color code, or we found it and
         * substituted above, let's just process the next character.
         * - Welcor
         */
        source_char++;

    } /* for loop */

    /* make sure output is nullptr - terminated */
    *dest_char = '\0';

    wanted = strlen(source_char); /* see if we wanted more space */
    strncpy(txt, save_pos, maxlen - 1);
    free(save_pos); /* plug memory leak */

    return NEW_STRING_LENGTH + wanted;
}

#undef NEW_STRING_LENGTH

/* Add a new string to a player's output queue. */
// It used to return how many bytes couldn't be written.
// That's no longer a problem so it always returns 0.
size_t vwrite_to_output(struct descriptor_data *t, const char *format, va_list args) {
    // Use a temporary buffer to get the required size
    va_list args_copy;
    va_copy(args_copy, args);  // Make a copy of va_list, since vsnprintf will alter it
    auto len = strlen(format);
    int required_size = std::vsnprintf(nullptr, 0, format, args_copy) + 1; // +1 for null-terminator
    va_end(args_copy);

    if (required_size <= 0) {
        // Handle error here (print message, throw exception, etc)
    }

    // Create a vector of the required size
    std::vector<char> buffer(required_size);
    std::vsnprintf(buffer.data(), buffer.size(), format, args);

    // Append to the output
    t->output += std::string(buffer.begin(), buffer.end() - 1); // -1 to ignore the null-terminator

    return required_size - 1; // Return the size of the appended string (-1 to exclude the null-terminator)
}

void descriptor_data::sendText(const std::string& txt) {
    output += txt;
}

void free_bufpool() {
    struct txt_block *tmp;

    while (bufpool) {
        tmp = bufpool->next;
        if (bufpool->text)
            free(bufpool->text);
        free(bufpool);
        bufpool = tmp;
    }
}

/* ******************************************************************
*  socket handling                                                  *
****************************************************************** */


void descriptor_data::start() {
    write_to_output(this, GREETANSI);
    write_to_output(this, "\r\n@w                  Welcome to Dragonball Advent Truth\r\n");
    write_to_output(this, "@D                 ---(@CPeak Logon Count Today@W: @w%4d@D)---@n\r\n", PCOUNT);
    write_to_output(this, "@D                 ---(@CHighest Logon Count   @W: @w%4d@D)---@n\r\n", HIGHPCOUNT);
    write_to_output(this, "@D                 ---(@CTotal Era %d Characters@W: @w%4s@D)---@n\r\n", CURRENT_ERA,
                    add_commas(players.size()).c_str());
    write_to_output(this,
                    "\r\n@cEnter your desired username or the username you have already made.\n@CEnter Username:@n\r\n");
}



/*
 * Send all of the output that we've accumulated for a player out to
 * the player's descriptor.
 *
 * 32 int8_tGARBAGE_SPACE in MAX_SOCK_BUF used for:
 *	 2 bytes: prepended \r\n
 *	14 bytes: overflow message
 *	 2 bytes: extra \r\n for non-comapct
 *      14 bytes: unused
 */
int process_output(struct descriptor_data *t) {
    /* we may need this \r\n for later -- see below */
    std::string out = "\r\n" + t->output, realout;
    char i[MAX_SOCK_BUF], *osb = i + 2;
    int result;

    /* add the extra CRLF if the person isn't in compact mode */
    if (STATE(t) == CON_PLAYING && t->character && !IS_NPC(t->character) && !PRF_FLAGGED(t->character, PRF_COMPACT))
        out.append("\r\n");    /* strcpy: OK (osb:MAX_SOCK_BUF-2 reserves space) */

    /* add a prompt */

    out.append(make_prompt(t));    /* strcpy: OK (i:MAX_SOCK_BUF reserves space) */

    /*
     * now, send the output.  If this is an 'interruption', use the prepended
     * CRLF, otherwise send the straight output sans CRLF.
     */
    if (t->has_prompt) {
        t->has_prompt = false;
        for(auto &[cid, c] : t->conns) c->sendText(out);
    } else {
        auto o = out.substr(2, out.length() - 2);
        for(auto &[cid, c] : t->conns) c->sendText(o);
    }

    /* Handle snooping: prepend "% " and send to snooper. */
    if (t->snoop_by)
        write_to_output(t->snoop_by, "\nvvvvvvvvvvvvv[Snoop]vvvvvvvvvvvvv\n%s\n^^^^^^^^^^^^^[Snoop]^^^^^^^^^^^^^\n",
                        t->output.c_str());

    t->output.clear();

    return (result);
}


/*
 * perform_socket_write: takes a descriptor, a pointer to text, and a
 * text length, and tries once to send that text to the OS.  This is
 * where we stuff all the platform-dependent stuff that used to be
 * ugly #ifdef's in write_to_descriptor().
 *
 * This function must return:
 *
 * -1  If a fatal error was encountered in writing to the descriptor.
 *  0  If a transient failure was encountered (e.g. socket buffer full).
 * >0  To indicate the number of bytes successfully written, possibly
 *     fewer than the number the caller requested be written.
 *
 * Right now there are two versions of this function: one for Windows,
 * and one for all other platforms.
 */

/* perform_socket_write for all Non-Windows platforms */

void close_socket(struct descriptor_data *d) {
    struct descriptor_data *temp;

    REMOVE_FROM_LIST(d, descriptor_list, next, temp);

    /* Forget snooping */
    if (d->snooping)
        d->snooping->snoop_by = nullptr;

    if (d->snoop_by) {
        write_to_output(d->snoop_by, "Your victim is no longer among us.\r\n");
        d->snoop_by->snooping = nullptr;
    }

    auto c = d->character;

    /* JE 2/22/95 -- part of my unending quest to make switch stable */
    if (d->original && d->original->desc)
        d->original->desc = nullptr;

    if (d->obj_name)
        free(d->obj_name);
    if (d->obj_short)
        free(d->obj_short);
    if (d->obj_long)
        free(d->obj_long);

    /*. Kill any OLC stuff .*/
    switch (d->connected) {
        case CON_OEDIT:
        case CON_IEDIT:
        case CON_REDIT:
        case CON_ZEDIT:
        case CON_MEDIT:
        case CON_SEDIT:
        case CON_TEDIT:
        case CON_AEDIT:
        case CON_TRIGEDIT:
            cleanup_olc(d, CLEANUP_ALL);
            break;
        default:
            break;
    }

    if(d->account) {
        d->account->descriptors.erase(d);
        d->account = nullptr;
    }

    if(c) c->desc = nullptr;
    for(auto &[cid, conn] : d->conns) {
        conn->desc = nullptr;
        if(d->connected == CON_QUITGAME) {
            conn->setParser(new net::CharacterMenu(conn, c));
        } else {
            conn->close();
        }
    }

    sessions.erase(d->id);
    delete d;
    if(c && c->active) extract_char(c);
}


/* ****************************************************************
*       Public routines for system-to-player-communication        *
**************************************************************** */

size_t send_to_char(struct char_data *ch, const char *messg, ...) {
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

int arena_watch(struct char_data *ch) {

    struct descriptor_data *d;
    int found = false, room = NOWHERE;

    for (d = descriptor_list; d; d = d->next) {
        if (STATE(d) != CON_PLAYING)
            continue;

        if (IN_ARENA(d->character)) {
            if (ARENA_IDNUM(ch) == GET_IDNUM(d->character)) {
                found = true;
                room = GET_ROOM_VNUM(IN_ROOM(d->character));
            }
        }
    }

    if (found == false) {
        REMOVE_BIT_AR(PRF_FLAGS(ch), PRF_ARENAWATCH);
        ARENA_IDNUM(ch) = -1;
        return (NOWHERE);
    } else {
        return (room);
    }
}


void send_to_all(const char *messg, ...) {
    struct descriptor_data *i;
    va_list args;

    if (messg == nullptr)
        return;

    for (i = descriptor_list; i; i = i->next) {
        if (STATE(i) != CON_PLAYING)
            continue;

        va_start(args, messg);
        vwrite_to_output(i, messg, args);
        va_end(args);
    }
}


void send_to_outdoor(const char *messg, ...) {
    struct descriptor_data *i;

    if (!messg || !*messg)
        return;

    for (i = descriptor_list; i; i = i->next) {
        va_list args;

        if (STATE(i) != CON_PLAYING || i->character == nullptr)
            continue;
        if (!AWAKE(i->character) || !OUTSIDE(i->character))
            continue;

        va_start(args, messg);
        vwrite_to_output(i, messg, args);
        va_end(args);
    }
}

void send_to_moon(const char *messg, ...) {
    struct descriptor_data *i;

    if (!messg || !*messg)
        return;

    for (i = descriptor_list; i; i = i->next) {
        va_list args;

        if (STATE(i) != CON_PLAYING || i->character == nullptr)
            continue;
        if (!AWAKE(i->character) || !HAS_MOON(i->character))
            continue;

        va_start(args, messg);
        vwrite_to_output(i, messg, args);
        va_end(args);
    }
}

void send_to_planet(int type, int planet, const char *messg, ...) {
    struct descriptor_data *i;

    if (!messg || !*messg)
        return;

    for (i = descriptor_list; i; i = i->next) {
        va_list args;

        if (STATE(i) != CON_PLAYING || i->character == nullptr)
            continue;
        if (!AWAKE(i->character) || !ROOM_FLAGGED(IN_ROOM(i->character), planet))
            continue;
        else {
            if (type == 0) {
                va_start(args, messg);
                vwrite_to_output(i, messg, args);
                va_end(args);
            } else if (OUTSIDE(i->character) && GET_SKILL(i->character, SKILL_SPOT) >= axion_dice(-5)) {
                va_start(args, messg);
                vwrite_to_output(i, messg, args);
                va_end(args);
            }
        }
    }
}


void send_to_room(room_rnum room, const char *messg, ...) {
    struct char_data *i;
    va_list args;

    if (messg == nullptr)
        return;

    for (i = world[room].people; i; i = i->next_in_room) {
        if (!i->desc)
            continue;

        va_start(args, messg);
        vwrite_to_output(i->desc, messg, args);
        va_end(args);
    }

    struct descriptor_data *d;

    for (d = descriptor_list; d; d = d->next) {
        if (STATE(d) != CON_PLAYING)
            continue;

        if (PRF_FLAGGED(d->character, PRF_ARENAWATCH)) {
            if (arena_watch(d->character) == room) {
                char buf[2000];
                *buf = '\0';
                sprintf(buf, "@c-----@CArena@c-----@n\r\n%s\r\n@c-----@CArena@c-----@n\r\n", messg);
                va_start(args, messg);
                vwrite_to_output(d, buf, args);
                va_end(args);
            }
        }
        if (GET_EAVESDROP(d->character) > 0) {
            int roll = rand_number(1, 101);
            if (GET_EAVESDROP(d->character) == room && GET_SKILL(d->character, SKILL_EAVESDROP) > roll) {
                char buf[1000];
                *buf = '\0';
                sprintf(buf, "-----Eavesdrop-----\r\n%s\r\n-----Eavesdrop-----\r\n", messg);
                va_start(args, messg);
                vwrite_to_output(d, buf, args);
                va_end(args);
            }
        }

    }

}


const char *ACTNULL = "<nullptr>";

#define CHECK_NULL(pointer, expression) \
  if ((pointer) == nullptr) i = ACTNULL; else i = (expression);


/* higher-level communication: the act() function */
void perform_act(const char *orig, struct char_data *ch, struct obj_data *obj, const void *vict_obj, struct char_data *to) {
    const char *i = nullptr;
    char lbuf[MAX_STRING_LENGTH], *buf, *j;
    bool uppercasenext = false;
    struct char_data *dg_victim = nullptr;
    struct obj_data *dg_target = nullptr;
    char *dg_arg = nullptr;

    buf = lbuf;

    for (;;) {
        if (*orig == '$') {
            switch (*(++orig)) {
                case 'n':
                    i = PERS(ch, to);
                    break;
                case 'N':
                    CHECK_NULL(vict_obj, PERS((struct char_data *) vict_obj, to));
                    dg_victim = (struct char_data *) vict_obj;
                    break;
                case 'm':
                    i = HMHR(ch);
                    break;
                case 'M':
                    CHECK_NULL(vict_obj, HMHR((struct char_data *) vict_obj));
                    dg_victim = (struct char_data *) vict_obj;
                    break;
                case 's':
                    i = HSHR(ch);
                    break;
                case 'S':
                    CHECK_NULL(vict_obj, HSHR((const struct char_data *) vict_obj));
                    dg_victim = (struct char_data *) vict_obj;
                    break;
                case 'e':
                    i = HSSH(ch);
                    break;
                case 'E':
                    CHECK_NULL(vict_obj, HSSH((struct char_data *) vict_obj));
                    dg_victim = (struct char_data *) vict_obj;
                    break;
                case 'o':
                    CHECK_NULL(obj, OBJN(obj, to));
                    break;
                case 'O':
                    CHECK_NULL(vict_obj, OBJN((struct obj_data *) vict_obj, to));
                    dg_target = (struct obj_data *) vict_obj;
                    break;
                case 'p':
                    CHECK_NULL(obj, OBJS(obj, to));
                    break;
                case 'P':
                    CHECK_NULL(vict_obj, OBJS((struct obj_data *) vict_obj, to));
                    dg_target = (struct obj_data *) vict_obj;
                    break;
                case 'a':
                    CHECK_NULL(obj, SANA(obj));
                    break;
                case 'A':
                    CHECK_NULL(vict_obj, SANA((struct obj_data *) vict_obj));
                    dg_target = (struct obj_data *) vict_obj;
                    break;
                case 'T':
                    CHECK_NULL(vict_obj, (char *) vict_obj);
                    dg_arg = (char *) vict_obj;
                    break;
                case 't':
                    CHECK_NULL(obj, (char *) obj);
                    break;
                case 'F':
                    CHECK_NULL(vict_obj, fname((const char *) vict_obj));
                    break;
                    /* uppercase previous word */
                case 'u':
                    for (j = buf; j > lbuf && !isspace((int) *(j - 1)); j--);
                    if (j != buf)
                        *j = UPPER(*j);
                    i = "";
                    break;
                    /* uppercase next word */
                case 'U':
                    uppercasenext = true;
                    i = "";
                    break;
                case '$':
                    i = "$";
                    break;
                default:
                    return;
                    break;
            }
            while ((*buf = *(i++))) {
                if (uppercasenext && !isspace((int) *buf)) {
                    *buf = UPPER(*buf);
                    uppercasenext = false;
                }
                buf++;
            }
            orig++;
        } else if (!(*(buf++) = *(orig++))) {
            break;
        } else if (uppercasenext && !isspace((int) *(buf - 1))) {
            *(buf - 1) = UPPER(*(buf - 1));
            uppercasenext = false;
        }
    }

    *(--buf) = '\r';
    *(++buf) = '\n';
    *(++buf) = '\0';

    if (to->desc)
        write_to_output(to->desc, "%s", CAP(lbuf));

    if ((IS_NPC(to) && dg_act_check) && (to != ch))
        act_mtrigger(to, lbuf, ch, dg_victim, obj, dg_target, dg_arg);

    if (last_act_message)
        free(last_act_message);
    last_act_message = strdup(lbuf);

}

char *act(const char *str, int hide_invisible, struct char_data *ch,
          struct obj_data *obj, const void *vict_obj, int type) {
    struct char_data *to;
    int to_sleeping, res_sneak, res_hide, dcval = 0, resskill = 0;

    if (!str || !*str)
        return nullptr;

    /* Warning: the following TO_SLEEP code is a hack. I wanted to be able to tell
     * act to deliver a message regardless of sleep without adding an additional
     * argument.  TO_SLEEP is 128 (a single bit high up).  It's ONLY legal to
     * combine TO_SLEEP with one other TO_x command.  It's not legal to combine
     * TO_x's with each other otherwise. TO_SLEEP only works because its value
     * "happens to be" a single bit; do not change it to something else.  In
     * short, it is a hack.  The same applies to TO_*RESIST.  */

    /* check if TO_SLEEP is there, and remove it if it is. */
    if ((to_sleeping = (type & TO_SLEEP)))
        type &= ~TO_SLEEP;

    if ((res_sneak = (type & TO_SNEAKRESIST)))
        type &= ~TO_SNEAKRESIST;

    if ((res_hide = (type & TO_HIDERESIST)))
        type &= ~TO_HIDERESIST;

    if (res_sneak && AFF_FLAGGED(ch, AFF_SNEAK)) {
        dcval = roll_skill(ch, SKILL_MOVE_SILENTLY); /* How difficult to counter? */
        if (GET_SKILL(ch, SKILL_BALANCE))
            dcval += GET_SKILL(ch, SKILL_BALANCE) / 10;
        if (IS_MUTANT(ch) && (GET_GENOME(ch, 0) == 5 || GET_GENOME(ch, 1) == 5)) {
            dcval += 10;
        }
        resskill = SKILL_SPOT;             /* Skill used to resist      */
    } else if (res_hide && AFF_FLAGGED(ch, AFF_HIDE)) {
        dcval = roll_skill(ch, SKILL_HIDE);
        if (GET_SKILL(ch, SKILL_BALANCE))
            dcval += GET_SKILL(ch, SKILL_BALANCE) / 10;
        resskill = SKILL_SPOT;
    }

    /* this is a hack as well - DG_NO_TRIG is 256 -- Welcor */
    /* If the bit is set, unset dg_act_check, thus the ! below */
    if (!(dg_act_check = !IS_SET(type, DG_NO_TRIG)))
        REMOVE_BIT(type, DG_NO_TRIG);

    if (type == TO_CHAR) {
        if (ch && SENDOK(ch) && (!resskill || (roll_skill(ch, resskill) >= dcval))) {
            perform_act(str, ch, obj, vict_obj, ch);
            return last_act_message;
        }
        return nullptr;
    }

    if (type == TO_VICT) {
        if ((to = (struct char_data *) vict_obj) != nullptr && SENDOK(to) &&
            (!resskill || (roll_skill(to, resskill) >= dcval))) {
            perform_act(str, ch, obj, vict_obj, to);
            return last_act_message;
        }
        return nullptr;
    }

    if (type == TO_GMOTE) {
        struct descriptor_data *i;
        char buf[MAX_STRING_LENGTH];
        for (i = descriptor_list; i; i = i->next) {
            if (!i->connected && i->character &&
                !PRF_FLAGGED(i->character, PRF_NOGOSS) &&
                !PLR_FLAGGED(i->character, PLR_WRITING) &&
                !ROOM_FLAGGED(IN_ROOM(i->character), ROOM_SOUNDPROOF)) {

                sprintf(buf, "@y%s@n", str);
                perform_act(buf, ch, obj, vict_obj, i->character);
                char buf2[MAX_STRING_LENGTH];
                sprintf(buf2, "%s\r\n", buf);
                add_history(i->character, buf2, HIST_GOSSIP);
            }
        }
        return last_act_message;
    }

    /* ASSUMPTION: at this point we know type must be TO_NOTVICT or TO_ROOM */

    if (ch && IN_ROOM(ch) != NOWHERE)
        to = ch->getRoom()->people;
    else if (obj && IN_ROOM(obj) != NOWHERE)
        to = obj->getRoom()->people;
    else {
        return nullptr;
    }

    if ((type & TO_ROOM)) {
        struct descriptor_data *d;

        for (d = descriptor_list; d; d = d->next) {
            if (STATE(d) != CON_PLAYING)
                continue;

            if (ch != nullptr) {
                if (IN_ARENA(ch)) {
                    if (PRF_FLAGGED(d->character, PRF_ARENAWATCH)) {
                        if (arena_watch(d->character) == GET_ROOM_VNUM(IN_ROOM(ch))) {
                            char buf3[2000];
                            *buf3 = '\0';
                            sprintf(buf3, "@c-----@CArena@c-----@n\r\n%s\r\n@c-----@CArena@c-----@n\r\n", str);
                            perform_act(buf3, ch, obj, vict_obj, d->character);
                        }
                    }
                }
            }
            if (GET_EAVESDROP(d->character) > 0) {
                int roll = rand_number(1, 101);
                if (!resskill || (roll_skill(d->character, resskill) >= dcval)) {
                    if (ch != nullptr && GET_EAVESDROP(d->character) == GET_ROOM_VNUM(IN_ROOM(ch)) &&
                        GET_SKILL(d->character, SKILL_EAVESDROP) > roll) {
                        char buf3[1000];
                        *buf3 = '\0';
                        sprintf(buf3, "-----Eavesdrop-----\r\n%s\r\n-----Eavesdrop-----\r\n", str);
                        perform_act(buf3, ch, obj, vict_obj, d->character);
                    } else if (obj != nullptr && GET_EAVESDROP(d->character) == GET_ROOM_VNUM(IN_ROOM(obj)) &&
                               GET_SKILL(d->character, SKILL_EAVESDROP) > roll) {
                        char buf3[1000];
                        *buf3 = '\0';
                        sprintf(buf3, "-----Eavesdrop-----\r\n%s\r\n-----Eavesdrop-----\r\n", str);
                        perform_act(buf3, ch, obj, vict_obj, d->character);
                    }
                }
            }
        }
    }

    for (; to; to = to->next_in_room) {
        if (!SENDOK(to) || (to == ch))
            continue;
        if (hide_invisible && ch && !CAN_SEE(to, ch))
            continue;
        if (type != TO_ROOM && to == vict_obj)
            continue;
        if (resskill && roll_skill(to, resskill) < dcval)
            continue;
        perform_act(str, ch, obj, vict_obj, to);
    }
    return last_act_message;
}


/* Prefer the file over the descriptor. */
void setup_log() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::info); // Set the console to output info level and above messages
    console_sink->set_pattern("[%^%l%$] %v"); // Example pattern: [INFO] some message

    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(config::logFile, 1024 * 1024 * 5, 3);
    file_sink->set_level(spdlog::level::trace); // Set the file to output all levels of messages

    std::vector<spdlog::sink_ptr> sinks {console_sink, file_sink};

    logger = std::make_shared<spdlog::logger>("logger", begin(sinks), end(sinks));
    logger->set_level(spdlog::level::trace); // Set the logger to trace level
    spdlog::register_logger(logger);
}

int open_logfile(const char *filename, FILE *stderr_fp) {
    if (stderr_fp)    /* freopen() the descriptor. */
        logfile = freopen(filename, "w", stderr_fp);
    else
        logfile = fopen(filename, "w");

    if (logfile) {
        printf("Using log file '%s'%s.\n",
               filename, stderr_fp ? " with redirection" : "");
        return (true);
    }

    printf("SYSERR: Error opening file '%s': %s\n", filename, strerror(errno));
    return (false);
}

/*
 * This may not be pretty but it keeps game_loop() neater than if it was inline.
 */


void show_help(std::shared_ptr<net::Connection>& co, const char *entry) {
    int chk, bot, top, mid, minlen;
    char buf[MAX_STRING_LENGTH];

    if (!help_table) return;

    bot = 0;
    top = top_of_helpt;
    minlen = strlen(entry);

    for (;;) {
        mid = (bot + top) / 2;

        if (bot > top) {
            return;
        } else if (!(chk = strncasecmp(entry, help_table[mid].keywords, minlen))) {
            while ((mid > 0) &&
                   (!(chk = strncasecmp(entry, help_table[mid - 1].keywords, minlen))))
                mid--;
            co->sendText(help_table[mid].entry);
            return;
        } else {
            if (chk > 0) bot = mid + 1;
            else top = mid - 1;
        }
    }
}

/* Thx to Jamie Nelson of 4D for this contribution */
void send_to_range(room_vnum start, room_vnum finish, const char *messg, ...) {
    struct char_data *i;
    va_list args;
    int j;

    if (start > finish) {
        basic_mud_log("send_to_range passed start room value greater then finish.");
        return;
    }
    if (messg == nullptr)
        return;

    for (auto &r : world) {
        if (r.first >= start && r.first <= finish) {
            for (i = r.second.people; i; i = i->next_in_room) {
                if (!i->desc)
                    continue;

                va_start(args, messg);
                vwrite_to_output(i->desc, messg, args);
                va_end(args);
            }
        }
    }
}


void descriptor_data::handle_input() {
    // Now we need to process the raw_input_queue, watching for special characters and also aliases.
    // Commands are processed first-come-first served...
    while(raw_input_queue->ready()) {
        if(!raw_input_queue->try_receive([&](boost::system::error_code ec, const std::string &command) {
            if(!ec) {
                if (snoop_by)
                    write_to_output(snoop_by, "%% %s\r\n", command.c_str());

                if(command == "--") {
                    // this is a special command that clears out the processed input_queue.
                    input_queue.clear();
                    write_to_output(this, "All queued commands cancelled.\r\n");
                } else {
                    perform_alias(this, (char*)command.c_str());
                }
            }
        })) continue;

    }

    if (character && GET_WAIT_STATE(character)) {
        GET_WAIT_STATE(character) -= 1;
        if(GET_WAIT_STATE(character) < 0) GET_WAIT_STATE(character) = 0;
        if (GET_WAIT_STATE(character)) return;
    }

    if(input_queue.empty()) return;
    auto command = input_queue.front();
    input_queue.pop_front();

    if (character) {
        /* Reset the idle timer & pull char back from void if necessary */
        character->timer = 0;
        if (STATE(this) == CON_PLAYING && GET_WAS_IN(character) != NOWHERE) {
            if (IN_ROOM(character) != NOWHERE)
                char_from_room(character);
            char_to_room(character, GET_WAS_IN(character));
            GET_WAS_IN(character) = NOWHERE;
            act("$n has returned.", true, character, nullptr, nullptr, TO_ROOM);
        }
        GET_WAIT_STATE(character) = 1;
    }
    has_prompt = false;

    auto comm = (char*)command.c_str();

    if (str)        /* Writing boards, mail, etc. */
        string_add(this, comm);
    else if (STATE(this) != CON_PLAYING) /* In menus, etc. */
        nanny(this, comm);
    else {            /* else: we're playing normally. */
        command_interpreter(character, comm); /* Send it to interpreter */
    }

}

void shutdown_game(int exitCode) {
    if(logger) {
        logger->info("Process exiting with exit code {}", exitCode);
    } else {
        std::cout << "Process exiting with exit code " << exitCode << std::endl;
    }
    std::exit(exitCode);
}

void descriptor_data::onConnectionClosed(int64_t connId) {
    conns.erase(connId);
    if(conns.empty()) {
        handleLostLastConnection(true);
    }
}

void descriptor_data::onConnectionLost(int64_t connId) {
    conns.erase(connId);
    if(conns.empty()) {
        handleLostLastConnection(false);
    }
}

void descriptor_data::handleLostLastConnection(bool graceful) {
    // At the moment, it doesn't really matter if the disconnect was graceful or not.
    // If they didn't use 'quit', then we have a problem.
    // We need to set the timeout for this character...
    if(character) {
        act("$n has lost $s link.", true, character, nullptr, nullptr, TO_ROOM);
    }
}