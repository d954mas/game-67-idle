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
    /* The Box3D category bits of this endpoint's own characters, 0 for none. When a body of
       class LIGHT becomes a copy, the seam takes these bits off the collision mask of each of
       its shapes, so a light copy never blocks this endpoint's character (another's character
       pushes it on the endpoint that simulates it). It puts back exactly the bits it took, on
       the same shapes, when the body is local again, unbound or the seam destroyed. Meanwhile
       the caller may change anything else of a copy's filters and add or remove shapes (one
       added is left as the caller made it), but must not set those bits on a copy. Other
       classes keep their masks. A body with more than BODY_SYNC_SHAPES_MAX shapes is refused
       at bind while this is set. */
    uint64_t character_category;
    /* The real seam's playback delay bounds, in ticks: 0 takes 4 and 15; 1..60, min <= max.
       The stub ignores them. */
    uint32_t delay_min;
    uint32_t delay_max;
    /* The second layer, kilograms, 0..1e6 (else create refuses); 0 (the default) is off. On a
       guest, a copy of class LIGHT and at most this mass, owned by another seat, neither held nor
       in flight under its owner, and not kinematic there, keeps character_category's bits and,
       while something of this side's moves at it or its owner moves, is simulated here as a
       dynamic body pulled softly toward its owner's playback pose: it answers this endpoint's own
       character and thrown bodies at once instead of standing until the owner's state comes.
       Resting, it is held exactly on its owner's pose like any copy. Its owner still judges it:
       nothing of a soft copy is sent, and nothing on the wire changes. A push by a body the seam
       does not bind moves a copy here only: the host never learns of it, so the pull takes it
       back once the pause chain ends. Each guest may use its
       own knob; the host ignores it, and so does the stub. */
    float light_mass_max;
    /* Who owns a free body; every endpoint must use the same mode and light_mass_max (the
       caller's handshake). INTERACTION (src/sync_policy.h, README "Authority follows
       interaction"): a seat's character, what it carries or threw and what it owns this way take
       the light bodies (light_mass_max, which must then be above 0) they set moving; the taker
       simulates them and the host shows them through playback until they rest, when the seam
       hands them back. The host grants or refuses those takes itself; its own agents come first.
       The stub ignores it. */
    body_sync_authority_mode_t authority;
    /* When a guest's or the host's own flight goes back to the host (README "Authority follows
       interaction", "A flight"); every endpoint uses the same, as with authority. CONTACT, the
       default, ends it at its first contact with a body that is not static; REST keeps it its
       thrower's until it sleeps. The stub follows it too. */
    body_sync_flight_end_t flight_end;
} body_sync_config_t;

/* NULL when the config is out of range, its limits are too wide for the implementation's wire
   (the real one: every coordinate of the box below 2048 m in magnitude, max_speed below
   262144 m/s, max_spin below 524288 rad/s), its state_budget has no room for one widest record
   per seat and one more (the real one's staleness bound needs a record after the characters,
   counted as one a seat: more characters a seat need a budget with room for them),
   or memory is short. All memory is taken here: the real seam about 2.7 MB, the stub about
   400 KB. */
body_sync_t *body_sync_create(const body_sync_config_t *config);
/* Puts back the mask bits it took off LIGHT copies (character_category); everything else a
   copy has stays as the seam set it: its pose, velocities, motion type and locks as a copy.
   Destroy the seam before its world. */
void body_sync_destroy(body_sync_t *sync);

/* The linked implementation and its version, for logs and metrics: "real 0.8.0" or
   "stub 0.8.0". */
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
   zero or a static body; TAKEN when the slot holds a body. On the host TAKEN too for a generation
   equal modulo 16 to one it unbound from that slot less than the evidence cap and the longest
   playback delay ago (75 ticks at 60 Hz): the real seam's wire names a generation modulo 16, and
   the old one's late packets would read as the new body's. Another generation binds at once, so
   on TAKEN a game binds the slot's next generation (a pool with rising generations always can) or
   takes another free slot. */
body_sync_result_t body_sync_bind(body_sync_t *sync, body_sync_id_t id, const body_sync_bind_t *bind);

/* Forgets the body; the caller destroys it afterwards. Later bytes naming this id are stale:
   dropped and counted, never FORMAT. */
body_sync_result_t body_sync_unbind(body_sync_t *sync, body_sync_id_t id);

/* b3_nullBodyId when nothing is bound under `id`. */
b3BodyId body_sync_body(const body_sync_t *sync, body_sync_id_t id);

/* A local body's opaque bytes (an animation state): up to BODY_SYNC_DATA_MAX, travelling with
   its state until acknowledged, as flags do. INVALID when not local or too long. On a copy,
   body_sync_data reads them as of the copy's playback time; when the played states carry none,
   and on the stub, it reads the newest the body's owner sent, which can lead the drawn pose.
   Returns the size written to `out`, at most `capacity`; 0 when none or nothing bound. */
body_sync_result_t body_sync_set_data(body_sync_t *sync, body_sync_id_t id, const void *data, uint32_t size);
uint32_t body_sync_data(const body_sync_t *sync, body_sync_id_t id, void *out, uint32_t capacity);

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
   claim sequence (never 0, also across the wrap), goes to the host. A copy takes over within
   the call (its pose, velocities and motion as body_sync_is_local says), so what the caller
   does to the body from then on (a grab carries it to the hand) is never undone. The claimant simulates the
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
   on with the claim's hold, no holder and a new number (body_sync_answer_as grants another). Refused: nothing changes and the
   claimant is told, so its optimistic copy turns back into a copy. UNKNOWN when that claim is
   no longer pending (released, claimed again, its seat left, the body unbound, or already
   answered): the caller drops the event, nothing is owed for it, and a newer claim, if any,
   comes from poll. */
body_sync_result_t body_sync_answer(body_sync_t *sync, const body_sync_claim_event_t *claim, bool granted);
/* Host only: grants a polled claim as body_sync_answer does, under `hold` instead of the claimed
   one: the host reads it for the claimant until her states come (a catch puts the body in her
   hand), while she keeps simulating under the hold she claimed. INVALID, answering nothing, for a
   hold a claim cannot name (only until release and until settled). */
body_sync_result_t body_sync_answer_as(body_sync_t *sync, const body_sync_claim_event_t *claim, body_sync_hold_t hold);

/* Host only: a forced move that answers no claim (taking a prize back, a removal, a game
   rule). The body continues on `seat` from its newest state, velocities included, with `hold`,
   no holder and a new number; the announcement says it answers no claim, so even a seat whose
   claim of the body was answered or released meanwhile takes it. Pending claims of the body
   stay pending. A move to the host's own seat takes over within the call, as a claim does: what
   the caller does to the body from then on (a recall puts it home at rest) is never undone; a body
   granted away in the same tick, never a copy yet, goes on as the host simulates it. */
body_sync_result_t body_sync_assign(body_sync_t *sync, body_sync_id_t id, uint8_t seat, body_sync_hold_t hold);

/* The character carrying a local body; it travels with the body's state. BODY_SYNC_ID_NONE
   lets go. INVALID when not local. */
body_sync_result_t body_sync_set_holder(body_sync_t *sync, body_sync_id_t id, body_sync_id_t holder);

/* State bits of a local body carried with its pose (low BODY_SYNC_FLAG_BITS, the caller's
   meaning). On a copy they read what its owner last sent. */
body_sync_result_t body_sync_set_flags(body_sync_t *sync, body_sync_id_t id, uint8_t flags);
uint8_t body_sync_flags(const body_sync_t *sync, body_sync_id_t id);

/* ---- contact claims ---- */

/* The authority number a contact claim of a local body names: the number its states declare,
   so a take still waiting names the number its grant will give. False when nothing is bound,
   the body is not local, or a claim that is not a take waits for its answer: the host takes no
   state of such a claim before its grant, and judges none of its contacts either. */
bool body_sync_contact_number(const body_sync_t *sync, body_sync_id_t id, uint16_t *number);

/* Host only: whether `seat` may claim a contact of its body `agent` with `target` under the
   authority number `number` (body_sync_contact_number on the claimant), and the host's own
   world around it in `out`. Every argument may come from a peer's bytes: anything out of
   range, unbound, of an old generation, the host's own seat, a guest calling, or `target`
   equal to `agent` is STALE; the call never asserts.

   The game sends its claim after that tick's seam records of the seat, on the same ordered
   lane: then a release (a flight's return at the contact) reaches the host before the claim of
   the contact that caused it, and a claim made before the seat heard of a host's move reaches
   it within a round trip of that move. The verdicts:
   - OK while the seat simulates the agent under that number.
   - OK within the grace after that ownership ended by the seat's own release, or by the host
     settling a contest (its take-back or claim, a grant or take to another seat); STALE after.
     The grace is the evidence's span: a round trip, a playback delay and a tick, capped at
     SYNC_POLICY_EVIDENCE_AGE_MAX_S (the stub, which plays no delay: its round trip plus four
     mean deviations of it, and a tick).
   - REVOKED, for good, when the host recalled it (body_sync_assign to any seat, the seat
     leaving): the recall drops the seat's late claims.
   - STALE otherwise: a number the seat never held, a claim older than the seat's newest ended
     ownership of the body (one is kept per seat and body), any ownership from before a seat
     joined again.
   `out` measures only what the host itself took of that ownership's states, never the velocity
   they declare (README "Contact claims"), at `contact_tick`, the claimant's tick of the contact
   (the one its states are stamped with): a contact lies at most SYNC_POLICY_CHORD_TICKS_MAX (4)
   ticks on from a taken state, else it has no evidence and the gap is INFINITY.
   A seat not joined gets nothing of a grace. The seam judges no effect: the game dedupes per target effect
   across seats, not per claim key (seat, agent, number, target): after an arbitrated end both
   seats may get OK for the same contact for up to the grace (1 s at most), and after any end the
   host's own simulation may meet the same target again. */
body_sync_contact_verdict_t body_sync_check_contact(const body_sync_t *sync, uint8_t seat, body_sync_id_t agent, uint16_t number,
                                                    body_sync_id_t target, uint32_t contact_tick, body_sync_contact_t *out);

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
   body_sync_resolve from coop_session_packet_fate once it is final. The stub wants them only for
   the round trip its contact claims' grace is built from. */
uint32_t body_sync_unresolved(const body_sync_t *sync, uint8_t seat, uint16_t *numbers, uint32_t max);
void body_sync_resolve(body_sync_t *sync, uint8_t seat, uint16_t number, bool arrived);

/* The oldest seam record owed to `seat`, copied out without removing it: false when none
   waits or it does not fit `capacity`. Pop it once the session took it; a record the session
   refused with FULL stays at the head for the next tick. */
bool body_sync_peek_record(const body_sync_t *sync, uint8_t seat, uint8_t *buffer, uint32_t capacity, uint32_t *size);
void body_sync_pop_record(body_sync_t *sync, uint8_t seat);

/* ---- what is drawn ---- */

/* The offset of a body's drawn pose from its physics pose: smoothing of a move that only the
   eye sees. False when nothing is bound; identity for the stub. rotation is x, y, z, w and
   applies before the offset. The real seam hands it a copy's placement and a body's change
   between copy and local, so the drawn pose does not jump, and decays it every apply toward
   identity; a body that turned local keeps it until it has decayed. A local body never gains
   a new one; physics never sees it. */
bool body_sync_render_offset(const body_sync_t *sync, body_sync_id_t id, float position[3], float rotation[4]);

/* The caller teleported a local body (a recall home, a respawn) with b3Body_SetTransform: it is
   drawn where physics has it from now on, its render offset dropped, so a take-over's smoothing
   just before (body_sync_assign) does not draw it off the new pose. UNKNOWN for an id not bound,
   INVALID for a copy, whose pose is its owner's. */
body_sync_result_t body_sync_teleported(body_sync_t *sync, body_sync_id_t id);

/* Measurement: the owner's tick a copy shows now, in that owner's tick count; for a local body
   the endpoint's own last collect tick. False when unknown, and while a copy has no state to
   play: one turned back into a copy, until the next apply drives it and its owner's first state
   since has come. The stub reports the tick of the newest state received. */
bool body_sync_playback(const body_sync_t *sync, body_sync_id_t id, double *owner_tick);

/* Measurement: true for a copy this side simulates itself now and pulls toward its owner (the
   second layer, light_mass_max), whose drawn pose is a prediction rather than its playback pose;
   false for one held on its owner's pose. Always false for the stub and with the layer off. */
bool body_sync_is_predicted(const body_sync_t *sync, body_sync_id_t id);

/* Measurement: true for a predicted copy whose pull waited at the last apply (the layer's pause
   after this side's touch or hit): its drawn pose is this side's own answer, which the owner
   shows at this side's moment. Past the pause it follows its playback pose. Always false for
   the stub and with the layer off. */
bool body_sync_is_pull_paused(const body_sync_t *sync, body_sync_id_t id);

/* Measurement: a take of this side's that lost while its claim waited turns back into a copy, its
   drawn pose handed back to the winner's (README "Conflicts"). Sets how many this body had here
   and the latest one's distance from the drawn pose to the winner's playback pose; true while the
   drawn pose still glides off it (its render offset outside the layer's 1 cm dead zone). Always
   false and 0 for the stub. */
bool body_sync_lost_take(const body_sync_t *sync, body_sync_id_t id, uint32_t *count, float *hand_back_m);

/* True for a copy the second layer holds (soft, in the rest hold or in flight): the seam keeps
   character_category's bits on it, so a game that sets its own collision filters must not take
   them off it as it does for other LIGHT copies. Always false for the stub and with the layer
   off. */
bool body_sync_is_layer_copy(const body_sync_t *sync, body_sync_id_t id);

/* Measurement: the round trip to a seat in ticks, from a state packet's send to its arrival fate
   (body_sync_resolve), smoothed; 0 before the first fate. The second layer's pause is built from
   it, and the stub's contact claim grace. */
float body_sync_round_trip(const body_sync_t *sync, uint8_t seat);

/* ---- measurements ---- */

bool body_sync_traffic(const body_sync_t *sync, uint8_t seat, body_sync_traffic_t *out);

/* Measurement: what this side dropped of `seat`'s bytes naming `id` (its states and its records)
   because they speak for an ownership this side does not hold for that seat: a number the seat
   no longer holds (the body was recalled or moved on), a seat that never owned it, the id
   unbound since. An old owner's late packets, told apart from the older samples and far ticks
   every path drops (body_sync_traffic counts all of them as stale or rejected). The count stays
   through the id's unbind and starts over when its slot is bound again; 0 for an id never bound.
   The real seam's wire names a generation modulo 16, so a slot's bytes count for the generation
   bound there last. */
uint32_t body_sync_old_owner_drops(const body_sync_t *sync, body_sync_id_t id, uint8_t seat);
/* Host: characters bound while the host already kept the older states of 16 (README "Contact
   claims"), since its creation. Each is judged on its last 12 taken states alone, so a claim of
   its contact the record lane resent is a missed effect. 0 on a guest. */
uint32_t body_sync_agent_overflow(const body_sync_t *sync);

#ifdef __cplusplus
}
#endif

#endif
