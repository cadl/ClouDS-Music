#include "diagnostic_text.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char output[192];

    diagnostic_sanitize_detail(
        output, sizeof(output),
        "cert verify failed: BADCERT_FUTURE");
    assert(strcmp(output, "cert verify failed: BADCERT_FUTURE") == 0);

    diagnostic_sanitize_detail(
        output, sizeof(output),
        "redirect https://example.test/file?token=secret failed");
    assert(strcmp(output, "redirect [url] failed") == 0);

    diagnostic_sanitize_detail(output, sizeof(output),
                               "line one\n\"line two\"\\end");
    assert(strcmp(output, "line one \\\"line two\\\"\\\\end") == 0);

    diagnostic_sanitize_detail(output, sizeof(output),
                               "Cookie: MUSIC_U=secret");
    assert(strcmp(output, "[redacted sensitive detail]") == 0);
    diagnostic_sanitize_detail(output, sizeof(output),
                               "redirect token=secret");
    assert(strcmp(output, "[redacted sensitive detail]") == 0);

    char short_output[5];
    assert(diagnostic_sanitize_detail(short_output, sizeof(short_output),
                                      "abcdef") == 4);
    assert(strcmp(short_output, "abcd") == 0);
    assert(diagnostic_sanitize_detail(NULL, 0, "ignored") == 0);

    puts("diagnostic text tests: ok");
    return 0;
}
