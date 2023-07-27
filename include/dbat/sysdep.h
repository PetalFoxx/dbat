#pragma once
//#include "conf.h"
#include "typestubs.h"

#define CIRCLE_GNU_LIBC_MEMORY_TRACK    0    /* 0 = off, 1 = on */

// Some C libraires
#include <cstdio>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <strings.h>
#include <cstdlib>
#include <sys/types.h>
#include <bsd/string.h>
#include <unistd.h>
#include <climits>
#include <cerrno>
#include <crypt.h>
#include <sys/time.h>
#include <ctime>
#include <cassert>
#include <zlib.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <csignal>
#include <sys/uio.h>
#include <sys/stat.h>
#include <cstddef>

#include <dirent.h>
#include <linux/limits.h>

#ifdef HAVE_LIBBSD

#include <bsd/string.h>

#else
#include "stringutils.h"
#endif

// C++ STD libraries
#include <string>
#include <list>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <set>
#include <random>
#include <chrono>
#include <optional>
#include <filesystem>
#include <array>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <mutex>

// non-standard libraries
#include "fmt/core.h"
#include <spdlog/spdlog.h>
#include "effolkronium/random.hpp"
using Random = effolkronium::random_static;

/* Basic system dependencies *******************************************/
#if CIRCLE_GNU_LIBC_MEMORY_TRACK && !defined(HAVE_MCHECK_H)
#error "Cannot use GNU C library memory tracking without <mcheck.h>"
#endif

#define HAS_RLIMIT

#define CIRCLE_UNSIGNED_INDEX 0    /* 0 = signed, 1 = unsigned */

#if CIRCLE_UNSIGNED_INDEX
#define IDXTYPE    uint32_t
#define NOWHERE    ((IDXTYPE)~0)
#else
#define IDXTYPE	int
#define NOWHERE	(-1)	/* nil reference for rooms	*/
#endif

#define NOTHING    NOWHERE
#define NOBODY    NOWHERE
#define NOFLAG  NOWHERE

#define I64T "lld"
#define SZT "lld"
#define TMT "ld"

/* Various virtual (human-reference) number types. */
typedef IDXTYPE vnum;
typedef vnum room_vnum;
typedef vnum obj_vnum;
typedef vnum mob_vnum;
typedef vnum zone_vnum;
typedef vnum shop_vnum;
typedef vnum trig_vnum;
typedef vnum guild_vnum;

/* Various real (array-reference) number types. */
typedef vnum room_rnum;
typedef vnum obj_rnum;
typedef vnum mob_rnum;
typedef vnum zone_rnum;
typedef vnum shop_rnum;
typedef vnum trig_rnum;
typedef vnum guild_rnum;

/*
 * Bitvector type for 32 bit unsigned long bitvectors.
 */
typedef uint32_t bitvector_t;

typedef void(*CommandFunc)(struct char_data *ch, char *argument, int cmd, int subcmd);

typedef int(*SpecialFunc)(struct char_data *ch, void *me, int cmd, char *argument);

#define ACMD(name) void (name)(struct char_data *ch, char *argument, int cmd, int subcmd)
#define SPECIAL(name) int (name)(struct char_data *ch, void *me, int cmd, char *argument)

template <typename Key, typename T>
class DebugMap : public std::map<Key, T> {
public:
    T& operator[](const Key& key) {
        if (key < 0) {
            throw std::runtime_error("Invalid key");
        }
        return std::map<Key, T>::operator[](key);
    }
};

template<typename T = bool>
using OpResult = std::pair<T, std::optional<std::string>>;

extern std::shared_ptr<spdlog::logger> logger;

extern std::random_device randomDevice;
extern std::default_random_engine randomEngine;

template<typename T>
T randomNumber(T min, T max) {
    std::uniform_int_distribution<T> dist(min, max);
    return dist(randomEngine);
}

template <typename Iterator, typename Key = std::function<std::string(typename std::iterator_traits<Iterator>::value_type)>>
Iterator partialMatch(
        const std::string& match_text,
        Iterator begin, Iterator end,
        bool exact = false,
        Key key = [](const auto& val){ return std::to_string(val); }
)
{
    // Use a multimap to automatically sort by the transformed key.
    using ValueType = typename std::iterator_traits<Iterator>::value_type;
    std::multimap<std::string, ValueType> sorted_map;
    std::for_each(begin, end, [&](const auto& val) {
        sorted_map.insert({key(val), val});
    });

    for (const auto& pair : sorted_map)
    {
        if (boost::iequals(pair.first, match_text))
        {
            return std::find(begin, end, pair.second);
        }
        else if (!exact && boost::istarts_with(pair.first, match_text))
        {
            return std::find(begin, end, pair.second);
        }
    }
    return end;
}