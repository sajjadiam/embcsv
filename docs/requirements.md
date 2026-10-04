# نیازمندی‌های embcsv

این سند تا پیش از v1.0 زنده است و ممکن است با تصمیم‌های معماری اصلاح شود.

## Functional Requirements

### FR-01
کتابخانه shall یک sequence از Fieldها را به CSV Record تبدیل کند.

### FR-02
کتابخانه shall امکان تولید Header را از طریق همان مکانیزم Record فراهم کند.

### FR-03
کتابخانه shall quoting/escaping مربوط به comma، double quote، CR و LF را مدیریت کند.

### FR-04
کتابخانه shall چند Instance مستقل را پشتیبانی کند.

### FR-05
کتابخانه shall داده encode‌شده را از طریق Output Interface انتزاعی تحویل دهد.

### FR-06
کتابخانه shall خطاهای Output Interface را به Caller منتقل کند.

## Non-Functional Requirements

### NFR-01
CSV Core shall به MCU vendor HAL وابسته نباشد.

### NFR-02
CSV Core shall به RTOS وابسته نباشد.

### NFR-03
CSV Core shall به Filesystem وابسته نباشد.

### NFR-04
CSV Core shall از Heap استفاده نکند.

### NFR-05
مصرف حافظه shall bounded و قابل پیش‌بینی باشد.

### NFR-06
معماری shall Backendهای synchronous و asynchronous را پشتیبانی کند.

## Execution Requirements

### ER-01
عمل Encoding در CSV Core به‌صورت synchronous انجام می‌شود.

### ER-02
Backend synchronous می‌تواند انتقال را قبل از return کامل کند.

### ER-03
Backend asynchronous می‌تواند انتقال فیزیکی را defer کند.

### ER-04
Completion عملیات Async shall قابل مشاهده باشد.

### ER-05
Lifetime و Ownership بافر در Async shall صریح تعریف شود.

### ER-06
Submission و Completion دو مفهوم جدا هستند.

## Performance / Resource Requirements

### PR-01
هیچ تخصیص حافظه پویا در Core انجام نشود.

### PR-02
تمام حافظه کاری توسط Caller تأمین شود.

### PR-03
مصرف حافظه bounded باشد.

### PR-04
هیچ عملیاتی خارج از ظرفیت بافر ننویسد.

### PR-05
کمبود ظرفیت با خطای تعریف‌شده گزارش شود.

### PR-06
هزینه Encoding نسبت به تعداد بایت‌های خروجی O(N) باشد.

### PR-07
Core نباید انتظار بدون کران (unbounded wait) داشته باشد.

## Concurrency Requirements

### CR-01
Core shall از mutable global state استفاده نکند.

### CR-02
Instanceهای مستقل باید مستقل از یکدیگر قابل استفاده باشند.

### CR-03
دسترسی هم‌زمان چند Thread به یک Instance نیازمند synchronization خارجی است، مگر آنکه بعداً خلاف آن صریحاً تعریف شود.

### CR-04
API synchronous به‌صورت پیش‌فرض ISR-safe فرض نمی‌شود.

### CR-05
Async submission از ISR فقط در صورت پشتیبانی صریح Backend مجاز است.

## Memory Requirements

### MEM-01
مالک حافظه Caller است.

### MEM-02
Core می‌تواند Pointer حافظه Caller را طبق قرارداد نگه دارد ولی مالک آن نمی‌شود.

### MEM-03
حافظه ثبت‌شده در Init باید در تمام Lifetime مربوطه معتبر باقی بماند.

### MEM-04
در Zero-copy، Slot تا دریافت Completion نباید تغییر یا آزاد شود.

### MEM-05
Payload و Metadata از یکدیگر جدا نگهداری می‌شوند.

## Verification

برای هر Requirement باید حداقل یکی از روش‌های زیر تعریف شود:

- Unit Test
- Integration Test
- Static Analysis
- Code Review
- Benchmark
- Hardware Integration Test

عبارت‌هایی مانند «سریع»، «کم‌حافظه» و «مقاوم» بدون معیار قابل اندازه‌گیری Requirement معتبر محسوب نمی‌شوند.


## Configuration Requirements

### CFG-01
`embcsv_config_t` shall contain `struct_size` for configuration-layout validation and controlled API evolution.

### CFG-02
Payload storage and metadata storage shall both expose their capacities explicitly through `buffer_size` and `slots_size`.

### CFG-03
v1 shall use fixed-size slots. Variable-sized slots are outside the current scope.

### CFG-04
`slot_count` and `slot_size` shall use 16-bit unsigned storage.

### CFG-05
The sink descriptor shall be copied by value during initialization. Ownership and lifetime of the object referenced by `sink.ctx` remain the caller/backend responsibility.

### CFG-06
Float and double formatting precision shall be configurable per instance.

### CFG-07
v1 shall use comma as the delimiter and CRLF as the record terminator. Custom CSV dialect configuration is outside the current v1 scope.

### CFG-08
Initialization shall validate payload and metadata capacity without relying on overflow-prone unchecked multiplication.


## Field Append Requirements

### FIELD-01
تمام عملیات `embcsv_add_*()` shall در سطح Field اتمیک باشند.

### FIELD-02
Core shall پیش از نوشتن delimiter یا Field، ظرفیت موردنیاز خروجی encode‌شده را بررسی کند.

### FIELD-03
در صورت کمبود ظرفیت، Row موجود shall بدون تغییر باقی بماند و `EMBCSV_EROW_TOO_LARGE` بازگردانده شود.

### FIELD-04
فضای لازم برای `CRLF` shall هنگام افزودن Fieldها رزرو شود تا پایان Record قابل تضمین باشد.

### FIELD-05
محاسبه ظرفیت String shall اثر quoting و escaping CSV را در نظر بگیرد.


## Runtime Context Requirements

### CTX-01
v1 shall maintain at most one sink request in `IN_FLIGHT` state per embcsv instance.

### CTX-02
`producer_index` shall identify the active slot while a row is being built; a separate active-slot field shall not be required.

### CTX-03
`consumer_index` shall identify the single in-flight slot when asynchronous output is pending; a separate in-flight-slot field shall not be required.

### CTX-04
The runtime context shall retain a persistent completion descriptor suitable for asynchronous sink lifetime requirements.

### CTX-05
An asynchronous completion error shall not silently discard the affected CSV record.

### CTX-06
The runtime context shall remain at a stable address after successful initialization while it is in use or while output is pending.

### CTX-07
`row_has_field` shall be tracked independently from encoded byte length.

### CTX-08
Concurrent access to the same v1 instance shall require external serialization. `volatile` shall not be used as a substitute for synchronization.


## Public API Requirements

### API-01
v1 shall expose `embcsv_init()` as the only lifecycle initialization operation and shall not require a deinit operation.

### API-02
v1 shall expose explicit row lifecycle operations: `embcsv_begin_row()`, `embcsv_abort_row()`, and `embcsv_end_row()`.

### API-03
v1 shall expose field append operations for string, bool, i32, u32, i64, u64, f32, and f64.

### API-04
v1 shall expose `embcsv_process()` as a non-blocking cooperative output progress engine.

### API-05
The asynchronous completion callback shall update state only and shall not recursively submit the next queued record.

### API-06
v1 shall expose `embcsv_can_begin_row()` instead of a generic ambiguous busy query.

### API-07
v1 shall not expose `pending_count()` until a concrete external use case requires it.
