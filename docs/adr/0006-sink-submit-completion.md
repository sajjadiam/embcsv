# ADR-0006: قرارداد Submit و Completion برای Output Sink

## Status

Proposed

## Context

نسخه اولیه Sink برای هر submit پنج آرگومان دریافت می‌کرد و سپس طرح دوم یک تابع bind جدا برای ثبت callback پیشنهاد کرد.

هدف این ADR رسیدن به قراردادی است که:

- مسیر پرتکرار submit کوچک بماند.
- Sync و Async را با یک Interface پوشش دهد.
- Backend بتواند بین چند `embcsv` instance مشترک باشد.
- Zero-copy و چند Slot هم‌زمان قابل پشتیبانی باشند.
- callback registration lifecycle جداگانه لازم نباشد.

## Decision پیشنهادی

`bind()` حذف شود.

اطلاعات transfer و completion در دو descriptor جدا قرار گیرند:

```c
typedef uint16_t embcsv_slot_id_t;

typedef struct
{
    const uint8_t *data;
    uint16_t len;
    embcsv_slot_id_t slot_id;
} embcsv_sink_request_t;

typedef void (*embcsv_sink_done_fn)(
    void *ctx,
    embcsv_slot_id_t slot_id,
    embcsv_status_t status
);

typedef struct
{
    embcsv_sink_done_fn fn;
    void *ctx;
} embcsv_sink_completion_t;

typedef embcsv_status_t (*embcsv_sink_submit_fn)(
    void *ctx,
    const embcsv_sink_request_t *req,
    const embcsv_sink_completion_t *completion
);

typedef struct
{
    embcsv_sink_submit_fn submit;
    void *ctx;
} embcsv_sink_t;
```

## قرارداد

- `EMBCSV_OK`: عملیات به‌صورت synchronous کامل شده است؛ Backend نباید بعداً callback را فراخوانی کند.
- `EMBCSV_PENDING`: کل request پذیرفته شده و Backend باید دقیقاً یک Completion بعدی گزارش کند.
- مقدار منفی: request پذیرفته نشده است و Backend نباید Completion ارسال کند.
- اگر submit پیش از return کامل شود باید `EMBCSV_OK` برگرداند؛ callback نباید پیش از بازگشت submit فراخوانی شود.
- در حالت `EMBCSV_PENDING`، Payload تا Completion immutable و معتبر باقی می‌ماند.
- `completion` تا پایان عملیات معتبر است و Backend در صورت Pending می‌تواند pointer آن را نگه دارد یا محتویات لازم را کپی کند.
- Request یک Record کامل را نمایش می‌دهد. Partial physical writes جزئیات Backend هستند؛ API عمومی bytes-transferred را expose نمی‌کند.
- این قرارداد به‌تنهایی Thread-safe یا ISR-safe بودن را تضمین نمی‌کند؛ synchronization در ADR جداگانه نهایی خواهد شد.

## دلایل رد bind

ثبت یک callback ثابت روی Backend می‌تواند استفاده اشتراکی یک Backend توسط چند CSV instance را مبهم یا محدود کند. Completion descriptor در هر submit مسیر برگشت را بدون state سراسری یا bind مجدد مشخص می‌کند.

## وضعیت

این ADR تا تأیید نهایی Public API در حالت Proposed باقی می‌ماند.
