#pragma once

#include <stddef.h>

#define WEAPI_SECRET_LENGTH 16U

int weapi_build_form(const char *json,
                     const unsigned char secret[WEAPI_SECRET_LENGTH],
                     char **form, char *error, size_t error_size);
