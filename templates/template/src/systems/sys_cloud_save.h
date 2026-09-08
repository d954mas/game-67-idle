#ifndef SYS_CLOUD_SAVE_H
#define SYS_CLOUD_SAVE_H

#include "game_save_cloud.h"

void cloud_save_init(game_save_cloud_choose_fn choose,
                     game_save_cloud_same_features_fn same_features);

#endif /* SYS_CLOUD_SAVE_H */
