#ifndef FEATURES_BODY_SYNC_BODY_SYNC_H
#define FEATURES_BODY_SYNC_BODY_SYNC_H

#include <stdbool.h>
#include <stdint.h>

#include "box3d/box3d.h"
#include "features/body_sync/body_sync_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The sync seam: one endpoint's Box3D bodies written to bytes and applied from bytes. It knows
   no transport, session, game or stand: the caller moves the bytes (coop-net in practice) and
   decides what may be grabbed and who wins a dispute.

   One body_sync_t per endpoint world. The caller owns the loop and calls, each tick, in this
   order: body_sync_set_peer_tick for each seat, body_sync_receive_state and
   body_sync_receive_record for what arrived, body_sync_apply, its own actions, b3World_Step,
   its own judging of the step's contacts (impacts of the bodies it simulates), body_sync_collect
   (which may end settled holds, so contacts are judged before it), then for each seat
   body_sync_write_state until it returns false and body_sync_peek_record / body_sync_pop_record
   until none is left, then the session's flush. Nothing calls back and nothing is allocated
   after body_sync_create.

   Which implementation is linked is a build choice: the stub (every bound body's raw pose,
   every tick, to every seat) or the real sender, receiver and authority. Both keep this
   contract, so the caller does not change when one replaces the other. How a copy is held (a
   kinematic body driven to its poses, or a dynamic one corrected toward them) and how a body's
   class is honoured (collision filters of copies, priorities) are the implementation's.

   Bytes from a peer are untrusted. A packet or record the seam cannot parse, or one whose
   values are out of range (outside the config's box and speeds included), is refused whole
   with BODY_SYNC_FORMAT: nothing of it is applied, the seam never asserts, never reads outside
   the bytes, and never hands Box3D a rotation it would refuse. */

typedef struct body_sync body_sync_t;

typedef struct body_sync_config {
    /* The endpoint's world. The seam reads and moves its bound bodies, never creates or
       destroys one, and never touches unbound bodies. */
    b3WorldId world;
    uint8_t seat;   /* this endpoint: BODY_SYNC_SEAT_HOST or a guest seat 1..seats-1 */
    uint32_t seats; /* 1..BODY_SYNC_SEATS_MAX, the host's included */
    uint32_t tick_hz; /* the fixed step, 1..1000 */
    /* The box every bound body stays in, metres, and the largest linear (m/s) and angular
       (rad/s) speeds: a pose or velocity from a peer outside them is FORMAT. Finite, min below
       max, speeds positive. body_sync_write_state never writes a value outside them: a
       position is clamped to the box, a velocity or spin scaled down to its limit, a velocity
       that is not a number sent as rest, and a body whose pose is not a number is not written
       at all; so an honest body never closes a session. A clamped pose reads like a true one:
       only the endpoint that simulates a body can tell it left the box, and it is the game
       there that notices and reports it (a guest to the host, which removes or returns the
       body). Every endpoint must create its seam with the same limits; the seam cannot tell,
       so the caller's handshake (its build key) is where a mismatch is caught. */
    float bounds_min[3];
    float bounds_max[3];
    float max_speed;
    float max_spin;
    /* Bytes a state packet may hold, before the capacity of each body_sync_write_state: zero
       takes the implementation's default (the real sender's is 1000). The stub ignores it. */
    uint32_t state_budget;
} body_sync_config_t;

/* NULL when the config is out of range, its limits are too wide for the implementation's wire
   (the real one: every coordinate of the box below 2048 m in magnitude, max_speed below
   262144 m/s, max_spin below 524288 rad/s), its state_budget has no room for one widest record
   per seat and one more (the real one's staleness bound needs a record after the characters,
   counted as one a seat: more characters a seat need a budget with room for them),
   or memory is short. All memory is taken here: the real seam about 1.25 MB, the stub about
   400 KB. */
body_sync_t *body_sync_create(const body_sync_config_t *config);
/* Leaves every bound body as it is; destroy the seam before its world. */
void body_sync_destroy(body_sync_t *sync);

/* The linked implementation and its version, for logs and metrics: "real 0.5.0" or
   "stub 0.5.0". */
const char *body_sync_implementation(void);

/* Text of the last INVALID or FORMAT this seam returned, "" before any. */
const char *body_sync_last_error(const body_sync_t *sync);

/* ---- seats ---- */

/* A seat that is not joined gets no state and no records, and its bytes are INVALID. The
   order of a seat's state ticks holds within one session of it: a seat that joins again counts
   anew, and what it sent before is forgotten. The seam relies on the session's order: nothing
   of a seat arrives after its leave, and its join comes before the new session's packets. */
void body_sync_seat_joined(body_sync_t *sync, uint8_t seat);
/* On the host every body under that seat's authority returns to the host at once (its number
   grows), starting from the newest state that seat sent: pose, velocity, motion type, flags,
   hold and holder (a carried body still names its carrier: dropping it is the caller's), and
   that seat's pending claims are dropped. On a guest the host's leave ends nothing here: the
   caller ends the run. */
void body_sync_seat_left(body_sync_t *sync, uint8_t seat);

/* ---- bodies ---- */

typedef struct body_sync_bind {
    b3BodyId body; /* dynamic or kinematic; static bodies are never bound */
    body_sync_class_t body_class;
    /* The seat that simulates it from the start: the host for scene bodies, a player's seat
       for its character. Every endpoint binds the same value. */
    uint8_t authority;
    /* The authority number it starts with: 0 for a body of the scene; the host's current
       number when a late joiner is given the table. */
    uint16_t number;
} body_sync_bind_t;

/* Binds `id` to a body of this endpoint's world. INVALID for a slot out of range, generation
   zero or a static body; TAKEN when the slot holds a body. */
body_sync_result_t body_sync_bind(body_sync_t *sync, body_sync_id_t id, const body_sync_bind_t *bind);

/* Forgets the body; the caller destroys it afterwards. Later bytes naming this id are stale:
   dropped and counted, never FORMAT. */
body_sync_result_t body_sync_unbind(body_sync_t *sync, body_sync_id_t id);

/* b3_nullBodyId when nothing is bound under `id`. */
b3BodyId body_sync_body(const body_sync_t *sync, body_sync_id_t id);

/* ---- authority ---- */

/* False when nothing is bound under `id`. */
bool body_sync_authority(const body_sync_t *sync, body_sync_id_t id, body_sync_authority_t *out);

/* True when this endpoint simulates the body now: its own pose goes out, nothing received
   moves it, and the caller may move it. While false the body is a copy: the seam owns its
   type, transform, velocity and motion locks (a copy is unlocked, so it turns to its owner's
   rotation), and the caller must not write them.

   An endpoint that becomes local for a body continues it from what the last owner left: its
   flags, motion type and motion locks from the record that moved authority (a grant, a forced
   move, a release), else from the newest state received, else from the bind; pose, velocities
   and hold from the newest state received; the host's records carry the body as it is when
   it answers or moves it. A claimant keeps on top what it changed itself since its claim: the
   flags bit by bit, the motion type and the locks, each apart. The holder is not inherited: a claim, a grant and a forced move start with no
   holder (the new owner sets its own; a claimant that set one before the grant keeps it), and
   only a seat that left hands its bodies back still naming their carrier. The owner changes a
   local body's motion type through Box3D; the seam reads it at collect and sends it.

   Claims and answers are matched by sequence equality, and authority by numbers that only
   grow; both rely on seam records arriving reliably and in order (the session's record lane
   keeps that). An implementation that sends them otherwise must keep the same matches. */
bool body_sync_is_local(const body_sync_t *sync, body_sync_id_t id);

/* Optimistic take: the body becomes local here at once and a claim, with this endpoint's next
   claim sequence (never 0, also across the wrap), goes to the host. The claimant simulates the
   body until the host answers that claim; the host ignores its poses until it is granted. An
   answer to an older claim of this endpoint (one it released since) changes nothing here. On
   the host a claim takes effect at once, reaches no queue, and is announced to every guest. A
   claim of a body already local only changes its hold. FULL when the queue to the host has no
   room. */
body_sync_result_t body_sync_claim(body_sync_t *sync, body_sync_id_t id, body_sync_hold_t hold);

/* The local endpoint gives the body back to the host with no holder; on the host it only
   clears the hold and the holder. INVALID when the body is not local here or no host is
   joined; FULL when the queue to the host has no room. A release carries the authority number
   its sender saw and its last claim's sequence. The host takes the body back when that number
   is its current one, or when the release crosses the grant of that very claim (the body then
   stays with the host). A release older than the current number (the host moved the body by
   force since) is stale and does not move the body. Every release that reaches the host
   withdraws its seat's pending claim of the body, whatever else it does. */
body_sync_result_t body_sync_release(body_sync_t *sync, body_sync_id_t id);

bool body_sync_poll_claim(body_sync_t *sync, body_sync_claim_event_t *out);

/* Host only: the answer to a polled claim. Granted: the claimant simulates the body from now
   on with the claim's hold, no holder and a new number. Refused: nothing changes and the
   claimant is told, so its optimistic copy turns back into a copy. UNKNOWN when that claim is
   no longer pending (released, claimed again, its seat left, the body unbound, or already
   answered): the caller drops the event, nothing is owed for it, and a newer claim, if any,
   comes from poll. */
body_sync_result_t body_sync_answer(body_sync_t *sync, const body_sync_claim_event_t *claim, bool granted);

/* Host only: a forced move that answers no claim (taking a prize back, a removal, a game
   rule). The body continues on `seat` from its newest state, velocities included, with `hold`,
   no holder and a new number; the announcement says it answers no claim, so even a seat whose
   claim of the body was answered or released meanwhile takes it. Pending claims of the body
   stay pending. */
body_sync_result_t body_sync_assign(body_sync_t *sync, body_sync_id_t id, uint8_t seat, body_sync_hold_t hold);

/* The character carrying a local body; it travels with the body's state. BODY_SYNC_ID_NONE
   lets go. INVALID when not local. */
body_sync_result_t body_sync_set_holder(body_sync_t *sync, body_sync_id_t id, body_sync_id_t holder);

/* State bits of a local body carried with its pose (low BODY_SYNC_FLAG_BITS, the caller's
   meaning). On a copy they read what its owner last sent. */
body_sync_result_t body_sync_set_flags(body_sync_t *sync, body_sync_id_t id, uint8_t flags);
uint8_t body_sync_flags(const body_sync_t *sync, body_sync_id_t id);

/* ---- the tick ---- */

/* `seat`'s tick as this endpoint sees it now (coop_session_peer_tick); negative while unknown.
   On a guest only the host's tick is known: bodies of another guest arrive relayed by the
   host, so their timeline must come from the seam's own bytes. The stub ignores it. */
void body_sync_set_peer_tick(body_sync_t *sync, uint8_t seat, double tick);

/* One state packet's payload from `seat`, stamped with the sender's tick. A guest takes state
   from the host only; the host takes from a guest only the bodies that guest simulates under
   the host's current authority number, and drops the rest as rejected. A state older than the
   newest one taken from the same sender is stale. Applied at the next body_sync_apply. */
body_sync_result_t body_sync_receive_state(body_sync_t *sync, uint8_t seat, uint32_t tick, const uint8_t *data, uint32_t size);

/* One reliable seam record from `seat` (a claim, a release, an authority announcement). */
body_sync_result_t body_sync_receive_record(body_sync_t *sync, uint8_t seat, uint32_t tick, const uint8_t *data, uint32_t size);

/* Before the step: copies are moved toward their poses for `tick` (how, buffer or none, is the
   implementation's), authority changes received so far take effect, and a body that became
   local continues from its newest state as body_sync_is_local says. */
void body_sync_apply(body_sync_t *sync, uint32_t tick);

/* After the step and the caller's contact judging: reads the poses, velocities and motion
   types of local bodies for sending and ends settled holds. */
void body_sync_collect(body_sync_t *sync, uint32_t tick);

/* One state packet for `seat`: writes at most `capacity` bytes, sets *size and returns true;
   false when this tick has nothing more for that seat. A budgeted sender writes at most one
   packet per seat between two body_sync_collect calls; the stub writes as many as every body
   takes. A capacity below 64 writes nothing. The capacity is what the transport carries after
   its own prefix (coop_session_state_max).

   The real seam relays a guest's bodies to the other guests from what the host decoded, one
   tick after the host took them. It needs the fates: without body_sync_unresolved and
   body_sync_resolve nothing is acknowledged, so sleeping bodies stay due at half a packet a
   tick each and every changed rare field rides every packet (about 1 KB a tick for 200
   sleeping bodies, against nothing once acknowledged). */
bool body_sync_write_state(body_sync_t *sync, uint8_t seat, uint8_t *buffer, uint32_t capacity, uint32_t *size);

/* The packet last written for `seat` left with this session number (coop_session_send_state);
   call nothing when the send failed, the packet then counts as lost. */
void body_sync_state_sent(body_sync_t *sync, uint8_t seat, uint16_t number);

/* Packet numbers sent to `seat` whose fate the seam still wants; the caller answers each with
   body_sync_resolve from coop_session_packet_fate once it is final. The stub wants none. */
uint32_t body_sync_unresolved(const body_sync_t *sync, uint8_t seat, uint16_t *numbers, uint32_t max);
void body_sync_resolve(body_sync_t *sync, uint8_t seat, uint16_t number, bool arrived);

/* The oldest seam record owed to `seat`, copied out without removing it: false when none
   waits or it does not fit `capacity`. Pop it once the session took it; a record the session
   refused with FULL stays at the head for the next tick. */
bool body_sync_peek_record(const body_sync_t *sync, uint8_t seat, uint8_t *buffer, uint32_t capacity, uint32_t *size);
void body_sync_pop_record(body_sync_t *sync, uint8_t seat);

/* ---- what is drawn ---- */

/* The offset of a copy's drawn pose from its physics pose: smoothing of a correction that only
   the eye sees. False when nothing is bound; identity for local bodies and for the stub.
   rotation is x, y, z, w and applies before the offset. */
bool body_sync_render_offset(const body_sync_t *sync, body_sync_id_t id, float position[3], float rotation[4]);

/* ---- measurements ---- */

bool body_sync_traffic(const body_sync_t *sync, uint8_t seat, body_sync_traffic_t *out);

#ifdef __cplusplus
}
#endif

#endif
