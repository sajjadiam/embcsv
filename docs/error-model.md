# مدل خطا

## هدف

تمام Public Operationها باید نتیجه قابل تشخیص داشته باشند و پس از خطا State سیستم تعریف‌شده باقی بماند.

## Statusهای اولیه پیشنهادی

```c
typedef enum
{
    CSV_OK              =  0,
    CSV_PENDING         =  1,

    CSV_EINVAL          = -1,
    CSV_ESTATE          = -2,
    CSV_ENO_BUFFER      = -3,
    CSV_EROW_TOO_LARGE  = -4,
    CSV_EBUSY           = -5,
    CSV_EIO             = -6,
    CSV_ETIMEOUT        = -7
} csv_status_t;
```

این enum هنوز تا نهایی‌شدن Public API قابل تغییر است.

## معنی کلی

### CSV_OK
عملیات کامل شده و Core/Backend دیگر طبق همان Call نیازی به Buffer ندارد.

### CSV_PENDING
عملیات پذیرفته شده ولی Completion نهایی هنوز رخ نداده است.

### CSV_EINVAL
پارامتر قابل بررسی نامعتبر است؛ مانند NULL در جایی که مجاز نیست.

### CSV_ESTATE
تابع در State نامعتبر فراخوانی شده است.

### CSV_ENO_BUFFER
Slot آزاد موجود نیست.

### CSV_EROW_TOO_LARGE
Record تولیدشده در Slot تعیین‌شده جا نمی‌شود.

### CSV_EBUSY
Resource مربوطه در حال استفاده است.

### CSV_EIO
Backend انتقال را نپذیرفته یا با خطای I/O مواجه شده است.

### CSV_ETIMEOUT
برای عملیاتی که قرارداد Timeout دارد، زمان مجاز تمام شده است.

## Recovery Principles

- ورودی نامعتبر نباید State را خراب کند.
- کمبود ظرفیت نباید باعث Buffer Overflow شود.
- خطای قابل بازیابی نباید الزاماً نیازمند Re-init باشد.
- خطای Backend باید به Caller propagate شود.
- بعد از هر خطای گزارش‌شده، State باید تعریف‌شده باشد.

## محدودیت تشخیص Pointer

کتابخانه می‌تواند `NULL` را بررسی کند، اما در C عمومی نمی‌تواند معتبر بودن arbitrary pointer، dangling pointer یا address اشتباه را تضمین کند.

این موارد بخشی از قرارداد Caller هستند.
