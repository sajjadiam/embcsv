# ADR-0002: استفاده از Output Sink Abstraction

## Status

Accepted

## Context

CSV Core باید بتواند بدون وابستگی مستقیم به UART، USB، Filesystem، شبکه یا HAL استفاده شود.

## Decision

خروجی از طریق یک Sink abstract به Backend تحویل داده می‌شود.

Sink دارای function pointer و opaque user context است.

`user_ctx` متعلق به Backend/Caller است. Core آن را تفسیر نمی‌کند.

## Consequences

### مثبت

- Portability
- امکان Mock Backend در Unit Test
- پشتیبانی از چند Instance
- عدم وابستگی Core به Platform

### منفی

- قرارداد lifetime و callback باید دقیق مستند شود.
- خطاهای Backend باید به مدل خطای عمومی map یا expose شوند.
