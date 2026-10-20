#include "online_room/memory_store.h"

#include <string.h>

static uint32_t account_slot(online_memory_store *memory, uint64_t account, bool create) {
    for (uint32_t index = 0U; index < ONLINE_MEMORY_STORE_ACCOUNTS; ++index) {
        if (memory->accounts[index] == account) { return index; }
    }
    for (uint32_t index = 0U; create && index < ONLINE_MEMORY_STORE_ACCOUNTS; ++index) {
        if (memory->accounts[index] == 0U) {
            memory->accounts[index] = account;
            memory->revs[index] = 0;
            memory->sizes[index] = 0U;
            memory->owners[index] = 0U;
            memory->writer_lives[index] = 0U;
            memory->writer_saves[index] = 0U;
            return index;
        }
    }
    return ONLINE_MEMORY_STORE_ACCOUNTS;
}

static uint32_t new_request(online_memory_store *memory, uint32_t life, uint64_t account) {
    const uint32_t id = ++memory->next_id;
    const uint32_t at = id % ONLINE_MEMORY_STORE_REQUESTS;
    memory->requests[at].id = id;
    memory->requests[at].life = life;
    memory->requests[at].status = ONLINE_STORE_PENDING;
    memory->requests[at].answer = (online_store_answer){0};
    memory->requests[at].load = false;
    memory->requests[at].save = false;
    memory->requests[at].account = account;
    memory->requests[at].size = 0U;
    return id;
}

// A load runs at once: the rev and the document as they stand together, and
// the loading room life owns the profile from then on.
static void run_load(online_memory_store *memory, uint32_t at) {
    memory->requests[at].load = false;
    const uint32_t slot = account_slot(memory, memory->requests[at].account, true);
    if (slot == ONLINE_MEMORY_STORE_ACCOUNTS) {
        memory->requests[at].status = ONLINE_STORE_FAILED;
        return;
    }
    memory->owners[slot] = memory->requests[at].life;
    memcpy(memory->requests[at].doc, memory->docs[slot], memory->sizes[slot]);
    memory->requests[at].size = memory->sizes[slot];
    memory->requests[at].answer = (online_store_answer){.size = memory->sizes[slot], .rev = memory->revs[slot]};
    memory->requests[at].status = ONLINE_STORE_DONE;
}

static uint32_t memory_load(void *user, uint64_t account) {
    const online_memory_life *life = user;
    online_memory_store *memory = life->memory;
    const uint32_t id = new_request(memory, life->life, account);
    const uint32_t at = id % ONLINE_MEMORY_STORE_REQUESTS;
    memory->requests[at].load = true;
    if (!memory->hold_loads) { run_load(memory, at); }
    return id;
}

static uint32_t memory_save(void *user, uint64_t account, int64_t if_rev, uint64_t save_id, const char *doc, size_t size) {
    const online_memory_life *life = user;
    online_memory_store *memory = life->memory;
    const uint32_t id = new_request(memory, life->life, account);
    const uint32_t at = id % ONLINE_MEMORY_STORE_REQUESTS;
    if (size == 0U || size > ONLINE_ROOM_PROFILE_TEXT_MAX || (memory->size_limit != 0U && size > memory->size_limit) || save_id == 0U ||
        if_rev < 0) {
        memory->requests[at].status = ONLINE_STORE_REFUSED;
        return id;
    }
    memory->requests[at].save = true;
    memory->requests[at].if_rev = if_rev;
    memory->requests[at].save_id = save_id;
    memory->requests[at].size = size;
    memcpy(memory->requests[at].doc, doc, size);
    return id;
}

static online_store_status verdict(online_memory_store *memory, uint32_t at, uint32_t slot) {
    const uint32_t life = memory->requests[at].life;
    const uint64_t save_id = memory->requests[at].save_id;
    const int64_t stored = memory->revs[slot];
    memory->requests[at].answer.rev = stored;
    // The save that wrote the stored rev, again: taken once, whoever owns the
    // profile now; the same id with another document is not that save.
    if (stored > 0 && memory->writer_lives[slot] == life && memory->writer_saves[slot] == save_id) {
        const bool same = memory->requests[at].size == memory->sizes[slot] &&
            memcmp(memory->requests[at].doc, memory->docs[slot], memory->sizes[slot]) == 0;
        if (!same) { memory->requests[at].answer.writer = save_id; }
        return same ? ONLINE_STORE_DONE : ONLINE_STORE_CONFLICT;
    }
    if (memory->owners[slot] != life) { return ONLINE_STORE_SUPERSEDED; }
    if (stored != memory->requests[at].if_rev) {
        memory->requests[at].answer.writer = memory->writer_lives[slot] == life ? memory->writer_saves[slot] : 0U;
        return ONLINE_STORE_CONFLICT;
    }
    memcpy(memory->docs[slot], memory->requests[at].doc, memory->requests[at].size);
    memory->sizes[slot] = memory->requests[at].size;
    memory->revs[slot] = stored + 1;
    memory->writer_lives[slot] = life;
    memory->writer_saves[slot] = save_id;
    memory->saves += 1U;
    memory->requests[at].answer.rev = memory->revs[slot];
    return ONLINE_STORE_DONE;
}

static void apply(online_memory_store *memory, uint32_t at) {
    memory->requests[at].save = false;
    const uint32_t slot = account_slot(memory, memory->requests[at].account, true);
    if (memory->fail_saves || slot == ONLINE_MEMORY_STORE_ACCOUNTS) {
        memory->requests[at].status = ONLINE_STORE_FAILED;
        return;
    }
    memory->requests[at].status = verdict(memory, at, slot);
    if (memory->lose_answers) {
        memory->requests[at].status = ONLINE_STORE_FAILED;
        memory->requests[at].answer = (online_store_answer){0};
    }
}

static online_store_status memory_poll(void *user, uint32_t request, char *doc, size_t cap, online_store_answer *answer) {
    online_memory_store *memory = ((const online_memory_life *)user)->memory;
    const uint32_t at = request % ONLINE_MEMORY_STORE_REQUESTS;
    if (request == 0U || memory->requests[at].id != request) { return ONLINE_STORE_FAILED; }
    if (memory->requests[at].load) {
        if (memory->hold_loads) { return ONLINE_STORE_PENDING; }
        run_load(memory, at);
    }
    if (memory->requests[at].save) {
        if (memory->hold_saves) { return ONLINE_STORE_PENDING; }
        apply(memory, at);
    }
    memory->requests[at].id = 0U;
    *answer = memory->requests[at].answer;
    // A load's answer carries the document; a save's carries only its rev.
    if (memory->requests[at].status == ONLINE_STORE_DONE && answer->size > 0U) {
        if (doc == NULL || answer->size > cap) { return ONLINE_STORE_FAILED; }
        memcpy(doc, memory->requests[at].doc, answer->size);
    }
    return memory->requests[at].status;
}

static void memory_cancel(void *user, uint32_t request) {
    online_memory_store *memory = ((const online_memory_life *)user)->memory;
    const uint32_t at = request % ONLINE_MEMORY_STORE_REQUESTS;
    if (request != 0U && memory->requests[at].id == request) { memory->requests[at].id = 0U; }
}

void online_memory_store_init(online_memory_store *memory) { memset(memory, 0, sizeof *memory); }

void online_memory_store_life(online_memory_store *memory, online_profile_store *out) {
    if (memory->life_count == ONLINE_MEMORY_STORE_LIVES) { __builtin_trap(); }
    online_memory_life *life = &memory->lives[memory->life_count];
    memory->life_count += 1U;
    *life = (online_memory_life){.memory = memory, .life = memory->life_count};
    *out = (online_profile_store){.user = life, .load = memory_load, .save = memory_save, .poll = memory_poll, .cancel = memory_cancel};
}

const char *online_memory_store_doc(const online_memory_store *memory, uint64_t account, size_t *size, int64_t *rev) {
    for (uint32_t index = 0U; index < ONLINE_MEMORY_STORE_ACCOUNTS; ++index) {
        if (memory->accounts[index] != account || memory->sizes[index] == 0U) { continue; }
        *size = memory->sizes[index];
        *rev = memory->revs[index];
        return memory->docs[index];
    }
    return NULL;
}

bool online_memory_store_put(online_memory_store *memory, uint64_t account, const char *doc, size_t size, int64_t rev) {
    const uint32_t slot = account_slot(memory, account, true);
    if (slot == ONLINE_MEMORY_STORE_ACCOUNTS || size > ONLINE_ROOM_PROFILE_TEXT_MAX) { return false; }
    memcpy(memory->docs[slot], doc, size);
    memory->sizes[slot] = size;
    memory->revs[slot] = rev;
    memory->owners[slot] = ONLINE_MEMORY_STORE_ELSEWHERE;
    memory->writer_lives[slot] = ONLINE_MEMORY_STORE_ELSEWHERE;
    memory->writer_saves[slot] = 0U;
    return true;
}
