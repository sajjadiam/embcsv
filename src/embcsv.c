/**
 * @file embcsv.c
 * @brief embcsv implementation skeleton.
 *
 * @details
 * This file intentionally contains only the public API skeleton.
 * Functional implementation is left for incremental development and review.
 */

#include "embcsv.h"
#include <stdio.h>

#define EMBCSV_MAX_F32_PRECISION 9U
#define EMBCSV_F32_TMP_SIZE      64U

static embcsv_status_t embcsv_validate_config(const embcsv_config_t *cfg) {
    if (!cfg) return EMBCSV_EINVAL;
    if (cfg->struct_size < sizeof(embcsv_config_t)) return EMBCSV_ECONFIG;
    if ((!cfg->buffer) || (cfg->buffer_size == 0U)) return EMBCSV_ECONFIG;
    if ((!cfg->slots) || (cfg->slots_size == 0U))   return EMBCSV_ECONFIG;
    if (!cfg->sink.submit) return EMBCSV_ECONFIG;
    if ((cfg->slot_size < 2U) || (cfg->slot_count == 0U)) return EMBCSV_ECONFIG;
    if ((size_t)cfg->slot_count > (cfg->buffer_size / (size_t)cfg->slot_size)) return EMBCSV_ECONFIG;
    if ((size_t)cfg->slot_count > (cfg->slots_size / sizeof(embcsv_slot_meta_t))) return EMBCSV_ECONFIG;
    if (cfg->float_precision > EMBCSV_MAX_F32_PRECISION) return EMBCSV_ECONFIG;

    return EMBCSV_OK;
}

static void embcsv_reset_runtime(embcsv_t *csv) {
    csv->producer_index = 0U;
    csv->consumer_index = 0U;

    csv->row_has_field  = 0U;
    csv->async_status   = EMBCSV_OK;

    for (uint16_t i = 0U; i < csv->slot_count; ++i) {
        csv->slots[i].len       = 0U;
        csv->slots[i].state     = EMBCSV_SLOT_FREE;
        csv->slots[i].reserved  = 0U;
    }

    /*
     * READY is set last so the instance is considered usable
     * only after all runtime state has been initialized.
     */
    csv->state = EMBCSV_STATE_READY;
}

embcsv_status_t embcsv_init(embcsv_t *csv, const embcsv_config_t *cfg) {
    if (!csv) return EMBCSV_EINVAL;

    embcsv_status_t status = embcsv_validate_config(cfg);
    if (status != EMBCSV_OK) return status;

    csv->buffer             = cfg->buffer;
    csv->slots              = cfg->slots;
    csv->sink               = cfg->sink;
    csv->slot_size          = cfg->slot_size;
    csv->slot_count         = cfg->slot_count;
    csv->float_precision    = cfg->float_precision;
    csv->double_precision   = cfg->double_precision;

     /*
     * Completion initialization will be added together with
     * embcsv_on_sink_done().
     */
    embcsv_reset_runtime(csv);

    return EMBCSV_OK;
}

embcsv_status_t embcsv_begin_row(embcsv_t *csv) {
    if(!csv) return EMBCSV_EINVAL;

    if(csv->state != EMBCSV_STATE_READY) return EMBCSV_ESTATE;

    embcsv_slot_meta_t *slot = &csv->slots[csv->producer_index];
    if (slot->state != EMBCSV_SLOT_FREE) return EMBCSV_ENO_BUFFER;

    slot->len           = 0U;
    slot->state         = EMBCSV_SLOT_FILLING;
    csv->row_has_field  = 0U;
    csv->state          = EMBCSV_STATE_BUILDING_ROW;

    return EMBCSV_OK;
}

embcsv_status_t embcsv_abort_row(embcsv_t *csv) {
    if (!csv) return EMBCSV_EINVAL;
    if (csv->state != EMBCSV_STATE_BUILDING_ROW) return EMBCSV_ESTATE;

    embcsv_slot_meta_t *slot = &csv->slots[csv->producer_index];
    if (slot->state != EMBCSV_SLOT_FILLING) return EMBCSV_ESTATE;

    slot->len           = 0U;
    slot->state         = EMBCSV_SLOT_FREE;
    csv->row_has_field  = 0U;
    csv->state          = EMBCSV_STATE_READY;

    return EMBCSV_OK;
}

embcsv_status_t embcsv_end_row(embcsv_t *csv) {
    if (!csv) return EMBCSV_EINVAL;
    if (csv->state != EMBCSV_STATE_BUILDING_ROW) return EMBCSV_ESTATE;

    embcsv_slot_meta_t *slot = &csv->slots[csv->producer_index];
    if (slot->state != EMBCSV_SLOT_FILLING) return EMBCSV_ESTATE;

    if ((slot->len > csv->slot_size) || ((uint16_t)(csv->slot_size - slot->len) < 2U)) 
        return EMBCSV_EROW_TOO_LARGE;

    uint8_t *payload = csv->buffer + ((size_t)csv->producer_index * (size_t)csv->slot_size);

    payload[slot->len++] = '\r';
    payload[slot->len++] = '\n';

    slot->state         = EMBCSV_SLOT_READY;
    csv->row_has_field  = 0U;
    csv->producer_index++;

    if (csv->producer_index >= csv->slot_count) csv->producer_index = 0U;

    csv->state = EMBCSV_STATE_READY;

    return EMBCSV_OK;
}

embcsv_status_t embcsv_add_string(embcsv_t *csv, const char *value) {
    size_t raw_len = 0U;
    size_t quote_count = 0U;
    bool needs_quotes = false;

    if (!csv || !value) return EMBCSV_EINVAL;
    if (csv->state != EMBCSV_STATE_BUILDING_ROW) return EMBCSV_ESTATE;

    embcsv_slot_meta_t *slot = &csv->slots[csv->producer_index];
    if (slot->state != EMBCSV_SLOT_FILLING) return EMBCSV_ESTATE;

    /*
     * First pass:
     * determine the encoded CSV size without modifying the row.
     */
    for (const char *src = value; *src != '\0'; ++src) {
        ++raw_len;

        if (*src == '"') {
            ++quote_count;
            needs_quotes = true;
        }
        else if ((*src == ',') || (*src == '\r') || (*src == '\n'))  needs_quotes = true;
    }

    size_t encoded_len = raw_len + quote_count;

    if (needs_quotes) encoded_len += 2U;

    size_t required = encoded_len;

    if (csv->row_has_field != 0U) required += 1U;

    /*
     * Two bytes must always remain available for CRLF.
     */
    if ((slot->len > csv->slot_size) || (csv->slot_size < 2U) ||
        (required > ((size_t)csv->slot_size - (size_t)slot->len - 2U)))
    {
        return EMBCSV_EROW_TOO_LARGE;
    }

    uint8_t *payload = csv->buffer + ((size_t)csv->producer_index * (size_t)csv->slot_size);

    size_t write_pos = slot->len;

    /*
     * From this point onward capacity is guaranteed.
     */

    if (csv->row_has_field != 0U) payload[write_pos++] = ',';
    if (needs_quotes) payload[write_pos++] = '"';

    for (const char *src = value; *src != '\0'; ++src) {
        if (*src == '"') {
            payload[write_pos++] = '"';
            payload[write_pos++] = '"';
        }
        else payload[write_pos++] = (uint8_t)*src;
    }

    if (needs_quotes) payload[write_pos++] = '"';

    slot->len = (uint16_t)write_pos;
    csv->row_has_field = 1U;

    return EMBCSV_OK;
}

embcsv_status_t embcsv_add_bool(embcsv_t *csv, bool value) {
    const char *text;
    size_t text_len;

    if (csv == NULL) return EMBCSV_EINVAL;
    if (csv->state != EMBCSV_STATE_BUILDING_ROW) return EMBCSV_ESTATE;

    embcsv_slot_meta_t *slot = &csv->slots[csv->producer_index];
    if (slot->state != EMBCSV_SLOT_FILLING) return EMBCSV_ESTATE;

    if (value) {
        text = "true";
        text_len = 4U;
    }
    else {
        text = "false";
        text_len = 5U;
    }

    size_t required = text_len;

    if (csv->row_has_field != 0U) required += 1U;

    if ((slot->len > csv->slot_size) || (csv->slot_size < 2U) ||
        (required > ((size_t)csv->slot_size - (size_t)slot->len - 2U)))
    {
        return EMBCSV_EROW_TOO_LARGE;
    }

    uint8_t *payload = csv->buffer + ((size_t)csv->producer_index * (size_t)csv->slot_size);
    size_t write_pos = slot->len;

    if (csv->row_has_field != 0U) payload[write_pos++] = ',';

    for (size_t i = 0U; i < text_len; ++i) {
        payload[write_pos++] = (uint8_t)text[i];
    }

    slot->len = (uint16_t)write_pos;
    csv->row_has_field = 1U;

    return EMBCSV_OK;
}

embcsv_status_t embcsv_add_i32(embcsv_t *csv, int32_t value) {
    char tmp[11];
    size_t len = 0U;
    uint32_t magnitude;

    if (csv == NULL) return EMBCSV_EINVAL;

    if (csv->state != EMBCSV_STATE_BUILDING_ROW) return EMBCSV_ESTATE;

    embcsv_slot_meta_t *slot = &csv->slots[csv->producer_index];
    if (slot->state != EMBCSV_SLOT_FILLING) return EMBCSV_ESTATE;

    bool negative = (value < 0);

    if (negative) magnitude = (uint32_t)(-(int64_t)value);
    else magnitude = (uint32_t)value;

    do {
        tmp[len++] = (char)('0' + (magnitude % 10U));
        magnitude /= 10U;
    }
    while (magnitude != 0U);

    if (negative) tmp[len++] = '-';

    size_t required = len;

    if (csv->row_has_field != 0U) required += 1U;

    if ((slot->len > csv->slot_size) || (csv->slot_size < 2U) ||
        (required > ((size_t)csv->slot_size - (size_t)slot->len - 2U)))
    {
        return EMBCSV_EROW_TOO_LARGE;
    }

    uint8_t *payload = csv->buffer + ((size_t)csv->producer_index * (size_t)csv->slot_size);

    size_t write_pos = slot->len;

    if (csv->row_has_field != 0U) payload[write_pos++] = ',';

    while (len != 0U) payload[write_pos++] = (uint8_t)tmp[--len];

    slot->len = (uint16_t)write_pos;
    csv->row_has_field = 1U;

    return EMBCSV_OK;
}

embcsv_status_t embcsv_add_u32(embcsv_t *csv, uint32_t value) {
    char tmp[10];
    size_t len = 0U;

    if (csv == NULL) return EMBCSV_EINVAL;

    if (csv->state != EMBCSV_STATE_BUILDING_ROW) return EMBCSV_ESTATE;

    embcsv_slot_meta_t *slot = &csv->slots[csv->producer_index];

    if (slot->state != EMBCSV_SLOT_FILLING) return EMBCSV_ESTATE;

    do {
        tmp[len++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (value != 0U);

    size_t required = len;

    if (csv->row_has_field != 0U) required += 1U;

    if ((slot->len > csv->slot_size) || (csv->slot_size < 2U) ||
        (required > ((size_t)csv->slot_size - (size_t)slot->len - 2U)))
    {
        return EMBCSV_EROW_TOO_LARGE;
    }

    uint8_t *payload = csv->buffer + ((size_t)csv->producer_index * (size_t)csv->slot_size);

    size_t write_pos = slot->len;

    if (csv->row_has_field != 0U) payload[write_pos++] = ',';

    while (len != 0U) payload[write_pos++] = (uint8_t)tmp[--len];

    slot->len = (uint16_t)write_pos;
    csv->row_has_field = 1U;

    return EMBCSV_OK;
}

embcsv_status_t embcsv_add_i64(embcsv_t *csv, int64_t value) {
    char tmp[20];
    size_t len = 0U;
    uint64_t magnitude;

    if (csv == NULL) return EMBCSV_EINVAL;

    if (csv->state != EMBCSV_STATE_BUILDING_ROW) return EMBCSV_ESTATE;

    embcsv_slot_meta_t *slot = &csv->slots[csv->producer_index];

    if (slot->state != EMBCSV_SLOT_FILLING) return EMBCSV_ESTATE;

    bool negative = (value < 0);

    if (negative) magnitude = (uint64_t)(-(value + 1)) + 1U;
    else magnitude = (uint64_t)value;

    do {
        tmp[len++] = (char)('0' + (magnitude % 10U));
        magnitude /= 10U;
    }
    while (magnitude != 0U);

    if (negative) tmp[len++] = '-';

    size_t required = len;

    if (csv->row_has_field != 0U) required += 1U;

    if ((slot->len > csv->slot_size) || (csv->slot_size < 2U) ||
        (required > ((size_t)csv->slot_size - (size_t)slot->len - 2U)))
    {
        return EMBCSV_EROW_TOO_LARGE;
    }

    uint8_t *payload = csv->buffer + ((size_t)csv->producer_index * (size_t)csv->slot_size);

    size_t write_pos = slot->len;

    if (csv->row_has_field != 0U) payload[write_pos++] = ',';

    while (len != 0U) payload[write_pos++] = (uint8_t)tmp[--len];

    slot->len = (uint16_t)write_pos;
    csv->row_has_field = 1U;

    return EMBCSV_OK;
}

embcsv_status_t embcsv_add_u64(embcsv_t *csv, uint64_t value) {
    char tmp[20];
    size_t len = 0U;

    if (csv == NULL) return EMBCSV_EINVAL;

    if (csv->state != EMBCSV_STATE_BUILDING_ROW) return EMBCSV_ESTATE;

    embcsv_slot_meta_t *slot = &csv->slots[csv->producer_index];

    if (slot->state != EMBCSV_SLOT_FILLING) return EMBCSV_ESTATE;

    do {
        tmp[len++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (value != 0U);

    size_t required = len;

    if (csv->row_has_field != 0U) required += 1U;

    if ((slot->len > csv->slot_size) || (csv->slot_size < 2U) ||
        (required > ((size_t)csv->slot_size - (size_t)slot->len - 2U)))
    {
        return EMBCSV_EROW_TOO_LARGE;
    }

    uint8_t *payload = csv->buffer + ((size_t)csv->producer_index * (size_t)csv->slot_size);

    size_t write_pos = slot->len;

    if (csv->row_has_field != 0U) payload[write_pos++] = ',';

    while (len != 0U) payload[write_pos++] = (uint8_t)tmp[--len];

    slot->len = (uint16_t)write_pos;
    csv->row_has_field = 1U;

    return EMBCSV_OK;
}

embcsv_status_t embcsv_add_f32(embcsv_t *csv, float value) {
    char tmp[EMBCSV_F32_TMP_SIZE];

    if (csv == NULL) return EMBCSV_EINVAL;

    if (csv->state != EMBCSV_STATE_BUILDING_ROW) return EMBCSV_ESTATE;

    embcsv_slot_meta_t *slot = &csv->slots[csv->producer_index];

    if (slot->state != EMBCSV_SLOT_FILLING) return EMBCSV_ESTATE;

    int formatted_len = snprintf(tmp, sizeof(tmp), "%.*f", (int)csv->float_precision, (double)value);

    if ((formatted_len < 0) || ((size_t)formatted_len >= sizeof(tmp))) return EMBCSV_EINVAL;

    size_t len = (size_t)formatted_len;
    size_t required = len;

    if (csv->row_has_field != 0U) required += 1U;

    if ((slot->len > csv->slot_size) || (csv->slot_size < 2U) ||
        (required > ((size_t)csv->slot_size - (size_t)slot->len - 2U)))
    {
        return EMBCSV_EROW_TOO_LARGE;
    }

    uint8_t *payload = csv->buffer + ((size_t)csv->producer_index * (size_t)csv->slot_size);

    size_t write_pos = slot->len;

    if (csv->row_has_field != 0U) payload[write_pos++] = ',';

    for (size_t i = 0U; i < len; ++i) {
        payload[write_pos++] = (uint8_t)tmp[i];
    }

    slot->len = (uint16_t)write_pos;
    csv->row_has_field = 1U;

    return EMBCSV_OK;
}

embcsv_status_t embcsv_add_f64(embcsv_t *csv, double value) {
    if (csv == NULL) return EMBCSV_EINVAL;

    if (csv->state != EMBCSV_STATE_BUILDING_ROW) return EMBCSV_ESTATE;

    embcsv_slot_meta_t *slot = &csv->slots[csv->producer_index];

    if (slot->state != EMBCSV_SLOT_FILLING) return EMBCSV_ESTATE;

    int formatted_len = snprintf(NULL, 0U, "%.*f", (int)csv->double_precision, value);

    if (formatted_len < 0) return EMBCSV_EINVAL;

    size_t len = (size_t)formatted_len;
    size_t required = len;

    if (csv->row_has_field != 0U) required += 1U;

    if ((slot->len > csv->slot_size) || (csv->slot_size < 2U) ||
        (required > ((size_t)csv->slot_size - (size_t)slot->len - 2U)))
    {
        return EMBCSV_EROW_TOO_LARGE;
    }

    uint8_t *payload = csv->buffer + ((size_t)csv->producer_index * (size_t)csv->slot_size);

    size_t write_pos = slot->len;

    if (csv->row_has_field != 0U) payload[write_pos++] = ',';

    formatted_len = snprintf((char *)&payload[write_pos], len + 1U, "%.*f", (int)csv->double_precision, value);

    if (formatted_len < 0) return EMBCSV_EINVAL;

    write_pos += len;

    slot->len = (uint16_t)write_pos;
    csv->row_has_field = 1U;

    return EMBCSV_OK;
}

embcsv_status_t embcsv_process(embcsv_t *csv) {
    embcsv_status_t status;
    embcsv_sink_request_t request;

    if (csv == NULL) return EMBCSV_EINVAL;

    if ((csv->slots == NULL) || (csv->slot_count == 0U) ||
        (csv->consumer_index >= csv->slot_count) ||
        (csv->sink.submit == NULL))
    {
        return EMBCSV_ESTATE;
    }

    /*
     * Report a previously completed asynchronous error first.
     * Clear the latch so the next process() call may retry the record.
     */
    if (csv->async_status < EMBCSV_OK) {
        status = csv->async_status;
        csv->async_status = EMBCSV_OK;

        return status;
    }

    embcsv_slot_meta_t *slot = &csv->slots[csv->consumer_index];

    if (slot->state == EMBCSV_SLOT_IN_FLIGHT) return EMBCSV_PENDING;

    /*
     * No complete record is currently available for the consumer.
     *
     * FREE:
     *   queue is empty.
     *
     * FILLING:
     *   producer is still building the current row.
     */
    if ((slot->state == EMBCSV_SLOT_FREE) || (slot->state == EMBCSV_SLOT_FILLING)) return EMBCSV_OK;
    if (slot->state != EMBCSV_SLOT_READY) return EMBCSV_ESTATE;

    request.data = csv->buffer + ((size_t)csv->consumer_index * (size_t)csv->slot_size);

    request.len     = slot->len;
    request.slot_id = csv->consumer_index;

    status = csv->sink.submit(csv->sink.ctx, &request, &csv->completion);

    if (status == EMBCSV_OK) {
        slot->len = 0U;
        slot->state = EMBCSV_SLOT_FREE;

        csv->consumer_index++;

        if (csv->consumer_index >= csv->slot_count) csv->consumer_index = 0U;

        return EMBCSV_OK;
    }

    if (status == EMBCSV_PENDING) {
        slot->state = EMBCSV_SLOT_IN_FLIGHT;

        return EMBCSV_PENDING;
    }

    /*
     * The sink rejected or failed the submission.
     * Keep the slot READY so the record is not lost and may be retried.
     */
    return status;
}

bool embcsv_can_begin_row(const embcsv_t *csv) {
    if (csv == NULL) return false;

    if (csv->state != EMBCSV_STATE_READY) return false;

    if ((csv->slots == NULL) || (csv->slot_count == 0U) || (csv->producer_index >= csv->slot_count)) return false;

    if (csv->slots[csv->producer_index].state != EMBCSV_SLOT_FREE) return false;

    return true;
}