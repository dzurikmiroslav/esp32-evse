#ifndef DUK_CONSOLE_H_
#define DUK_CONSOLE_H_

#include "duktape.h"

#define DUK_CONSOLE_PROXY_WRAPPER  (1U << 0)

/* Flush output after every call. */
#define DUK_CONSOLE_FLUSH          (1U << 1)

#define DUK_CONSOLE_STDOUT_ONLY    (1U << 2)

#define DUK_CONSOLE_STDERR_ONLY    (1U << 3)

void duk_console_init(duk_context *ctx, duk_uint_t flags);

#endif /* DUK_CONSOLE_H_ */