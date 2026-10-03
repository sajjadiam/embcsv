# معماری embcsv

## نمای کلی

```text
Application
    ↓
Public API
    ↓
CSV Core
    ├── Field Formatter
    ├── CSV Escaper
    └── Record Encoder
    ↓
Fixed-size Slot Pool
    ↓
Output Sink Interface
    ↓
Backend
```

## Public API

وظایف:

- اعتبارسنجی پارامترهای قابل بررسی
- کنترل State منطقی Core
- ارائه API پایدار به Application

## Field Formatter

وظیفه تبدیل Typeهای پشتیبانی‌شده به نمایش متنی CSV را دارد.

نمونه:

```text
uint32_t → "1234"
float    → "25.4"
bool     → "true"
```

Policy دقیق NaN/Inf و precision عدد اعشاری هنوز نهایی نشده است.

## CSV Escaper

وظیفه اعمال قواعد quoting و escaping را دارد.

مثال:

```text
Motor,Left
```

به:

```csv
"Motor,Left"
```

تبدیل می‌شود.

## Record Encoder

چند Field را داخل یک Slot به یک Record کامل تبدیل می‌کند.

اصل فعلی:

> یک Record ابتدا کامل ساخته می‌شود و سپس برای Sink submit می‌شود.

بنابراین Fieldها مستقیماً یکی‌یکی به Backend ارسال نمی‌شوند.

## Fixed-size Slot Pool

هر Slot یک CSV Record کامل را نگهداری می‌کند.

State مفهومی Slot:

```text
FREE
 ↓
FILLING
 ↓
READY
 ↓
IN_FLIGHT
 ↓
FREE
```

نسخه v1 بر مدل Single Producer / Single Consumer متمرکز است.

## Output Sink

قرارداد مفهومی Sink:

```c
typedef csv_status_t (*csv_sink_submit_fn)(
    void *ctx,
    const uint8_t *data,
    size_t len,
    csv_sink_done_fn done,
    void *done_ctx
);
```

`ctx` یک opaque pointer متعلق به Backend/Caller است. CSV Core مقدار آن را تفسیر نمی‌کند و همان مقدار را به callback مربوطه تحویل می‌دهد.

## Dependency Direction

Dependency باید فقط در جهت زیر باشد:

```text
Application
    ↓
CSV Core
    ↓
Abstract Output Interface
    ↓
Backend
```

Core نباید از نوع Backend آگاه باشد.
