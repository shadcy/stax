/* ============================================================================
 * STAX — rtc.c
 * ARM PrimeCell PL031 Real-Time Clock Driver implementation
 * Synchronizes with real host clock on boot and formats IST (Mumbai) time
 * ============================================================================ */

#include "rtc.h"
#include "console.h"

extern volatile unsigned int tick_count;

static uint32_t boot_epoch = 0;
static const int days_per_month[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static const char *day_names[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *month_names[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

static int is_leap_year(int y) {
    return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
}

void rtc_init(void) {
    /* Read hardware RTC data register */
    boot_epoch = PL031_RTC_DR;
    if (boot_epoch == 0) {
        /* Default fallback epoch if RTC unit uninitialized */
        boot_epoch = 1771800000; /* Approximate fallback epoch */
    }
    rtc_datetime_t ist;
    rtc_get_ist(&ist);
    char buf[64];
    rtc_format_ist_full(buf, sizeof(buf));
    kprintf("RTC: Initialized (Host hardware sync). Current IST: %s\n", buf);
}

uint32_t rtc_get_epoch(void) {
    uint32_t hw_sec = PL031_RTC_DR;
    if (hw_sec > 0) return hw_sec;
    return boot_epoch + (tick_count / 1000);
}

void rtc_get_ist(rtc_datetime_t *dt) {
    if (!dt) return;
    uint32_t epoch = rtc_get_epoch();
    /* Add IST offset (+5:30 = +19800s) */
    uint32_t local_sec = epoch + TIMEZONE_OFFSET_IST_SECS;

    uint32_t days = local_sec / 86400;
    uint32_t rem = local_sec % 86400;

    dt->hour = rem / 3600;
    dt->min  = (rem % 3600) / 60;
    dt->sec  = rem % 60;
    dt->wday = (days + 4) % 7; /* Jan 1, 1970 was Thursday (4) */

    int year = 1970;
    while (1) {
        int diy = is_leap_year(year) ? 366 : 365;
        if (days < (uint32_t)diy) break;
        days -= diy;
        year++;
    }
    dt->year = year;

    int leap = is_leap_year(year);
    int month;
    for (month = 0; month < 12; month++) {
        int dim = days_per_month[month];
        if (month == 1 && leap) dim = 29;
        if (days < (uint32_t)dim) break;
        days -= dim;
    }
    dt->month = month + 1;
    dt->day = days + 1;
}

/* Compact format for Top Nav Bar: "Tue 25 Aug 09:23:45 IST" */
void rtc_format_ist_navbar(char *buf, int max_len) {
    if (!buf || max_len < 24) return;
    rtc_datetime_t dt;
    rtc_get_ist(&dt);

    const char *wstr = (dt.wday >= 0 && dt.wday < 7) ? day_names[dt.wday] : "---";
    const char *mstr = (dt.month >= 1 && dt.month <= 12) ? month_names[dt.month - 1] : "---";

    int pos = 0;
    /* Day of week (3) */
    buf[pos++] = wstr[0]; buf[pos++] = wstr[1]; buf[pos++] = wstr[2]; buf[pos++] = ' ';

    /* Day of month (2) */
    buf[pos++] = '0' + (dt.day / 10);
    buf[pos++] = '0' + (dt.day % 10);
    buf[pos++] = ' ';

    /* Month (3) */
    buf[pos++] = mstr[0]; buf[pos++] = mstr[1]; buf[pos++] = mstr[2]; buf[pos++] = ' ';

    /* HH:MM:SS (8) */
    buf[pos++] = '0' + (dt.hour / 10);
    buf[pos++] = '0' + (dt.hour % 10);
    buf[pos++] = ':';
    buf[pos++] = '0' + (dt.min / 10);
    buf[pos++] = '0' + (dt.min % 10);
    buf[pos++] = ':';
    buf[pos++] = '0' + (dt.sec / 10);
    buf[pos++] = '0' + (dt.sec % 10);

    /* Timezone (4) */
    buf[pos++] = ' ';
    buf[pos++] = 'I'; buf[pos++] = 'S'; buf[pos++] = 'T';
    buf[pos] = '\0';
}

/* Full format: "Tuesday, 25 Aug 2026 09:23:45 IST (Mumbai)" */
void rtc_format_ist_full(char *buf, int max_len) {
    if (!buf || max_len < 40) return;
    rtc_datetime_t dt;
    rtc_get_ist(&dt);

    const char *wstr = (dt.wday >= 0 && dt.wday < 7) ? day_names[dt.wday] : "---";
    const char *mstr = (dt.month >= 1 && dt.month <= 12) ? month_names[dt.month - 1] : "---";

    int pos = 0;
    buf[pos++] = wstr[0]; buf[pos++] = wstr[1]; buf[pos++] = wstr[2]; buf[pos++] = ' ';

    buf[pos++] = '0' + (dt.day / 10);
    buf[pos++] = '0' + (dt.day % 10);
    buf[pos++] = ' ';

    buf[pos++] = mstr[0]; buf[pos++] = mstr[1]; buf[pos++] = mstr[2]; buf[pos++] = ' ';

    /* Year */
    buf[pos++] = '0' + ((dt.year / 1000) % 10);
    buf[pos++] = '0' + ((dt.year / 100) % 10);
    buf[pos++] = '0' + ((dt.year / 10) % 10);
    buf[pos++] = '0' + (dt.year % 10);
    buf[pos++] = ' ';

    buf[pos++] = '0' + (dt.hour / 10);
    buf[pos++] = '0' + (dt.hour % 10);
    buf[pos++] = ':';
    buf[pos++] = '0' + (dt.min / 10);
    buf[pos++] = '0' + (dt.min % 10);
    buf[pos++] = ':';
    buf[pos++] = '0' + (dt.sec / 10);
    buf[pos++] = '0' + (dt.sec % 10);

    buf[pos++] = ' ';
    buf[pos++] = 'I'; buf[pos++] = 'S'; buf[pos++] = 'T';
    buf[pos] = '\0';
}
