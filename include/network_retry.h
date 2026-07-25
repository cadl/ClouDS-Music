#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint64_t next_retry_ms;
    unsigned int failure_count;
    bool immediate;
    bool probe_in_flight;
} NetworkRetryState;

typedef enum {
    NETWORK_REQUEST_FAILURE_NONE = 0,
    NETWORK_REQUEST_FAILURE_CANCELLED,
    NETWORK_REQUEST_FAILURE_TRANSIENT,
    NETWORK_REQUEST_FAILURE_TLS_VERIFY,
    NETWORK_REQUEST_FAILURE_PERMANENT
} NetworkRequestFailure;

void network_retry_init(NetworkRetryState *state);
void network_retry_request_immediate(NetworkRetryState *state);
bool network_retry_pending(const NetworkRetryState *state);
bool network_retry_due(const NetworkRetryState *state, uint64_t now_ms);
void network_retry_started(NetworkRetryState *state);
void network_retry_succeeded(NetworkRetryState *state);
void network_retry_failed(NetworkRetryState *state, uint64_t now_ms);
void network_retry_cancelled(NetworkRetryState *state, uint64_t now_ms);

bool network_request_retry_allowed(NetworkRequestFailure failure,
                                   unsigned int completed_attempts,
                                   bool output_published);
