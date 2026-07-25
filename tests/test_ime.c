#include "ime_pinyin.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void type(PinyinIme *ime, const char *letters) {
    for (const char *p = letters; *p; p++) ime_input(ime, *p);
}

static int candidate_index(PinyinIme *ime, const char *word) {
    for (int i = 0; i < ime_candidate_count(ime); i++)
        if (strcmp(ime_candidate(ime, i), word) == 0) return i;
    return -1;
}

static int contains(PinyinIme *ime, const char *word) {
    return candidate_index(ime, word) >= 0;
}

static void expect_candidate(PinyinIme *ime, const char *pinyin,
                             const char *word) {
    ime_clear(ime);
    type(ime, pinyin);
    assert(contains(ime, word));
}

int main(void) {
    PinyinIme *ime = ime_create("romfs/pinyin_dict.bin");
    assert(ime != NULL);
    type(ime, "nihao");
    assert(ime_active(ime));
    assert(ime_matched_length(ime) == 5);
    assert(ime_candidate_count(ime) > 0);
    assert(contains(ime, "你好"));
    assert(ime_candidate_count(ime) <= IME_MAX_CANDIDATES);
    ime_clear(ime);
    type(ime, "ni");
    assert(strcmp(ime_candidate(ime, 0), "你") == 0);
    assert(candidate_index(ime, "妮") >= 0);
    assert(candidate_index(ime, "妮") < candidate_index(ime, "年"));
    ime_clear(ime);
    type(ime, "zhongguo");
    assert(contains(ime, "中国"));
    ime_clear(ime);
    type(ime, "wangyiyun");
    assert(contains(ime, "网易云"));
    ime_clear(ime);
    type(ime, "wyy");
    assert(contains(ime, "网易云"));
    ime_backspace(ime);
    assert(ime_active(ime));
    ime_clear(ime);
    type(ime, "shi");
    assert(ime_candidate_count(ime) > 9);
    const char *first = ime_candidate(ime, 0);
    assert(first != NULL);
    assert(strcmp(ime_commit(ime, 0), first) == 0);
    assert(!ime_active(ime));
    expect_candidate(ime, "yinyue", "音乐");
    expect_candidate(ime, "wangyiyun", "网易云");
    expect_candidate(ime, "wangyiyunyinyue", "网易云音乐");
    expect_candidate(ime, "gequ", "歌曲");
    expect_candidate(ime, "geshou", "歌手");
    expect_candidate(ime, "zhuanji", "专辑");
    expect_candidate(ime, "zhoujielun", "周杰伦");
    expect_candidate(ime, "chenyixun", "陈奕迅");
    expect_candidate(ime, "linjunjie", "林俊杰");
    expect_candidate(ime, "dengziqi", "邓紫棋");
    expect_candidate(ime, "wuyuetian", "五月天");
    expect_candidate(ime, "sunyanzi", "孙燕姿");
    expect_candidate(ime, "wangfei", "王菲");
    expect_candidate(ime, "zeng", "曾");
    ime_destroy(ime);
    puts("ime tests: ok");
    return 0;
}
