#ifndef FEATURES_TEST_GOLDENS_H
#define FEATURES_TEST_GOLDENS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Record-or-compare storage for values a test must watch but must not own.
//
// A game's numbers are design knobs: balance constants, spawn counts, layout
// digests, budget sizes. Pinning them inside test source turns every design
// change into test repair. These helpers keep the value in a text bank beside
// the test instead, so re-recording is one command and the diff shows what the
// change actually moved.
//
// Compare mode (default): returns the recorded value; the caller asserts.
// Record mode (GAME_UPDATE_GOLDENS=1): stores `actual` and returns it.
// A key with no recorded value fails the process with the command to record it.
//
// Banks live in GAME_GOLDENS_DIR, or `tests/goldens` relative to the working
// directory when that variable is unset.

uint64_t test_golden_u64(const char *bank, const char *key, uint64_t actual);
int64_t test_golden_i64(const char *bank, const char *key, int64_t actual);
double test_golden_f64(const char *bank, const char *key, double actual);

// The returned pointer is owned by the bank and stays valid until the next call
// that touches the same bank.
const char *test_golden_text(const char *bank, const char *key, const char *actual);

#ifdef __cplusplus
}
#endif

#endif
