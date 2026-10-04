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
 * @par ISR usage
 * Synchronous APIs are not considered ISR-safe by default. Asynchronous sink
 * behavior from ISR context depends on the configured backend contract.
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

#ifdef __cplusplus
}
#endif

#endif /* EMBCSV_H */
