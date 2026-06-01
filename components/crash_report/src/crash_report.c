#include "crash_report.h"

#include <esp_attr.h>
#include <esp_log.h>
#include <esp_system.h>
#include <stdio.h>
#include <string.h>

#include "esp_debug_helpers.h"
#include "esp_private/panic_internal.h"

#if __XTENSA__
#include "xtensa_context.h"
#endif

#define CRASH_MAGIC    0xC0FFEE01u
#define CRASH_BT_DEPTH 16

static const char* TAG = "crash_report";

// Captured crash record. RTC_NOINIT survives a software/panic reset (which is
// what a panic is) but is intentionally undefined after a cold power-on or
// brownout. The magic field plus the ESP_RST_PANIC check in crash_report_init()
// guard against treating uninitialized RTC RAM as a real crash.
typedef struct {
    uint32_t magic;                  // CRASH_MAGIC when the record is valid
    uint32_t core;                   // core that triggered the panic
    uint32_t exception;              // panic_exception_t value
    uint32_t exc_addr;               // faulting instruction address (info->addr)
    uint8_t bt_depth;                // number of valid entries in bt_pc
    bool bt_truncated;               // backtrace was cut short or hit a bad frame
    uint32_t bt_pc[CRASH_BT_DEPTH];  // backtrace program counters
} crash_rec_t;

static RTC_NOINIT_ATTR crash_rec_t s_crash_rec;

// Xtensa return addresses encode the window-call increment in the top two bits
// and point just after the call site. This mirrors esp_cpu_process_stack_pc()
// so the logged addresses match the addresses produced by the normal IDF panic
// backtrace and resolve cleanly with addr2line. Reimplemented locally to avoid
// depending on the (version-dependent) location of that inline.
static inline uint32_t IRAM_ATTR process_pc(uint32_t pc)
{
#if __XTENSA__
    if (pc & 0x80000000) {
        pc = (pc & 0x3fffffff) | 0x40000000;
    }
    return pc - 3;
#else
    return pc;
#endif
}

// Panic-time hook (installed via -Wl,--wrap=esp_panic_handler). Runs in panic
// context: interrupts disabled, possibly with cache off. It must stay minimal —
// no heap, no locks, no logging, and no reads from flash-mapped rodata (so the
// human-readable reason string is mapped from info->exception at boot instead of
// dereferenced here). It only reads registers / the on-stack exception frame and
// writes plain words into RTC RAM, then forwards to the real handler so the
// normal panic + reboot flow is unchanged.
extern void __real_esp_panic_handler(panic_info_t* info);

void IRAM_ATTR __wrap_esp_panic_handler(panic_info_t* info)
{
    s_crash_rec.magic = 0;  // invalidate while we are writing
    s_crash_rec.core = (uint32_t)info->core;
    s_crash_rec.exception = (uint32_t)info->exception;
    s_crash_rec.exc_addr = (uint32_t)info->addr;
    s_crash_rec.bt_depth = 0;
    s_crash_rec.bt_truncated = false;

#if __XTENSA__
    if (info->frame != NULL) {
        const XtExcFrame* xt = (const XtExcFrame*)info->frame;
        esp_backtrace_frame_t frame = {
            .pc = xt->pc,
            .sp = xt->a1,
            .next_pc = xt->a0,
        };

        s_crash_rec.bt_pc[s_crash_rec.bt_depth++] = process_pc(frame.pc);

        while (s_crash_rec.bt_depth < CRASH_BT_DEPTH && frame.next_pc != 0) {
            if (!esp_backtrace_get_next_frame(&frame)) {
                s_crash_rec.bt_truncated = true;
                break;
            }
            s_crash_rec.bt_pc[s_crash_rec.bt_depth++] = process_pc(frame.pc);
        }

        if (!s_crash_rec.bt_truncated && frame.next_pc != 0) {
            // Ran out of slots before reaching the bottom of the stack.
            s_crash_rec.bt_truncated = true;
        }
    }
#endif

    s_crash_rec.magic = CRASH_MAGIC;

    __real_esp_panic_handler(info);
}

static const char* reset_reason_str(esp_reset_reason_t reason)
{
    switch (reason) {
    case ESP_RST_POWERON: return "Power-on";
    case ESP_RST_EXT: return "External pin";
    case ESP_RST_SW: return "Software (esp_restart)";
    case ESP_RST_PANIC: return "Panic / exception";
    case ESP_RST_INT_WDT: return "Interrupt watchdog";
    case ESP_RST_TASK_WDT: return "Task watchdog";
    case ESP_RST_WDT: return "Other watchdog";
    case ESP_RST_DEEPSLEEP: return "Deep-sleep wake";
    case ESP_RST_BROWNOUT: return "Brownout (power supply dip)";
    case ESP_RST_SDIO: return "SDIO";
    case ESP_RST_UNKNOWN:
    default: return "Unknown";
    }
}

static const char* exception_str(uint32_t exception)
{
    switch ((panic_exception_t)exception) {
    case PANIC_EXCEPTION_DEBUG: return "Debug exception";
    case PANIC_EXCEPTION_IWDT: return "Interrupt watchdog";
    case PANIC_EXCEPTION_TWDT: return "Task watchdog";
    case PANIC_EXCEPTION_ABORT: return "abort()";
    case PANIC_EXCEPTION_FAULT: return "CPU fault";
    default: return "Unknown";
    }
}

void crash_report_init(void)
{
    esp_reset_reason_t reason = esp_reset_reason();

    ESP_LOGW(TAG, "Last reset reason: %s", reset_reason_str(reason));

    if (reason == ESP_RST_PANIC && s_crash_rec.magic == CRASH_MAGIC) {
        ESP_LOGW(TAG, "=== Last crash: PANIC ===");
        ESP_LOGW(TAG, "Exception: %s (core %u), fault addr 0x%08x", exception_str(s_crash_rec.exception),
            (unsigned int)s_crash_rec.core, (unsigned int)s_crash_rec.exc_addr);

        // Single space-separated "0x..." line so it can be pasted straight into
        // xtensa-esp32-elf-addr2line -pfiaC -e build/esp32-evse.elf <addresses>
        char line[16 + CRASH_BT_DEPTH * 11 + 1];
        int pos = snprintf(line, sizeof(line), "Backtrace:");
        for (uint8_t i = 0; i < s_crash_rec.bt_depth && pos > 0 && pos < (int)sizeof(line); i++) {
            pos += snprintf(line + pos, sizeof(line) - pos, " 0x%08x", (unsigned int)s_crash_rec.bt_pc[i]);
        }
        ESP_LOGW(TAG, "%s%s", line, s_crash_rec.bt_truncated ? " <-TRUNCATED/CORRUPTED" : "");
    }

    // Consume the record so a later clean reboot does not re-report this crash.
    s_crash_rec.magic = 0;
}
