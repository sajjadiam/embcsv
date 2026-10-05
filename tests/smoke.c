#include "embcsv.h"

#include <assert.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    uint8_t out[4096];
    size_t out_len;

    int async_mode;
    embcsv_sink_done_fn done_fn;
    void *done_ctx;
    embcsv_slot_id_t done_slot;
    const uint8_t *pending_data;
    uint16_t pending_len;
    unsigned submit_count;
} test_sink_ctx_t;

static embcsv_status_t test_submit(
    void *ctx,
    const embcsv_sink_request_t *req,
    const embcsv_sink_completion_t *completion)
{
    test_sink_ctx_t *s = (test_sink_ctx_t *)ctx;

    assert(s != NULL);
    assert(req != NULL);
    assert(completion != NULL);
    assert(req->data != NULL);

    ++s->submit_count;

    if (s->async_mode != 0)
    {
        s->done_fn = completion->fn;
        s->done_ctx = completion->ctx;
        s->done_slot = req->slot_id;
        s->pending_data = req->data;
        s->pending_len = req->len;

        return EMBCSV_PENDING;
    }

    assert((s->out_len + req->len) <= sizeof(s->out));

    memcpy(&s->out[s->out_len], req->data, req->len);
    s->out_len += req->len;

    return EMBCSV_OK;
}

static void complete_async(
    test_sink_ctx_t *sink,
    embcsv_status_t status)
{
    assert(sink != NULL);
    assert(sink->done_fn != NULL);

    if (status == EMBCSV_OK)
    {
        assert(sink->pending_data != NULL);
        assert((sink->out_len + sink->pending_len) <= sizeof(sink->out));

        memcpy(
            &sink->out[sink->out_len],
            sink->pending_data,
            sink->pending_len);

        sink->out_len += sink->pending_len;
    }

    embcsv_sink_done_fn fn = sink->done_fn;
    void *ctx = sink->done_ctx;
    embcsv_slot_id_t slot = sink->done_slot;

    sink->done_fn = NULL;
    sink->done_ctx = NULL;
    sink->pending_data = NULL;
    sink->pending_len = 0U;

    fn(ctx, slot, status);
}

static embcsv_config_t make_config(
    uint8_t *buffer,
    size_t buffer_size,
    embcsv_slot_meta_t *slots,
    size_t slots_size,
    uint16_t slot_size,
    uint16_t slot_count,
    test_sink_ctx_t *sink_ctx)
{
    embcsv_config_t cfg;

    memset(&cfg, 0, sizeof(cfg));

    cfg.struct_size = sizeof(cfg);
    cfg.buffer = buffer;
    cfg.buffer_size = buffer_size;
    cfg.slots = slots;
    cfg.slots_size = slots_size;
    cfg.sink.submit = test_submit;
    cfg.sink.ctx = sink_ctx;
    cfg.slot_size = slot_size;
    cfg.slot_count = slot_count;
    cfg.float_precision = 2U;
    cfg.double_precision = 3U;

    return cfg;
}

static void assert_output(
    const test_sink_ctx_t *sink,
    const char *expected)
{
    const size_t expected_len = strlen(expected);

    assert(sink != NULL);
    assert(expected != NULL);
    assert(sink->out_len == expected_len);
    assert(memcmp(sink->out, expected, expected_len) == 0);
}

static void test_integer_boundaries(void)
{
    uint8_t buffer[256];
    embcsv_slot_meta_t slots[1];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        (uint16_t)sizeof(buffer), 1U, &sink);

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);
    assert(embcsv_begin_row(&csv) == EMBCSV_OK);

    assert(embcsv_add_i32(&csv, INT32_MIN) == EMBCSV_OK);
    assert(embcsv_add_i32(&csv, INT32_MAX) == EMBCSV_OK);
    assert(embcsv_add_u32(&csv, 0U) == EMBCSV_OK);
    assert(embcsv_add_u32(&csv, UINT32_MAX) == EMBCSV_OK);
    assert(embcsv_add_i64(&csv, INT64_MIN) == EMBCSV_OK);
    assert(embcsv_add_i64(&csv, INT64_MAX) == EMBCSV_OK);
    assert(embcsv_add_u64(&csv, UINT64_C(0)) == EMBCSV_OK);
    assert(embcsv_add_u64(&csv, UINT64_MAX) == EMBCSV_OK);
    assert(embcsv_add_bool(&csv, false) == EMBCSV_OK);
    assert(embcsv_add_bool(&csv, true) == EMBCSV_OK);

    assert(embcsv_end_row(&csv) == EMBCSV_OK);
    assert(embcsv_process(&csv) == EMBCSV_OK);

    assert_output(
        &sink,
        "-2147483648,2147483647,0,4294967295,"
        "-9223372036854775808,9223372036854775807,"
        "0,18446744073709551615,false,true\r\n");
}

static void test_strings_empty_and_escape(void)
{
    uint8_t buffer[256];
    embcsv_slot_meta_t slots[2];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        128U, 2U, &sink);

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);

    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "") == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "") == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "a,b") == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "a\"b") == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "a\rb") == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "a\nb") == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "a\r\nb") == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);
    assert(embcsv_process(&csv) == EMBCSV_OK);

    assert_output(
        &sink,
        ",,\"a,b\",\"a\"\"b\",\"a\rb\",\"a\nb\",\"a\r\nb\"\r\n");
}

static void test_empty_row(void)
{
    uint8_t buffer[16];
    embcsv_slot_meta_t slots[1];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        16U, 1U, &sink);

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);
    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);
    assert(embcsv_process(&csv) == EMBCSV_OK);

    assert_output(&sink, "\r\n");
}

static void test_abort_row(void)
{
    uint8_t buffer[64];
    embcsv_slot_meta_t slots[2];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        32U, 2U, &sink);

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);
    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "discard") == EMBCSV_OK);

    assert(embcsv_abort_row(&csv) == EMBCSV_OK);
    assert(csv.producer_index == 0U);
    assert(csv.state == EMBCSV_STATE_READY);
    assert(slots[0].state == EMBCSV_SLOT_FREE);
    assert(slots[0].len == 0U);

    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "kept") == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);
    assert(embcsv_process(&csv) == EMBCSV_OK);

    assert_output(&sink, "kept\r\n");
}

static void test_transactional_string(void)
{
    uint8_t buffer[8] = {0};
    embcsv_slot_meta_t slots[1];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        8U, 1U, &sink);

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);
    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "abc") == EMBCSV_OK);

    uint8_t before[sizeof(buffer)];
    memcpy(before, buffer, sizeof(before));

    const uint16_t before_len = slots[0].len;
    const uint8_t before_has_field = csv.row_has_field;

    assert(
        embcsv_add_string(&csv, "toolong") ==
        EMBCSV_EROW_TOO_LARGE);

    assert(slots[0].len == before_len);
    assert(csv.row_has_field == before_has_field);
    assert(memcmp(buffer, before, sizeof(buffer)) == 0);
}

static void test_transactional_float(void)
{
    uint8_t buffer[7] = {0};
    embcsv_slot_meta_t slots[1];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        7U, 1U, &sink);

    cfg.float_precision = 2U;
    cfg.double_precision = 3U;

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);
    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "x") == EMBCSV_OK);

    uint8_t before[sizeof(buffer)];
    memcpy(before, buffer, sizeof(before));

    const uint16_t before_len = slots[0].len;
    const uint8_t before_has_field = csv.row_has_field;

    assert(
        embcsv_add_f32(&csv, 1.25f) ==
        EMBCSV_EROW_TOO_LARGE);

    assert(slots[0].len == before_len);
    assert(csv.row_has_field == before_has_field);
    assert(memcmp(buffer, before, sizeof(buffer)) == 0);

    assert(
        embcsv_add_f64(&csv, 1.25) ==
        EMBCSV_EROW_TOO_LARGE);

    assert(slots[0].len == before_len);
    assert(csv.row_has_field == before_has_field);
    assert(memcmp(buffer, before, sizeof(buffer)) == 0);
}

static void test_queue_wrap_sync(void)
{
    uint8_t buffer[64];
    embcsv_slot_meta_t slots[2];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        32U, 2U, &sink);

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);

    for (uint32_t i = 0U; i < 6U; ++i)
    {
        assert(embcsv_begin_row(&csv) == EMBCSV_OK);
        assert(embcsv_add_u32(&csv, i) == EMBCSV_OK);
        assert(embcsv_end_row(&csv) == EMBCSV_OK);
        assert(embcsv_process(&csv) == EMBCSV_OK);
    }

    assert_output(
        &sink,
        "0\r\n1\r\n2\r\n3\r\n4\r\n5\r\n");
}

static void test_queue_full(void)
{
    uint8_t buffer[32];
    embcsv_slot_meta_t slots[2];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        16U, 2U, &sink);

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);

    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "A") == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);

    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "B") == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);

    assert(!embcsv_can_begin_row(&csv));
    assert(embcsv_begin_row(&csv) == EMBCSV_ENO_BUFFER);

    assert(embcsv_process(&csv) == EMBCSV_OK);
    assert(embcsv_can_begin_row(&csv));

    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "C") == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);

    assert(embcsv_process(&csv) == EMBCSV_OK);
    assert(embcsv_process(&csv) == EMBCSV_OK);

    assert_output(&sink, "A\r\nB\r\nC\r\n");
}

static void test_async_success_and_ordering(void)
{
    uint8_t buffer[96];
    embcsv_slot_meta_t slots[3];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        32U, 3U, &sink);

    sink.async_mode = 1;

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);

    for (const char *text = "A"; *text <= 'C'; ++text)
    {
        char field[2] = {*text, '\0'};

        assert(embcsv_begin_row(&csv) == EMBCSV_OK);
        assert(embcsv_add_string(&csv, field) == EMBCSV_OK);
        assert(embcsv_end_row(&csv) == EMBCSV_OK);
    }

    assert(embcsv_process(&csv) == EMBCSV_PENDING);
    assert(sink.submit_count == 1U);
    assert(slots[0].state == EMBCSV_SLOT_IN_FLIGHT);

    assert(embcsv_process(&csv) == EMBCSV_PENDING);
    assert(sink.submit_count == 1U);

    complete_async(&sink, EMBCSV_OK);
    assert(csv.consumer_index == 1U);

    assert(embcsv_process(&csv) == EMBCSV_PENDING);
    assert(sink.submit_count == 2U);
    assert(slots[1].state == EMBCSV_SLOT_IN_FLIGHT);

    complete_async(&sink, EMBCSV_OK);
    assert(csv.consumer_index == 2U);

    assert(embcsv_process(&csv) == EMBCSV_PENDING);
    assert(sink.submit_count == 3U);

    complete_async(&sink, EMBCSV_OK);
    assert(csv.consumer_index == 0U);
    assert(embcsv_process(&csv) == EMBCSV_OK);

    assert_output(&sink, "A\r\nB\r\nC\r\n");
}

static void test_producer_while_async_in_flight(void)
{
    uint8_t buffer[64];
    embcsv_slot_meta_t slots[2];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        32U, 2U, &sink);

    sink.async_mode = 1;

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);

    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "A") == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);

    assert(embcsv_process(&csv) == EMBCSV_PENDING);
    assert(slots[0].state == EMBCSV_SLOT_IN_FLIGHT);

    assert(embcsv_can_begin_row(&csv));
    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "B") == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);

    complete_async(&sink, EMBCSV_OK);

    assert(embcsv_process(&csv) == EMBCSV_PENDING);
    complete_async(&sink, EMBCSV_OK);

    assert_output(&sink, "A\r\nB\r\n");
}

static void test_async_error_retry(void)
{
    uint8_t buffer[64];
    embcsv_slot_meta_t slots[2];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        32U, 2U, &sink);

    sink.async_mode = 1;

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);
    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "retry-me") == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);

    assert(embcsv_process(&csv) == EMBCSV_PENDING);
    assert(slots[0].state == EMBCSV_SLOT_IN_FLIGHT);
    assert(sink.submit_count == 1U);

    complete_async(&sink, EMBCSV_EIO);

    assert(slots[0].state == EMBCSV_SLOT_READY);
    assert(csv.consumer_index == 0U);
    assert(csv.async_status == EMBCSV_EIO);

    assert(embcsv_process(&csv) == EMBCSV_EIO);
    assert(csv.async_status == EMBCSV_OK);
    assert(slots[0].state == EMBCSV_SLOT_READY);
    assert(sink.submit_count == 1U);

    assert(embcsv_process(&csv) == EMBCSV_PENDING);
    assert(sink.submit_count == 2U);

    complete_async(&sink, EMBCSV_OK);

    assert(slots[0].state == EMBCSV_SLOT_FREE);
    assert(csv.consumer_index == 1U);
    assert_output(&sink, "retry-me\r\n");
}

static void test_invalid_async_completion_status(void)
{
    uint8_t buffer[32];
    embcsv_slot_meta_t slots[1];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        32U, 1U, &sink);

    sink.async_mode = 1;

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);
    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "row") == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);

    assert(embcsv_process(&csv) == EMBCSV_PENDING);

    complete_async(&sink, EMBCSV_PENDING);

    assert(csv.async_status == EMBCSV_ESTATE);
    assert(slots[0].state == EMBCSV_SLOT_READY);
    assert(embcsv_process(&csv) == EMBCSV_ESTATE);

    assert(embcsv_process(&csv) == EMBCSV_PENDING);
    complete_async(&sink, EMBCSV_OK);

    assert_output(&sink, "row\r\n");

    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "row2") == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);
    assert(embcsv_process(&csv) == EMBCSV_PENDING);

    complete_async(&sink, (embcsv_status_t)2);

    assert(csv.async_status == EMBCSV_ESTATE);
    assert(slots[0].state == EMBCSV_SLOT_READY);
    assert(embcsv_process(&csv) == EMBCSV_ESTATE);

    assert(embcsv_process(&csv) == EMBCSV_PENDING);
    complete_async(&sink, EMBCSV_OK);

    assert_output(&sink, "row\r\nrow2\r\n");
}

static void test_invalid_async_slot_id(void)
{
    uint8_t buffer[64];
    embcsv_slot_meta_t slots[2];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        32U, 2U, &sink);

    sink.async_mode = 1;

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);
    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "row") == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);
    assert(embcsv_process(&csv) == EMBCSV_PENDING);

    assert(sink.done_fn != NULL);

    sink.done_fn(
        sink.done_ctx,
        (embcsv_slot_id_t)1U,
        EMBCSV_OK);

    assert(csv.async_status == EMBCSV_ESTATE);
    assert(slots[0].state == EMBCSV_SLOT_IN_FLIGHT);

    assert(embcsv_process(&csv) == EMBCSV_ESTATE);
    assert(embcsv_process(&csv) == EMBCSV_PENDING);

    complete_async(&sink, EMBCSV_OK);

    assert(slots[0].state == EMBCSV_SLOT_FREE);
    assert_output(&sink, "row\r\n");
}

static void test_float_specials_and_negative_zero(void)
{
    uint8_t buffer[256];
    embcsv_slot_meta_t slots[2];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        128U, 2U, &sink);

    cfg.float_precision = 2U;
    cfg.double_precision = 3U;

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);
    assert(embcsv_begin_row(&csv) == EMBCSV_OK);

    assert(embcsv_add_f32(&csv, -0.0f) == EMBCSV_OK);
    assert(embcsv_add_f32(&csv, NAN) == EMBCSV_OK);
    assert(embcsv_add_f32(&csv, INFINITY) == EMBCSV_OK);
    assert(embcsv_add_f64(&csv, -INFINITY) == EMBCSV_OK);
    assert(embcsv_add_f64(&csv, -0.0) == EMBCSV_OK);

    assert(embcsv_end_row(&csv) == EMBCSV_OK);
    assert(embcsv_process(&csv) == EMBCSV_OK);

    assert_output(
        &sink,
        "-0.00,nan,inf,-inf,-0.000\r\n");
}

static void test_float_locale_independence(void)
{
    uint8_t buffer[64];
    embcsv_slot_meta_t slots[1];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        64U, 1U, &sink);

    const char *loc =
        setlocale(LC_NUMERIC, "de_DE.UTF-8");

    assert(loc != NULL);

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);
    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_f32(&csv, 1.25f) == EMBCSV_OK);
    assert(embcsv_add_f64(&csv, 1.25) == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);
    assert(embcsv_process(&csv) == EMBCSV_OK);

    assert_output(&sink, "1.25,1.250\r\n");

    assert(setlocale(LC_NUMERIC, "C") != NULL);
}

static void test_config_validation(void)
{
    uint8_t buffer[64];
    embcsv_slot_meta_t slots[2];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t valid = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        32U, 2U, &sink);

    assert(embcsv_init(NULL, &valid) == EMBCSV_EINVAL);
    assert(embcsv_init(&csv, NULL) == EMBCSV_EINVAL);

#define EXPECT_BAD_CONFIG(statement)          \
    do                                        \
    {                                         \
        embcsv_config_t cfg = valid;          \
        statement;                            \
        assert(embcsv_init(&csv, &cfg) ==     \
               EMBCSV_ECONFIG);               \
    } while (0)

    EXPECT_BAD_CONFIG(cfg.struct_size = sizeof(cfg) - 1U);
    EXPECT_BAD_CONFIG(cfg.buffer = NULL);
    EXPECT_BAD_CONFIG(cfg.buffer_size = 0U);
    EXPECT_BAD_CONFIG(cfg.slots = NULL);
    EXPECT_BAD_CONFIG(cfg.slots_size = 0U);
    EXPECT_BAD_CONFIG(cfg.sink.submit = NULL);
    EXPECT_BAD_CONFIG(cfg.slot_size = 1U);
    EXPECT_BAD_CONFIG(cfg.slot_count = 0U);
    EXPECT_BAD_CONFIG(cfg.buffer_size = 63U);
    EXPECT_BAD_CONFIG(cfg.slots_size = sizeof(embcsv_slot_meta_t));
    EXPECT_BAD_CONFIG(cfg.float_precision = 10U);
    EXPECT_BAD_CONFIG(cfg.double_precision = 18U);

#undef EXPECT_BAD_CONFIG

    valid.float_precision = 9U;
    valid.double_precision = 17U;

    assert(embcsv_init(&csv, &valid) == EMBCSV_OK);
}

static void test_api_misuse(void)
{
    uint8_t buffer[64];
    embcsv_slot_meta_t slots[2];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};

    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        32U, 2U, &sink);

    assert(embcsv_begin_row(NULL) == EMBCSV_EINVAL);
    assert(embcsv_abort_row(NULL) == EMBCSV_EINVAL);
    assert(embcsv_end_row(NULL) == EMBCSV_EINVAL);
    assert(embcsv_add_string(NULL, "x") == EMBCSV_EINVAL);
    assert(embcsv_add_bool(NULL, true) == EMBCSV_EINVAL);
    assert(embcsv_add_i32(NULL, 0) == EMBCSV_EINVAL);
    assert(embcsv_add_u32(NULL, 0U) == EMBCSV_EINVAL);
    assert(embcsv_add_i64(NULL, 0) == EMBCSV_EINVAL);
    assert(embcsv_add_u64(NULL, 0U) == EMBCSV_EINVAL);
    assert(embcsv_add_f32(NULL, 0.0f) == EMBCSV_EINVAL);
    assert(embcsv_add_f64(NULL, 0.0) == EMBCSV_EINVAL);
    assert(embcsv_process(NULL) == EMBCSV_EINVAL);
    assert(!embcsv_can_begin_row(NULL));

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);

    assert(embcsv_abort_row(&csv) == EMBCSV_ESTATE);
    assert(embcsv_end_row(&csv) == EMBCSV_ESTATE);
    assert(embcsv_add_string(&csv, "x") == EMBCSV_ESTATE);
    assert(embcsv_add_string(&csv, NULL) == EMBCSV_EINVAL);
    assert(embcsv_add_bool(&csv, true) == EMBCSV_ESTATE);
    assert(embcsv_add_i32(&csv, 0) == EMBCSV_ESTATE);
    assert(embcsv_add_u32(&csv, 0U) == EMBCSV_ESTATE);
    assert(embcsv_add_i64(&csv, 0) == EMBCSV_ESTATE);
    assert(embcsv_add_u64(&csv, 0U) == EMBCSV_ESTATE);
    assert(embcsv_add_f32(&csv, 0.0f) == EMBCSV_ESTATE);
    assert(embcsv_add_f64(&csv, 0.0) == EMBCSV_ESTATE);

    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_begin_row(&csv) == EMBCSV_ESTATE);

    /* Processing a producer slot that is still being filled is a no-op. */
    assert(embcsv_process(&csv) == EMBCSV_OK);

    assert(embcsv_abort_row(&csv) == EMBCSV_OK);
}

int main(void)
{
    test_integer_boundaries();
    test_strings_empty_and_escape();
    test_empty_row();
    test_abort_row();
    test_transactional_string();
    test_transactional_float();
    test_queue_wrap_sync();
    test_queue_full();
    test_async_success_and_ordering();
    test_producer_while_async_in_flight();
    test_async_error_retry();
    test_invalid_async_completion_status();
    test_invalid_async_slot_id();
    test_float_specials_and_negative_zero();
    test_float_locale_independence();
    test_config_validation();
    test_api_misuse();

    puts("embcsv core smoke tests: PASS");

    return 0;
}
