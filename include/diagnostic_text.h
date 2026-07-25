#pragma once

#include <stddef.h>

/* Produces a single-line, quoted-field-safe diagnostic string.  Full URLs and
 * values associated with credentials are deliberately omitted from logs. */
size_t diagnostic_sanitize_detail(char *output, size_t output_size,
                                  const char *input);
