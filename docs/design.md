# طراحی کلی embcsv

## 1. هدف

`embcsv` داده‌های فیلدبندی‌شده را دریافت می‌کند و آن‌ها را مطابق قواعد CSV به جریان بایت تبدیل می‌کند.

## 2. Scope

ورودی منطقی کتابخانه مجموعه‌ای از Fieldهای مشخص است؛ مانند:

- signed integer
- unsigned integer
- float / double
- bool
- string

خروجی یک CSV encoded byte stream است.

## 3. Non-Goals

CSV Core مسئول موارد زیر نیست:

- خواندن Sensor
- ساخت Timestamp
- مدیریت UART
- مدیریت USB
- مدیریت Filesystem
- باز و بسته کردن File
- Mount کردن Media
- سیاست Logging
- مدیریت RTOS
- تخصیص Heap

## 4. Use Caseهای اصلی

1. تولید Header به‌عنوان یک CSV Record معمولی
2. تولید یک Row از چند Field
3. تبدیل انواع داده پشتیبانی‌شده به نمایش متنی
4. Quote و Escape صحیح Stringها
5. تحویل خروجی به Backend مستقل از Platform
6. انتقال خطای Backend به Caller
7. پشتیبانی از چند Instance مستقل
8. پشتیبانی از Backendهای Sync و Async

## 5. اصل Dependency

اگر چیزی برای معنای CSV ضروری نیست و به محیط اجرا مربوط است، CSV Core نباید مستقیماً به آن وابسته باشد.

بنابراین Core نباید headerهایی مانند موارد زیر را include کند:

```c
#include "stm32xxxx_hal.h"
#include "ff.h"
#include "FreeRTOS.h"
```

وابستگی‌های Platform از طریق Interfaceهای abstract تزریق می‌شوند.

## 6. State

State مربوط به انتقال Async متعلق به Backend است.

CSV Core فقط ممکن است State منطقی مربوط به ساخت Record داشته باشد، مانند:

```text
READY
  ↓ begin_row()
BUILDING_ROW
  ↓ end_row()
READY
```

Backend Async می‌تواند State مستقل زیر را داشته باشد:

```text
IDLE → BUSY → IDLE
          ↓
        ERROR
```
