#ifndef MINE_CARDS_MODEL_PROOF_H
#define MINE_CARDS_MODEL_PROOF_H

#include <stdbool.h>

typedef struct nt_skeletal_anim_clip nt_skeletal_anim_clip_t;

void mine_cards_model_proof_init(void);
void mine_cards_model_proof_bind_skeleton(nt_skeletal_anim_clip_t *clip);
void mine_cards_model_proof_step(float swing, float sweep);
void mine_cards_model_proof_step_ozz(const float *model_matrices, int matrix_count);
bool mine_cards_model_proof_can_draw(void);
void mine_cards_model_proof_draw(float w, float h);
void mine_cards_model_proof_draw_in_box(float screen_w, float screen_h, float x, float y, float w, float h);
void mine_cards_model_proof_restore_gpu(void);
void mine_cards_model_proof_shutdown(void);

#endif /* MINE_CARDS_MODEL_PROOF_H */
