#include "sync_policy.h"

#include "features/body_sync/body_sync_types.h"

bool sync_policy_returns(const sync_policy_in_t *in) {
    return in->hold == BODY_SYNC_HOLD_UNTIL_SETTLED && (in->asleep || in->touched_body);
}
