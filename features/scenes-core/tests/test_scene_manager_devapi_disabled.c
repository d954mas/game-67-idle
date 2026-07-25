#include "features/scenes/scene_manager_devapi.h"

int main(void) {
    return scene_manager_register_devapi(NULL) ? 0 : 1;
}
