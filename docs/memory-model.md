# مدل حافظه و Ownership

## 1. اصل کلی

`embcsv` در Core از Heap استفاده نمی‌کند.

تمام حافظه موردنیاز توسط Caller تأمین می‌شود.

## 2. Ownership

مالک حافظه همیشه Caller است.

CSV Core یا Backend فقط می‌توانند حافظه را طبق قرارداد borrow کنند.

```text
Owner    = Caller
Borrower = CSV Core / Backend
```

استفاده از یک Buffer به معنی انتقال Ownership نیست.

## 3. Slot Pool

یک حافظه Payload بزرگ به چند Slot با اندازه ثابت تقسیم می‌شود:

```text
┌────────────┬────────────┬────────────┐
│ Slot 0     │ Slot 1     │ Slot 2     │
└────────────┴────────────┴────────────┘
```

هر Record باید در یک Slot جا شود.

اگر:

```text
encoded_row_size > slot_size
```

کتابخانه باید خطای مشخص مانند `CSV_EROW_TOO_LARGE` برگرداند.

## 4. Metadata

Metadata هر Slot از Payload جدا است.

حداقل اطلاعات مورد انتظار:

- encoded length
- state

رابطه:

```text
metadata[0] ↔ payload slot 0
metadata[1] ↔ payload slot 1
metadata[2] ↔ payload slot 2
```

## 5. Zero-copy

در Async Zero-copy:

1. Core یک Slot آزاد را claim می‌کند.
2. Record مستقیماً در همان Slot encode می‌شود.
3. Slot به Backend submit می‌شود.
4. تا Completion، Slot در حالت `IN_FLIGHT` باقی می‌ماند.
5. Caller/Core نباید محتوای Slot را تغییر دهد.
6. پس از Completion، Slot دوباره `FREE` می‌شود.

## 6. Padding و Alignment

اندازه یک struct نباید با جمع ساده اندازه اعضا فرض شود.

مثلاً:

```c
typedef struct
{
    uint8_t state;
    uint32_t len;
} slot_meta_t;
```

ممکن است به دلیل padding بزرگ‌تر از 5 بایت باشد.

قاعده:

- از `sizeof` استفاده شود.
- `packed` بدون نیاز مشخص استفاده نشود.
- ترتیب اعضای struct با توجه به alignment بررسی شود.
- alignment موردنیاز DMA مسئولیت Backend/Caller است.
- Core نباید alignment خاصی مانند 32-byte را به‌صورت ثابت فرض کند.

## 7. Lifetime

هر حافظه‌ای که در Init ثبت می‌شود باید تا پایان Lifetime مربوط به Instance معتبر باقی بماند.

Pointer به storage محلی تابعی که پس از Init از Scope خارج می‌شود معتبر نیست.


## 8. ساختار Metadata نسخه فعلی

ساختار تأییدشده اولیه:

```c
typedef uint8_t csv_slot_state_t;

typedef struct
{
    uint16_t len;
    csv_slot_state_t state;
    uint8_t reserved;
} csv_slot_meta_t;
```

قیود این تصمیم:

- `slot_size` نباید از `UINT16_MAX` بیشتر شود.
- `reserved` در initialization باید صفر شود.
- ساختار نباید با `packed` تعریف شود، مگر اینکه در آینده دلیل معماری مشخصی ثبت شود.
- اندازه واقعی ساختار باید با `sizeof(csv_slot_meta_t)` سنجیده شود و نباید صرفاً از جمع اندازه اعضا فرض شود.
- semantics مربوط به هم‌زمانی و دسترسی Producer/Consumer به `state` هنوز در Buffer Pool contract نهایی می‌شود.
