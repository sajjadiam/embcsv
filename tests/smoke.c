#include "embcsv.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <math.h>

typedef struct
{
    uint8_t out[1024];
    size_t out_len;

    int async_mode;
    embcsv_sink_done_fn done_fn;
    void *done_ctx;
    embcsv_slot_id_t done_slot;
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

    s->submit_count++;

    if (s->async_mode != 0)
    {
        s->done_fn = completion->fn;
        s->done_ctx = completion->ctx;
        s->done_slot = req->slot_id;
        return EMBCSV_PENDING;
    }

    assert((s->out_len + req->len) <= sizeof(s->out));
    memcpy(&s->out[s->out_len], req->data, req->len);
    s->out_len += req->len;

    return EMBCSV_OK;
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

static void test_sync_basic(void)
{
    uint8_t buffer[3U * 64U];
    embcsv_slot_meta_t slots[3];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};
    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        64U, 3U, &sink);

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);
    assert(embcsv_can_begin_row(&csv));

    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "hello,world") == EMBCSV_OK);
    assert(embcsv_add_i32(&csv, -2147483647 - 1) == EMBCSV_OK);
    assert(embcsv_add_u32(&csv, UINT32_MAX) == EMBCSV_OK);
    assert(embcsv_add_bool(&csv, true) == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);

    assert(embcsv_process(&csv) == EMBCSV_OK);

    {
        const char expected[] =
            "\"hello,world\",-2147483648,4294967295,true\r\n";
        assert(sink.out_len == (sizeof(expected) - 1U));
        assert(memcmp(sink.out, expected, sizeof(expected) - 1U) == 0);
    }
}

static void test_escape_and_abort(void)
{
    uint8_t buffer[2U * 48U];
    embcsv_slot_meta_t slots[2];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};
    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        48U, 2U, &sink);

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);

    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "a\"b") == EMBCSV_OK);
    assert(embcsv_abort_row(&csv) == EMBCSV_OK);
    assert(csv.producer_index == 0U);
    assert(slots[0].state == EMBCSV_SLOT_FREE);

    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_string(&csv, "a\"b") == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);
    assert(embcsv_process(&csv) == EMBCSV_OK);

    {
        const char expected[] = "\"a\"\"b\"\r\n";
        assert(sink.out_len == (sizeof(expected) - 1U));
        assert(memcmp(sink.out, expected, sizeof(expected) - 1U) == 0);
    }
}

static void test_capacity_transactional(void)
{
    uint8_t buffer[8];
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

    {
        const uint16_t before_len = slots[0].len;
        const uint8_t before0 = buffer[0];
        const uint8_t before1 = buffer[1];
        const uint8_t before2 = buffer[2];

        assert(embcsv_add_string(&csv, "toolong") == EMBCSV_EROW_TOO_LARGE);
        assert(slots[0].len == before_len);
        assert(buffer[0] == before0);
        assert(buffer[1] == before1);
        assert(buffer[2] == before2);
    }
}

static void test_queue_wrap_sync(void)
{
    uint8_t buffer[2U * 32U];
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

    {
        const char expected[] = "0\r\n1\r\n2\r\n3\r\n4\r\n5\r\n";
        assert(sink.out_len == (sizeof(expected) - 1U));
        assert(memcmp(sink.out, expected, sizeof(expected) - 1U) == 0);
    }
}

static void test_async_success(void)
{
    uint8_t buffer[2U * 32U];
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
    assert(embcsv_add_string(&csv, "async") == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);

    assert(embcsv_process(&csv) == EMBCSV_PENDING);
    assert(slots[0].state == EMBCSV_SLOT_IN_FLIGHT);
    assert(sink.done_fn != NULL);

    sink.done_fn(sink.done_ctx, sink.done_slot, EMBCSV_OK);

    assert(slots[0].state == EMBCSV_SLOT_FREE);
    assert(csv.consumer_index == 1U);
    assert(embcsv_process(&csv) == EMBCSV_OK);
}


static void test_async_error_retry(void)
{
    uint8_t buffer[2U * 32U];
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

    sink.done_fn(sink.done_ctx, sink.done_slot, EMBCSV_EIO);

    assert(slots[0].state == EMBCSV_SLOT_READY);
    assert(csv.consumer_index == 0U);
    assert(csv.async_status == EMBCSV_EIO);

    /* First call reports the latched error and does not retry yet. */
    assert(embcsv_process(&csv) == EMBCSV_EIO);
    assert(csv.async_status == EMBCSV_OK);
    assert(slots[0].state == EMBCSV_SLOT_READY);
    assert(sink.submit_count == 1U);

    /* Next call retries the preserved record. */
    assert(embcsv_process(&csv) == EMBCSV_PENDING);
    assert(slots[0].state == EMBCSV_SLOT_IN_FLIGHT);
    assert(sink.submit_count == 2U);

    sink.done_fn(sink.done_ctx, sink.done_slot, EMBCSV_OK);

    assert(slots[0].state == EMBCSV_SLOT_FREE);
    assert(csv.consumer_index == 1U);
    assert(embcsv_process(&csv) == EMBCSV_OK);
}

static void test_float_specials_c_locale(void)
{
    uint8_t buffer[4U * 64U];
    embcsv_slot_meta_t slots[4];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};
    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        64U, 4U, &sink);

    assert(setlocale(LC_NUMERIC, "C") != NULL);
    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);

    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_f32(&csv, NAN) == EMBCSV_OK);
    assert(embcsv_add_f32(&csv, INFINITY) == EMBCSV_OK);
    assert(embcsv_add_f64(&csv, -INFINITY) == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);
    assert(embcsv_process(&csv) == EMBCSV_OK);

    {
        const char expected[] = "nan,inf,-inf\r\n";
        assert(sink.out_len == (sizeof(expected) - 1U));
        assert(memcmp(sink.out, expected, sizeof(expected) - 1U) == 0);
    }
}

static void test_float_locale_hazard(void)
{
    uint8_t buffer[2U * 64U];
    embcsv_slot_meta_t slots[2];
    embcsv_t csv;
    test_sink_ctx_t sink = {0};
    embcsv_config_t cfg = make_config(
        buffer, sizeof(buffer),
        slots, sizeof(slots),
        64U, 2U, &sink);

    const char *loc = setlocale(LC_NUMERIC, "de_DE.UTF-8");
    assert(loc != NULL);

    assert(embcsv_init(&csv, &cfg) == EMBCSV_OK);
    assert(embcsv_begin_row(&csv) == EMBCSV_OK);
    assert(embcsv_add_f32(&csv, 1.25f) == EMBCSV_OK);
    assert(embcsv_end_row(&csv) == EMBCSV_OK);
    assert(embcsv_process(&csv) == EMBCSV_OK);

    /*
     * Float formatting is locale-independent. The decimal separator must
     * remain '.' even when LC_NUMERIC uses a comma decimal separator.
     */
    {
        const char expected[] = "1.25\r\n";
        assert(sink.out_len == (sizeof(expected) - 1U));
        assert(memcmp(sink.out, expected, sizeof(expected) - 1U) == 0);
    }

    assert(setlocale(LC_NUMERIC, "C") != NULL);
}

int main(void)
{
    test_sync_basic();
    test_escape_and_abort();
    test_capacity_transactional();
    test_queue_wrap_sync();
    test_async_success();
    test_async_error_retry();
    test_float_specials_c_locale();
    test_float_locale_hazard();

    puts("embcsv smoke tests: PASS");
    return 0;
}
