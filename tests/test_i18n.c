#include "i18n.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char status[96];
    i18n_set_language(APP_LANGUAGE_CHINESE);
    assert(strcmp(i18n_text("设置"), "设置") == 0);

    i18n_set_language(APP_LANGUAGE_ENGLISH);
    assert(strcmp(i18n_text("设置"), "Settings") == 0);
    assert(strcmp(i18n_text("搜索中…"), "Searching...") == 0);
    assert(i18n_snprintf(status, sizeof(status),
                         "搜索中… 第 %u 页", 2U) > 0);
    assert(strcmp(status, "Searching... page 2") == 0);
    assert(strcmp(i18n_text("untranslated song title"),
                  "untranslated song title") == 0);

    assert(i18n_snprintf(status, sizeof(status),
                         "缓存上限已设为 %llu MB", 128ULL) > 0);
    assert(strcmp(status, "Cache limit set to 128 MB") == 0);
    assert(strcmp(i18n_text("不限"), "Off") == 0);
    assert(strcmp(i18n_text("无上限"), "No cap") == 0);
    assert(strcmp(i18n_text("缓存上限已设为无上限"),
                  "Cache limit set to unlimited") == 0);
    assert(i18n_snprintf(status, sizeof(status),
                         "SD 卡空间不足（需保留 %llu MiB）", 64ULL) > 0);
    assert(strcmp(status,
                  "Not enough SD card space (64 MiB must remain free)") == 0);
    assert(strcmp(i18n_text("调试日志"), "Debug logging") == 0);
    assert(strcmp(i18n_text("版本"), "Version") == 0);
    assert(strcmp(i18n_text("GitHub 仓库"), "GitHub repository") == 0);
    assert(strcmp(i18n_text("联系作者反馈"),
                  "Contact & feedback") == 0);
    assert(strcmp(i18n_text("邮箱：cadl@duck.com"),
                  "Email: cadl@duck.com") == 0);
    assert(strcmp(i18n_text("小红书号：cadl11"),
                  "Xiaohongshu: cadl11") == 0);
    assert(strcmp(i18n_text("开源软件，免费发布"),
                  "Open source - released free") == 0);
    assert(strcmp(i18n_text("浏览"), "Browse") == 0);
    assert(strcmp(i18n_text("无操作"), "No action") == 0);
    assert(strcmp(i18n_text("打开网易云音乐 APP"),
                  "Open NetEase Music") == 0);
    assert(strcmp(i18n_text("扫码登录"), "Scan to log in") == 0);
    assert(i18n_snprintf(status, sizeof(status),
                         "正在准备播放 %u%%", 42U) > 0);
    assert(strcmp(status, "Preparing playback 42%") == 0);
    assert(i18n_snprintf(status, sizeof(status),
                         "正在加载下一首 %u%%", 42U) > 0);
    assert(strcmp(status, "Loading next track 42%") == 0);
    assert(strcmp(i18n_text("正在缓冲"), "Buffering") == 0);
    assert(strcmp(i18n_text("准备播放"), "Preparing playback") == 0);
    assert(strcmp(i18n_text("沉浸"), "Immerse") == 0);
    assert(strcmp(i18n_text("沉浸歌词"), "Immersive lyrics") == 0);
    assert(strcmp(i18n_text("Y 切换"), "Y Style") == 0);
    assert(strcmp(i18n_text("歌词滚轮"), "Lyric wheel") == 0);
    assert(strcmp(i18n_text("中心翻转"), "Center flip") == 0);
    assert(strcmp(i18n_text("骤现渐隐"), "Flash fade") == 0);
    assert(strcmp(i18n_text("星际字幕"), "Opening crawl") == 0);
    assert(strcmp(i18n_text("专辑"), "Album") == 0);
    assert(strcmp(i18n_text("查看专辑"), "View album") == 0);
    assert(strcmp(i18n_text("专辑控制"), "Album controls") == 0);
    assert(strcmp(i18n_text("艺人"), "Artist") == 0);
    assert(strcmp(i18n_text("艺人控制"), "Artist controls") == 0);
    assert(strcmp(i18n_text("Y 专辑"), "Y Albums") == 0);
    assert(strcmp(i18n_text("Y 歌曲"), "Y Songs") == 0);
    assert(strcmp(i18n_text("没有可查看的当前歌曲"),
                  "No current song to view") == 0);
    assert(i18n_snprintf(status, sizeof(status),
                         "正在加载艺人专辑 · 第 %u 页", 2U) > 0);
    assert(strcmp(status, "Loading artist albums · Page 2") == 0);
    assert(i18n_snprintf(status, sizeof(status),
                         "正在加载艺人歌曲 · 第 %u 页", 3U) > 0);
    assert(strcmp(status, "Loading artist songs · Page 3") == 0);
    assert(strcmp(i18n_text("滚动"), "Scroll") == 0);
    assert(i18n_snprintf(status, sizeof(status),
                         "正在加载专辑歌曲 · 第 %u 页", 2U) > 0);
    assert(strcmp(status, "Loading album tracks · Page 2") == 0);
    assert(i18n_snprintf(status, sizeof(status),
                         "正在读取专辑歌曲 · 第 %u 页", 2U) > 0);
    assert(strcmp(status, "Reading album tracks · Page 2") == 0);
    assert(strcmp(i18n_text("已进入沉浸歌词"),
                  "Immersive lyrics opened") == 0);
    assert(strcmp(i18n_text("已退出沉浸歌词"),
                  "Immersive lyrics closed") == 0);
    assert(i18n_snprintf(status, sizeof(status),
                         "歌曲加载中 · 第 %u 页", 3U) > 0);
    assert(strcmp(status, "Loading tracks · Page 3") == 0);
    assert(strcmp(i18n_text("正在准备播放"),
                  "Preparing playback") == 0);
    assert(strcmp(i18n_text("正在播放 · 跳转不可用"),
                  "Playing · Seeking disabled") == 0);
    assert(strcmp(i18n_text("当前任务尚未完成"),
                  "Current task is still running") == 0);
    assert(strcmp(i18n_text("离线模式 · 仅播放缓存"),
                  "Offline · Cached songs only") == 0);
    assert(strcmp(i18n_text("证书校验失败"),
                  "Certificate verification failed") == 0);
    assert(strcmp(i18n_text("检查 3DS 系统日期与时间"),
                  "Check the 3DS date and time") == 0);
    assert(strcmp(i18n_text("检查后按 A 重试"),
                  "A Retry after checking") == 0);
    assert(strcmp(i18n_text("B 取消"), "B Cancel") == 0);
    assert(strcmp(i18n_text("韩文字体不可用，部分文字可能缺字"),
                  "Korean font unavailable; some text may be missing glyphs") == 0);
    assert(strcmp(i18n_text("沉浸点阵字体不可用"),
                  "Immersive bitmap font unavailable") == 0);
    assert(strcmp(i18n_text("将按页添加当前来源的全部推荐歌曲"),
                  "All recommendations from this source will be added "
                  "page by page") == 0);
    assert(strcmp(i18n_text("确认将推荐全部加入播放列表"),
                  "Confirm adding all recommendations to the queue") == 0);
    assert(strcmp(i18n_text("将按页添加当前艺人的全部热门歌曲"),
                  "All popular artist tracks will be added page by page") ==
           0);
    assert(strcmp(i18n_text("确认将艺人歌曲全部加入播放列表"),
                  "Confirm adding all artist tracks to the queue") == 0);
    assert(i18n_snprintf(status, sizeof(status),
                         "正在全部加入艺人歌曲 · 第 %u 页", 4U) > 0);
    assert(strcmp(status, "Adding artist tracks · Page 4") == 0);
    assert(i18n_snprintf(status, sizeof(status),
                         "歌曲数：%u 首", 30U) > 0);
    assert(strcmp(status, "Tracks: 30") == 0);
    assert(i18n_snprintf(status, sizeof(status),
                         "歌曲数：至少 %u 首", 19U) > 0);
    assert(strcmp(status, "Tracks: at least 19") == 0);
    assert(strcmp(i18n_text("需要 DSP 固件"),
                  "DSP firmware required") == 0);
    assert(strcmp(i18n_text("未找到 DSP 固件，请打开 Rosalina"),
                  "DSP firmware missing. Open Rosalina:") == 0);
    assert(strcmp(i18n_text("按 HOME 返回主菜单"),
                  "HOME: return to HOME Menu") == 0);
    assert(strcmp(i18n_text("默认组合键：L + ↓ + SELECT"),
                  "Default: L + ↓ + SELECT") == 0);
    assert(strcmp(i18n_text("进入 Miscellaneous options..."),
                  "Open Miscellaneous options...") == 0);
    assert(strcmp(i18n_text("选择 Dump DSP firmware，按 A"),
                  "Dump DSP firmware > A") == 0);
    assert(strcmp(i18n_text("完全退出并重启应用"),
                  "Fully restart ClouDS Music") == 0);
    assert(strcmp(i18n_text("需要 Luma3DS v10.3 或更高版本"),
                  "Requires Luma3DS v10.3+") == 0);
    assert(strcmp(i18n_text("A / B 关闭"), "A / B Close") == 0);

    i18n_set_language((AppLanguage)99);
    assert(i18n_get_language() == APP_LANGUAGE_CHINESE);
    puts("i18n tests: ok");
    return 0;
}
