#pragma once
#include <glm/glm.hpp>

namespace megamol {
namespace moldyn_gl {
namespace halton {

float createHaltonNumber(unsigned int index, int base) {
    float f = 1;
    float r = 0;
    int current = index;
    do {
        f = f / base;
        r = r + f * (current % base);
        current = (int)glm::floor(current / base);
    } while (current > 0);
    return r;
}
} // namespace halton
} // namespace moldyn_gl
} // namespace megamol
