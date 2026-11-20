#ifndef FEATURES_STEAM_PLATFORM_TESTS_STEAM_LIVE_HPP
#define FEATURES_STEAM_PLATFORM_TESTS_STEAM_LIVE_HPP

/* For the tests that run against the developer's Steam client as Spacewar (480): start or
   skip, real-time frames, waits with a bound, ports no other run is likely to hold. */

#include "features/steam_platform/steam_platform.h"
#include "steam_platform_internal.h"

#include <chrono>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <thread>

namespace steam_live {

constexpr int EXIT_SKIP = 77;

/* The variable's text, empty when unset. */
inline std::string env(const char *name) {
#ifdef _WIN32
    char *value = nullptr;
    size_t length = 0;
    std::string text;
    if (_dupenv_s(&value, &length, name) == 0 && value != nullptr) {
        text = value;
    }
    free(value);
    return text;
#else
    const char *value = getenv(name);
    return value != nullptr ? value : "";
#endif
}

/* A run that claims to have used Steam sets STEAM_PLATFORM_REQUIRE_CLIENT, and then a missing
   client fails instead of skipping: CTest shows a skip only as "Skipped" and exits 0. */
inline int no_client(const char *message) {
    const std::string require = env("STEAM_PLATFORM_REQUIRE_CLIENT");
    if (!require.empty() && require != "0") {
        printf("FAIL: no Steam client and STEAM_PLATFORM_REQUIRE_CLIENT is set: %s\n", message);
        return 1;
    }
    printf("SKIP: no Steam client: %s\n", message);
    return EXIT_SKIP;
}

inline bool simulate_no_client(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--simulate-no-client") == 0) {
            return true;
        }
    }
    return false;
}

/* 0 ready, EXIT_SKIP without a Steam client (1 when one is required), 1 on any other answer.
   `simulate` takes the no-client path without touching Steam. */
inline int start(bool simulate = false) {
    if (simulate) {
        steam_platform_internal_simulate_failure(false, STEAM_PLATFORM_INTERNAL_INIT_NO_STEAM_CLIENT, "simulated: Steam is not running");
    }
    const steam_platform_config_t config = {480, false};
    char message[256];
    const steam_platform_status_t status = steam_platform_start(&config, message, sizeof message);
    if (status == STEAM_PLATFORM_NO_CLIENT) {
        return no_client(message);
    }
    if (status != STEAM_PLATFORM_READY) {
        printf("FAIL: steam_platform_start answered %d: %s\n", static_cast<int>(status), message);
        return 1;
    }
    return 0;
}

inline double now_s() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

/* Lets real time pass, then lets Steam's callbacks and the links' frame work run. */
inline void pass(double seconds) {
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
    steam_platform_run_frame();
}

template <typename Done> bool pass_until(double limit_s, Done done, double step_s = 0.01) {
    const double end = now_s() + limit_s;
    while (now_s() < end) {
        pass(step_s);
        if (done()) {
            return true;
        }
    }
    return done();
}

/* A block of ports per live test, so the tests can run at once (ctest -j) and a guest never
   dials a port another test hosts on: a test hosts and dials only inside its own block, and an
   open walks on past a port something else holds. STEAM_LIVE_PORT_SHIFT (default 0) moves all
   blocks, so two runs on one machine need not share ports. */
enum block { BLOCK_CONTRACT = 0, BLOCK_PROBE = 1, BLOCK_TWO_WINDOWS = 2, BLOCK_CLOSES = 3, BLOCK_COUNT = 4 };
constexpr uint16_t BLOCK_BASE = 27600, BLOCK_SIZE = 200;
inline uint16_t port_block(block which) {
    const int shift = atoi(env("STEAM_LIVE_PORT_SHIFT").c_str());
    const int base = BLOCK_BASE + shift + static_cast<int>(which) * BLOCK_SIZE;
    /* Clear of the system's ephemeral range and of Steam's own ports below. */
    return static_cast<uint16_t>(base >= 1024 && base + BLOCK_COUNT * BLOCK_SIZE < 49152 ? base : BLOCK_BASE + static_cast<int>(which) * BLOCK_SIZE);
}
inline bool port_in_block(block which, uint16_t port) { return port >= port_block(which) && port < port_block(which) + BLOCK_SIZE; }

/* One line per case with its time, from setUp and tearDown, so slow cases show. */
inline double &case_start() {
    static double start;
    return start;
}
inline void case_began() { case_start() = now_s(); }
inline void case_ended(const char *name) { printf("case %s: %.2f s\n", name, now_s() - case_start()); }

} // namespace steam_live

#endif
