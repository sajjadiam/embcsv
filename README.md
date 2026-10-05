# embcsv

کتابخانه‌ای سبک و قابل‌حمل برای تبدیل داده‌های فیلدبندی‌شده به رکوردهای CSV در سامانه‌های Embedded و محیط‌های محدود از نظر منابع.

> وضعیت پروژه: **طراحی معماری و API — پیش از v1.0**

## اهداف اصلی

- مستقل از STM32 HAL، RTOS و Filesystem
- بدون تخصیص حافظه پویا (Heap)
- حافظه قابل پیش‌بینی و Caller-provided
- پشتیبانی از Backendهای synchronous و asynchronous
- امکان Zero-copy در Backendهای مناسب
- مناسب برای UART، USB، File، شبکه و Backendهای سفارشی
- قابل تست روی Host بدون نیاز به سخت‌افزار

## معماری مفهومی

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

CSV Core نباید از نوع Backend اطلاعی داشته باشد.

## ساختار پروژه

```text
embcsv/
├── README.md
├── LICENSE
├── CHANGELOG.md
├── include/
├── src/
├── tests/
├── examples/
└── docs/
    ├── design.md
    ├── requirements.md
    ├── architecture.md
    ├── memory-model.md
    ├── error-model.md
    └── adr/
```

## فایل‌های Source

برای استفاده از کتابخانه، هر دو فایل پیاده‌سازی زیر باید در build قرار بگیرند:

```text
src/embcsv.c
src/embcsv_float.c
```

فایل `src/embcsv_float.h` یک هدر داخلی است و بخشی از Public API نیست. Formatter شناور
بدون Heap و بدون وابستگی به `stdio` یا locale کار می‌کند و برای `float` و `double`
خروجی ثابت و قابل‌پیش‌بینی تولید می‌کند.

## مستندات طراحی

تصمیم‌های معماری مهم در پوشه `docs/adr/` به‌صورت ADR ثبت می‌شوند تا دلیل تصمیم‌ها و پیامدهای آن‌ها در طول توسعه قابل ردیابی باشد.

## استاندارد CSV

هدف نسخه v1 پیاده‌سازی قواعد متداول CSV با سازگاری رفتاری با RFC 4180 است؛ جزئیات دقیق delimiter، quoting، escaping، CR/LF و policy مربوط به مقادیر خاص عددی پیش از تثبیت v1 نهایی می‌شود.

## وضعیت فعلی

هنوز Public API نهایی نشده است. ساختارها، قرارداد Async، Error Model و جزئیات Buffer Pool در حال طراحی هستند.
