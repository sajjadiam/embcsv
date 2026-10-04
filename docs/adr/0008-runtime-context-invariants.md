# ADR-0008: Runtime Context و Invariantهای صف

## Status

Accepted

## Context

پس از تثبیت Config، Slot Pool و قرارداد Async باید state اجرایی هر instance تعریف شود. طراحی اولیه شامل `active_slot` و `in_flight_slot` جداگانه بود، اما این اطلاعات با indexهای producer/consumer قابل استخراج هستند و نگهداری همزمان آن‌ها دو منبع حقیقت ایجاد می‌کند.

## Decision

`embcsv_t` موارد زیر را نگه می‌دارد:

- Pointer به Payload
- Pointer به Slot Metadata
- Sink کپی‌شده
- Completion descriptor پایدار
- نتیجه Async latch شده
- اندازه و تعداد Slotها
- `producer_index`
- `consumer_index`
- precisionهای عددی
- `row_has_field`
- state منطقی ساخت Row

`active_slot` جداگانه نگهداری نمی‌شود. در حالت `BUILDING_ROW`، `producer_index` همان Slot فعال است.

`in_flight_slot` جداگانه نگهداری نمی‌شود. در v1 حداکثر یک انتقال Async همزمان وجود دارد و در صورت وجود، همان `consumer_index` Slot در حالت `IN_FLIGHT` را مشخص می‌کند.

## Invariants

1. فقط Slot واقع در `producer_index` می‌تواند توسط Producer به `FILLING` تبدیل شود.
2. `producer_index` فقط بعد از commit موفق Row جلو می‌رود.
3. `consumer_index` قدیمی‌ترین Record صف‌شده را مشخص می‌کند.
4. در v1 حداکثر یک Slot `IN_FLIGHT` وجود دارد.
5. اگر انتقال Async موفق شود، Slot آزاد شده و `consumer_index` جلو می‌رود.
6. اگر Completion با خطا برگردد، Record نباید بی‌صدا از دست برود؛ Slot برای recovery/retry حفظ می‌شود و خطا latch می‌شود.
7. `row_has_field` مستقل از طول encoded نگهداری می‌شود؛ چون اولین Field می‌تواند empty باشد.
8. Instance بعد از Init نباید در حافظه relocate شود، زیرا Completion context به خود Instance ارجاع دارد.
9. دسترسی همزمان به یک Instance در v1 باید externally serialized باشد.

## Async error policy

در خطای Completion:

- `consumer_index` جلو نمی‌رود.
- Payload مربوط به Record حفظ می‌شود.
- Slot به حالت قابل retry بازگردانده می‌شود.
- خطا در `async_status` latch می‌شود.
- `embcsv_process()` خطای latch‌شده را قبل از retry بعدی به Caller گزارش می‌کند.

## Consequences

### مثبت

- حذف state تکراری
- RAM کمتر
- invariantهای ساده‌تر
- حفظ FIFO
- عدم drop بی‌صدای Record در خطای Async

### منفی

- v1 فقط یک request همزمان `IN_FLIGHT` دارد.
- Instance پس از Init قابل جابه‌جایی با memcpy نیست.
- Backendهای ISR-based باید callback را با Public API همان instance serialize کنند یا completion را به context مناسب defer کنند.
