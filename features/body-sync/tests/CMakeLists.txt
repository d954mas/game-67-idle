set(BODY_SYNC_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
set(BODY_SYNC_SCENARIOS "${BODY_SYNC_DIR}/scenarios")

function(body_sync_test name)
    cmake_parse_arguments(T "" "" "LIBS" ${ARGN})
    add_executable(${name} ${name}.c)
    # Internal headers: the tests reach the wire codec, the logic and the world directly.
    target_include_directories(${name} PRIVATE "${BODY_SYNC_DIR}/src")
    target_compile_definitions(${name} PRIVATE BODY_SYNC_SCENARIOS_DIR="${BODY_SYNC_SCENARIOS}")
    target_link_libraries(${name} PRIVATE ${T_LIBS} unity)
    nt_set_warning_flags(${name})
    nt_set_sanitizer_flags(${name})
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endfunction()

body_sync_test(test_body_sync_logic LIBS body_sync_stand_logic)
body_sync_test(test_body_sync_wire)
target_sources(test_body_sync_wire PRIVATE "${BODY_SYNC_DIR}/src/stub_wire.c")
if(UNIX)
    target_link_libraries(test_body_sync_wire PRIVATE m)
endif()
body_sync_test(test_body_sync_stub LIBS body_sync_stub)
# The same seam suite on the real implementation.
add_executable(test_body_sync_seam_real test_body_sync_stub.c)
target_include_directories(test_body_sync_seam_real PRIVATE "${BODY_SYNC_DIR}/src")
target_compile_definitions(test_body_sync_seam_real PRIVATE SEAM_REAL=1)
target_link_libraries(test_body_sync_seam_real PRIVATE body_sync net_bits unity)
nt_set_warning_flags(test_body_sync_seam_real)
nt_set_sanitizer_flags(test_body_sync_seam_real)
add_test(NAME test_body_sync_seam_real COMMAND test_body_sync_seam_real WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
body_sync_test(test_body_sync_codec LIBS body_sync_parts)
body_sync_test(test_body_sync_sender LIBS body_sync_parts)
body_sync_test(test_body_sync_authority LIBS body_sync_parts)
# The contact gate's revocation cases must each fail on a planted defect: a recall recorded as
# an arbitrated end.
add_test(NAME test_body_sync_authority_mf_revoked_as_arbitrated
    COMMAND "${CMAKE_COMMAND}" -DEXE=$<TARGET_FILE:test_body_sync_authority> -DARGS=--plant-revoked-as-arbitrated
            "-DCASES=test_contact_revoked_by_an_assign_to_the_host|test_contact_revoked_by_an_assign_to_another_seat|test_contact_revoked_by_an_assign_to_the_same_seat|test_contact_revoked_when_the_seat_left"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_unity_fail.cmake")
# The evidence's gap cases must each fail when the evidence grows over gaps again.
add_test(NAME test_body_sync_authority_mf_evidence_grows_over_gaps
    COMMAND "${CMAKE_COMMAND}" -DEXE=$<TARGET_FILE:test_body_sync_authority> -DARGS=--plant-evidence-grows-over-gaps
            "-DCASES=test_evidence_of_a_silent_agent_does_not_grow|test_evidence_of_a_body_that_slept_has_no_long_chord|test_a_stamp_jump_buys_no_long_ray"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_unity_fail.cmake")
# The contact's approach is the step at it, a state stands only within the claim's lag, a teleport
# starts the evidence over, and a target's states count only under an ownership the seat may claim.
add_test(NAME test_body_sync_authority_mf_approach_from_ring
    COMMAND "${CMAKE_COMMAND}" -DEXE=$<TARGET_FILE:test_body_sync_authority> -DARGS=--plant-approach-from-ring
            "-DCASES=test_approach_is_the_step_at_the_contact"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_unity_fail.cmake")
add_test(NAME test_body_sync_authority_mf_evidence_age_unbounded
    COMMAND "${CMAKE_COMMAND}" -DEXE=$<TARGET_FILE:test_body_sync_authority> -DARGS=--plant-evidence-age-unbounded
            "-DCASES=test_a_contact_named_long_after_has_no_evidence|test_evidence_of_a_silent_agent_does_not_grow"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_unity_fail.cmake")
add_test(NAME test_body_sync_authority_mf_teleport_keeps_ring
    COMMAND "${CMAKE_COMMAND}" -DEXE=$<TARGET_FILE:test_body_sync_authority> -DARGS=--plant-teleport-keeps-ring
            "-DCASES=test_a_teleport_starts_the_evidence_over|test_a_teleport_step_is_refused"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_unity_fail.cmake")
add_test(NAME test_body_sync_authority_mf_target_any_ownership
    COMMAND "${CMAKE_COMMAND}" -DEXE=$<TARGET_FILE:test_body_sync_authority> -DARGS=--plant-target-any-ownership
            "-DCASES=test_target_states_count_only_under_a_claimable_ownership"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_unity_fail.cmake")
body_sync_test(test_body_sync_playback LIBS body_sync_parts)
body_sync_test(test_body_sync_soft LIBS body_sync_parts)
body_sync_test(test_body_sync_policy LIBS body_sync_parts)
body_sync_test(test_body_sync_real LIBS body_sync net_bits)
body_sync_test(test_body_sync_stand LIBS body_sync_stand body_sync)
body_sync_test(test_body_sync_free_step LIBS body_sync_stand body_sync)
body_sync_test(test_body_sync_checks LIBS body_sync_stand body_sync)
box3d_enable(test_body_sync_checks)
body_sync_test(test_body_sync_claims LIBS body_sync_stand body_sync)
box3d_enable(test_body_sync_claims)
# The host's limits on impact claims must each fail the cases meant for them when planted: an
# approach read as 0 refuses every honest throw's claim, an unbounded reach takes a forged one.
add_test(NAME test_body_sync_claims_mf_approach_scale0
    COMMAND "${CMAKE_COMMAND}" -DEXE=$<TARGET_FILE:test_body_sync_claims> -DARGS=--plant-claim-approach-scale0
            "-DCASES=test_guest_throw_breaks_glass_host|test_guest_throw_breaks_glass_interaction|test_guest_throw_knocks_dummy_host|test_guest_throw_knocks_dummy_interaction|test_second_throw_claims_again|test_resent_claim_applies_once"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_unity_fail.cmake")
add_test(NAME test_body_sync_claims_mf_reach_unbounded
    COMMAND "${CMAKE_COMMAND}" -DEXE=$<TARGET_FILE:test_body_sync_claims> -DARGS=--plant-claim-reach-unbounded
            "-DCASES=test_forged_far_claim_is_rejected_host|test_forged_far_claim_is_rejected_interaction|test_flung_far_claim_is_rejected_host|test_flung_far_claim_is_rejected_interaction"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_unity_fail.cmake")
add_test(NAME test_body_sync_claims_mf_take_reach_unbounded
    COMMAND "${CMAKE_COMMAND}" -DEXE=$<TARGET_FILE:test_body_sync_claims> -DARGS=--plant-take-reach-unbounded
            "-DCASES=test_inflated_far_take_is_refused"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_unity_fail.cmake")
add_test(NAME test_body_sync_claims_mf_evidence_grows_over_gaps
    COMMAND "${CMAKE_COMMAND}" -DEXE=$<TARGET_FILE:test_body_sync_claims> -DARGS=--plant-evidence-grows-over-gaps
            "-DCASES=test_forger_at_600ms_never_fits_host|test_forger_after_a_cut_never_fits_host"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_unity_fail.cmake")
add_test(NAME test_body_sync_claims_mf_approach_from_ring
    COMMAND "${CMAKE_COMMAND}" -DEXE=$<TARGET_FILE:test_body_sync_claims> -DARGS=--plant-approach-from-ring
            "-DCASES=test_a_gentle_touch_claims_no_hard_hit_host|test_a_gentle_touch_claims_no_hard_hit_interaction"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_unity_fail.cmake")
add_test(NAME test_body_sync_claims_mf_evidence_age_unbounded
    COMMAND "${CMAKE_COMMAND}" -DEXE=$<TARGET_FILE:test_body_sync_claims> -DARGS=--plant-evidence-age-unbounded
            "-DCASES=test_an_old_contact_tick_has_no_evidence_host|test_an_old_contact_tick_has_no_evidence_interaction"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_unity_fail.cmake")
add_test(NAME test_body_sync_claims_mf_transform_sleep
    COMMAND "${CMAKE_COMMAND}" -DEXE=$<TARGET_FILE:test_body_sync_claims> -DARGS=--plant-transform-sleep
            "-DCASES=test_a_carried_rock_follows_its_carrier_host|test_a_carried_rock_follows_its_carrier_interaction"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_unity_fail.cmake")
add_test(NAME test_body_sync_claims_mf_approach_unbounded
    COMMAND "${CMAKE_COMMAND}" -DEXE=$<TARGET_FILE:test_body_sync_claims> -DARGS=--plant-claim-approach-unbounded
            "-DCASES=test_inflated_claim_speed_is_rejected"
            -P "${CMAKE_CURRENT_LIST_DIR}/expect_unity_fail.cmake")
# Honest throws under loss and jitter, over seeds: expensive, so its own slow case.
add_test(NAME test_body_sync_claims_loss_sweep COMMAND test_body_sync_claims --sweep WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
box3d_enable(test_body_sync_stand)
box3d_enable(test_body_sync_free_step)

# Every shipped scenario is a named case of the command line, on each implementation: its
# checks are its invariants. The real seam's cases keep the plain names; the stub's say stub.
set(BODY_SYNC_CORE_SCENARIOS two_walk)
set(BODY_SYNC_SLOW_SCENARIOS stack_punch pile_throw cabinet_carry mannequin_hits grab_dispute breakables_break load_four
    edge_throw_cut edge_stall edge_deaf load_overflow pile_settle_loss10 throw_catch
    bounce_wall cut_short cut_long hitch_guest hitch_host rate_mismatch rate_rough two_ping latency_drop)
# The same scenarios with the second layer on (the real seam; the stub has no layer): their
# checks hold in both modes. The layer scenes name it themselves.
set(BODY_SYNC_LAYER_KG 20)
# What only the real seam does: a guest's character held dynamic on the host. The stub's case
# of it is a must-fail below, the naive receiver's kinematic copy shoving the cabinet.
set(BODY_SYNC_REAL_SCENARIOS char_push layer_bounce layer_pile layer_push layer_rest layer_stand layer_nudge layer_leave)
# One seed, two processes, byte-equal metrics: the core case and the playback's edges.
set(BODY_SYNC_TWICE_SCENARIOS two_walk bounce_wall cut_short cut_long hitch_guest hitch_host rate_mismatch rate_rough two_ping)
set(BODY_SYNC_IMPLS "body_sync_stand_cli|" "body_sync_stand_cli_stub|stub_")

set(core_tests test_body_sync_logic test_body_sync_playback test_body_sync_soft test_body_sync_policy test_body_sync_wire test_body_sync_stub test_body_sync_seam_real test_body_sync_codec test_body_sync_sender test_body_sync_authority test_body_sync_real
    test_body_sync_stand test_body_sync_checks test_body_sync_authority_mf_revoked_as_arbitrated
    test_body_sync_authority_mf_evidence_grows_over_gaps
    test_body_sync_claims test_body_sync_claims_mf_approach_scale0 test_body_sync_claims_mf_reach_unbounded
    test_body_sync_claims_mf_approach_unbounded test_body_sync_claims_mf_take_reach_unbounded
    test_body_sync_claims_mf_evidence_grows_over_gaps test_body_sync_claims_mf_transform_sleep
    test_body_sync_authority_mf_approach_from_ring test_body_sync_authority_mf_evidence_age_unbounded
    test_body_sync_authority_mf_teleport_keeps_ring test_body_sync_authority_mf_target_any_ownership
    test_body_sync_claims_mf_approach_from_ring test_body_sync_claims_mf_evidence_age_unbounded)
set(slow_tests test_body_sync_free_step test_body_sync_claims_loss_sweep)
foreach(impl IN LISTS BODY_SYNC_IMPLS)
    string(REPLACE "|" ";" parts "${impl}")
    list(GET parts 0 cli)
    list(LENGTH parts n)
    set(tag "")
    if(n GREATER 1)
        list(GET parts 1 tag)
    endif()
    foreach(scn IN LISTS BODY_SYNC_CORE_SCENARIOS BODY_SYNC_SLOW_SCENARIOS)
        add_test(NAME test_body_sync_${tag}scn_${scn}
            COMMAND ${cli} "${BODY_SYNC_SCENARIOS}/${scn}.scn" --out "${CMAKE_CURRENT_BINARY_DIR}/${tag}${scn}.json" --quiet)
    endforeach()
    foreach(scn IN LISTS BODY_SYNC_CORE_SCENARIOS)
        list(APPEND core_tests test_body_sync_${tag}scn_${scn})
    endforeach()
    foreach(scn IN LISTS BODY_SYNC_SLOW_SCENARIOS)
        list(APPEND slow_tests test_body_sync_${tag}scn_${scn})
    endforeach()
    foreach(scn IN LISTS BODY_SYNC_TWICE_SCENARIOS)
        set(twice test_body_sync_${tag}two_processes_${scn})
        if(scn STREQUAL "two_walk")
            set(twice test_body_sync_${tag}two_processes)
        endif()
        add_test(NAME ${twice}
            COMMAND "${CMAKE_COMMAND}" -DCLI=$<TARGET_FILE:${cli}> -DSCENARIO=${BODY_SYNC_SCENARIOS}/${scn}.scn
                    -DOUT=${CMAKE_CURRENT_BINARY_DIR} -DTAG=${tag} -P "${CMAKE_CURRENT_LIST_DIR}/run_twice.cmake")
        if(scn STREQUAL "two_walk")
            list(APPEND core_tests ${twice})
        else()
            list(APPEND slow_tests ${twice})
        endif()
    endforeach()
endforeach()
foreach(scn IN LISTS BODY_SYNC_CORE_SCENARIOS BODY_SYNC_SLOW_SCENARIOS BODY_SYNC_REAL_SCENARIOS)
    if(NOT scn MATCHES "^layer_")
        add_test(NAME test_body_sync_layer_scn_${scn}
            COMMAND body_sync_stand_cli "${BODY_SYNC_SCENARIOS}/${scn}.scn" --light-mass ${BODY_SYNC_LAYER_KG}
                    --out "${CMAKE_CURRENT_BINARY_DIR}/layer_${scn}.json" --quiet)
        list(APPEND slow_tests test_body_sync_layer_scn_${scn})
    endif()
endforeach()
foreach(scn IN LISTS BODY_SYNC_REAL_SCENARIOS)
    add_test(NAME test_body_sync_scn_${scn}
        COMMAND body_sync_stand_cli "${BODY_SYNC_SCENARIOS}/${scn}.scn" --out "${CMAKE_CURRENT_BINARY_DIR}/${scn}.json" --quiet)
    list(APPEND slow_tests test_body_sync_scn_${scn})
    if(scn MATCHES "^layer_")
        add_test(NAME test_body_sync_two_processes_${scn}
            COMMAND "${CMAKE_COMMAND}" -DCLI=$<TARGET_FILE:body_sync_stand_cli> -DSCENARIO=${BODY_SYNC_SCENARIOS}/${scn}.scn
                    -DOUT=${CMAKE_CURRENT_BINARY_DIR} -DTAG= -P "${CMAKE_CURRENT_LIST_DIR}/run_twice.cmake")
        list(APPEND slow_tests test_body_sync_two_processes_${scn})
    endif()
endforeach()
# Authority following interaction (the real seam only): its own scenes, and the second layer's,
# the throws' and the heap's scenes run under it with the layer on.
set(BODY_SYNC_AUTHORITY_SCENARIOS authority_host_push authority_two_guests authority_two_guests_far authority_leave
    authority_three_host authority_three_guest authority_three_cross)
set(BODY_SYNC_INTERACTION_SCENARIOS layer_bounce layer_pile layer_push layer_rest layer_stand layer_nudge layer_leave pile_throw throw_catch
    load_four char_push edge_throw_cut pile_settle_loss10 breakables_break mannequin_hits cabinet_carry grab_dispute)
foreach(scn IN LISTS BODY_SYNC_AUTHORITY_SCENARIOS)
    add_test(NAME test_body_sync_scn_${scn}
        COMMAND body_sync_stand_cli "${BODY_SYNC_SCENARIOS}/${scn}.scn" --out "${CMAKE_CURRENT_BINARY_DIR}/${scn}.json" --quiet)
    list(APPEND slow_tests test_body_sync_scn_${scn})
endforeach()
foreach(scn IN LISTS BODY_SYNC_INTERACTION_SCENARIOS)
    add_test(NAME test_body_sync_interaction_scn_${scn}
        COMMAND body_sync_stand_cli "${BODY_SYNC_SCENARIOS}/${scn}.scn" --authority interaction --light-mass ${BODY_SYNC_LAYER_KG}
                --out "${CMAKE_CURRENT_BINARY_DIR}/interaction_${scn}.json" --quiet)
    list(APPEND slow_tests test_body_sync_interaction_scn_${scn})
endforeach()
add_test(NAME test_body_sync_stub_contrast_char_push
    COMMAND "${CMAKE_COMMAND}" -DCLI=$<TARGET_FILE:body_sync_stand_cli_stub> -DSCENARIO=${BODY_SYNC_SCENARIOS}/char_push.scn
            -DOUT=${CMAKE_CURRENT_BINARY_DIR} -DTAG=stub_ -DRC=1 "-DEXPECT=FAIL line 24 moved~cabinet moved" -P "${CMAKE_CURRENT_LIST_DIR}/expect_fail.cmake")
list(APPEND slow_tests test_body_sync_stub_contrast_char_push)
set_tests_properties(${core_tests} PROPERTIES LABELS "core")
set_tests_properties(${slow_tests} PROPERTIES LABELS "slow")

# Scenarios that must not pass, each on a named check and on each implementation: a check that
# cannot fail proves nothing.
set(BODY_SYNC_MUST_FAIL "${CMAKE_CURRENT_LIST_DIR}/must_fail")
function(body_sync_must_fail name rc expect)
    foreach(impl IN LISTS BODY_SYNC_IMPLS)
        string(REPLACE "|" ";" parts "${impl}")
        list(GET parts 0 cli)
        list(LENGTH parts n)
        set(tag "")
        if(n GREATER 1)
            list(GET parts 1 tag)
        endif()
        add_test(NAME test_body_sync_${tag}mf_${name}
            COMMAND "${CMAKE_COMMAND}" -DCLI=$<TARGET_FILE:${cli}> -DSCENARIO=${BODY_SYNC_MUST_FAIL}/${name}.scn
                    -DOUT=${CMAKE_CURRENT_BINARY_DIR} -DTAG=${tag} -DRC=${rc} "-DEXPECT=${expect}" -P "${CMAKE_CURRENT_LIST_DIR}/expect_fail.cmake")
        set_tests_properties(test_body_sync_${tag}mf_${name} PROPERTIES LABELS "core")
    endforeach()
endfunction()
body_sync_must_fail(mf_cut_whole 1 "FAIL line 8 agree_at_rest~g1 left at tick|FAIL line 9 no_stuck~g1 left at tick|FAIL line 10 effects_once~g1 left at tick")
body_sync_must_fail(mf_cut_delay 1 "FAIL line 7 metric~host_ticks.max")
body_sync_must_fail(mf_result 1 "FAIL line 6 result~g: failed, expected granted")
body_sync_must_fail(mf_metric 1 "FAIL line 7 metric~divergence.max")
body_sync_must_fail(mf_hard_end_moving 1 "FAIL line 9 agree_at_rest~came to rest")
body_sync_must_fail(mf_one_of 1 "FAIL line 8 one_of~0 of 2 ended granted")
body_sync_must_fail(mf_loss_agree 1 "FAIL line 7 agree_at_rest~off by")
body_sync_must_fail(mf_out_of_bounds 1 "FAIL line 9 no_stuck~outside the bounds")
body_sync_must_fail(mf_claim_unanswered 1 "FAIL line 9 no_stuck~a claim unanswered")
body_sync_must_fail(mf_frozen_copy 1 "FAIL line 8 no_stuck~frozen")
body_sync_must_fail(mf_knock_unseen 1 "FAIL line 9 effects_once~shown 0 times")
body_sync_must_fail(mf_left_not 1 "FAIL line 6 left~g1 stayed")
body_sync_must_fail(mf_guests0 2 "mf_guests0.scn:6: this check needs a guest")
body_sync_must_fail(mf_angle_zero 1 "FAIL line 9 agree_at_rest~turned")
body_sync_must_fail(mf_no_copy 1 "FAIL line 8 metric~no copy was measured")
body_sync_must_fail(mf_behind 1 "FAIL line 9 metric~behind_ticks.max")
body_sync_must_fail(mf_layer_off 1 "FAIL line 14 metric~response_ticks.max")
# The stub takes nothing by interaction, so only the real seam has a take that can lose.
add_test(NAME test_body_sync_mf_lost_take
    COMMAND "${CMAKE_COMMAND}" -DCLI=$<TARGET_FILE:body_sync_stand_cli> -DSCENARIO=${BODY_SYNC_MUST_FAIL}/mf_lost_take.scn
            -DOUT=${CMAKE_CURRENT_BINARY_DIR} -DTAG= -DRC=1 "-DEXPECT=FAIL line 19 metric~lost_takes.max" -P "${CMAKE_CURRENT_LIST_DIR}/expect_fail.cmake")
set_tests_properties(test_body_sync_mf_lost_take PROPERTIES LABELS "core")
