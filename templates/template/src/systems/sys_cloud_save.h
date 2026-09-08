#ifndef SYS_CLOUD_SAVE_H
#define SYS_CLOUD_SAVE_H

#include "game_save_cloud.h"

/* min_write_interval_sec is the shortest gap between acknowledged uploads; 0
   uploads as fast as the transport allows. A portal that meters writes wants
   a real interval here, because the local save changes far more often than a
   cloud copy needs to. */
void cloud_save_init(game_save_cloud_choose_fn choose,
                     game_save_cloud_same_features_fn same_features,
                     double min_write_interval_sec);

#endif /* SYS_CLOUD_SAVE_H */
