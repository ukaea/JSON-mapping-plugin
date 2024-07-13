#include <algorithm>
#include <clientserver/udaStructs.h>

#include "gsl/gsl-lite.hpp"

namespace JMP::map_transform
{

int transform_offset(DataBlock* data_block, float offset);
int transform_scale(DataBlock* data_block, float scale);

template <typename T> int offset_value(T& var, float offset)
{

    *var += offset;
    return 0;
}

template <typename T> int offset_span(gsl::span<T> span, float offset)
{

    if (span.empty()) {
        return 1;
    }

    std::for_each(span.begin(), span.end(), [&](T& elem) { elem += offset; });

    return 0;
}

template <typename T> int scale_value(T& var, float scale)
{

    *var *= scale;
    return 0;
}

template <typename T> int scale_span(gsl::span<T> span, float scale)
{

    if (span.empty()) {
        return 1;
    }

    std::for_each(span.begin(), span.end(), [&](T& elem) { elem *= scale; });

    return 0;
}

} // namespace JMP::map_transform
