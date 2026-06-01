#ifndef CRASH_REPORT_H_
#define CRASH_REPORT_H_

/**
 * @brief Report the cause of the last restart.
 *
 * Logs the reset reason (via the standard ESP log, which is routed into the
 * logger ring buffer / "System logs"). When the last reset was a panic, also
 * logs the crash backtrace that was captured by the panic handler hook into
 * RTC no-init memory.
 *
 * Must be called once early in app_main(), after the logger vprintf hook has
 * been installed, so its output ends up in the System logs.
 */
void crash_report_init(void);

#endif /* CRASH_REPORT_H_ */
