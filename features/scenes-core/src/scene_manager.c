#include "features/scenes/scene_manager.h"
#include "scene_id.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifndef SCENE_MANAGER_ASSERT
#ifdef NDEBUG
#define SCENE_MANAGER_ASSERT(condition) \
    do {                                \
        if (!(condition)) {             \
            abort();                    \
        }                               \
    } while (0)
#else
#define SCENE_MANAGER_ASSERT(condition) assert(condition)
#endif
#endif

typedef struct scene_record {
    scene_residency_t residency;
    bool presented;
    bool preload_requested;
    uint64_t load_start_step;
} scene_record_t;

typedef struct scene_entry {
    uint8_t scene_index;
    uint8_t args_size;
    unsigned char args[SCENE_ROUTE_ARGS_INLINE_CAPACITY];
} scene_entry_t;

typedef enum command_phase {
    COMMAND_PHASE_NONE,
    COMMAND_PHASE_PENDING,
    COMMAND_PHASE_WAIT_READY,
    COMMAND_PHASE_EXIT,
    COMMAND_PHASE_RECREATE_UNLOAD,
    COMMAND_PHASE_WAIT_RECREATE_READY,
    COMMAND_PHASE_APPLY,
    COMMAND_PHASE_ENTER,
    COMMAND_PHASE_CLEANUP
} command_phase_t;

typedef struct scene_command {
    scene_operation_id_t id;
    scene_operation_kind_t kind;
    command_phase_t phase;
    uint8_t target_index;
    const char *target_id;
    bool recreates_focus;
    bool transition_begun;
    bool old_focus_changes;
    bool new_focus_changes;
    uint64_t transition_start_step;
    size_t navigation_value;
    scene_entry_t target_entry;
    size_t candidate_count;
    scene_entry_t candidate[SCENE_MANAGER_MAX_HISTORY];
    uint8_t old_visible[2];
    size_t old_visible_count;
    uint8_t new_visible[2];
    size_t new_visible_count;
} scene_command_t;

/* Public fixed storage has byte-array type; make the private overlay alias-safe. */
#if defined(__GNUC__) || defined(__clang__)
#define SCENE_MANAGER_MAY_ALIAS __attribute__((__may_alias__))
#elif defined(_MSC_VER)
/* MSVC C does not apply GCC-style strict-aliasing TBAA to this overlay. */
#define SCENE_MANAGER_MAY_ALIAS
#else
#error "scenes-core opaque storage requires Clang, GCC, or MSVC"
#endif

typedef struct SCENE_MANAGER_MAY_ALIAS scene_manager_impl {
    const scene_descriptor_t *catalog;
    size_t scene_count;
    scene_record_t records[SCENE_MANAGER_MAX_SCENES];
    scene_entry_t history[SCENE_MANAGER_MAX_HISTORY];
    size_t history_count;
    scene_command_t command;
    scene_operation_id_t next_operation_id;
    scene_operation_status_t last_completed;
    uint64_t current_frame_index;
    uint64_t step_index;
    bool has_last_completed;
    bool initialized;
    bool shutting_down;
    unsigned dispatch_depth;
    unsigned consumer_dispatch_depth;
} scene_manager_impl_t;

#undef SCENE_MANAGER_MAY_ALIAS

_Static_assert(sizeof(scene_manager_impl_t) <= SCENE_MANAGER_STORAGE_BYTES,
               "SCENE_MANAGER_STORAGE_BYTES is too small");
_Static_assert(_Alignof(scene_manager_impl_t) <= _Alignof(scene_manager_t),
               "scene_manager_t alignment is too small");

static scene_manager_impl_t *impl(scene_manager_t *manager) {
    return (scene_manager_impl_t *)(void *)manager->_storage;
}

static const scene_manager_impl_t *cimpl(const scene_manager_t *manager) {
    return (const scene_manager_impl_t *)(const void *)manager->_storage;
}

static void require_manager(const scene_manager_impl_t *m) {
    SCENE_MANAGER_ASSERT(m != NULL && m->initialized && !m->shutting_down);
}

static size_t find_index(const scene_manager_impl_t *m, const char *id) {
    size_t i;
    if (id == NULL) {
        return SIZE_MAX;
    }
    for (i = 0; i < m->scene_count; ++i) {
        if (strcmp(m->catalog[i].id, id) == 0) {
            return i;
        }
    }
    return SIZE_MAX;
}

static size_t descriptor_index(const scene_manager_impl_t *m,
                               const scene_descriptor_t *scene) {
    size_t index;
    for (index = 0; index < m->scene_count; ++index) {
        if (&m->catalog[index] == scene) {
            return index;
        }
    }
    SCENE_MANAGER_ASSERT(false);
    return 0;
}

static void validate_args(const scene_descriptor_t *scene,
                          scene_route_args_view_t args) {
    SCENE_MANAGER_ASSERT(args.size == scene->route_args_size);
    SCENE_MANAGER_ASSERT((args.size == 0) == (args.data == NULL));
}

static scene_entry_t make_entry(size_t scene_index,
                                scene_route_args_view_t args) {
    scene_entry_t entry;
    memset(&entry, 0, sizeof entry);
    SCENE_MANAGER_ASSERT(scene_index < SCENE_MANAGER_MAX_SCENES);
    entry.scene_index = (uint8_t)scene_index;
    entry.args_size = args.size;
    if (args.size > 0) {
        memcpy(entry.args, args.data, args.size);
    }
    return entry;
}

static bool command_busy(const scene_manager_impl_t *m) {
    return m->command.phase != COMMAND_PHASE_NONE;
}

static scene_result_t reject_busy(const scene_manager_impl_t *m,
                                  scene_operation_id_t *blocking_id) {
    if (!command_busy(m)) {
        return SCENE_RESULT_ACCEPTED;
    }
    if (blocking_id != NULL) {
        *blocking_id = m->command.id;
    }
    return SCENE_RESULT_BUSY;
}

static scene_result_t accept(scene_manager_impl_t *m,
                             scene_operation_kind_t kind,
                             size_t target_index,
                             const char *target_id,
                             scene_operation_id_t *operation_id,
                             scene_operation_id_t *blocking_id) {
    const scene_result_t busy = reject_busy(m, blocking_id);
    if (busy == SCENE_RESULT_BUSY) {
        return busy;
    }
    SCENE_MANAGER_ASSERT(m->dispatch_depth == 0);
    SCENE_MANAGER_ASSERT(
        m->next_operation_id < SCENE_OPERATION_ID_MAX);
    memset(&m->command, 0, sizeof m->command);
    m->command.id = ++m->next_operation_id;
    m->command.kind = kind;
    m->command.phase = COMMAND_PHASE_PENDING;
    m->command.target_index = target_index < SCENE_MANAGER_MAX_SCENES
                                  ? (uint8_t)target_index
                                  : UINT8_MAX;
    m->command.target_id =
        target_index < m->scene_count ? m->catalog[target_index].id
                                      : target_id;
    if (operation_id != NULL) {
        *operation_id = m->command.id;
    }
    return SCENE_RESULT_ACCEPTED;
}

static void copy_current_history(scene_manager_impl_t *m) {
    m->command.candidate_count = m->history_count;
    if (m->history_count > 0) {
        memcpy(m->command.candidate, m->history,
               m->history_count * sizeof m->history[0]);
    }
}

static void project_visible(const scene_manager_impl_t *m,
                            const scene_entry_t *history,
                            size_t history_count,
                            uint8_t visible[2],
                            size_t *visible_count) {
    size_t screen_position;
    if (history_count == 0) {
        *visible_count = 0;
        return;
    }
    if (m->catalog[history[history_count - 1].scene_index].kind ==
        SCENE_KIND_SCREEN) {
        visible[0] = history[history_count - 1].scene_index;
        *visible_count = 1;
        return;
    }
    screen_position = history_count - 1;
    while (screen_position > 0 &&
           m->catalog[history[screen_position].scene_index].kind ==
               SCENE_KIND_MODAL) {
        --screen_position;
    }
    SCENE_MANAGER_ASSERT(
        m->catalog[history[screen_position].scene_index].kind ==
        SCENE_KIND_SCREEN);
    visible[0] = history[screen_position].scene_index;
    visible[1] = history[history_count - 1].scene_index;
    *visible_count = visible[0] == visible[1] ? 1 : 2;
}

static bool visible_has(const uint8_t visible[2],
                        size_t visible_count,
                        uint8_t scene_index) {
    size_t i;
    for (i = 0; i < visible_count; ++i) {
        if (visible[i] == scene_index) {
            return true;
        }
    }
    return false;
}

static size_t visible_occurrence_position(
    const scene_manager_impl_t *m,
    const scene_entry_t *history,
    size_t history_count,
    uint8_t scene_index) {
    size_t i = history_count;
    (void)m;
    while (i > 0) {
        --i;
        if (history[i].scene_index == scene_index) {
            return i;
        }
    }
    return SIZE_MAX;
}

static void dispatch_begin(scene_manager_impl_t *m) {
    ++m->dispatch_depth;
}

static void dispatch_end(scene_manager_impl_t *m) {
    SCENE_MANAGER_ASSERT(m->dispatch_depth > 0);
    --m->dispatch_depth;
}

static void consumer_dispatch_begin(scene_manager_impl_t *m) {
    SCENE_MANAGER_ASSERT(m->consumer_dispatch_depth == 0);
    ++m->consumer_dispatch_depth;
}

static void consumer_dispatch_end(scene_manager_impl_t *m) {
    SCENE_MANAGER_ASSERT(m->consumer_dispatch_depth == 1);
    --m->consumer_dispatch_depth;
}

static void ensure_loading(scene_manager_impl_t *m, uint8_t index) {
    scene_record_t *record = &m->records[index];
    const scene_descriptor_t *scene = &m->catalog[index];
    if (record->residency != SCENE_RESIDENCY_UNLOADED) {
        return;
    }
    record->residency = SCENE_RESIDENCY_LOADING;
    record->preload_requested = false;
    record->load_start_step = m->step_index;
    if (scene->api->load_begin != NULL) {
        dispatch_begin(m);
        scene->api->load_begin(scene->instance);
        dispatch_end(m);
    }
}

static bool required_ready(const scene_manager_impl_t *m) {
    size_t i;
    for (i = 0; i < m->command.new_visible_count; ++i) {
        const uint8_t index = m->command.new_visible[i];
        if (!visible_has(m->command.old_visible,
                         m->command.old_visible_count, index) &&
            m->records[index].residency != SCENE_RESIDENCY_READY) {
            return false;
        }
    }
    return true;
}

static bool screen_update_eligible(const scene_manager_impl_t *m,
                                   const scene_entry_t *history,
                                   size_t count,
                                   uint8_t screen_index) {
    size_t i;
    bool found = false;
    for (i = 0; i < count; ++i) {
        if (history[i].scene_index == screen_index &&
            m->catalog[screen_index].kind == SCENE_KIND_SCREEN) {
            found = true;
            continue;
        }
        if (found && m->catalog[history[i].scene_index].kind ==
                         SCENE_KIND_MODAL &&
            m->catalog[history[i].scene_index].modal_update_policy ==
                SCENE_MODAL_PAUSE_BELOW) {
            return false;
        }
    }
    return found;
}

static bool step_transition(scene_manager_impl_t *m,
                            uint8_t index,
                            scene_transition_direction_t direction,
                            float dt) {
    const scene_descriptor_t *scene = &m->catalog[index];
    if (scene->transitions == NULL) {
        return true;
    }
    if (!m->command.transition_begun) {
        m->command.transition_begun = true;
        m->command.transition_start_step = m->step_index;
        if (scene->transitions->begin != NULL) {
            dispatch_begin(m);
            scene->transitions->begin(scene->instance, direction);
            dispatch_end(m);
        }
    }
    if (scene->transitions->step == NULL) {
        return true;
    }
    SCENE_MANAGER_ASSERT(
        m->step_index >= m->command.transition_start_step &&
        m->step_index - m->command.transition_start_step <=
            SCENE_MANAGER_TRANSITION_DEADLINE_STEPS);
    dispatch_begin(m);
    const scene_transition_result_t result =
        scene->transitions->step(scene->instance, dt);
    dispatch_end(m);
    SCENE_MANAGER_ASSERT(result == SCENE_TRANSITION_PENDING ||
                         result == SCENE_TRANSITION_DONE);
    return result == SCENE_TRANSITION_DONE;
}

static void build_candidate(scene_manager_impl_t *m) {
    scene_command_t *command = &m->command;
    size_t target = command->target_index;
    size_t i;

    copy_current_history(m);
    switch (command->kind) {
    case SCENE_OPERATION_START:
        SCENE_MANAGER_ASSERT(m->history_count == 0);
        SCENE_MANAGER_ASSERT(m->catalog[target].kind == SCENE_KIND_SCREEN);
        command->candidate_count = 1;
        command->candidate[0] = command->target_entry;
        break;
    case SCENE_OPERATION_SHOW:
        if (m->catalog[target].kind == SCENE_KIND_SCREEN) {
            while (command->candidate_count > 0 &&
                   m->catalog[command->candidate[command->candidate_count - 1]
                                  .scene_index]
                           .kind == SCENE_KIND_MODAL) {
                --command->candidate_count;
            }
            if (m->catalog[target].route_args_size == 0 &&
                command->candidate_count > 0 &&
                command->candidate[command->candidate_count - 1].scene_index ==
                    target) {
                break;
            }
        }
        SCENE_MANAGER_ASSERT(command->candidate_count <
                             SCENE_MANAGER_MAX_HISTORY);
        command->candidate[command->candidate_count++] =
            command->target_entry;
        break;
    case SCENE_OPERATION_REPLACE:
        SCENE_MANAGER_ASSERT(m->history_count > 0);
        SCENE_MANAGER_ASSERT(
            m->catalog[m->history[m->history_count - 1].scene_index].kind ==
            m->catalog[target].kind);
        command->candidate_count = m->history_count;
        command->recreates_focus =
            m->history[m->history_count - 1].scene_index == target;
        command->candidate[command->candidate_count - 1] =
            command->target_entry;
        break;
    case SCENE_OPERATION_RELOAD:
        SCENE_MANAGER_ASSERT(m->history_count > 0);
        command->recreates_focus = true;
        break;
    case SCENE_OPERATION_BACK:
        SCENE_MANAGER_ASSERT(command->navigation_value <
                             command->candidate_count);
        command->candidate_count -= command->navigation_value;
        break;
    case SCENE_OPERATION_BACK_TO:
    case SCENE_OPERATION_CLOSE_MODALS:
        SCENE_MANAGER_ASSERT(command->navigation_value > 0 &&
                             command->navigation_value <=
                                 command->candidate_count);
        command->candidate_count = command->navigation_value;
        break;
    }

    project_visible(m, m->history, m->history_count, command->old_visible,
                    &command->old_visible_count);
    project_visible(m, command->candidate, command->candidate_count,
                    command->new_visible, &command->new_visible_count);
    command->old_focus_changes = false;
    command->new_focus_changes = false;
    if (command->old_visible_count > 0) {
        const uint8_t old_focus =
            command->old_visible[command->old_visible_count - 1];
        command->old_focus_changes =
            command->recreates_focus ||
            !visible_has(command->new_visible,
                         command->new_visible_count, old_focus) ||
            visible_occurrence_position(
                m, m->history, m->history_count, old_focus) !=
                visible_occurrence_position(
                    m, command->candidate,
                    command->candidate_count, old_focus);
    }
    if (command->new_visible_count > 0) {
        const uint8_t new_focus =
            command->new_visible[command->new_visible_count - 1];
        command->new_focus_changes =
            command->recreates_focus ||
            !visible_has(command->old_visible,
                         command->old_visible_count, new_focus) ||
            visible_occurrence_position(
                m, m->history, m->history_count, new_focus) !=
                visible_occurrence_position(
                    m, command->candidate,
                    command->candidate_count, new_focus);
    }
    for (i = 0; i < command->new_visible_count; ++i) {
        const uint8_t index = command->new_visible[i];
        if (!visible_has(command->old_visible, command->old_visible_count,
                         index)) {
            ensure_loading(m, index);
        }
    }
}

static void activate_command(scene_manager_impl_t *m) {
    scene_command_t *command = &m->command;
    SCENE_MANAGER_ASSERT(command->phase == COMMAND_PHASE_PENDING);
    build_candidate(m);
    command->phase = command->recreates_focus ? COMMAND_PHASE_EXIT
                                               : COMMAND_PHASE_WAIT_READY;
}

static void hide_old(scene_manager_impl_t *m) {
    scene_command_t *command = &m->command;
    size_t i = command->old_visible_count;
    while (i > 0) {
        const uint8_t index = command->old_visible[--i];
        const size_t old_pos = visible_occurrence_position(
            m, m->history, m->history_count, index);
        const size_t new_pos = visible_occurrence_position(
            m, command->candidate, command->candidate_count, index);
        const bool reactivated =
            visible_has(command->new_visible, command->new_visible_count,
                        index) &&
            old_pos != new_pos &&
            ((command->old_visible_count > 0 &&
              index == command->old_visible[
                           command->old_visible_count - 1]) ||
             (command->new_visible_count > 0 &&
              index == command->new_visible[
                           command->new_visible_count - 1]));
        const bool leaving =
            !visible_has(command->new_visible, command->new_visible_count,
                         index);
        const bool recreated =
            command->recreates_focus &&
            index == m->history[m->history_count - 1].scene_index;
        if ((leaving || reactivated || recreated) &&
            m->records[index].presented) {
            const scene_descriptor_t *scene = &m->catalog[index];
            m->records[index].presented = false;
            if (scene->api->on_hide != NULL) {
                dispatch_begin(m);
                scene->api->on_hide(scene->instance);
                dispatch_end(m);
            }
        }
    }
}

static void apply_presentation(scene_manager_impl_t *m) {
    scene_command_t *command = &m->command;
    bool old_screen_eligible = false;
    bool new_screen_eligible = false;
    uint8_t staying_screen = UINT8_MAX;
    bool should_show[2] = {false, false};
    size_t i;

    if (command->old_visible_count > 0 &&
        m->catalog[command->old_visible[0]].kind == SCENE_KIND_SCREEN &&
        visible_has(command->new_visible, command->new_visible_count,
                    command->old_visible[0])) {
        staying_screen = command->old_visible[0];
        old_screen_eligible =
            screen_update_eligible(m, m->history, m->history_count,
                                   staying_screen);
        new_screen_eligible =
            screen_update_eligible(m, command->candidate,
                                   command->candidate_count, staying_screen);
    }

    for (i = 0; i < command->new_visible_count; ++i) {
        const uint8_t index = command->new_visible[i];
        const size_t old_pos = visible_occurrence_position(
            m, m->history, m->history_count, index);
        const size_t new_pos = visible_occurrence_position(
            m, command->candidate, command->candidate_count, index);
        const bool entering =
            !visible_has(command->old_visible, command->old_visible_count,
                         index);
        const bool reactivated =
            visible_has(command->old_visible, command->old_visible_count,
                        index) &&
            old_pos != new_pos &&
            index == command->new_visible[command->new_visible_count - 1];
        const bool recreated =
            command->recreates_focus && command->candidate_count > 0 &&
            index ==
                command->candidate[command->candidate_count - 1].scene_index;
        should_show[i] =
            entering || reactivated || recreated ||
            !m->records[index].presented;
    }

    hide_old(m);
    m->history_count = command->candidate_count;
    if (m->history_count > 0) {
        memcpy(m->history, command->candidate,
               m->history_count * sizeof m->history[0]);
    }

    if (staying_screen != UINT8_MAX &&
        old_screen_eligible != new_screen_eligible &&
        m->records[staying_screen].presented) {
        const scene_descriptor_t *screen = &m->catalog[staying_screen];
        void (*callback)(void *) =
            new_screen_eligible ? screen->api->on_resume
                                : screen->api->on_pause;
        if (callback != NULL) {
            dispatch_begin(m);
            callback(screen->instance);
            dispatch_end(m);
        }
    }

    for (i = 0; i < command->new_visible_count; ++i) {
        const uint8_t index = command->new_visible[i];
        const size_t new_pos = visible_occurrence_position(
            m, m->history, m->history_count, index);
        if (should_show[i] &&
            m->records[index].residency == SCENE_RESIDENCY_READY) {
            const scene_descriptor_t *scene = &m->catalog[index];
            const scene_entry_t *entry = &m->history[new_pos];
            const scene_route_args_view_t args = {
                .data = entry->args_size > 0 ? entry->args : NULL,
                .size = entry->args_size,
            };
            if (scene->api->on_show != NULL) {
                dispatch_begin(m);
                scene->api->on_show(scene->instance, args);
                dispatch_end(m);
            }
            m->records[index].presented = true;
        }
    }
}

static void cleanup_leaving(scene_manager_impl_t *m) {
    scene_command_t *command = &m->command;
    size_t i;
    for (i = 0; i < command->old_visible_count; ++i) {
        const uint8_t index = command->old_visible[i];
        scene_record_t *record = &m->records[index];
        const scene_descriptor_t *scene = &m->catalog[index];
        if (!visible_has(command->new_visible, command->new_visible_count,
                         index) &&
            !scene->keep_loaded &&
            record->residency != SCENE_RESIDENCY_UNLOADED) {
            if (scene->api->unload != NULL) {
                dispatch_begin(m);
                scene->api->unload(scene->instance);
                dispatch_end(m);
            }
            record->residency = SCENE_RESIDENCY_UNLOADED;
            record->preload_requested = false;
        }
    }
}

static void complete_command(scene_manager_impl_t *m) {
    m->last_completed.id = m->command.id;
    m->last_completed.kind = m->command.kind;
    m->last_completed.state = SCENE_OPERATION_COMPLETED;
    m->last_completed.target_scene_id = m->command.target_id;
    m->has_last_completed = true;
    memset(&m->command, 0, sizeof m->command);
}

void scene_manager_init(scene_manager_t *manager,
                        const scene_descriptor_t *catalog,
                        size_t scene_count) {
    scene_manager_impl_t *m;
    size_t i;
    SCENE_MANAGER_ASSERT(manager != NULL);
    SCENE_MANAGER_ASSERT(catalog != NULL);
    SCENE_MANAGER_ASSERT(scene_count > 0 &&
                         scene_count <= SCENE_MANAGER_MAX_SCENES);
    memset(manager, 0, sizeof *manager);
    m = impl(manager);
    m->catalog = catalog;
    m->scene_count = scene_count;
    m->next_operation_id = 0;
    for (i = 0; i < scene_count; ++i) {
        const scene_descriptor_t *scene = &catalog[i];
        size_t j;
        SCENE_MANAGER_ASSERT(scene_id_is_valid(scene->id));
        SCENE_MANAGER_ASSERT(scene->api != NULL);
        SCENE_MANAGER_ASSERT(scene->kind == SCENE_KIND_SCREEN ||
                             scene->kind == SCENE_KIND_MODAL);
        SCENE_MANAGER_ASSERT(scene->route_args_size <=
                             SCENE_ROUTE_ARGS_INLINE_CAPACITY);
        SCENE_MANAGER_ASSERT(
            scene->modal_update_policy == SCENE_MODAL_PAUSE_BELOW ||
            scene->modal_update_policy == SCENE_MODAL_CONTINUE_BELOW);
        for (j = 0; j < i; ++j) {
            SCENE_MANAGER_ASSERT(strcmp(scene->id, catalog[j].id) != 0);
        }
    }
    m->initialized = true;
}

void scene_manager_shutdown(scene_manager_t *manager) {
    scene_manager_impl_t *m = impl(manager);
    size_t i;
    require_manager(m);
    SCENE_MANAGER_ASSERT(m->dispatch_depth == 0);
    SCENE_MANAGER_ASSERT(m->consumer_dispatch_depth == 0);
    m->shutting_down = true;
    for (i = m->scene_count; i > 0; --i) {
        const size_t index = i - 1;
        const scene_descriptor_t *scene = &m->catalog[index];
        if (m->records[index].presented) {
            m->records[index].presented = false;
            if (scene->api->on_hide != NULL) {
                scene->api->on_hide(scene->instance);
            }
        }
    }
    for (i = 0; i < m->scene_count; ++i) {
        const scene_descriptor_t *scene = &m->catalog[i];
        if (m->records[i].residency != SCENE_RESIDENCY_UNLOADED) {
            if (scene->api->unload != NULL) {
                scene->api->unload(scene->instance);
            }
            m->records[i].residency = SCENE_RESIDENCY_UNLOADED;
        }
        m->records[i].preload_requested = false;
    }
    m->history_count = 0;
    memset(&m->command, 0, sizeof m->command);
    m->initialized = false;
}

bool scene_manager_has_scene(const scene_manager_t *manager,
                             const char *scene_id) {
    const scene_manager_impl_t *m = cimpl(manager);
    require_manager(m);
    return find_index(m, scene_id) != SIZE_MAX;
}

const scene_descriptor_t *scene_manager_find_scene(
    const scene_manager_t *manager,
    const char *scene_id) {
    const scene_manager_impl_t *m = cimpl(manager);
    size_t index;
    require_manager(m);
    index = find_index(m, scene_id);
    return index == SIZE_MAX ? NULL : &m->catalog[index];
}

size_t scene_manager_scene_count(const scene_manager_t *manager) {
    const scene_manager_impl_t *m = cimpl(manager);
    require_manager(m);
    return m->scene_count;
}

const scene_descriptor_t *scene_manager_catalog_scene(
    const scene_manager_t *manager,
    size_t index) {
    const scene_manager_impl_t *m = cimpl(manager);
    require_manager(m);
    SCENE_MANAGER_ASSERT(index < m->scene_count);
    return &m->catalog[index];
}

scene_residency_t scene_manager_scene_residency(
    const scene_manager_t *manager,
    const scene_descriptor_t *scene) {
    const scene_manager_impl_t *m = cimpl(manager);
    require_manager(m);
    return m->records[descriptor_index(m, scene)].residency;
}

size_t scene_manager_history_count(const scene_manager_t *manager) {
    const scene_manager_impl_t *m = cimpl(manager);
    require_manager(m);
    return m->history_count;
}

scene_history_entry_view_t scene_manager_history_entry(
    const scene_manager_t *manager,
    size_t index) {
    const scene_manager_impl_t *m = cimpl(manager);
    scene_history_entry_view_t view;
    require_manager(m);
    SCENE_MANAGER_ASSERT(index < m->history_count);
    view.scene = &m->catalog[m->history[index].scene_index];
    view.args.data =
        m->history[index].args_size > 0 ? m->history[index].args : NULL;
    view.args.size = m->history[index].args_size;
    return view;
}

scene_history_entry_view_t scene_manager_top(const scene_manager_t *manager) {
    const scene_manager_impl_t *m = cimpl(manager);
    require_manager(m);
    SCENE_MANAGER_ASSERT(m->history_count > 0);
    return scene_manager_history_entry(manager, m->history_count - 1);
}

bool scene_manager_contains(const scene_manager_t *manager,
                            const char *scene_id) {
    const scene_manager_impl_t *m = cimpl(manager);
    size_t index;
    size_t i;
    require_manager(m);
    index = find_index(m, scene_id);
    if (index == SIZE_MAX) {
        return false;
    }
    for (i = 0; i < m->history_count; ++i) {
        if (m->history[i].scene_index == index) {
            return true;
        }
    }
    return false;
}

bool scene_manager_is_presented(const scene_manager_t *manager,
                                const scene_descriptor_t *scene) {
    const scene_manager_impl_t *m = cimpl(manager);
    size_t index;
    uint8_t visible[2];
    size_t count;
    require_manager(m);
    index = descriptor_index(m, scene);
    project_visible(m, m->history, m->history_count, visible, &count);
    return m->records[index].presented &&
           m->records[index].residency == SCENE_RESIDENCY_READY &&
           visible_has(visible, count, (uint8_t)index);
}

scene_preload_result_t scene_manager_preload(scene_manager_t *manager,
                                             const char *scene_id) {
    scene_manager_impl_t *m = impl(manager);
    size_t index;
    require_manager(m);
    SCENE_MANAGER_ASSERT(m->dispatch_depth == 0);
    index = find_index(m, scene_id);
    SCENE_MANAGER_ASSERT(index != SIZE_MAX);
    if (m->records[index].residency == SCENE_RESIDENCY_READY) {
        return SCENE_PRELOAD_ALREADY_READY;
    }
    if (m->records[index].residency == SCENE_RESIDENCY_LOADING) {
        return SCENE_PRELOAD_ALREADY_LOADING;
    }
    if (m->records[index].preload_requested) {
        return SCENE_PRELOAD_ALREADY_SCHEDULED;
    }
    m->records[index].preload_requested = true;
    return SCENE_PRELOAD_SCHEDULED;
}

scene_result_t scene_manager_start(scene_manager_t *manager,
                                   const char *scene_id,
                                   scene_route_args_view_t args,
                                   scene_operation_id_t *operation_id,
                                   scene_operation_id_t *blocking_id) {
    scene_manager_impl_t *m = impl(manager);
    size_t index;
    scene_result_t result;
    require_manager(m);
    SCENE_MANAGER_ASSERT(m->dispatch_depth == 0);
    if (command_busy(m)) {
        return reject_busy(m, blocking_id);
    }
    SCENE_MANAGER_ASSERT(m->history_count == 0);
    index = find_index(m, scene_id);
    SCENE_MANAGER_ASSERT(index != SIZE_MAX);
    SCENE_MANAGER_ASSERT(m->catalog[index].kind == SCENE_KIND_SCREEN);
    validate_args(&m->catalog[index], args);
    result = accept(m, SCENE_OPERATION_START, index, scene_id, operation_id,
                    blocking_id);
    if (result == SCENE_RESULT_ACCEPTED) {
        m->command.target_entry = make_entry(index, args);
    }
    return result;
}

scene_result_t scene_manager_show(scene_manager_t *manager,
                                  const char *scene_id,
                                  scene_route_args_view_t args,
                                  scene_operation_id_t *operation_id,
                                  scene_operation_id_t *blocking_id) {
    scene_manager_impl_t *m = impl(manager);
    size_t index;
    scene_result_t result;
    require_manager(m);
    SCENE_MANAGER_ASSERT(m->dispatch_depth == 0);
    if (command_busy(m)) {
        return reject_busy(m, blocking_id);
    }
    SCENE_MANAGER_ASSERT(m->history_count > 0);
    index = find_index(m, scene_id);
    SCENE_MANAGER_ASSERT(index != SIZE_MAX);
    validate_args(&m->catalog[index], args);
    if (m->catalog[index].route_args_size == 0 &&
        m->history[m->history_count - 1].scene_index == index) {
        return SCENE_RESULT_ALREADY_TOP;
    }
    result = accept(m, SCENE_OPERATION_SHOW, index, scene_id, operation_id,
                    blocking_id);
    if (result == SCENE_RESULT_ACCEPTED) {
        m->command.target_entry = make_entry(index, args);
    }
    return result;
}

scene_result_t scene_manager_replace(
    scene_manager_t *manager,
    const char *scene_id,
    scene_route_args_view_t args,
    scene_operation_id_t *operation_id,
    scene_operation_id_t *blocking_id) {
    scene_manager_impl_t *m = impl(manager);
    size_t index;
    scene_result_t result;
    require_manager(m);
    SCENE_MANAGER_ASSERT(m->dispatch_depth == 0);
    if (command_busy(m)) {
        return reject_busy(m, blocking_id);
    }
    SCENE_MANAGER_ASSERT(m->history_count > 0);
    index = find_index(m, scene_id);
    SCENE_MANAGER_ASSERT(index != SIZE_MAX);
    validate_args(&m->catalog[index], args);
    SCENE_MANAGER_ASSERT(
        m->catalog[index].kind ==
        m->catalog[m->history[m->history_count - 1].scene_index].kind);
    result = accept(m, SCENE_OPERATION_REPLACE, index, scene_id, operation_id,
                    blocking_id);
    if (result == SCENE_RESULT_ACCEPTED) {
        m->command.target_entry = make_entry(index, args);
    }
    return result;
}

scene_result_t scene_manager_reload(
    scene_manager_t *manager,
    scene_operation_id_t *operation_id,
    scene_operation_id_t *blocking_id) {
    scene_manager_impl_t *m = impl(manager);
    require_manager(m);
    SCENE_MANAGER_ASSERT(m->dispatch_depth == 0);
    if (command_busy(m)) {
        return reject_busy(m, blocking_id);
    }
    SCENE_MANAGER_ASSERT(m->history_count > 0);
    return accept(
        m, SCENE_OPERATION_RELOAD,
        m->history[m->history_count - 1].scene_index,
        m->catalog[m->history[m->history_count - 1].scene_index].id,
        operation_id, blocking_id);
}

scene_result_t scene_manager_back(scene_manager_t *manager,
                                  size_t count,
                                  scene_operation_id_t *operation_id,
                                  scene_operation_id_t *blocking_id) {
    scene_manager_impl_t *m = impl(manager);
    scene_result_t result;
    require_manager(m);
    SCENE_MANAGER_ASSERT(m->dispatch_depth == 0);
    if (command_busy(m)) {
        return reject_busy(m, blocking_id);
    }
    SCENE_MANAGER_ASSERT(count > 0);
    if (count >= m->history_count) {
        return SCENE_RESULT_ROOT_PROTECTED;
    }
    result = accept(m, SCENE_OPERATION_BACK, SIZE_MAX, NULL, operation_id,
                    blocking_id);
    if (result == SCENE_RESULT_ACCEPTED) {
        m->command.navigation_value = count;
    }
    return result;
}

scene_result_t scene_manager_back_to(
    scene_manager_t *manager,
    const char *scene_id,
    scene_operation_id_t *operation_id,
    scene_operation_id_t *blocking_id) {
    scene_manager_impl_t *m = impl(manager);
    size_t index;
    size_t i;
    scene_result_t result;
    require_manager(m);
    SCENE_MANAGER_ASSERT(m->dispatch_depth == 0);
    if (command_busy(m)) {
        return reject_busy(m, blocking_id);
    }
    index = find_index(m, scene_id);
    SCENE_MANAGER_ASSERT(index != SIZE_MAX);
    i = m->history_count;
    SCENE_MANAGER_ASSERT(i > 1);
    --i;
    while (i > 0 && m->history[i - 1].scene_index != index) {
        --i;
    }
    SCENE_MANAGER_ASSERT(i > 0);
    result = accept(m, SCENE_OPERATION_BACK_TO, index, scene_id, operation_id,
                    blocking_id);
    if (result == SCENE_RESULT_ACCEPTED) {
        m->command.navigation_value = i;
    }
    return result;
}

bool scene_manager_can_back_to(const scene_manager_t *manager,
                               const char *scene_id) {
    const scene_manager_impl_t *m = cimpl(manager);
    size_t index;
    size_t i;
    require_manager(m);
    index = find_index(m, scene_id);
    if (index == SIZE_MAX || m->history_count < 2) {
        return false;
    }
    for (i = 0; i + 1 < m->history_count; ++i) {
        if (m->history[i].scene_index == index) {
            return true;
        }
    }
    return false;
}

scene_result_t scene_manager_close_modals(
    scene_manager_t *manager,
    scene_operation_id_t *operation_id,
    scene_operation_id_t *blocking_id) {
    scene_manager_impl_t *m = impl(manager);
    size_t count;
    scene_result_t result;
    require_manager(m);
    SCENE_MANAGER_ASSERT(m->dispatch_depth == 0);
    if (command_busy(m)) {
        return reject_busy(m, blocking_id);
    }
    count = m->history_count;
    while (count > 0 &&
           m->catalog[m->history[count - 1].scene_index].kind ==
               SCENE_KIND_MODAL) {
        --count;
    }
    if (count == m->history_count) {
        return SCENE_RESULT_ALREADY_TOP;
    }
    result = accept(m, SCENE_OPERATION_CLOSE_MODALS, SIZE_MAX, NULL,
                    operation_id, blocking_id);
    if (result == SCENE_RESULT_ACCEPTED) {
        m->command.navigation_value = count;
    }
    return result;
}

void scene_manager_step(scene_manager_t *manager,
                        uint64_t frame_index,
                        float dt) {
    scene_manager_impl_t *m = impl(manager);
    scene_command_t *command;
    size_t i;
    require_manager(m);
    SCENE_MANAGER_ASSERT(m->dispatch_depth == 0);
    SCENE_MANAGER_ASSERT(m->consumer_dispatch_depth == 0);
    SCENE_MANAGER_ASSERT(frame_index >= m->current_frame_index);
    m->current_frame_index = frame_index;
    SCENE_MANAGER_ASSERT(m->step_index < UINT64_MAX);
    ++m->step_index;

    if (m->command.phase == COMMAND_PHASE_PENDING) {
        activate_command(m);
    }

    for (i = 0; i < m->scene_count; ++i) {
        if (m->records[i].preload_requested) {
            ensure_loading(m, (uint8_t)i);
        }
    }
    if (m->command.phase == COMMAND_PHASE_WAIT_RECREATE_READY) {
        ensure_loading(m, m->command.target_index);
    }
    for (i = 0; i < m->scene_count; ++i) {
        scene_record_t *record = &m->records[i];
        const scene_descriptor_t *scene = &m->catalog[i];
        if (record->residency == SCENE_RESIDENCY_LOADING) {
            scene_load_result_t result = SCENE_LOAD_READY;
            SCENE_MANAGER_ASSERT(
                m->step_index >= record->load_start_step &&
                m->step_index - record->load_start_step <=
                    SCENE_MANAGER_LOAD_DEADLINE_STEPS);
            if (scene->api->load_step != NULL) {
                dispatch_begin(m);
                result = scene->api->load_step(scene->instance);
                dispatch_end(m);
            }
            SCENE_MANAGER_ASSERT(result == SCENE_LOAD_PENDING ||
                                 result == SCENE_LOAD_READY);
            if (result == SCENE_LOAD_READY) {
                record->residency = SCENE_RESIDENCY_READY;
            }
        }
    }

    command = &m->command;
    if (command->phase == COMMAND_PHASE_NONE) {
        return;
    }
    if (command->phase == COMMAND_PHASE_WAIT_READY) {
        if (!required_ready(m)) {
            return;
        }
        command->phase = COMMAND_PHASE_EXIT;
    }
    if (command->phase == COMMAND_PHASE_EXIT) {
        if (command->old_visible_count > 0) {
            const uint8_t old_focus =
                command->old_visible[command->old_visible_count - 1];
            if (command->old_focus_changes &&
                !step_transition(m, old_focus, SCENE_TRANSITION_EXIT, dt)) {
                return;
            }
        }
        command->transition_begun = false;
        if (command->recreates_focus) {
            hide_old(m);
            command->phase = COMMAND_PHASE_RECREATE_UNLOAD;
        } else {
            command->phase = COMMAND_PHASE_APPLY;
        }
    }
    if (command->phase == COMMAND_PHASE_RECREATE_UNLOAD) {
        const uint8_t index = command->target_index;
        const scene_descriptor_t *scene = &m->catalog[index];
        if (m->records[index].residency != SCENE_RESIDENCY_UNLOADED) {
            if (scene->api->unload != NULL) {
                dispatch_begin(m);
                scene->api->unload(scene->instance);
                dispatch_end(m);
            }
            m->records[index].residency = SCENE_RESIDENCY_UNLOADED;
        }
        command->phase = COMMAND_PHASE_WAIT_RECREATE_READY;
        return;
    }
    if (command->phase == COMMAND_PHASE_WAIT_RECREATE_READY) {
        if (m->records[command->target_index].residency !=
            SCENE_RESIDENCY_READY) {
            return;
        }
        command->phase = COMMAND_PHASE_APPLY;
    }
    if (command->phase == COMMAND_PHASE_APPLY) {
        apply_presentation(m);
        command->phase = COMMAND_PHASE_ENTER;
    }
    if (command->phase == COMMAND_PHASE_ENTER) {
        if (command->new_visible_count > 0) {
            const uint8_t new_focus =
                command->new_visible[command->new_visible_count - 1];
            if (command->new_focus_changes &&
                !step_transition(m, new_focus, SCENE_TRANSITION_ENTER, dt)) {
                return;
            }
        }
        command->phase = COMMAND_PHASE_CLEANUP;
    }
    if (command->phase == COMMAND_PHASE_CLEANUP) {
        cleanup_leaving(m);
        complete_command(m);
    }
}

void scene_manager_update(scene_manager_t *manager, float dt) {
    scene_manager_impl_t *m = impl(manager);
    uint8_t visible[2];
    size_t count;
    size_t i;
    require_manager(m);
    SCENE_MANAGER_ASSERT(m->dispatch_depth == 0);
    SCENE_MANAGER_ASSERT(m->consumer_dispatch_depth == 0);
    project_visible(m, m->history, m->history_count, visible, &count);
    for (i = 0; i < count; ++i) {
        const uint8_t index = visible[i];
        const scene_descriptor_t *scene = &m->catalog[index];
        bool eligible = true;
        if (!m->records[index].presented ||
            m->records[index].residency != SCENE_RESIDENCY_READY) {
            continue;
        }
        if (scene->kind == SCENE_KIND_SCREEN) {
            eligible = screen_update_eligible(
                m, m->history, m->history_count, index);
        }
        if (eligible && scene->api->on_update != NULL) {
            consumer_dispatch_begin(m);
            scene->api->on_update(scene->instance, dt);
            consumer_dispatch_end(m);
        }
    }
}

void scene_manager_build_ui(scene_manager_t *manager, void *ui_context) {
    scene_manager_impl_t *m = impl(manager);
    uint8_t visible[2];
    size_t count;
    size_t i;
    require_manager(m);
    SCENE_MANAGER_ASSERT(m->dispatch_depth == 0);
    SCENE_MANAGER_ASSERT(m->consumer_dispatch_depth == 0);
    project_visible(m, m->history, m->history_count, visible, &count);
    for (i = 0; i < count; ++i) {
        const uint8_t index = visible[i];
        const scene_descriptor_t *scene = &m->catalog[index];
        scene_ui_mode_t mode;
        if (!m->records[index].presented ||
            m->records[index].residency != SCENE_RESIDENCY_READY ||
            scene->api->on_ui == NULL) {
            continue;
        }
        mode = command_busy(m) || i + 1 < count ? SCENE_UI_PASSIVE
                                                : SCENE_UI_INTERACTIVE;
        consumer_dispatch_begin(m);
        scene->api->on_ui(scene->instance, ui_context, mode);
        consumer_dispatch_end(m);
    }
}

bool scene_manager_can_process_input(
    const scene_manager_t *manager,
    const scene_descriptor_t *scene) {
    const scene_manager_impl_t *m = cimpl(manager);
    uint8_t visible[2];
    size_t count;
    size_t index;
    require_manager(m);
    if (command_busy(m) || m->history_count == 0) {
        return false;
    }
    index = descriptor_index(m, scene);
    project_visible(m, m->history, m->history_count, visible, &count);
    return count > 0 && visible[count - 1] == index &&
           m->records[index].presented &&
           m->records[index].residency == SCENE_RESIDENCY_READY;
}

bool scene_manager_input_gated(const scene_manager_t *manager) {
    const scene_manager_impl_t *m = cimpl(manager);
    require_manager(m);
    return command_busy(m);
}

scene_operation_state_t scene_manager_operation_status(
    const scene_manager_t *manager,
    scene_operation_id_t operation_id,
    scene_operation_status_t *status) {
    const scene_manager_impl_t *m = cimpl(manager);
    require_manager(m);
    if (command_busy(m) && m->command.id == operation_id) {
        if (status != NULL) {
            status->id = m->command.id;
            status->kind = m->command.kind;
            status->state = m->command.phase == COMMAND_PHASE_PENDING
                                ? SCENE_OPERATION_PENDING
                                : SCENE_OPERATION_ACTIVE;
            status->target_scene_id = m->command.target_id;
        }
        return m->command.phase == COMMAND_PHASE_PENDING
                   ? SCENE_OPERATION_PENDING
                   : SCENE_OPERATION_ACTIVE;
    }
    if (m->has_last_completed && m->last_completed.id == operation_id) {
        if (status != NULL) {
            *status = m->last_completed;
        }
        return SCENE_OPERATION_COMPLETED;
    }
    return SCENE_OPERATION_NOT_FOUND;
}

bool scene_manager_current_operation(const scene_manager_t *manager,
                                     scene_operation_status_t *status) {
    const scene_manager_impl_t *m = cimpl(manager);
    require_manager(m);
    if (!command_busy(m)) {
        return false;
    }
    (void)scene_manager_operation_status(manager, m->command.id, status);
    return true;
}

bool scene_manager_last_completed_operation(
    const scene_manager_t *manager,
    scene_operation_status_t *status) {
    const scene_manager_impl_t *m = cimpl(manager);
    require_manager(m);
    if (!m->has_last_completed) {
        return false;
    }
    if (status != NULL) {
        *status = m->last_completed;
    }
    return true;
}
