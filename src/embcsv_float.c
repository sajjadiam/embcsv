#include "embcsv_float.h"

#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define EMBCSV_FLOAT_BIG_LIMBS 34U
#define EMBCSV_FLOAT_FIXED_MIN_EXP (-4)
#define EMBCSV_FLOAT_FIXED_MAX_EXP 6

_Static_assert(CHAR_BIT == 8, "embcsv float formatter requires 8-bit bytes");
_Static_assert(sizeof(float) == 4U, "embcsv float formatter requires 32-bit float");
_Static_assert(sizeof(double) == 8U, "embcsv float formatter requires 64-bit double");
_Static_assert(FLT_RADIX == 2, "embcsv float formatter requires binary floating point");
_Static_assert(FLT_MANT_DIG == 24, "embcsv float formatter requires IEEE-754 binary32");
_Static_assert(FLT_MAX_EXP == 128, "embcsv float formatter requires IEEE-754 binary32 exponent range");
_Static_assert(DBL_MANT_DIG == 53, "embcsv float formatter requires IEEE-754 binary64");
_Static_assert(DBL_MAX_EXP == 1024, "embcsv float formatter requires IEEE-754 binary64 exponent range");

typedef struct {
    uint32_t limb[EMBCSV_FLOAT_BIG_LIMBS];
} embcsv_big_t;

typedef enum {
    EMBCSV_FP_ZERO = 0,
    EMBCSV_FP_FINITE,
    EMBCSV_FP_INFINITY,
    EMBCSV_FP_NAN
} embcsv_fp_class_t;

typedef struct {
    bool negative;
    embcsv_fp_class_t classification;
    uint64_t significand;
    int32_t exponent2;
} embcsv_fp_decoded_t;

static void big_zero(embcsv_big_t *value)
{
    memset(value, 0, sizeof(*value));
}

static bool big_is_zero(const embcsv_big_t *value)
{
    for (size_t i = 0U; i < EMBCSV_FLOAT_BIG_LIMBS; ++i) {
        if (value->limb[i] != 0U) return false;
    }
    return true;
}

static void big_from_u64(embcsv_big_t *value, uint64_t input)
{
    big_zero(value);
    value->limb[0] = (uint32_t)input;
    value->limb[1] = (uint32_t)(input >> 32U);
}

static bool big_from_u64_shifted(embcsv_big_t *value, uint64_t input, uint32_t shift)
{
    const uint32_t word_shift = shift / 32U;
    const uint32_t bit_shift = shift % 32U;

    big_zero(value);

    for (uint32_t src = 0U; src < 2U; ++src) {
        const uint32_t part = (uint32_t)(input >> (src * 32U));
        const uint32_t dst = word_shift + src;

        if (part == 0U) continue;
        if (dst >= EMBCSV_FLOAT_BIG_LIMBS) return false;

        value->limb[dst] |= part << bit_shift;

        if (bit_shift != 0U) {
            const uint32_t carry = part >> (32U - bit_shift);

            if (carry != 0U) {
                if ((dst + 1U) >= EMBCSV_FLOAT_BIG_LIMBS) return false;
                value->limb[dst + 1U] |= carry;
            }
        }
    }

    return true;
}

static bool big_pow2(embcsv_big_t *value, uint32_t bit)
{
    const uint32_t word = bit / 32U;
    const uint32_t offset = bit % 32U;

    big_zero(value);

    if (word >= EMBCSV_FLOAT_BIG_LIMBS) return false;

    value->limb[word] = UINT32_C(1) << offset;
    return true;
}

static int big_cmp(const embcsv_big_t *a, const embcsv_big_t *b)
{
    for (size_t i = EMBCSV_FLOAT_BIG_LIMBS; i != 0U; --i) {
        const size_t idx = i - 1U;

        if (a->limb[idx] < b->limb[idx]) return -1;
        if (a->limb[idx] > b->limb[idx]) return 1;
    }

    return 0;
}

static bool big_mul_u32(embcsv_big_t *value, uint32_t multiplier)
{
    uint64_t carry = 0U;

    for (size_t i = 0U; i < EMBCSV_FLOAT_BIG_LIMBS; ++i) {
        const uint64_t product =
            ((uint64_t)value->limb[i] * (uint64_t)multiplier) + carry;

        value->limb[i] = (uint32_t)product;
        carry = product >> 32U;
    }

    return carry == 0U;
}

static bool big_mul_u32_to(
    embcsv_big_t *out,
    const embcsv_big_t *value,
    uint32_t multiplier)
{
    uint64_t carry = 0U;

    for (size_t i = 0U; i < EMBCSV_FLOAT_BIG_LIMBS; ++i) {
        const uint64_t product =
            ((uint64_t)value->limb[i] * (uint64_t)multiplier) + carry;

        out->limb[i] = (uint32_t)product;
        carry = product >> 32U;
    }

    return carry == 0U;
}

static void big_sub(embcsv_big_t *a, const embcsv_big_t *b)
{
    uint64_t borrow = 0U;

    for (size_t i = 0U; i < EMBCSV_FLOAT_BIG_LIMBS; ++i) {
        const uint64_t av = a->limb[i];
        const uint64_t bv = (uint64_t)b->limb[i] + borrow;

        a->limb[i] = (uint32_t)(av - bv);
        borrow = (av < bv) ? 1U : 0U;
    }
}

/*
 * Precondition: 0 <= numerator / denominator < 10.
 */
static uint8_t big_div_digit(
    embcsv_big_t *numerator,
    const embcsv_big_t *denominator)
{
    uint8_t digit = 0U;

    while (big_cmp(numerator, denominator) >= 0) {
        big_sub(numerator, denominator);
        ++digit;

        if (digit > 9U) break;
    }

    return digit;
}

static embcsv_fp_decoded_t decode_f32(float value)
{
    uint32_t bits = 0U;
    embcsv_fp_decoded_t decoded;

    memcpy(&bits, &value, sizeof(bits));

    const uint32_t raw_exp = (bits >> 23U) & UINT32_C(0xFF);
    const uint32_t fraction = bits & UINT32_C(0x007FFFFF);

    decoded.negative = (bits >> 31U) != 0U;
    decoded.significand = 0U;
    decoded.exponent2 = 0;

    if (raw_exp == UINT32_C(0xFF)) {
        decoded.classification =
            (fraction == 0U) ? EMBCSV_FP_INFINITY : EMBCSV_FP_NAN;
    }
    else if (raw_exp == 0U) {
        if (fraction == 0U) {
            decoded.classification = EMBCSV_FP_ZERO;
        }
        else {
            decoded.classification = EMBCSV_FP_FINITE;
            decoded.significand = fraction;
            decoded.exponent2 = -149;
        }
    }
    else {
        decoded.classification = EMBCSV_FP_FINITE;
        decoded.significand =
            UINT64_C(0x00800000) | (uint64_t)fraction;

        decoded.exponent2 =
            (int32_t)raw_exp - 127 - 23;
    }

    return decoded;
}

static embcsv_fp_decoded_t decode_f64(double value)
{
    uint64_t bits = 0U;
    embcsv_fp_decoded_t decoded;

    memcpy(&bits, &value, sizeof(bits));

    const uint32_t raw_exp =
        (uint32_t)((bits >> 52U) & UINT64_C(0x7FF));

    const uint64_t fraction =
        bits & UINT64_C(0x000FFFFFFFFFFFFF);

    decoded.negative = (bits >> 63U) != 0U;
    decoded.significand = 0U;
    decoded.exponent2 = 0;

    if (raw_exp == UINT32_C(0x7FF)) {
        decoded.classification =
            (fraction == 0U) ? EMBCSV_FP_INFINITY : EMBCSV_FP_NAN;
    }
    else if (raw_exp == 0U) {
        if (fraction == 0U) {
            decoded.classification = EMBCSV_FP_ZERO;
        }
        else {
            decoded.classification = EMBCSV_FP_FINITE;
            decoded.significand = fraction;
            decoded.exponent2 = -1074;
        }
    }
    else {
        decoded.classification = EMBCSV_FP_FINITE;

        decoded.significand =
            UINT64_C(0x0010000000000000) | fraction;

        decoded.exponent2 =
            (int32_t)raw_exp - 1023 - 52;
    }

    return decoded;
}

static bool make_rational(
    const embcsv_fp_decoded_t *decoded,
    embcsv_big_t *numerator,
    embcsv_big_t *denominator)
{
    if ((decoded == NULL) ||
        (decoded->classification != EMBCSV_FP_FINITE))
    {
        return false;
    }

    if (decoded->exponent2 >= 0) {
        if (!big_from_u64_shifted(
                numerator,
                decoded->significand,
                (uint32_t)decoded->exponent2))
        {
            return false;
        }

        big_from_u64(denominator, 1U);
    }
    else {
        big_from_u64(numerator, decoded->significand);

        if (!big_pow2(
                denominator,
                (uint32_t)(-decoded->exponent2)))
        {
            return false;
        }
    }

    return true;
}

/*
 * Normalize the exact rational so that:
 *
 *     1 <= numerator / denominator < 10
 *
 * exp10 receives the corresponding decimal exponent.
 */
static bool normalize_decimal(
    embcsv_big_t *numerator,
    embcsv_big_t *denominator,
    int32_t *exp10)
{
    *exp10 = 0;

    for (;;) {
        embcsv_big_t ten_denominator;

        if (!big_mul_u32_to(
                &ten_denominator,
                denominator,
                10U))
        {
            return false;
        }

        if (big_cmp(numerator, &ten_denominator) >= 0) {
            *denominator = ten_denominator;
            ++(*exp10);
        }
        else {
            break;
        }
    }

    while (big_cmp(numerator, denominator) < 0) {
        if (!big_mul_u32(numerator, 10U)) return false;
        --(*exp10);
    }

    return true;
}

static bool round_digits(
    char *digits,
    size_t digit_count,
    uint8_t guard,
    const embcsv_big_t *remainder,
    int32_t *exp10)
{
    bool round_up = false;

    if (guard > 5U) {
        round_up = true;
    }
    else if (guard == 5U) {
        if (!big_is_zero(remainder)) {
            round_up = true;
        }
        else if (digit_count != 0U) {
            round_up =
                (((uint8_t)(digits[digit_count - 1U] - '0')) & 1U) != 0U;
        }
    }

    if (!round_up) return true;

    for (size_t i = digit_count; i != 0U; --i) {
        const size_t idx = i - 1U;

        if (digits[idx] != '9') {
            ++digits[idx];
            return true;
        }

        digits[idx] = '0';
    }

    if (digit_count == 0U) return false;

    digits[0] = '1';

    for (size_t i = 1U; i < digit_count; ++i)
        digits[i] = '0';

    ++(*exp10);

    return true;
}

static bool generate_rounded_digits(
    embcsv_big_t numerator,
    const embcsv_big_t *denominator,
    size_t keep_digits,
    char *digits,
    int32_t *exp10)
{
    for (size_t i = 0U; i < keep_digits; ++i) {
        const uint8_t digit =
            big_div_digit(&numerator, denominator);

        if (digit > 9U) return false;

        digits[i] = (char)('0' + digit);

        if (!big_mul_u32(&numerator, 10U))
            return false;
    }

    const uint8_t guard =
        big_div_digit(&numerator, denominator);

    if (guard > 9U) return false;

    return round_digits(
        digits,
        keep_digits,
        guard,
        &numerator,
        exp10);
}

static size_t write_fixed(
    char *out,
    bool negative,
    const char *digits,
    size_t digit_count,
    int32_t exp10,
    uint8_t precision)
{
    size_t pos = 0U;

    if (negative)
        out[pos++] = '-';

    if (exp10 >= 0) {
        const size_t integer_digits =
            (size_t)exp10 + 1U;

        for (size_t i = 0U; i < integer_digits; ++i)
            out[pos++] =
                (i < digit_count) ? digits[i] : '0';
    }
    else {
        out[pos++] = '0';
    }

    if (precision != 0U) {
        out[pos++] = '.';

        for (uint8_t frac = 1U;
             frac <= precision;
             ++frac)
        {
            const int32_t idx =
                exp10 + (int32_t)frac;

            if ((idx >= 0) &&
                ((size_t)idx < digit_count))
            {
                out[pos++] = digits[(size_t)idx];
            }
            else {
                out[pos++] = '0';
            }
        }
    }

    return pos;
}

static size_t write_scientific(
    char *out,
    bool negative,
    const char *digits,
    size_t digit_count,
    int32_t exp10,
    uint8_t precision)
{
    size_t pos = 0U;
    char exp_digits[4];
    size_t exp_count = 0U;
    uint32_t magnitude;

    if (negative)
        out[pos++] = '-';

    out[pos++] =
        (digit_count != 0U) ? digits[0] : '0';

    if (precision != 0U) {
        out[pos++] = '.';

        for (uint8_t i = 0U;
             i < precision;
             ++i)
        {
            const size_t idx =
                (size_t)i + 1U;

            out[pos++] =
                (idx < digit_count) ? digits[idx] : '0';
        }
    }

    out[pos++] = 'e';

    if (exp10 < 0) {
        out[pos++] = '-';
        magnitude = (uint32_t)(-exp10);
    }
    else {
        out[pos++] = '+';
        magnitude = (uint32_t)exp10;
    }

    if (magnitude < 10U)
        out[pos++] = '0';

    do {
        exp_digits[exp_count++] =
            (char)('0' + (magnitude % 10U));

        magnitude /= 10U;
    }
    while (magnitude != 0U);

    while (exp_count != 0U)
        out[pos++] = exp_digits[--exp_count];

    return pos;
}

static embcsv_float_status_t format_decoded(
    const embcsv_fp_decoded_t *decoded,
    uint8_t precision,
    char *out,
    size_t out_capacity,
    size_t *out_len)
{
    char temp[EMBCSV_FLOAT_BUFFER_SIZE];
    char digits[25];
    size_t len = 0U;

    if ((decoded == NULL) ||
        (out == NULL) ||
        (out_len == NULL))
    {
        return EMBCSV_FLOAT_EINVAL;
    }

    if (decoded->classification == EMBCSV_FP_NAN) {
        memcpy(temp, "nan", 3U);
        len = 3U;
    }
    else if (decoded->classification ==
             EMBCSV_FP_INFINITY)
    {
        if (decoded->negative) {
            memcpy(temp, "-inf", 4U);
            len = 4U;
        }
        else {
            memcpy(temp, "inf", 3U);
            len = 3U;
        }
    }
    else if (decoded->classification ==
             EMBCSV_FP_ZERO)
    {
        len = write_fixed(
            temp,
            decoded->negative,
            NULL,
            0U,
            -1,
            precision);
    }
    else {
        embcsv_big_t numerator;
        embcsv_big_t denominator;
        int32_t exp10;

        if (!make_rational(
                decoded,
                &numerator,
                &denominator))
        {
            return EMBCSV_FLOAT_EINTERNAL;
        }

        if (!normalize_decimal(
                &numerator,
                &denominator,
                &exp10))
        {
            return EMBCSV_FLOAT_EINTERNAL;
        }

        const bool use_fixed =
            (exp10 >= EMBCSV_FLOAT_FIXED_MIN_EXP) &&
            (exp10 <= EMBCSV_FLOAT_FIXED_MAX_EXP);

        if (use_fixed) {
            const int32_t keep_signed =
                exp10 + 1 + (int32_t)precision;

            if (keep_signed < 0) {
                len = write_fixed(
                    temp,
                    decoded->negative,
                    NULL,
                    0U,
                    -1,
                    precision);
            }
            else if (keep_signed == 0) {
                embcsv_big_t remainder =
                    numerator;

                const uint8_t guard =
                    big_div_digit(
                        &remainder,
                        &denominator);

                if (guard > 9U)
                    return EMBCSV_FLOAT_EINTERNAL;

                /*
                 * No significant digit is retained.
                 * The last retained decimal digit is an implicit zero,
                 * therefore an exact 5 tie rounds down (zero is even).
                 */
                const bool round_up =
                    (guard > 5U) ||
                    ((guard == 5U) &&
                     !big_is_zero(&remainder));

                if (round_up) {
                    digits[0] = '1';

                    const int32_t rounded_exp10 =
                        -(int32_t)precision;

                    len = write_fixed(
                        temp,
                        decoded->negative,
                        digits,
                        1U,
                        rounded_exp10,
                        precision);
                }
                else {
                    len = write_fixed(
                        temp,
                        decoded->negative,
                        NULL,
                        0U,
                        -1,
                        precision);
                }
            }
            else {
                const size_t keep_digits =
                    (size_t)keep_signed;

                int32_t rounded_exp10 =
                    exp10;

                if (keep_digits > sizeof(digits))
                    return EMBCSV_FLOAT_EINTERNAL;

                if (!generate_rounded_digits(
                        numerator,
                        &denominator,
                        keep_digits,
                        digits,
                        &rounded_exp10))
                {
                    return EMBCSV_FLOAT_EINTERNAL;
                }

                len = write_fixed(
                    temp,
                    decoded->negative,
                    digits,
                    keep_digits,
                    rounded_exp10,
                    precision);
            }
        }
        else {
            const size_t keep_digits =
                (size_t)precision + 1U;

            int32_t rounded_exp10 =
                exp10;

            if (keep_digits > sizeof(digits))
                return EMBCSV_FLOAT_EINTERNAL;

            if (!generate_rounded_digits(
                    numerator,
                    &denominator,
                    keep_digits,
                    digits,
                    &rounded_exp10))
            {
                return EMBCSV_FLOAT_EINTERNAL;
            }

            len = write_scientific(
                temp,
                decoded->negative,
                digits,
                keep_digits,
                rounded_exp10,
                precision);
        }
    }

    if (len > sizeof(temp))
        return EMBCSV_FLOAT_EINTERNAL;

    *out_len = len;

    if (out_capacity < len)
        return EMBCSV_FLOAT_ENOSPC;

    memcpy(out, temp, len);

    return EMBCSV_FLOAT_OK;
}

embcsv_float_status_t embcsv_float_format_f32(
    float value,
    uint8_t precision,
    char *out,
    size_t out_capacity,
    size_t *out_len)
{
    if (precision >
        EMBCSV_FLOAT_F32_MAX_PRECISION)
    {
        return EMBCSV_FLOAT_EINVAL;
    }

    const embcsv_fp_decoded_t decoded =
        decode_f32(value);

    return format_decoded(
        &decoded,
        precision,
        out,
        out_capacity,
        out_len);
}

embcsv_float_status_t embcsv_float_format_f64(
    double value,
    uint8_t precision,
    char *out,
    size_t out_capacity,
    size_t *out_len)
{
    if (precision >
        EMBCSV_FLOAT_F64_MAX_PRECISION)
    {
        return EMBCSV_FLOAT_EINVAL;
    }

    const embcsv_fp_decoded_t decoded =
        decode_f64(value);

    return format_decoded(
        &decoded,
        precision,
        out,
        out_capacity,
        out_len);
}
