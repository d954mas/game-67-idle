#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "features/body_sync/body_sync_stand.h"
#include "stand_logic.h"

static int usage(void) {
    fprintf(stderr, "usage: body_sync_stand <scenario.scn> [--seed N] [--light-mass KG] [--authority host|interaction] [--flight-end contact|rest] [--out metrics.json] [--no-series] [--quiet]\n"
                    "       body_sync_stand --list-scenes\n");
    return 2;
}

static const body_sync_scene_entry_t *find_scene(const char *name, const body_sync_scene_entry_t *extra, uint32_t extra_count) {
    const body_sync_scene_entry_t *own = body_sync_scene_lookup(extra, extra_count, name);
    if (own) {
        return own;
    }
    uint32_t count = 0;
    const body_sync_scene_entry_t *catalog = body_sync_scene_catalog(&count);
    return body_sync_scene_lookup(catalog, count, name);
}

static bool parse_seed(const char *text, uint64_t *out) {
    uint64_t v = 0;
    if (!text || !*text) {
        return false;
    }
    for (; *text; text++) {
        if (*text < '0' || *text > '9') {
            return false;
        }
        uint64_t digit = (uint64_t)(*text - '0');
        if (v > (UINT64_MAX - digit) / 10u) {
            return false;
        }
        v = v * 10u + digit;
    }
    *out = v;
    return true;
}

void body_sync_stand_print_report(const body_sync_report_t *report, bool failures_only) {
    if (!failures_only) {
        printf("%s on %s, seed %llu, %u ticks, %s\n", report->scenario, report->scene, (unsigned long long)report->seed, report->ticks,
               report->implementation);
        for (uint32_t g = 0; g < report->guest_count; g++) {
            const body_sync_guest_metrics_t *m = &report->guests[g];
            double down = 0.0, up = 0.0;
            for (uint32_t k = 0; k < BODY_SYNC_KINDS_ALL; k++) {
                down += m->to_guest.mean_bytes_per_s[k];
                up += m->to_host.mean_bytes_per_s[k];
            }
            printf("  g%u: divergence p95 %.3g m max %.3g m, jerk p95 %.3g m, %.0f B/s down, %.0f B/s up\n", (unsigned)m->seat,
                   m->divergence_p95_m, m->divergence_max_m, m->jerk_p95_m, down, up);
        }
    }
    for (uint32_t i = 0; i < report->check_count; i++) {
        const body_sync_check_metrics_t *c = &report->checks[i];
        if (!failures_only || !c->passed) {
            printf("  %s line %u %s: %.4g (limit %.4g) %s\n", c->passed ? "PASS" : "FAIL", c->line, stand_expect_name(c->kind), c->value, c->limit,
                   c->detail);
        }
    }
}

int body_sync_stand_main(int argc, char **argv, const body_sync_scene_entry_t *extra, uint32_t extra_count) {
    const char *scenario_path = NULL;
    const char *out_path = NULL;
    bool quiet = false, series = true, override_seed = false, override_light = false, override_authority = false, override_flight_end = false;
    uint8_t authority = BODY_SYNC_AUTHORITY_HOST, flight_end = BODY_SYNC_FLIGHT_END_CONTACT;
    uint64_t seed = 0;
    float light = 0.0f;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--list-scenes") == 0) {
            uint32_t count = 0;
            const body_sync_scene_entry_t *catalog = body_sync_scene_catalog(&count);
            for (uint32_t k = 0; k < count; k++) {
                if (!body_sync_scene_lookup(extra, extra_count, catalog[k].name)) {
                    printf("%-16s %s\n", catalog[k].name, catalog[k].summary);
                }
            }
            for (uint32_t k = 0; k < extra_count; k++) {
                printf("%-16s %s\n", extra[k].name, extra[k].summary ? extra[k].summary : "");
            }
            return 0;
        } else if (strcmp(arg, "--seed") == 0 && i + 1 < argc) {
            if (!parse_seed(argv[++i], &seed)) {
                return usage();
            }
            override_seed = true;
        } else if (strcmp(arg, "--light-mass") == 0 && i + 1 < argc) {
            char *end = NULL;
            double kg = strtod(argv[++i], &end);
            if (!end || *end || !(kg >= 0.0 && kg <= 1000.0)) {
                return usage();
            }
            light = (float)kg;
            override_light = true;
        } else if (strcmp(arg, "--authority") == 0 && i + 1 < argc) {
            const char *mode = argv[++i];
            if (strcmp(mode, "host") != 0 && strcmp(mode, "interaction") != 0) {
                return usage();
            }
            authority = strcmp(mode, "interaction") == 0 ? BODY_SYNC_AUTHORITY_INTERACTION : BODY_SYNC_AUTHORITY_HOST;
            override_authority = true;
        } else if (strcmp(arg, "--flight-end") == 0 && i + 1 < argc) {
            const char *end = argv[++i];
            if (strcmp(end, "contact") != 0 && strcmp(end, "rest") != 0) {
                return usage();
            }
            flight_end = strcmp(end, "rest") == 0 ? BODY_SYNC_FLIGHT_END_REST : BODY_SYNC_FLIGHT_END_CONTACT;
            override_flight_end = true;
        } else if (strcmp(arg, "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (strcmp(arg, "--no-series") == 0) {
            series = false;
        } else if (strcmp(arg, "--quiet") == 0) {
            quiet = true;
        } else if (arg[0] == '-' || scenario_path) {
            return usage();
        } else {
            scenario_path = arg;
        }
    }
    if (!scenario_path) {
        return usage();
    }
    int status = 2;
    body_sync_scenario_t *scenario = malloc(sizeof(*scenario));
    body_sync_scene_t *scene = malloc(sizeof(*scene));
    body_sync_stand_t *stand = NULL;
    body_sync_error_t error;
    if (!scenario || !scene) {
        fprintf(stderr, "out of memory\n");
        goto done;
    }
    if (!body_sync_scenario_load(scenario_path, scenario, &error)) {
        fprintf(stderr, "%s:%u: %s\n", scenario_path, error.line, error.text);
        goto done;
    }
    const body_sync_scene_entry_t *entry = find_scene(scenario->scene, extra, extra_count);
    if (!entry) {
        fprintf(stderr, "%s: no scene '%s' (see --list-scenes)\n", scenario_path, scenario->scene);
        goto done;
    }
    entry->build(scene);
    body_sync_stand_config_t config = {.scene = scene, .scenario = scenario, .override_seed = override_seed, .seed = seed, .series = series,
                                       .override_light_mass = override_light, .light_mass_max = light,
                                       .override_authority = override_authority, .authority = authority,
                                       .override_flight_end = override_flight_end, .flight_end = flight_end};
    stand = body_sync_stand_create(&config, &error);
    if (!stand) {
        /* Line 0 is a refusal of the scene, which is C data, not of a scenario line. */
        if (error.line) {
            fprintf(stderr, "%s:%u: %s\n", scenario_path, error.line, error.text);
        } else {
            fprintf(stderr, "%s: scene %s: %s\n", scenario_path, scenario->scene, error.text);
        }
        goto done;
    }
    const body_sync_report_t *report = body_sync_stand_run(stand);
    body_sync_stand_print_report(report, quiet);
    char default_path[BODY_SYNC_NAME_MAX + 16];
    if (!out_path) {
        snprintf(default_path, sizeof(default_path), "%s.json", report->scenario);
        out_path = default_path;
    }
    if (!body_sync_stand_save_json(stand, out_path, &error)) {
        fprintf(stderr, "%s: %s\n", out_path, error.text);
        goto done;
    }
    status = report->passed ? 0 : 1;
done:
    body_sync_stand_destroy(stand);
    free(scene);
    free(scenario);
    return status;
}
