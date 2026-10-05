#ifndef EMBCSV_FLOAT_H
#define EMBCSV_FLOAT_H

#include <stddef.h>
#include <stdint.h>

/*
 * Internal floating-point formatter policy.
 *
 * - Locale independent.
 * - No heap allocation.
 * - Fixed notation for decimal exponents [-4, 6].
 * - Scientific notation outside that range.
 * - Round-to-nearest, ties-to-even.
 * - Special values: "nan", "inf", "-inf".
 */
#define EMBCSV_FLOAT_F32_MAX_PRECISION 9U
#define EMBCSV_FLOAT_F64_MAX_PRECISION 17U
#define EMBCSV_FLOAT_BUFFER_SIZE       32U

typedef enum {
    EMBCSV_FLOAT_OK = 0,
    EMBCSV_FLOAT_EINVAL = -1,
    EMBCSV_FLOAT_ENOSPC = -2,
    EMBCSV_FLOAT_EINTERNAL = -3
} embcsv_float_status_t;

/*
 * Format one IEEE-754 binary32 value.
 *
 * On EMBCSV_FLOAT_OK, out_len contains the number of bytes written.
 * On EMBCSV_FLOAT_ENOSPC, out_len contains the required number of bytes and
 * the caller shall ignore the contents of out.
 */
embcsv_float_status_t embcsv_float_format_f32(
    float value,
    uint8_t precision,
    char *out,
    size_t out_capacity,
    size_t *out_len);

/*
 * Format one IEEE-754 binary64 value.
 *
 * On EMBCSV_FLOAT_OK, out_len contains the number of bytes written.
 * On EMBCSV_FLOAT_ENOSPC, out_len contains the required number of bytes and
 * the caller shall ignore the contents of out.
 */
embcsv_float_status_t embcsv_float_format_f64(
    double value,
    uint8_t precision,
    char *out,
    size_t out_capacity,
    size_t *out_len);

#endif /* EMBCSV_FLOAT_H */
