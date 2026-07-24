#define FLOS_UTILS
#include "base.c"

static float random_gaussian(void) {
    return sqrtf(-2.0f * logf(mrw_random_f32(0.0001, 1.0f))) * cosf(2.0f * (float)M_PI * mrw_random());
}

vec3s random_on_sphere(void) {
    return glms_vec3_normalize((vec3s){
        .x = random_gaussian(),
        .y = random_gaussian(),
        .z = random_gaussian(),
    });
}
