/* ============================================================================
 * STAX — rtc.h
 * ARM PrimeCell PL031 Real-Time Clock (RTC) Driver with IST (Mumbai) Timezone
 * ============================================================================ */
#ifndef RTC_H
#define RTC_H

#include <stdint.h>

#define PL031_RTC_BASE 0x101E8000UL
#define PL031_RTC_DR   (*(volatile uint32_t *)(PL031_RTC_BASE + 0x00)) /* Data register / current seconds */
#define PL031_RTC_MR   (*(volatile uint32_t *)(PL031_RTC_BASE + 0x04)) /* Match register */
#define PL031_RTC_LR   (*(volatile uint32_t *)(PL031_RTC_BASE + 0x08)) /* Load register */
#define PL031_RTC_CR   (*(volatile uint32_t *)(PL031_RTC_BASE + 0x0C)) /* Control register */

/* IST (Indian Standard Time - Mumbai/Kolkata): UTC + 5h 30m = +19800 seconds */
#define TIMEZONE_OFFSET_IST_SECS 19800

typedef struct {
    int year;    /* e.g. 2026 */
    int month;   /* 1 - 12 */
    int day;     /* 1 - 31 */
    int hour;    /* 0 - 23 */
    int min;     /* 0 - 59 */
    int sec;     /* 0 - 59 */
    int wday;    /* 0 = Sun, 1 = Mon, ..., 6 = Sat */
} rtc_datetime_t;

void rtc_init(void);
uint32_t rtc_get_epoch(void);
void rtc_get_ist(rtc_datetime_t *dt);
void rtc_format_ist_navbar(char *buf, int max_len);
void rtc_format_ist_full(char *buf, int max_len);

#endif /* RTC_H */
