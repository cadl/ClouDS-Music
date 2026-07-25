#include "qrcodegen.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    uint8_t temp[qrcodegen_BUFFER_LEN_MAX];
    uint8_t qr[qrcodegen_BUFFER_LEN_MAX];
    assert(qrcodegen_encodeText(
        "https://music.163.com/login?codekey=test-key", temp, qr,
        qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO, true));
    int size = qrcodegen_getSize(qr);
    assert(size > 0 && size <= 177);
    assert(qrcodegen_getModule(qr, 0, 0));
    puts("QR tests: ok");
    return 0;
}
