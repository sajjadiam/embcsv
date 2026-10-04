# ADR-0009: سطح Public API نسخه v1

## Status

Accepted

## Context

پس از تثبیت معماری، Buffer Pool، Sink contract و Runtime Context باید سطح API عمومی نسخه v1 قبل از ورود به جزئیات implementation بسته شود.

هدف این است که API حداقل، دقیق و قابل توسعه بماند.

## Decision

API عمومی v1 شامل خانواده‌های زیر است:

### Lifecycle

- `embcsv_init()`

`deinit()` در v1 وجود ندارد، زیرا Core هیچ Heap یا Resource مالک‌شده‌ای برای آزادسازی ندارد.

### Row lifecycle

- `embcsv_begin_row()`
- `embcsv_abort_row()`
- `embcsv_end_row()`

`abort_row()` صریح باقی می‌ماند تا Application بتواند ساخت Row را در اثر خطای خارجی یا توقف چرخه logging لغو کند.

### Field append

- `embcsv_add_string()`
- `embcsv_add_bool()`
- `embcsv_add_i32()`
- `embcsv_add_u32()`
- `embcsv_add_i64()`
- `embcsv_add_u64()`
- `embcsv_add_f32()`
- `embcsv_add_f64()`

APIهای i8/i16/u8/u16 در v1 اضافه نمی‌شوند. Caller می‌تواند مقادیر کوچک‌تر را به typeهای موجود promote کند.

### Output progress

- `embcsv_process()`

Completion callback فقط state را update می‌کند و Pipeline را recursively جلو نمی‌برد. Application یا Task با فراخوانی `embcsv_process()` خروجی را به‌صورت cooperative و non-blocking جلو می‌برد.

### Query

- `embcsv_can_begin_row()`

یک `is_busy()` عمومی در v1 ارائه نمی‌شود، زیرا مفهوم busy در حضور چند Slot مبهم است. Query دقیق‌تر نشان می‌دهد آیا Row جدید همین حالا قابل شروع است.

### حذف‌شده از v1

- `embcsv_deinit()`
- `embcsv_is_busy()`
- `embcsv_pending_count()`

`pending_count()` در آینده در صورت وجود Use Case واقعی می‌تواند بدون شکستن API اضافه شود.

## Consequences

### مثبت

- سطح API کوچک و روشن
- تفکیک Row construction از Output progress
- سازگار با Async بدون اجرای سنگین در ISR
- امکان توسعه آینده بدون اضافه‌کردن API speculative

### منفی

- Application در مدل cooperative باید `embcsv_process()` را در main loop/task فراخوانی کند.
- query آماری صف در v1 وجود ندارد.
