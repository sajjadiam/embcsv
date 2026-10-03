# ADR-0005: جداسازی Slot Metadata از Payload

## Status

Accepted

## Context

قرار دادن Metadata و Payload در یک Block مشترک، محاسبات alignment، padding و نیازهای DMA را پیچیده می‌کند.

## Decision

Payload memory و Slot metadata به‌صورت منطقی و فیزیکی جدا نگهداری می‌شوند.

Caller هر دو Storage موردنیاز را تأمین می‌کند.

از `packed` برای Metadata داخلی بدون دلیل مشخص استفاده نمی‌شود.

## Consequences

### مثبت

- Layout قابل فهم‌تر
- کنترل بهتر alignment مربوط به Payload
- کاهش coupling بین DMA constraints و Metadata
- امکان بررسی `sizeof` واقعی Metadata توسط Compiler

### منفی

- Caller باید دو ناحیه Storage فراهم کند.
- Configuration کمی بزرگ‌تر می‌شود.
