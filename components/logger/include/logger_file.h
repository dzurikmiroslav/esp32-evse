#ifndef LOGGER_FILE_H_
#define LOGGER_FILE_H_

/**
 * @brief Start persisting the system log to littlefs.
 *
 * Rotates the existing /usr/system.log to /usr/system.log.previous (so the log
 * of the session before the last restart is preserved), opens a fresh
 * /usr/system.log, and starts a background task that drains the logger ring
 * buffer into it. The files are reachable over WebDAV at /dav/system.log and
 * /dav/system.log.previous.
 *
 * Must be called after the littlefs partition is mounted.
 */
void logger_file_init(void);

#endif /* LOGGER_FILE_H_ */
