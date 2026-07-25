#pragma once

#include "netease.h"

#include <stddef.h>

int auth_load(NeteaseClient *client, const char *path,
              char *error, size_t error_size);
int auth_save(const NeteaseClient *client, const char *path,
              char *error, size_t error_size);
bool auth_should_clear_after_validation(NeteaseFailure failure);
void auth_clear(NeteaseClient *client, const char *path);
