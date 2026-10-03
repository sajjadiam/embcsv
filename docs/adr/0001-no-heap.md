# ADR-0001: عدم استفاده از Heap در CSV Core

## Status

Accepted

## Context

کتابخانه برای Embedded و محیط‌های محدود از نظر منابع طراحی می‌شود. استفاده از allocation پویا می‌تواند مصرف حافظه و failure modeها را کمتر قابل پیش‌بینی کند.

## Decision

CSV Core از `malloc`، `calloc`، `realloc` و `free` استفاده نمی‌کند.

حافظه کاری توسط Caller تأمین می‌شود.

## Consequences

### مثبت

- مصرف حافظه قابل پیش‌بینی
- مناسب Bare-metal و RTOS
- تست ساده‌تر failureهای حافظه
- عدم fragmentation ناشی از Core

### منفی

- Caller باید Storage را مدیریت کند.
- ظرفیت‌ها باید از قبل طراحی شوند.
