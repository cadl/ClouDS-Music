#include "network_retry.h"
#include "net.h"

#include <assert.h>

int main(void) {
    assert(net_error_is_transport(NET_ERROR_TRANSPORT));
    assert(net_error_is_transport(NET_ERROR_TLS_VERIFY));
    assert(!net_error_is_transport(NET_ERROR_HTTP));

    NetworkRetryState state;
    network_retry_init(&state);
    assert(!network_retry_pending(&state));
    assert(!network_retry_due(&state, 1000));

    network_retry_request_immediate(&state);
    assert(network_retry_pending(&state));
    assert(network_retry_due(&state, 1000));
    network_retry_started(&state);
    assert(state.probe_in_flight);
    assert(!network_retry_due(&state, 1000));

    network_retry_failed(&state, 1000);
    assert(!network_retry_due(&state, 2999));
    assert(network_retry_due(&state, 3000));
    network_retry_started(&state);
    network_retry_failed(&state, 3000);
    assert(!network_retry_due(&state, 7999));
    assert(network_retry_due(&state, 8000));

    network_retry_started(&state);
    network_retry_request_immediate(&state);
    network_retry_cancelled(&state, 9000);
    assert(network_retry_due(&state, 9000));

    network_retry_started(&state);
    network_retry_failed(&state, 10000);
    assert(network_retry_due(&state, 20000));
    network_retry_started(&state);
    network_retry_failed(&state, 20000);
    assert(network_retry_due(&state, 50000));
    network_retry_started(&state);
    network_retry_failed(&state, 50000);
    assert(network_retry_due(&state, 80000));

    network_retry_succeeded(&state);
    assert(!network_retry_pending(&state));
    assert(!network_retry_due(&state, UINT64_MAX));

    assert(network_request_retry_allowed(
        NETWORK_REQUEST_FAILURE_TRANSIENT, 1, false));
    assert(network_request_retry_allowed(
        NETWORK_REQUEST_FAILURE_TLS_VERIFY, 1, false));
    assert(!network_request_retry_allowed(
        NETWORK_REQUEST_FAILURE_TRANSIENT, 2, false));
    assert(!network_request_retry_allowed(
        NETWORK_REQUEST_FAILURE_TRANSIENT, 1, true));
    assert(!network_request_retry_allowed(
        NETWORK_REQUEST_FAILURE_CANCELLED, 1, false));
    assert(!network_request_retry_allowed(
        NETWORK_REQUEST_FAILURE_PERMANENT, 1, false));
    return 0;
}
