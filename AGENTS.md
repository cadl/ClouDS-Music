# ClouDS Music agent guide

本文件适用于整个仓库。它记录后续开发者和编码代理需要优先遵守的工程约束；
功能说明和用户操作仍以 `README.md` 为准。

## 协作方式与资料来源

- 不要对看似次优、含糊或有风险的要求直接照做。先检查边界条件、性能、可维护性
  和 3DS 硬件差异，再用具体理由提出更稳妥的方案。
- 需求存在会实质改变结果的歧义时应先澄清；不影响方向的小缺口可以作保守假设并
  明确说明。
- 主动指出高杠杆改进和回归风险，但不要借机扩大用户授权的修改范围。
- 涉及代码生成、安装/配置步骤，或库、API、CLI 文档时，必须先使用 Context7
  获取当前资料。Context7 没有对应项目时，再使用上游官方文档和源码，并说明回退
  的依据。

## 项目目标与边界

- 这是面向 Nintendo 3DS Homebrew Launcher 的原生 C 应用，主要产物是
  `ClouDS-Music.3dsx`，真机安装路径固定为
  `/3ds/ClouDS-Music/ClouDS-Music.3dsx`。
- 保持 Old 3DS/2DS 可运行，不得只按 New 3DS 或桌面模拟器的资源余量设计。
- 网易云接口并非公开稳定 API。解析响应时应防御字段缺失和接口变化，并把可操作的
  错误显示给用户；不要绕过 VIP、地区限制或下架状态。
- 保持像素化双屏 UI 和现有焦点模型：上屏是内容，下屏左侧是播放控制、右侧是
  Library；`SELECT` 显式切换焦点，`B` 返回上屏，`L/R` 切换 Tab 并重置焦点。

## 开始工作前

1. 保留用户尚未提交的修改，不要用破坏性 Git 命令清理工作区。
2. 初始化固定版本的依赖：

   ```sh
   git submodule update --init --recursive
   ```

3. 优先阅读与修改范围直接相关的文件。主要模块见 `README.md` 的“代码结构”。
4. 不要机械格式化 `external/` 和 `third_party/`；其中包含上游源码、原始许可证及其
   换行格式。

## 构建与最低验证

- 无 devkitPro 也可以运行主机测试：

  ```sh
  make host-test
  ```

- 本机已配置 devkitARM 时使用 `make -j2`。否则使用仓库封装的 Docker 构建：

  ```sh
  make emulator-build
  ```

- macOS 模拟器流程：

  ```sh
  make azahar-install
  make run
  ```

- 提交功能代码前至少执行 `make host-test` 和 `make emulator-build`。网络、NDSP、
  SD 卡、睡眠恢复或输入相关修改还必须按 `docs/HARDWARE_TEST.md` 做真机验证。
- GitHub Actions 会在普通 push/PR 运行主机测试、3DSX 构建和凭据扫描；Release
  工作流另行构建 CIA、完整源码包、许可包和摘要并附加到 Release。CI 不能替代本地
  验证和真机检查。

## Azahar 调试

### 固定环境和数据目录

- `tools/install-azahar.sh` 固定 Azahar 版本和 SHA-256，安装在仓库私有的
  `.tools/Azahar.app`；不要改用来源不明的模拟器构建。
- macOS 用户数据默认位于：

  ```text
  ~/Library/Application Support/Azahar
  ```

- 模拟 SD 卡位于其 `sdmc/` 子目录。应用诊断日志对应：

  ```text
  ~/Library/Application Support/Azahar/sdmc/3ds/ClouDS-Music/hardware.log
  ```

- `make run` 使用 `open -na`，可能留下多个 Azahar 实例。遇到日志重复、输入发往
  错误窗口或配置未刷新时，先确认只运行一个实例。

### DSP/NDSP

- libctru 的 `ndspInit()` 会先读取模拟 SD 卡上的 `/3ds/dspfirm.cdc`，找不到时再
  尝试 Homebrew Launcher 提供的 `hb:ndsp`。
- 正常真机必须使用机主从自己 3DS 提取的真实 `dspfirm.cdc`。不要下载、提交、
  分发或要求用户共享任天堂固件。
- Azahar 的 **HLE (fast)** 音频实现不执行该 DSP 程序，但 libctru 仍要求组件文件
  存在。因此在没有真机可提取固件时，可以仅为模拟器创建非空占位文件：

  ```sh
  mkdir -p "$HOME/Library/Application Support/Azahar/sdmc/3ds"
  printf x > "$HOME/Library/Application Support/Azahar/sdmc/3ds/dspfirm.cdc"
  ```

- 这个占位文件不是固件，只能配合 Azahar HLE 使用。不得复制到真机、加入 RomFS、
  放入发行包或提交到 Git。真机可用后应在同一路径换成自己提取的真实文件。
- 需要采集诊断时，先在 Settings 开启默认关闭的调试日志并重启应用，再用
  `hardware.log` 验证最新一行是 `dsp=ready`；同时在 Azahar 日志确认
  `Audio_Emulation: HLE` 和 `Cubeb Audio Stream Started`。窗口出现不等于音频链路
  已通过。
- NDSP PCM 缓冲必须使用 `linearAlloc()`，提交给硬件前调用
  `DSP_FlushDataCache()`，退出时按相反顺序清理 wave buffer、linear memory 和
  `ndspExit()`。

### 模拟器能验证与不能验证的内容

- 适合快速验证：UI、焦点、按键映射、JSON、HTTPS、二维码登录、推荐/搜索、歌词、
  封面、缓存和基础 NDSP 调用。
- 不能代替真机验证：Old 3DS 内存压力、DSP 时序、物理音量滑块、耳机切换、合盖
  睡眠、真实 Wi-Fi、SD 卡延迟和长时间稳定性。
- macOS 自动化产生的极短键盘/触摸事件可能被 3DS 的逐帧输入轮询漏掉。操作前先
  聚焦渲染画面，结果以 UI 状态和日志为准；真实扬声器输出仍应人工确认。
- Azahar 默认键位包括：键盘 `A` = 3DS `A`、`S` = `B`、`Z` = `X`、
  `X` = `Y`、`Q/W` = `L/R`、`M` = `START`、`N` = `SELECT`。

### 断点和崩溃

- 模拟器 GDB：

  ```sh
  make debug GDB_PORT=24689
  /opt/devkitpro/devkitARM/bin/arm-none-eabi-gdb ClouDS-Music.elf
  ```

  然后执行 `target remote localhost:24689`。
- Luma3DS 的 ARM11 dump 通常在 SD 卡 `/luma/dumps/arm11/`。3DS RTC 不可靠，
  文件时间可能不代表实际崩溃时间，应结合文件序号和复现时间判断最新 dump。
- 符号化必须使用与崩溃构建完全匹配的 `.elf`；不要只保存 `.3dsx`。可先用
  `arm-none-eabi-addr2line -f -C -e ClouDS-Music.elf <address>` 定位，3DSX
  重定位导致地址不匹配时再结合加载基址分析。

## 核心实现约束

### 主线程、Worker 和取消

- 主线程负责 `aptMainLoop()`、输入、轻量状态切换和渲染；网络、下载、图片处理和
  MP3 预解析不得阻塞主线程。
- 后台任务通过 `NetworkWorker` 传递，跨线程状态必须在现有 `LightLock`/
  `LightEvent` 边界内访问。不要把 libcurl 句柄或 UI/GPU 对象跨线程共享。
- `WorkerResult` 很大，主线程当前使用静态存储接收结果；不要把它或大字典/图像数组
  放到 3DS 小栈上。Worker 栈目前只有 96 KiB。
- `B` 取消必须及时传播到请求和下载，并清理 `.part` 文件；取消不是普通失败，不应
  覆盖为误导性的网络错误。

### 音频和内存

- 当前策略是渐进式落盘播放，不是把 HTTP 数据直接送入解码器：媒体线程顺序写
  `.part`，只发布已经刷盘的数据；播放器通常预缓冲文件的 25% 后开始，门槛限制为
  256 KiB–1 MiB，总大小未知时使用 256 KiB。欠载时进入 `BUFFERING`，下载完成后
  原子提交缓存并切换到完整 seek 索引。不要绕过这个可见长度、取消和提交边界，也
  不要改成整首内存缓冲。
- 渐进文件的 128 KiB 预取由独立低优先级 I/O 线程执行；主线程只能读取已准备好的
  内存窗口，不能退回到在渲染/输入循环中同步读取增长中的 `.part`。
- minimp3 使用文件回调和六组 NDSP 缓冲，不能把整首 MP3 或解码后的 PCM 读入
  内存。增加缓冲前先测 Old 3DS 的 `app_free` 和 `linear_free`。
- `romfs/ui-menu-font.bcfnt`、`romfs/content-point-font.bin`、按需加载的
  `romfs/immersive-font.bin` 和 `romfs/pinyin_dict.bin` 已占用较大空间。避免再常驻
  字体副本、字典、封面像素或网络响应；大对象优先复用或按需加载。
- 资源分配失败必须可恢复并给出明确状态，不能继续解引用空指针或把半初始化对象
  交给播放线程。

### 网络和凭据

- 保持 TLS 1.2、CA 校验、主机名校验和 HTTPS-only 重定向。不要为了“让请求成功”
  关闭证书验证或允许媒体 URL 降级到 HTTP。
- CA、像素字体和拼音词典由 `tools/fetch-*.sh`/`tools/gen_pinyin_dict.py` 固定来源
  与摘要生成。更新资产时同时更新来源、校验值、许可证说明并重新构建。
- `sdmc:/3ds/ClouDS-Music/auth.bin` 包含可复用的 `MUSIC_U`，属于敏感凭据。不得输出到
  console、Azahar 日志、`hardware.log`、测试 fixture、issue 或 commit。
- 缓存和凭据只写入 `sdmc:/3ds/ClouDS-Music`。正式发布后如需更改该路径，
  必须同时提供明确的升级和数据迁移方案。

## UI 修改检查

- 上屏逻辑分辨率为 400×240，下屏为 320×240。所有新增中文、长歌名和歌单名都要
  用真实边界测试截断/滚动，不能依赖桌面窗口放大后的视觉效果。
- 普通文字必须使用 `source/ui.c` 集中定义的 `UI_TEXT_*` 语义字号，不要在调用点传裸
  缩放倍率。固定菜单、按钮、对话框和控制提示使用 `menu_text_*`/`label_*` 路径及
  精简 A4 BCFNT；歌名、歌手、歌词、昵称、搜索输入等动态内容使用点阵路径。普通点阵
  保持 18px 语义行高，字面固定按原生 12px 栅格化以匹配旧内容字体的视觉大小；
  21/24px 动态标题使用原生 15px 大字面档。文字超宽时应截断，不得运行时继续缩小。
  纯 ASCII 像素装饰文字可以继续使用整数倍 5×7 点阵。
- 触摸控件、焦点框和实体按键必须表达同一状态；不要让触摸操作暗中改变 Library/
  上屏焦点。
- 新页面或模式必须更新下屏左上方的上下文按键说明，让用户无需 README 也能知道
  当前 `A/B/X/Y/SELECT/L/R` 的作用。
- 保持常用 3DS 语义：`A` 确认/播放，`B` 返回/取消，`L/R` 切页，`START` 退出需要
  二次确认。改变这些映射前先说明理由并更新 README、UI 提示和测试。

## 完成工作时

- 报告实际执行的测试，并区分模拟器验证与真机验证，不要把“启动成功”描述成
  “已经实际出声”或“所有机型兼容”。
- 检查 `git status`，确保没有提交 `.tools/`、`build/`、`.3dsx`、`.elf`、缓存、
  crash dump、`auth.bin` 或 `dspfirm.cdc`。
- 涉及第三方源码/资产时同步维护 `THIRD_PARTY_NOTICES.md`，并保留上游许可证。
