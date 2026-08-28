#include "../example/post_aa_policy.h"

#include <assert.h>
#include <math.h>

static int near(float a, float b) { return fabsf(a - b) < 0.0001F; }

int main(void) {
  assert(near(post_aa_world_scale(POST_AA_OFF, 1.0F), 1.0F));
  assert(near(post_aa_world_scale(POST_AA_FXAA, 1.5F), 1.0F));
  assert(near(post_aa_world_scale(POST_AA_SSAA, 1.0F), 1.5F));
  assert(near(post_aa_world_scale(POST_AA_SSAA, 1.5F), 1.25F));
  assert(near(post_aa_world_scale(POST_AA_SSAA, 2.0F), 1.0F));
  return 0;
}
