#ifndef UTIL_TRY_HPP
#define UTIL_TRY_HPP

#define TRY(...)                                 \
    ({                                           \
        auto res = __VA_ARGS__;                  \
        if (!res.has_value()) {                  \
            return std::unexpected(res.error()); \
        }                                        \
        *res;                                    \
    })

#define TRY_WITH_ERR(e, ...)           \
    ({                                 \
        auto res = __VA_ARGS__;        \
        if (!res.has_value()) {        \
            return std::unexpected(e); \
        }                              \
        *res;                          \
    })

#endif  // UTIL_TRY_HPP
