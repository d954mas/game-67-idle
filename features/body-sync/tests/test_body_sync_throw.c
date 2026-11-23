#include <math.h>
#include <stdio.h>
#include <string.h>

#include "features/body_sync/body_sync.h"
#include "features/body_sync/body_sync_stand.h"
#include "stand_testing.h"
#include "unity.h"

/* A guest's throw into a resting pile, on the stand: the host replays the flight from the
   release, so its impact is the guest's, a one-way later, with the same speed and the same
   push on what it hits; and a throw that lands before the host hears of it never puts the
   thrown body inside another one there. */

enum { TICKS = 400, BODIES = 128 };

void setUp(void) {}
void tearDown(void) {}

static body_sync_scene_t scene;
static body_sync_scenario_t scenario;

static float speed[2][TICKS][BODIES];
static float deepest; /* the host's deepest contact of the thrown body, metres below zero */

static float length3(b3Vec3 v) { return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); }

static body_sync_stand_t *make(uint32_t rtt_ms) {
    char text[512];
    snprintf(text, sizeof(text),
             "scenario throw_seam\nscene pile60\nseed 52\nguests 1\nend 6s\nlight_mass 20kg\nnet g1 rtt %ums\n"
             "p1 grab ball as grab\np1 wait %ums\np1 throw pile31 speed 12 as throw\n",
             rtt_ms, 300u + 2u * rtt_ms);
    body_sync_error_t error;
    TEST_ASSERT_TRUE_MESSAGE(body_sync_scenario_parse(text, strlen(text), &scenario, &error), error.text);
    uint32_t count = 0;
    const body_sync_scene_entry_t *catalog = body_sync_scene_catalog(&count);
    body_sync_scene_lookup(catalog, count, "pile60")->build(&scene);
    body_sync_stand_config_t config = {.scene = &scene, .scenario = &scenario};
    body_sync_stand_t *stand = body_sync_stand_create(&config, &error);
    TEST_ASSERT_NOT_NULL_MESSAGE(stand, error.text);
    return stand;
}

/* Runs the throw; the ball's slot, the guest's release tick, and per endpoint the first tick its
   speed changed by more than 1 m/s (its first contact). */
static void run(uint32_t rtt_ms, uint32_t *ball, int32_t *release, int32_t contact[2]) {
    memset(speed, 0, sizeof(speed));
    deepest = 0.0f;
    body_sync_stand_t *stand = make(rtt_ms);
    *ball = BODIES;
    *release = -1;
    for (uint32_t t = 0; t < TICKS && body_sync_stand_tick(stand); t++) {
        for (uint32_t ep = 0; ep < 2; ep++) {
            body_sync_t *sync = stand_testing_sync(stand, ep);
            for (uint32_t k = 0; k < body_sync_stand_body_count(stand, ep); k++) {
                body_sync_body_view_t view;
                if (!body_sync_stand_body(stand, ep, k, &view) || (view.flags & (BODY_SYNC_VIEW_STATIC | BODY_SYNC_VIEW_CHARACTER))) {
                    continue;
                }
                uint32_t slot = body_sync_id_slot(view.id);
                b3BodyId body = body_sync_body(sync, view.id);
                if (slot >= BODIES || !b3Body_IsValid(body)) {
                    continue;
                }
                speed[ep][t][slot] = length3(b3Body_GetLinearVelocity(body));
                if (strcmp(view.name, "ball") == 0) {
                    *ball = slot;
                    if (ep == 1 && *release < 0 && (view.flags & BODY_SYNC_VIEW_FLYING)) {
                        *release = (int32_t)t;
                    }
                    if (ep == 0) {
                        b3ContactData contacts[16];
                        int n = b3Body_GetContactData(body, contacts, 16);
                        for (int i = 0; i < n; i++) {
                            for (int m = 0; m < contacts[i].manifoldCount; m++) {
                                for (int p = 0; p < contacts[i].manifolds[m].pointCount; p++) {
                                    deepest = fminf(deepest, contacts[i].manifolds[m].points[p].separation);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    body_sync_stand_destroy(stand);
    TEST_ASSERT_TRUE_MESSAGE(*ball < BODIES && *release > 0, "no throw seen");
    for (int ep = 0; ep < 2; ep++) {
        /* From the tick after the body flies there: the host's replay starts it moving at once. */
        int32_t from = *release;
        while (from < TICKS - 1 && speed[ep][from][*ball] < 5.0f) {
            from++;
        }
        contact[ep] = -1;
        for (int32_t t = from + 1; t < TICKS && contact[ep] < 0; t++) {
            contact[ep] = fabsf(speed[ep][t][*ball] - speed[ep][t - 1][*ball]) > 1.0f ? t : -1;
        }
        TEST_ASSERT_TRUE_MESSAGE(contact[ep] > 0, ep ? "the guest's ball never hit" : "the host's ball never hit");
    }
}

/* The pile body the guest's ball pushed hardest, and its peak speed over the six ticks from a
   contact on each endpoint. */
static void hit_body(uint32_t ball, const int32_t contact[2], float peak[2]) {
    uint32_t hit = BODIES;
    float best = 0.0f;
    for (uint32_t s = 0; s < BODIES; s++) {
        for (int32_t t = contact[1]; t < contact[1] + 6 && s != ball; t++) {
            if (speed[1][t][s] > best) {
                best = speed[1][t][s];
                hit = s;
            }
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(hit < BODIES && best > 0.5f, "the ball pushed nothing on the guest");
    for (int ep = 0; ep < 2; ep++) {
        peak[ep] = 0.0f;
        for (int32_t t = contact[ep]; t < contact[ep] + 6; t++) {
            peak[ep] = fmaxf(peak[ep], speed[ep][t][hit]);
        }
    }
}

static void replayed(uint32_t rtt_ms, int32_t one_way_ticks) {
    uint32_t ball;
    int32_t release, contact[2];
    run(rtt_ms, &ball, &release, contact);
    char what[160];
    snprintf(what, sizeof(what), "rtt %u: contact host %d guest %d", rtt_ms, contact[0], contact[1]);
    TEST_ASSERT_TRUE_MESSAGE(contact[0] >= contact[1] && contact[0] - contact[1] <= one_way_ticks + 1, what);
    float host = speed[0][contact[0] - 1][ball], guest = speed[1][contact[1] - 1][ball];
    snprintf(what, sizeof(what), "rtt %u: the ball met the pile at %.3f m/s on the host, %.3f on the guest", rtt_ms, (double)host, (double)guest);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(host - guest) <= 0.01f * guest, what);
    float peak[2];
    hit_body(ball, contact, peak);
    snprintf(what, sizeof(what), "rtt %u: the body hit moved at %.3f m/s on the host, %.3f on the guest", rtt_ms, (double)peak[0], (double)peak[1]);
    TEST_ASSERT_TRUE_MESSAGE(fabsf(peak[0] - peak[1]) <= 0.1f * peak[1], what);
}

static void test_the_host_replays_the_throw_at_rtt_0(void) { replayed(0, 1); }
static void test_the_host_replays_the_throw_at_rtt_100(void) { replayed(100, 3); }

/* At 600 ms the ball has hit and gone back to the host before the host hears of the throw (the
   grab was granted first: a throw inside the grab's round trip is the claim's case, not this
   one): the host starts from a flight state, never from a pose inside the pile, so its impact
   goes no deeper than the same throw heard at once. */
static void test_a_throw_that_lands_before_the_host_hears_of_it(void) {
    uint32_t ball;
    int32_t release, contact[2];
    run(0, &ball, &release, contact);
    float at_once = deepest;
    run(600, &ball, &release, contact);
    TEST_ASSERT_TRUE_MESSAGE(contact[0] - contact[1] >= 12, "the host heard of the throw before it landed: the case is not tested");
    char what[128];
    snprintf(what, sizeof(what), "the late host's ball sank %.3f m into a body, %.3f m heard at once", (double)-deepest, (double)-at_once);
    TEST_ASSERT_TRUE_MESSAGE(deepest >= at_once - 0.01f, what);
    float host = speed[0][contact[0] - 1][ball], guest = speed[1][contact[1] - 1][ball];
    TEST_ASSERT_TRUE_MESSAGE(fabsf(host - guest) <= 0.01f * guest, "the late host's ball did not fly the guest's flight");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_the_host_replays_the_throw_at_rtt_0);
    RUN_TEST(test_the_host_replays_the_throw_at_rtt_100);
    RUN_TEST(test_a_throw_that_lands_before_the_host_hears_of_it);
    return UNITY_END();
}
