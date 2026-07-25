#include "network_retry.h"

#include <stddef.h>
#include <string.h>

static const uint32_t retry_delays_ms[] = {2000U, 5000U, 10000U, 30000U};

static uint32_t retry_delay(unsigned int failure_count) {
    size_t index = failure_count > 0 ? (size_t)failure_count - 1U : 0U;
    size_t count = sizeof(retry_delays_ms) / sizeof(retry_delays_ms[0]);
    if (index >= count) index = count - 1U;
    return retry_delays_ms[index];
}

void network_retry_init(NetworkRetryState *state) {
    if (state) memset(state, 0, sizeof(*state));
}

void network_retry_request_immediate(NetworkRetryState *state) {
    if (!state) return;
    state->immediate = true;
    state->next_retry_ms = 0;
}

bool network_retry_pending(const NetworkRetryState *state) {
    return state && (state->immediate || state->probe_in_flight ||
                     state->next_retry_ms != 0);
}

bool network_retry_due(const NetworkRetryState *state, uint64_t now_ms) {
    if (!state || state->probe_in_flight) return false;
    return state->immediate ||
           (state->next_retry_ms != 0 && now_ms >= state->next_retry_ms);
}

void network_retry_started(NetworkRetryState *state) {
    if (!state) return;
    state->immediate = false;
    state->next_retry_ms = 0;
    state->probe_in_flight = true;
}

void network_retry_succeeded(NetworkRetryState *state) {
    network_retry_init(state);
}

void network_retry_failed(NetworkRetryState *state, uint64_t now_ms) {
    if (!state) return;
    size_t count = sizeof(retry_delays_ms) / sizeof(retry_delays_ms[0]);
    if (state->failure_count < count) state->failure_count++;
    state->probe_in_flight = false;
    state->immediate = false;
    state->next_retry_ms = now_ms + retry_delay(state->failure_count);
}

void network_retry_cancelled(NetworkRetryState *state, uint64_t now_ms) {
    if (!state) return;
    state->probe_in_flight = false;
    if (state->immediate) {
        state->next_retry_ms = 0;
        return;
    }
    state->next_retry_ms = now_ms + retry_delay(state->failure_count);
}

bool network_request_retry_allowed(NetworkRequestFailure failure,
                                   unsigned int completed_attempts,
                                   bool output_published) {
    if (completed_attempts != 1U || output_published) return false;
    return failure == NETWORK_REQUEST_FAILURE_TRANSIENT ||
           failure == NETWORK_REQUEST_FAILURE_TLS_VERIFY;
}
