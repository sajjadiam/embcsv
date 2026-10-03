# ADR-0004: پشتیبانی از Async Zero-copy در Backend

## Status

Accepted

## Context

در Backendهایی مانند DMA یا USB asynchronous، کپی اضافه باعث مصرف CPU و RAM می‌شود.

## Decision

معماری اجازه می‌دهد Backend همان Slot ساخته‌شده توسط CSV Core را تا Completion نگه دارد.

در این حالت:

```text
FREE → FILLING → READY → IN_FLIGHT → FREE
```

Backend پس از اتمام استفاده از Slot باید Completion را گزارش کند.

## Consequences

### مثبت

- حذف memcpy بین Core و Backend
- مناسب DMA
- کاهش مصرف CPU

### منفی

- Lifetime بافر پیچیده‌تر می‌شود.
- Slot تا Completion قابل reuse نیست.
- Backpressure باید صریح مدیریت شود.
