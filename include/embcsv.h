/**
 * @file embcsv.h
 * @brief Public API for the embcsv portable embedded CSV encoder.
 *
 * @details
 * embcsv is a platform-independent CSV encoder intended for embedded systems.
 * The core library does not depend on an MCU HAL, RTOS, filesystem, or heap.
 *
 * The output path is abstracted through a sink interface. A sink may complete
 * synchronously or asynchronously.
 *
 * @par Ownership
 * All memory supplied to embcsv remains owned by the caller. The library may
 * borrow caller-provided buffers according to the documented lifetime rules.
 *
 * @par Threading
 * Unless explicitly documented otherwise, concurrent access to the same
 * embcsv instance requires external synchronization.
 *
 * @par Execution model
 * CSV encoding and row construction are synchronous CPU operations. Output
 * submission is progressed explicitly through embcsv_process(). A sink may
 * complete a submission synchronously or return EMBCSV_PENDING and complete
 * it later through the completion callback.
 *
 * The completion callback only updates embcsv state. It shall not recursively
 * advance the output pipeline or submit the next record. Application code
 * progresses queued output by calling embcsv_process() again.
 *
 * @par Ownership
 * All storage supplied to embcsv remains caller-owned. embcsv borrows payload
 * and metadata storage for the lifetime of the initialized instance.
 *
 * @par Threading
 * The v1 API does not provide internal locking. Concurrent access to the same
 * embcsv instance requires external serialization.
 *
 * @par ISR usage
 * Public row-building and processing APIs are not considered ISR-safe by
 * default. A backend completion callback may execute in ISR context only when
 * access to the same embcsv instance is externally serialized according to
 * the backend contract.
 *
 * @par CSV dialect
 * v1 uses comma (',') as the field delimiter and CRLF ("\\r\\n") as the
 * record terminator. Custom delimiters and record terminators are outside the
 * current v1 scope.
 *
 * @par Error model
 * EMBCSV_OK indicates completed success, EMBCSV_PENDING indicates accepted
 * asynchronous work, and negative embcsv_status_t values indicate errors.
 */

#ifndef EMBCSV_H
#define EMBCSV_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* -------------------------------------------------------------------------- */
/* Status                                                                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief Status code returned by embcsv operations.
 *
 * @note Positive values are non-error states.
 * @note Negative values indicate errors.
 */
typedef enum
{
    /** Operation completed successfully. */
    EMBCSV_OK = 0,

    /**
     * Operation was accepted but has not completed yet.
     *
     * When returned by an asynchronous sink, the submitted payload remains
     * borrowed by the backend until the registered completion callback is
     * invoked exactly once.
     */
    EMBCSV_PENDING = 1,

    /** Invalid argument or invalid pointer detected by the API. */
    EMBCSV_EINVAL = -1,

    /** Operation is not valid in the current logical state. */
    EMBCSV_ESTATE = -2,

    /** No free slot or usable output buffer is currently available. */
    EMBCSV_ENO_BUFFER = -3,

    /** Encoded CSV row does not fit in a single configured slot. */
    EMBCSV_EROW_TOO_LARGE = -4,

    /** Requested resource is currently busy. */
    EMBCSV_EBUSY = -5,

    /** Backend or output I/O operation failed. */
    EMBCSV_EIO = -6,

    /** A bounded wait exceeded its configured timeout. */
    EMBCSV_ETIMEOUT = -7

} embcsv_status_t;


/* -------------------------------------------------------------------------- */
/* Slot                                                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Numeric identifier of a slot in the fixed-size buffer pool.
 *
 * @note The current v1 design limits the number of addressable slots to
 *       UINT16_MAX.
 */
typedef uint16_t embcsv_slot_id_t;

/**
 * @brief Storage type used for internal slot state values.
 *
 * @note This is intentionally an 8-bit integer rather than a C enum so that
 *       metadata storage width remains explicit and predictable.
 */
typedef uint8_t embcsv_slot_state_t;

/** @brief Slot is available for reuse. */
#define EMBCSV_SLOT_FREE     ((embcsv_slot_state_t)0U)

/** @brief Slot is currently being filled by the CSV encoder. */
#define EMBCSV_SLOT_FILLING     ((embcsv_slot_state_t)1U)

/** @brief Slot contains a complete record waiting for submission. */
#define EMBCSV_SLOT_READY     ((embcsv_slot_state_t)2U)

/** @brief Slot is owned temporarily by an asynchronous backend. */
#define EMBCSV_SLOT_IN_FLIGHT     ((embcsv_slot_state_t)3U)

/**
 * @brief Metadata associated with one fixed-size payload slot.
 *
 * @details
 * Payload bytes are stored separately from this metadata structure.
 *
 * The structure must not be packed. Its actual size and alignment are defined
 * by the target ABI and compiler and must be queried with sizeof()/_Alignof()
 * rather than inferred from member sizes.
 *
 * @note The @ref reserved member shall be initialized to zero.
 * @note The current v1 design limits encoded row length to UINT16_MAX bytes.
 */
typedef struct
{
    /** Number of valid encoded bytes currently stored in the slot. */
    uint16_t len;

    /** Current logical state of the slot. */
    embcsv_slot_state_t state;

    /** Reserved for future use. Must be initialized to zero. */
    uint8_t reserved;

} embcsv_slot_meta_t;


/* -------------------------------------------------------------------------- */
/* Sink request                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Description of one complete CSV record submitted to an output sink.
 *
 * @details
 * A request always represents one complete, contiguous CSV record.
 *
 * If the sink returns @ref EMBCSV_PENDING, the memory referenced by @ref data
 * remains valid and immutable until completion is reported.
 *
 * If the sink returns @ref EMBCSV_OK or a negative error code, the sink shall
 * not retain the payload pointer after returning.
 */
typedef struct
{
    /** Pointer to the first byte of the complete CSV record. */
    const uint8_t *data;

    /** Number of valid bytes referenced by @ref data. */
    uint16_t len;

    /** Identifier of the buffer-pool slot containing this record. */
    embcsv_slot_id_t slot_id;

} embcsv_sink_request_t;


/* -------------------------------------------------------------------------- */
/* Completion                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Completion callback used by asynchronous sinks.
 *
 * @param ctx
 * Opaque context pointer supplied by embcsv through
 * @ref embcsv_sink_completion_t::ctx.
 *
 * The sink shall pass this value back unchanged. The sink shall not interpret
 * or modify the object referenced by this pointer.
 *
 * @param slot_id
 * Identifier of the slot whose asynchronous operation has completed.
 *
 * @param status
 * Final completion status.
 *
 * @warning
 * This callback shall be invoked exactly once for every submission for which
 * the sink returned @ref EMBCSV_PENDING.
 *
 * @warning
 * This callback shall not be invoked for a submission that returned
 * @ref EMBCSV_OK or any negative error code.
 *
 * @warning
 * The sink shall not invoke this callback before the corresponding
 * submit function has returned @ref EMBCSV_PENDING.
 */
typedef void (*embcsv_sink_done_fn)(
    void *ctx,
    embcsv_slot_id_t slot_id,
    embcsv_status_t status
);

/**
 * @brief Completion route associated with one sink submission.
 *
 * @details
 * This descriptor allows the same backend instance to service multiple
 * embcsv instances without a global or permanently bound completion callback.
 *
 * The descriptor itself, and any object referenced by @ref ctx, shall remain
 * valid until completion if the sink returns @ref EMBCSV_PENDING.
 */
typedef struct
{
    /** Completion function to invoke for an asynchronous operation. */
    embcsv_sink_done_fn fn;

    /** Opaque completion context passed back unchanged to @ref fn. */
    void *ctx;

} embcsv_sink_completion_t;


/* -------------------------------------------------------------------------- */
/* Sink                                                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Submit one complete CSV record to an output backend.
 *
 * @param ctx
 * Backend-specific opaque context pointer.
 *
 * This pointer is owned by the caller/backend. embcsv does not interpret it.
 *
 * @param req
 * Request descriptor describing one complete contiguous CSV record.
 *
 * The pointer is valid for the duration required by the return contract:
 *
 * - If @ref EMBCSV_OK is returned, the backend must have completed all use of
 *   req->data before returning.
 * - If @ref EMBCSV_PENDING is returned, the backend may continue using
 *   req->data until it reports completion.
 * - If a negative error is returned, the request is considered rejected and
 *   the backend shall not retain req or req->data.
 *
 * @param completion
 * Completion route to use only when @ref EMBCSV_PENDING is returned.
 *
 * The sink may retain this pointer only while an asynchronous request is
 * pending. The associated callback must later be invoked exactly once.
 *
 * @return
 * - @ref EMBCSV_OK:
 *   request completed synchronously; no callback will follow.
 * - @ref EMBCSV_PENDING:
 *   request was accepted asynchronously; exactly one callback shall follow.
 * - Negative @ref embcsv_status_t:
 *   request was rejected or failed; no callback shall follow.
 *
 * @par Atomicity contract
 * The public sink contract is record-oriented, not byte-oriented. Partial
 * physical transfers are an internal backend concern. A backend shall not
 * expose a partially accepted record as successful completion.
 *
 * @par Reentrancy
 * The sink shall not invoke the completion callback before this function has
 * returned @ref EMBCSV_PENDING.
 *
 * @par Thread/ISR behavior
 * This type does not imply thread safety or ISR safety. Those properties are
 * backend-specific and shall be documented by each backend implementation.
 */
typedef embcsv_status_t (*embcsv_sink_submit_fn)(
    void *ctx,
    const embcsv_sink_request_t *req,
    const embcsv_sink_completion_t *completion
);

/**
 * @brief Output sink interface used by embcsv.
 *
 * @details
 * The sink separates CSV encoding from platform-specific transport.
 *
 * Typical backends include:
 * - UART polling
 * - UART DMA
 * - USB CDC
 * - filesystem adapters
 * - sockets
 * - RAM test sinks
 *
 * Multiple embcsv instances may reference the same sink object if the backend
 * implementation itself supports that usage.
 */
typedef struct
{
    /** Function used to submit one complete CSV record. Must not be NULL. */
    embcsv_sink_submit_fn submit;

    /**
     * Backend-specific opaque context.
     *
     * The value is passed unchanged as the first argument to @ref submit.
     * Ownership remains with the caller/backend.
     */
    void *ctx;

} embcsv_sink_t;


/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Runtime configuration for one embcsv instance.
 *
 * @details
 * The caller supplies all payload and metadata storage. embcsv does not
 * allocate heap memory. The sink descriptor is copied by value during
 * initialization, while any object referenced by sink.ctx remains owned by
 * the caller/backend.
 *
 * Slot payloads are fixed-size in v1.
 */
typedef struct
{
    /**
     * Size of this structure in bytes.
     *
     * Must be initialized to sizeof(embcsv_config_t). Used for compatibility
     * and validation when the structure evolves in later API versions.
     */
    size_t struct_size;

    /** Caller-owned payload memory used for fixed-size CSV record slots. */
    uint8_t *buffer;

    /** Total payload-memory capacity in bytes. */
    size_t buffer_size;

    /** Caller-owned array containing metadata for each payload slot. */
    embcsv_slot_meta_t *slots;

    /** Total metadata-storage capacity in bytes. */
    size_t slots_size;

    /**
     * Output sink descriptor.
     *
     * The descriptor itself is copied by value during initialization.
     * Ownership of the object referenced by sink.ctx remains with the caller.
     */
    embcsv_sink_t sink;

    /**
     * Capacity of each fixed-size payload slot in bytes.
     *
     * Must be greater than zero and must not exceed UINT16_MAX.
     */
    uint16_t slot_size;

    /**
     * Number of payload slots.
     *
     * Must be greater than zero.
     */
    uint16_t slot_count;

    /** Number of fractional digits used when formatting float values. */
    uint8_t float_precision;

    /** Number of fractional digits used when formatting double values. */
    uint8_t double_precision;

} embcsv_config_t;


/* -------------------------------------------------------------------------- */
/* Runtime context                                                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Storage type used for the logical state of one embcsv instance.
 */
typedef uint8_t embcsv_state_t;

/** @brief Instance has not been initialized successfully. */
#define EMBCSV_STATE_UNINITIALIZED \
    ((embcsv_state_t)0U)

/** @brief Instance is initialized and no row is currently being built. */
#define EMBCSV_STATE_READY \
    ((embcsv_state_t)1U)

/** @brief A row is currently being encoded into the producer slot. */
#define EMBCSV_STATE_BUILDING_ROW \
    ((embcsv_state_t)2U)

/**
 * @brief Runtime context of one embcsv instance.
 *
 * @details
 * The caller owns the context storage, but all members are private by
 * contract and shall not be modified directly by application code.
 *
 * The instance stores only runtime state required after initialization.
 * Configuration capacities used only for validation, such as buffer_size and
 * slots_size, are intentionally not duplicated here.
 *
 * @par Stable address requirement
 * After successful initialization, this object shall remain at the same
 * address while it is in use or while any sink request is pending. The
 * internal asynchronous completion route refers back to this instance.
 *
 * @par Concurrency
 * v1 uses a single-producer/single-consumer model with at most one sink
 * request in flight. Concurrent access to the same instance shall be
 * externally serialized. volatile is intentionally not used as a
 * synchronization mechanism.
 */
typedef struct embcsv
{
    /** Caller-owned contiguous payload storage. */
    uint8_t *buffer;

    /** Caller-owned metadata array corresponding to payload slots. */
    embcsv_slot_meta_t *slots;

    /** Output sink copied by value from embcsv_config_t during initialization. */
    embcsv_sink_t sink;

    /**
     * Persistent completion descriptor used for asynchronous submissions.
     *
     * Its callback context refers to this embcsv instance, therefore the
     * instance address shall remain stable after initialization.
     */
    embcsv_sink_completion_t completion;

    /**
     * Latched result of a completed asynchronous operation.
     *
     * EMBCSV_OK means no unreported asynchronous error is pending.
     * A negative status is reported by embcsv_process() before retrying or
     * advancing further output work.
     */
    embcsv_status_t async_status;

    /** Capacity in bytes of each fixed-size payload slot. */
    uint16_t slot_size;

    /** Number of slots in the fixed-size buffer pool. */
    uint16_t slot_count;

    /**
     * Index of the next producer slot.
     *
     * While state is EMBCSV_STATE_BUILDING_ROW, this index also identifies
     * the slot currently in EMBCSV_SLOT_FILLING state.
     */
    embcsv_slot_id_t producer_index;

    /**
     * Index of the oldest queued consumer slot.
     *
     * With the v1 single-in-flight invariant, an asynchronous
     * EMBCSV_SLOT_IN_FLIGHT slot is always identified by this index.
     */
    embcsv_slot_id_t consumer_index;

    /** Number of fractional digits used for float formatting. */
    uint8_t float_precision;

    /** Number of fractional digits used for double formatting. */
    uint8_t double_precision;

    /**
     * Nonzero after at least one field has been appended to the active row.
     *
     * This cannot be derived from encoded row length because the first field
     * may legally be an empty string and therefore contribute zero payload
     * bytes before a following delimiter is required.
     */
    uint8_t row_has_field;

    /** Current logical row-building state. */
    embcsv_state_t state;

} embcsv_t;


/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize one embcsv instance.
 *
 * @param csv
 * Pointer to caller-owned runtime context storage.
 *
 * @param cfg
 * Pointer to a validated runtime configuration.
 *
 * @return
 * - @ref EMBCSV_OK on successful initialization.
 * - @ref EMBCSV_EINVAL if a required argument or configuration field is
 *   invalid.
 *
 * @pre
 * - @p csv and @p cfg shall not be NULL.
 * - cfg->struct_size shall describe a compatible embcsv_config_t layout.
 * - cfg->buffer and cfg->slots shall reference caller-owned storage that
 *   remains valid for the lifetime of @p csv.
 * - cfg->slot_size and cfg->slot_count shall be nonzero.
 * - cfg->buffer_size shall be sufficient for all configured payload slots.
 * - cfg->slots_size shall be sufficient for all slot metadata.
 * - cfg->sink.submit shall not be NULL.
 *
 * @post
 * On success:
 * - the instance is in @ref EMBCSV_STATE_READY,
 * - all slots are initialized to @ref EMBCSV_SLOT_FREE,
 * - producer and consumer indices are reset,
 * - the sink descriptor is copied by value into the runtime context.
 *
 * @note
 * embcsv performs no heap allocation.
 *
 * @warning
 * After successful initialization, the @p csv object shall remain at a stable
 * address while it is in use or while asynchronous output is pending.
 */
embcsv_status_t embcsv_init(
    embcsv_t *csv,
    const embcsv_config_t *cfg
);

/**
 * @brief Begin construction of a new CSV row.
 *
 * @param csv
 * Initialized embcsv instance.
 *
 * @return
 * - @ref EMBCSV_OK if a free producer slot was claimed successfully.
 * - @ref EMBCSV_ENO_BUFFER if no free slot is available.
 * - @ref EMBCSV_ESTATE if a row is already being built or the instance state
 *   does not allow a new row.
 * - @ref EMBCSV_EINVAL if @p csv is NULL.
 *
 * @post
 * On success the instance enters @ref EMBCSV_STATE_BUILDING_ROW and the
 * producer slot enters @ref EMBCSV_SLOT_FILLING.
 *
 * @note
 * This function is non-blocking. It does not wait for a slot to become free.
 */
embcsv_status_t embcsv_begin_row(
    embcsv_t *csv
);

/**
 * @brief Abort the row currently being constructed.
 *
 * @param csv
 * Initialized embcsv instance.
 *
 * @return
 * - @ref EMBCSV_OK if the active row was discarded successfully.
 * - @ref EMBCSV_ESTATE if no row is currently being built.
 * - @ref EMBCSV_EINVAL if @p csv is NULL.
 *
 * @post
 * The active producer slot is returned to @ref EMBCSV_SLOT_FREE, its encoded
 * length is reset, row-building state is cleared, and producer_index is not
 * advanced.
 *
 * @note
 * This operation affects only the row currently in
 * @ref EMBCSV_STATE_BUILDING_ROW. Already queued or in-flight rows are not
 * modified.
 */
embcsv_status_t embcsv_abort_row(
    embcsv_t *csv
);

/**
 * @brief Commit the current row to the output queue.
 *
 * @param csv
 * Initialized embcsv instance currently building a row.
 *
 * @return
 * - @ref EMBCSV_OK if CRLF was appended and the row was committed.
 * - @ref EMBCSV_ESTATE if no row is currently being built.
 * - @ref EMBCSV_EROW_TOO_LARGE if the row cannot be terminated safely within
 *   the configured slot capacity.
 * - @ref EMBCSV_EINVAL if @p csv is NULL.
 *
 * @post
 * On success:
 * - the row slot transitions from @ref EMBCSV_SLOT_FILLING to
 *   @ref EMBCSV_SLOT_READY,
 * - producer_index advances to the next slot,
 * - the instance returns to @ref EMBCSV_STATE_READY.
 *
 * @note
 * Field append operations reserve enough capacity for CRLF, therefore
 * EMBCSV_EROW_TOO_LARGE from this function should normally indicate an
 * internal invariant violation or corrupted state rather than routine input.
 *
 * @note
 * This function does not submit the record to the sink. Output progression is
 * performed by @ref embcsv_process.
 */
embcsv_status_t embcsv_end_row(
    embcsv_t *csv
);

/**
 * @brief Append a string field to the active CSV row.
 *
 * @param csv
 * Initialized embcsv instance in @ref EMBCSV_STATE_BUILDING_ROW.
 *
 * @param value
 * Null-terminated input string.
 *
 * @return
 * - @ref EMBCSV_OK on success.
 * - @ref EMBCSV_EROW_TOO_LARGE if the complete encoded field does not fit.
 * - @ref EMBCSV_ESTATE if no row is currently being built.
 * - @ref EMBCSV_EINVAL if @p csv or @p value is NULL.
 *
 * @details
 * CSV quoting and escaping are applied as required. Comma, double quote, CR,
 * and LF are handled according to the v1 CSV dialect.
 *
 * @par Transactional guarantee
 * The operation is atomic at field level. Capacity is checked before any
 * delimiter or encoded field bytes are committed. On
 * @ref EMBCSV_EROW_TOO_LARGE the existing row remains unchanged.
 */
embcsv_status_t embcsv_add_string(
    embcsv_t *csv,
    const char *value
);

/**
 * @brief Append a boolean field to the active CSV row.
 *
 * @param csv
 * Initialized embcsv instance in @ref EMBCSV_STATE_BUILDING_ROW.
 *
 * @param value
 * Boolean value to encode.
 *
 * @return
 * - @ref EMBCSV_OK on success.
 * - @ref EMBCSV_EROW_TOO_LARGE if the complete field does not fit.
 * - @ref EMBCSV_ESTATE if no row is currently being built.
 * - @ref EMBCSV_EINVAL if @p csv is NULL.
 *
 * @par Transactional guarantee
 * On failure due to insufficient capacity, the active row is unchanged.
 */
embcsv_status_t embcsv_add_bool(
    embcsv_t *csv,
    bool value
);

/**
 * @brief Append a signed 32-bit integer field.
 *
 * @param csv Initialized embcsv instance currently building a row.
 * @param value Value to encode in base-10 textual form.
 *
 * @return
 * @ref EMBCSV_OK, @ref EMBCSV_EROW_TOO_LARGE, @ref EMBCSV_ESTATE, or
 * @ref EMBCSV_EINVAL.
 *
 * @par Transactional guarantee
 * On failure due to insufficient capacity, the active row is unchanged.
 */
embcsv_status_t embcsv_add_i32(
    embcsv_t *csv,
    int32_t value
);

/**
 * @brief Append an unsigned 32-bit integer field.
 *
 * @param csv Initialized embcsv instance currently building a row.
 * @param value Value to encode in base-10 textual form.
 *
 * @return
 * @ref EMBCSV_OK, @ref EMBCSV_EROW_TOO_LARGE, @ref EMBCSV_ESTATE, or
 * @ref EMBCSV_EINVAL.
 *
 * @par Transactional guarantee
 * On failure due to insufficient capacity, the active row is unchanged.
 */
embcsv_status_t embcsv_add_u32(
    embcsv_t *csv,
    uint32_t value
);

/**
 * @brief Append a signed 64-bit integer field.
 *
 * @param csv Initialized embcsv instance currently building a row.
 * @param value Value to encode in base-10 textual form.
 *
 * @return
 * @ref EMBCSV_OK, @ref EMBCSV_EROW_TOO_LARGE, @ref EMBCSV_ESTATE, or
 * @ref EMBCSV_EINVAL.
 *
 * @par Transactional guarantee
 * On failure due to insufficient capacity, the active row is unchanged.
 */
embcsv_status_t embcsv_add_i64(
    embcsv_t *csv,
    int64_t value
);

/**
 * @brief Append an unsigned 64-bit integer field.
 *
 * @param csv Initialized embcsv instance currently building a row.
 * @param value Value to encode in base-10 textual form.
 *
 * @return
 * @ref EMBCSV_OK, @ref EMBCSV_EROW_TOO_LARGE, @ref EMBCSV_ESTATE, or
 * @ref EMBCSV_EINVAL.
 *
 * @par Transactional guarantee
 * On failure due to insufficient capacity, the active row is unchanged.
 */
embcsv_status_t embcsv_add_u64(
    embcsv_t *csv,
    uint64_t value
);

/**
 * @brief Append a single-precision floating-point field.
 *
 * @param csv
 * Initialized embcsv instance currently building a row.
 *
 * @param value
 * Floating-point value to encode.
 *
 * @return
 * - @ref EMBCSV_OK on success.
 * - @ref EMBCSV_EROW_TOO_LARGE if the complete formatted field does not fit.
 * - @ref EMBCSV_ESTATE if no row is currently being built.
 * - @ref EMBCSV_EINVAL if @p csv is NULL.
 *
 * @details
 * Fractional formatting uses the per-instance float_precision configuration.
 * The exact policy for NaN, positive infinity, and negative infinity shall be
 * documented with the formatter implementation before v1.0 is frozen.
 *
 * @par Transactional guarantee
 * On failure due to insufficient capacity, the active row is unchanged.
 */
embcsv_status_t embcsv_add_f32(
    embcsv_t *csv,
    float value
);

/**
 * @brief Append a double-precision floating-point field.
 *
 * @param csv
 * Initialized embcsv instance currently building a row.
 *
 * @param value
 * Floating-point value to encode.
 *
 * @return
 * - @ref EMBCSV_OK on success.
 * - @ref EMBCSV_EROW_TOO_LARGE if the complete formatted field does not fit.
 * - @ref EMBCSV_ESTATE if no row is currently being built.
 * - @ref EMBCSV_EINVAL if @p csv is NULL.
 *
 * @details
 * Fractional formatting uses the per-instance double_precision configuration.
 * The exact policy for NaN, positive infinity, and negative infinity shall be
 * documented with the formatter implementation before v1.0 is frozen.
 *
 * @par Transactional guarantee
 * On failure due to insufficient capacity, the active row is unchanged.
 */
embcsv_status_t embcsv_add_f64(
    embcsv_t *csv,
    double value
);

/**
 * @brief Progress queued CSV output without blocking.
 *
 * @param csv
 * Initialized embcsv instance.
 *
 * @return
 * - @ref EMBCSV_OK if no asynchronous wait remains after this call or if
 *   there was no output work to start.
 * - @ref EMBCSV_PENDING if a sink request is currently in flight after this
 *   call.
 * - @ref EMBCSV_EBUSY if output cannot be progressed immediately due to a
 *   backend/resource condition that did not accept a new request.
 * - @ref EMBCSV_EIO, @ref EMBCSV_ETIMEOUT, or another negative status if a
 *   latched asynchronous completion error or sink submission error is
 *   reported.
 * - @ref EMBCSV_EINVAL if @p csv is NULL.
 *
 * @details
 * At most one sink request may be in flight per embcsv instance in v1.
 *
 * If no request is in flight and the consumer slot is
 * @ref EMBCSV_SLOT_READY, this function submits one complete record to the
 * configured sink.
 *
 * If the sink returns @ref EMBCSV_OK, the consumed slot is released
 * synchronously and the consumer index advances.
 *
 * If the sink returns @ref EMBCSV_PENDING, the slot becomes
 * @ref EMBCSV_SLOT_IN_FLIGHT and remains immutable until completion.
 *
 * A completion callback never recursively calls the sink and never advances
 * the next queued record by itself. Application code calls embcsv_process()
 * again to make further progress.
 *
 * @note
 * This function is a cooperative progress engine and shall not perform an
 * unbounded wait.
 */
embcsv_status_t embcsv_process(
    embcsv_t *csv
);

/**
 * @brief Query whether a new row can be started immediately.
 *
 * @param csv
 * Initialized embcsv instance.
 *
 * @return
 * true if embcsv_begin_row() can claim the current producer slot without
 * waiting; false otherwise.
 *
 * @details
 * This query is intentionally more precise than a generic "busy" flag. Output
 * may be in flight while another slot is still available for row production.
 *
 * @note
 * The result is only a snapshot. In a concurrently accessed system, external
 * synchronization is required if the caller needs the result and subsequent
 * embcsv_begin_row() call to be atomic with respect to other contexts.
 */
bool embcsv_can_begin_row(
    const embcsv_t *csv
);

#ifdef __cplusplus
}
#endif

#endif /* EMBCSV_H */
