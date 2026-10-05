#include "embcsv_float.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

static uint64_t rng_state =
    UINT64_C(0x9e3779b97f4a7c15);

static uint64_t rng64(void)
{
    uint64_t x = rng_state;

    x ^= x >> 12U;
    x ^= x << 25U;
    x ^= x >> 27U;

    rng_state = x;

    return x * UINT64_C(2685821657736338717);
}

static void format_f32(
    float value,
    uint8_t precision,
    char *out,
    size_t capacity)
{
    size_t len = 0U;

    assert(capacity > 0U);

    assert(
        embcsv_float_format_f32(
            value,
            precision,
            out,
            capacity - 1U,
            &len) ==
        EMBCSV_FLOAT_OK);

    assert(len < capacity);
    out[len] = '\0';
}

static void format_f64(
    double value,
    uint8_t precision,
    char *out,
    size_t capacity)
{
    size_t len = 0U;

    assert(capacity > 0U);

    assert(
        embcsv_float_format_f64(
            value,
            precision,
            out,
            capacity - 1U,
            &len) ==
        EMBCSV_FLOAT_OK);

    assert(len < capacity);
    out[len] = '\0';
}

static void expected_f32(
    float value,
    unsigned precision,
    char *out,
    size_t capacity)
{
    if (isnan(value))
    {
        (void)snprintf(out, capacity, "nan");
        return;
    }

    if (isinf(value))
    {
        (void)snprintf(
            out,
            capacity,
            signbit(value) ? "-inf" : "inf");

        return;
    }

    const double magnitude =
        fabs((double)value);

    if ((magnitude == 0.0) ||
        ((magnitude >= 1e-4) &&
         (magnitude < 1e7)))
    {
        (void)snprintf(
            out,
            capacity,
            "%.*f",
            (int)precision,
            (double)value);
    }
    else
    {
        (void)snprintf(
            out,
            capacity,
            "%.*e",
            (int)precision,
            (double)value);
    }
}

static void expected_f64(
    double value,
    unsigned precision,
    char *out,
    size_t capacity)
{
    if (isnan(value))
    {
        (void)snprintf(out, capacity, "nan");
        return;
    }

    if (isinf(value))
    {
        (void)snprintf(
            out,
            capacity,
            signbit(value) ? "-inf" : "inf");

        return;
    }

    const double magnitude = fabs(value);

    if ((magnitude == 0.0) ||
        ((magnitude >= 1e-4) &&
         (magnitude < 1e7)))
    {
        (void)snprintf(
            out,
            capacity,
            "%.*f",
            (int)precision,
            value);
    }
    else
    {
        (void)snprintf(
            out,
            capacity,
            "%.*e",
            (int)precision,
            value);
    }
}

static void compare_f32(
    uint32_t bits,
    unsigned precision)
{
    float value;
    char actual[64];
    char expected[64];

    memcpy(&value, &bits, sizeof(value));

    format_f32(
        value,
        (uint8_t)precision,
        actual,
        sizeof(actual));

    expected_f32(
        value,
        precision,
        expected,
        sizeof(expected));

    if (strcmp(actual, expected) != 0)
    {
        fprintf(
            stderr,
            "f32 mismatch bits=%08x p=%u actual=[%s] expected=[%s]\n",
            bits,
            precision,
            actual,
            expected);

        assert(false);
    }
}

static void compare_f64(
    uint64_t bits,
    unsigned precision)
{
    double value;
    char actual[64];
    char expected[64];

    memcpy(&value, &bits, sizeof(value));

    format_f64(
        value,
        (uint8_t)precision,
        actual,
        sizeof(actual));

    expected_f64(
        value,
        precision,
        expected,
        sizeof(expected));

    if (strcmp(actual, expected) != 0)
    {
        fprintf(
            stderr,
            "f64 mismatch bits=%016llx p=%u actual=[%s] expected=[%s]\n",
            (unsigned long long)bits,
            precision,
            actual,
            expected);

        assert(false);
    }
}

static void test_rounding_ties_even(void)
{
    char out[64];

    format_f32(2.5f, 0U, out, sizeof(out));
    assert(strcmp(out, "2") == 0);

    format_f32(3.5f, 0U, out, sizeof(out));
    assert(strcmp(out, "4") == 0);

    format_f32(1.125f, 2U, out, sizeof(out));
    assert(strcmp(out, "1.12") == 0);

    format_f32(1.375f, 2U, out, sizeof(out));
    assert(strcmp(out, "1.38") == 0);

    format_f64(2.5, 0U, out, sizeof(out));
    assert(strcmp(out, "2") == 0);

    format_f64(3.5, 0U, out, sizeof(out));
    assert(strcmp(out, "4") == 0);

    format_f64(-2.5, 0U, out, sizeof(out));
    assert(strcmp(out, "-2") == 0);

    format_f64(-3.5, 0U, out, sizeof(out));
    assert(strcmp(out, "-4") == 0);
}

static void test_notation_boundaries(void)
{
    char out[64];

    const float low_f32 =
        nextafterf(1.0e-4f, 0.0f);

    const float high_f32 =
        nextafterf(1.0e-4f, INFINITY);

    format_f32(low_f32, 3U, out, sizeof(out));
    assert(strchr(out, 'e') != NULL);

    format_f32(high_f32, 3U, out, sizeof(out));
    assert(strchr(out, 'e') == NULL);

    const float below_upper_f32 =
        nextafterf(1.0e7f, 0.0f);

    format_f32(
        below_upper_f32,
        3U,
        out,
        sizeof(out));

    assert(strchr(out, 'e') == NULL);

    format_f32(1.0e7f, 3U, out, sizeof(out));
    assert(strchr(out, 'e') != NULL);

    const double below_low_f64 =
        nextafter(1.0e-4, 0.0);

    const double at_low_f64 = 1.0e-4;

    format_f64(
        below_low_f64,
        3U,
        out,
        sizeof(out));

    assert(strchr(out, 'e') != NULL);

    format_f64(
        at_low_f64,
        3U,
        out,
        sizeof(out));

    assert(strchr(out, 'e') == NULL);

    const double below_upper_f64 =
        nextafter(1.0e7, 0.0);

    format_f64(
        below_upper_f64,
        3U,
        out,
        sizeof(out));

    assert(strchr(out, 'e') == NULL);

    format_f64(1.0e7, 3U, out, sizeof(out));
    assert(strchr(out, 'e') != NULL);
}

static void test_specials_and_zero(void)
{
    char out[64];

    format_f32(NAN, 9U, out, sizeof(out));
    assert(strcmp(out, "nan") == 0);

    format_f32(INFINITY, 9U, out, sizeof(out));
    assert(strcmp(out, "inf") == 0);

    format_f32(-INFINITY, 9U, out, sizeof(out));
    assert(strcmp(out, "-inf") == 0);

    format_f32(-0.0f, 3U, out, sizeof(out));
    assert(strcmp(out, "-0.000") == 0);

    format_f64(NAN, 17U, out, sizeof(out));
    assert(strcmp(out, "nan") == 0);

    format_f64(INFINITY, 17U, out, sizeof(out));
    assert(strcmp(out, "inf") == 0);

    format_f64(-INFINITY, 17U, out, sizeof(out));
    assert(strcmp(out, "-inf") == 0);

    format_f64(-0.0, 4U, out, sizeof(out));
    assert(strcmp(out, "-0.0000") == 0);
}

static void test_formatter_contract(void)
{
    char out[64];
    size_t len = 0U;

    assert(
        embcsv_float_format_f32(
            1.25f,
            10U,
            out,
            sizeof(out),
            &len) ==
        EMBCSV_FLOAT_EINVAL);

    assert(
        embcsv_float_format_f64(
            1.25,
            18U,
            out,
            sizeof(out),
            &len) ==
        EMBCSV_FLOAT_EINVAL);

    assert(
        embcsv_float_format_f32(
            1.25f,
            2U,
            NULL,
            sizeof(out),
            &len) ==
        EMBCSV_FLOAT_EINVAL);

    assert(
        embcsv_float_format_f64(
            1.25,
            2U,
            out,
            sizeof(out),
            NULL) ==
        EMBCSV_FLOAT_EINVAL);

    len = 0U;

    assert(
        embcsv_float_format_f64(
            1.25,
            2U,
            out,
            1U,
            &len) ==
        EMBCSV_FLOAT_ENOSPC);

    assert(len == 4U);
}

static void test_edge_patterns(void)
{
    static const uint32_t f32_edges[] =
    {
        UINT32_C(0x00000000),
        UINT32_C(0x80000000),
        UINT32_C(0x00000001),
        UINT32_C(0x80000001),
        UINT32_C(0x007fffff),
        UINT32_C(0x00800000),
        UINT32_C(0x3f000000),
        UINT32_C(0x3fc00000),
        UINT32_C(0x7f7fffff),
        UINT32_C(0x7f800000),
        UINT32_C(0xff800000),
        UINT32_C(0x7fc00000)
    };

    static const uint64_t f64_edges[] =
    {
        UINT64_C(0x0000000000000000),
        UINT64_C(0x8000000000000000),
        UINT64_C(0x0000000000000001),
        UINT64_C(0x8000000000000001),
        UINT64_C(0x000fffffffffffff),
        UINT64_C(0x0010000000000000),
        UINT64_C(0x3fe0000000000000),
        UINT64_C(0x3ff8000000000000),
        UINT64_C(0x7fefffffffffffff),
        UINT64_C(0x7ff0000000000000),
        UINT64_C(0xfff0000000000000),
        UINT64_C(0x7ff8000000000000)
    };

    for (size_t i = 0U;
         i < (sizeof(f32_edges) / sizeof(f32_edges[0]));
         ++i)
    {
        for (unsigned p = 0U; p <= 9U; ++p)
            compare_f32(f32_edges[i], p);
    }

    for (size_t i = 0U;
         i < (sizeof(f64_edges) / sizeof(f64_edges[0]));
         ++i)
    {
        for (unsigned p = 0U; p <= 17U; ++p)
            compare_f64(f64_edges[i], p);
    }
}

static void test_random_differential(void)
{
    for (unsigned i = 0U; i < 200000U; ++i)
    {
        const uint32_t bits = (uint32_t)rng64();
        const unsigned precision =
            (unsigned)(rng64() % 10U);

        compare_f32(bits, precision);
    }

    for (unsigned i = 0U; i < 100000U; ++i)
    {
        const uint64_t bits = rng64();
        const unsigned precision =
            (unsigned)(rng64() % 18U);

        compare_f64(bits, precision);
    }
}

int main(void)
{
    assert(setlocale(LC_NUMERIC, "C") != NULL);

    test_rounding_ties_even();
    test_notation_boundaries();
    test_specials_and_zero();
    test_formatter_contract();
    test_edge_patterns();
    test_random_differential();

    puts("embcsv float formatter tests: PASS");

    return 0;
}
