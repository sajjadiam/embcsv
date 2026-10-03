# ADR-0003: استفاده از Fixed-size Slot Pool

## Status

Accepted

## Context

در Async Zero-copy باید بتوان چند Record را بدون overwrite شدن داده‌های در حال انتقال نگهداری کرد.

یک Circular Buffer بایتی معمولی می‌تواند Record را در نقطه wrap به دو قسمت تقسیم کند و انتقال DMA/Zero-copy را پیچیده‌تر سازد.

## Decision

حافظه Payload به Slotهای هم‌اندازه تقسیم می‌شود.

هر Slot حداکثر یک CSV Record کامل را نگهداری می‌کند.

`slot_count` و `slot_size` در Configuration تعیین می‌شوند.

نسخه اولیه روی Single Producer / Single Consumer متمرکز است.

## Consequences

### مثبت

- Record همیشه contiguous است.
- مناسب DMA و Zero-copy
- State هر Record مستقل است.
- حافظه bounded و deterministic است.

### منفی

- بزرگ‌ترین Row باید داخل یک Slot جا شود.
- Slotهای بزرگ برای Rowهای کوچک ممکن است مقداری RAM بلااستفاده ایجاد کنند.
