#include "weapi.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    static const unsigned char secret[] =
        "abcdefghijklmnop";
    char error[192] = {0};
    char *form = NULL;
    assert(weapi_build_form(
        "{\"afresh\":false,\"csrf_token\":\"\"}", secret,
        &form, error, sizeof(error)) == 0);
    assert(form != NULL);
    static const char expected[] =
        "params=GHYAB1BmYzHTXIVakSE1IGuCIb7bHmMrEpPijqI%2BfsCpMQ7BjIbsKAFMyv"
        "3cF3CT6wRUpYRFvNnPOHvNHbB3%2B7lz4%2BfqjyOkTqAqt7jeo3c%3D&encSec"
        "Key=d15a1683c992095d0c234c19966605c5c5964911268bbeda8cb8d08d834913e"
        "59d53b32358903a121b5fca784c1f5ae44951fd02524df58ecc98e52cc7cf868"
        "9b42c2e93ddf05b0592512d87f5960467e2f086c018849d76014d323500e30f1"
        "3ef4cafbb0cf5a66731a3f1776c75ca35d0062dac70a3e33245afabcf479384"
        "87";
    assert(strcmp(form, expected) == 0);
    assert(strncmp(form, "params=", 7) == 0);
    const char *key = strstr(form, "&encSecKey=");
    assert(key != NULL);
    key += strlen("&encSecKey=");
    assert(strlen(key) == 256);
    assert(strspn(key, "0123456789abcdef") == 256);
    free(form);

    static const unsigned char invalid[] =
        "abcdefghijklmno+";
    form = NULL;
    assert(weapi_build_form("{}", invalid, &form,
                            error, sizeof(error)) != 0);
    assert(form == NULL);
    static const unsigned char embedded_zero[WEAPI_SECRET_LENGTH] =
        "abcdefghijklmno";
    assert(weapi_build_form("{}", embedded_zero, &form,
                            error, sizeof(error)) != 0);
    assert(form == NULL);
    puts("weapi tests: ok");
    return 0;
}
