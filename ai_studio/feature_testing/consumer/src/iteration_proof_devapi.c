/* Dedicated leaf-C fixture for the source-edit-to-runtime proof. The Python
   orchestrator parses this exact define before building; changing the token
   must rebuild this translation unit and change the live command response. */
#define GAME_ITERATION_C_FIXTURE "template-leaf-c-v1"

#if NT_DEVAPI_ENABLED

#include "cJSON.h"
#include "devapi/nt_devapi.h"
#include "game_state.h"
#include "iteration_proof_devapi.h"

static bool iteration_proof(
    const cJSON *params,
    cJSON *result_obj,
    nt_devapi_error *err,
    void *user
) {
    cJSON_AddStringToObject(result_obj, "cFixture", GAME_ITERATION_C_FIXTURE);
    cJSON_AddStringToObject(result_obj, "schemaFixture", GAME_STATE_TEST_LABEL_TEXT_DEFAULT);
    return true;
}

void game_iteration_proof_register_devapi(void) {
    static const nt_devapi_command cmds[] = {
        {.method = "game.iteration.proof",
            .summary = "Return exact template leaf-C and generated-schema iteration fixtures.",
            .params_shape = "none",
            .result_shape = "cFixture, schemaFixture",
            .fn = iteration_proof},
    };
    nt_devapi_register(cmds, NT_COUNTOF(cmds), NULL);
}

#endif
