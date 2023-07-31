#include "gsl/gsl-lite.hpp"
#include <clientserver/udaStructs.h>

namespace JMP::map_transform {

int transform_offset(DataBlock* data_block, float offset);
int transform_scale(DataBlock* data_block, float scale);

template <typename T> int offset_value(T& temp_var, float offset) {

    *temp_var += offset;
    return 0;
};

template <typename T> int offset_span(gsl::span<T> temp_span, float offset) {

    if (temp_span.empty()) {
        return 1;
    }

    std::for_each(temp_span.begin(), temp_span.end(),
                  [&offset](T& elem) { elem += offset; });

    return 0;
};

template <typename T> int scale_value(T& temp_var, float scale) {

    *temp_var *= scale;
    return 0;
};

template <typename T> int scale_span(gsl::span<T> temp_span, float scale) {

    if (temp_span.empty()) {
        return 1;
    }

    std::for_each(temp_span.begin(), temp_span.end(),
                  [&scale](T& elem) { elem *= scale; });

    return 0;
};

} // namespace JMP::map_transform
