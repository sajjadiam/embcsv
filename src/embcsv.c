/**
 * @file embcsv.c
 * @brief embcsv implementation skeleton.
 *
 * @details
 * This file intentionally contains only the public API skeleton.
 * Functional implementation is left for incremental development and review.
 */

#include "embcsv.h"

static embcsv_status_t embcsv_validate_config(const embcsv_config_t *cfg) {
    if (cfg == NULL) {
        return EMBCSV_EINVAL;
    }

    if (cfg->struct_size < sizeof(embcsv_config_t)) {
        return EMBCSV_ECONFIG;
    }

    if ((cfg->buffer == NULL) || (cfg->buffer_size == 0U)) {
        return EMBCSV_ECONFIG;
    }

    if ((cfg->slots == NULL) || (cfg->slots_size == 0U)) {
        return EMBCSV_ECONFIG;
    }

    if (cfg->sink.submit == NULL) {
        return EMBCSV_ECONFIG;
    }

    if ((cfg->slot_size == 0U) || (cfg->slot_count == 0U)) {
        return EMBCSV_ECONFIG;
    }

    if ((size_t)cfg->slot_count > (cfg->buffer_size / (size_t)cfg->slot_size)) {
        return EMBCSV_ECONFIG;
    }

    if ((size_t)cfg->slot_count > (cfg->slots_size / sizeof(embcsv_slot_meta_t))) {
        return EMBCSV_ECONFIG;
    }

    return EMBCSV_OK;
}

static void embcsv_reset_runtime(embcsv_t *csv) {
    uint16_t i;

    csv->producer_index = 0U;
    csv->consumer_index = 0U;

    csv->row_has_field = 0U;
    csv->async_status = EMBCSV_OK;

    for (i = 0U; i < csv->slot_count; ++i) {
        csv->slots[i].len = 0U;
        csv->slots[i].state = EMBCSV_SLOT_FREE;
        csv->slots[i].reserved = 0U;
    }

    /*
     * READY is set last so the instance is considered usable
     * only after all runtime state has been initialized.
     */
    csv->state = EMBCSV_STATE_READY;
}

embcsv_status_t embcsv_init(embcsv_t *csv, const embcsv_config_t *cfg) {
    embcsv_status_t status;
    uint16_t i;

    if (csv == NULL) {
        return EMBCSV_EINVAL;
    }

    status = embcsv_validate_config(cfg);

    if (status != EMBCSV_OK) {
        return status;
    }

    csv->buffer = cfg->buffer;
    csv->slots = cfg->slots;
    csv->sink = cfg->sink;

    csv->slot_size = cfg->slot_size;
    csv->slot_count = cfg->slot_count;

    csv->float_precision = cfg->float_precision;
    csv->double_precision = cfg->double_precision;

     /*
     * Completion initialization will be added together with
     * embcsv_on_sink_done().
     */

    embcsv_reset_runtime(csv);

    return EMBCSV_OK;
}

embcsv_status_t embcsv_begin_row(embcsv_t *csv) {
    (void)csv;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_abort_row(embcsv_t *csv) {
    (void)csv;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_end_row(embcsv_t *csv) {
    (void)csv;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_add_string(embcsv_t *csv, const char *value) {
    (void)csv;
    (void)value;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_add_bool(embcsv_t *csv, bool value) {
    (void)csv;
    (void)value;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_add_i32(embcsv_t *csv, int32_t value) {
    (void)csv;
    (void)value;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_add_u32(embcsv_t *csv, uint32_t value) {
    (void)csv;
    (void)value;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_add_i64(embcsv_t *csv, int64_t value) {
    (void)csv;
    (void)value;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_add_u64(embcsv_t *csv, uint64_t value) {
    (void)csv;
    (void)value;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_add_f32(embcsv_t *csv, float value) {
    (void)csv;
    (void)value;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_add_f64(embcsv_t *csv, double value) {
    (void)csv;
    (void)value;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_process(embcsv_t *csv) {
    (void)csv;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

bool embcsv_can_begin_row(const embcsv_t *csv) {
    (void)csv;

    /* TODO: implement */
    return false;
}
