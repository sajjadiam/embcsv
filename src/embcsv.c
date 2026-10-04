/**
 * @file embcsv.c
 * @brief embcsv implementation skeleton.
 *
 * @details
 * This file intentionally contains only the public API skeleton.
 * Functional implementation is left for incremental development and review.
 */

#include "embcsv.h"

embcsv_status_t embcsv_init(embcsv_t *csv, const embcsv_config_t *cfg) {
    if(csv == NULL || cfg == NULL) {
        return EMBCSV_EINVAL;
    }

    if(cfg->struct_size < sizeof(embcsv_config_t)) {
        return EMBCSV_ECONFIG;
    }

    if(cfg->buffer == NULL || cfg->buffer_size == 0) {
        return EMBCSV_ECONFIG;
    }

    if(cfg->slots == NULL || cfg->slots_size == 0) {
        return EMBCSV_ECONFIG;
    }

    if(cfg->sink.submit == NULL) {
        return EMBCSV_ECONFIG;
    }

    if(cfg->slot_count == 0 || cfg->slot_size == 0) {
        return EMBCSV_ECONFIG;
    }

    if(cfg->buffer_size < (size_t)cfg->slot_count * cfg->slot_size) {
        return EMBCSV_ECONFIG;
    }

    if(cfg->slots_size < (size_t)cfg->slot_count * sizeof(embcsv_slot_meta_t)) {
        return EMBCSV_ECONFIG;
    }

    csv->buffer = cfg->buffer;
    csv->slots = cfg->slots;
    csv->sink = cfg->sink;
    csv->slot_size = cfg->slot_size;
    csv->slot_count = cfg->slot_count;
    csv->float_precision = cfg->float_precision;

    return EMBCSV_OK;
}

embcsv_status_t embcsv_begin_row(
    embcsv_t *csv)
{
    (void)csv;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_abort_row(
    embcsv_t *csv)
{
    (void)csv;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_end_row(
    embcsv_t *csv)
{
    (void)csv;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_add_string(
    embcsv_t *csv,
    const char *value)
{
    (void)csv;
    (void)value;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_add_bool(
    embcsv_t *csv,
    bool value)
{
    (void)csv;
    (void)value;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_add_i32(
    embcsv_t *csv,
    int32_t value)
{
    (void)csv;
    (void)value;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_add_u32(
    embcsv_t *csv,
    uint32_t value)
{
    (void)csv;
    (void)value;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_add_i64(
    embcsv_t *csv,
    int64_t value)
{
    (void)csv;
    (void)value;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_add_u64(
    embcsv_t *csv,
    uint64_t value)
{
    (void)csv;
    (void)value;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_add_f32(
    embcsv_t *csv,
    float value)
{
    (void)csv;
    (void)value;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_add_f64(
    embcsv_t *csv,
    double value)
{
    (void)csv;
    (void)value;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

embcsv_status_t embcsv_process(
    embcsv_t *csv)
{
    (void)csv;

    /* TODO: implement */
    return EMBCSV_ESTATE;
}

bool embcsv_can_begin_row(
    const embcsv_t *csv)
{
    (void)csv;

    /* TODO: implement */
    return false;
}
