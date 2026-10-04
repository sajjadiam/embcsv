# ADR-0007: اتمیک بودن افزودن Field

## Status

Accepted

## Context

در زمان ساخت یک CSV Record ممکن است فضای باقی‌مانده Slot برای Field جدید کافی نباشد. اگر Encoder ابتدا بخشی از Field یا delimiter را بنویسد و سپس کمبود فضا را تشخیص دهد، Row وارد وضعیت نیمه‌خراب می‌شود و recovery پیچیده خواهد شد.

## Decision

تمام عملیات `embcsv_add_*()` باید در سطح یک Field اتمیک باشند.

قبل از تغییر Payload، Core باید اندازه خروجی encode‌شده Field را محاسبه و ظرفیت موردنیاز را بررسی کند.

اگر ظرفیت کافی نباشد:

- `EMBCSV_EROW_TOO_LARGE` بازگردانده می‌شود.
- هیچ delimiter یا بخشی از Field نوشته نمی‌شود.
- طول فعلی Row تغییر نمی‌کند.
- Slot در حالت `FILLING` باقی می‌ماند.
- Caller می‌تواند Field دیگری را امتحان کند یا Row را در آینده abort کند.

برای String، محاسبه اندازه باید اثر quoting و escaping را در نظر بگیرد.

فضای لازم برای Record terminator یعنی `\r\n` باید پیش از افزودن Field رزرو شود تا `embcsv_end_row()` به علت پر شدن کامل Slot شکست نخورد.

## Consequences

### مثبت

- Row بعد از خطای ظرفیت همچنان معتبر می‌ماند.
- recovery ساده‌تر می‌شود.
- partial field تولید نمی‌شود.
- رفتار API قابل تست و deterministic است.

### منفی

- برخی Fieldها، به‌ویژه Stringها، ممکن است برای اندازه‌گیری و سپس encode شدن دو بار پیمایش شوند.
- Formatter عددی ممکن است به scratch storage کوچک نیاز داشته باشد.
