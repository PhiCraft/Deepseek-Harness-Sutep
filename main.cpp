#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* ---------- ANSI 颜色 ---------- */
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define DIM     "\033[2m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"

/* ================================================================
 * 跨平台辅助函数
 * ================================================================ */

/* 设置控制台窗口标题 */
static void set_title(const char *title)
{
#ifdef _WIN32
    SetConsoleTitleA(title);
#else
    printf("\033]0;%s\007", title);
#endif
}

/* 隐藏光标 */
static void hide_cursor(void)
{
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    GetConsoleCursorInfo(h, &info);
    info.bVisible = FALSE;
    SetConsoleCursorInfo(h, &info);
#else
    printf("\033[?25l");
#endif
}

/* 恢复光标 */
static void show_cursor(void)
{
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info;
    GetConsoleCursorInfo(h, &info);
    info.bVisible = TRUE;
    SetConsoleCursorInfo(h, &info);
#else
    printf("\033[?25h");
#endif
}

/* 等待按键退出 */
static void wait_for_key(void)
{
#ifdef _WIN32
    system("pause");
#else
    printf("按回车键退出...");
    fflush(stdout);
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { /* 忽略 */ }
#endif
}

/* 命令是否在 PATH 中 */
static int command_exists(const char *cmd)
{
    char buf[256];
#ifdef _WIN32
    snprintf(buf, sizeof(buf), "where %s >nul 2>nul", cmd);
#else
    snprintf(buf, sizeof(buf), "command -v %s >/dev/null 2>&1", cmd);
#endif
    return system(buf) == 0;
}

/* 把目录追加到当前进程 PATH */
static void add_to_path(const char *dir)
{
    const char *old = getenv("PATH");
    if (old == NULL) old = "";
    if (strstr(old, dir) != NULL) return;

#ifdef _WIN32
    const char *sep = ";";
#else
    const char *sep = ":";
#endif

    size_t len = strlen(old) + strlen(dir) + strlen(sep) + 1;
    char *new_path = (char *) malloc(len);
    if (new_path == NULL) return;
    snprintf(new_path, len, "%s%s%s", old, sep, dir);

#ifdef _WIN32
    _putenv_s("PATH", new_path);
#else
    setenv("PATH", new_path, 1);
#endif
    free(new_path);
}

/* 补齐 Node.js 常见安装位置到 PATH */
static void ensure_node_in_path(void)
{
#ifdef _WIN32
    const char *dirs[] = {
        "C:\\Program Files\\nodejs",
        "C:\\Program Files (x86)\\nodejs",
    };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        if (GetFileAttributesA(dirs[i]) != INVALID_FILE_ATTRIBUTES)
            add_to_path(dirs[i]);
    }
    const char *local = getenv("LOCALAPPDATA");
    if (local != NULL) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s\\Programs\\nodejs", local);
        if (GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES)
            add_to_path(buf);
    }
    const char *appdata = getenv("APPDATA");
    if (appdata != NULL) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s\\npm", appdata);
        if (GetFileAttributesA(buf) != INVALID_FILE_ATTRIBUTES)
            add_to_path(buf);
    }
#elif __APPLE__
    const char *dirs[] = {
        "/opt/homebrew/bin", "/opt/homebrew/sbin",
        "/usr/local/bin", "/usr/local/sbin", "/usr/bin",
    };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        if (access(dirs[i], F_OK) == 0) add_to_path(dirs[i]);
    }
    const char *home = getenv("HOME");
    if (home != NULL) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s/.nodejs/bin", home);
        if (access(buf, F_OK) == 0) add_to_path(buf);
    }
#else
    const char *dirs[] = { "/usr/local/bin", "/usr/bin", "/opt/homebrew/bin" };
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
        if (access(dirs[i], F_OK) == 0) add_to_path(dirs[i]);
    }
#endif
}

/* ================================================================
 * 界面输出
 * ================================================================ */

static void print_line(void)
{
    printf("%s────────────────────────────────────────────────────%s\n", BLUE, RESET);
}

static void print_step(int step, const char *msg)
{
    printf("\n%s[%d/3]%s %s%s%s\n", CYAN, step, RESET, BOLD, msg, RESET);
}

static void print_cmd(const char *cmd)
{
    printf("%s  ▶ %s%s%s\n", MAGENTA, BOLD, cmd, RESET);
}

static void print_ok(const char *msg)
{
    printf("%s  √ %s%s%s\n", GREEN, BOLD, msg, RESET);
}

static void print_fail(const char *msg)
{
    printf("%s  × %s%s%s\n", RED, BOLD, msg, RESET);
}

static void print_info(const char *msg)
{
    printf("%s  ℹ %s%s\n", CYAN, msg, RESET);
}

static void print_warn(const char *msg)
{
    printf("%s  ⚠ %s%s\n", YELLOW, msg, RESET);
}

static void print_wait(const char *msg)
{
    printf("%s  ⏳ %s，请稍等...%s\n", DIM, msg, RESET);
    fflush(stdout);
}

/* 循环等待动画，duration_ms 毫秒 */
static void progress_bar(int duration_ms)
{
    static const char *frames[] = { "◐", "◓", "◑", "◒" };
    int idx = 0;
    for (int t = 0; t < duration_ms; t += 100) {
        printf("\r%s  %s 请稍等...%s", CYAN, frames[idx++ % 4], RESET);
        fflush(stdout);
#ifdef _WIN32
        Sleep(100);
#else
        usleep(100000);
#endif
    }
    printf("\r%60s\r", "");
}

/* ================================================================
 * 安装 Node.js — Windows
 * ================================================================ */
#ifdef _WIN32

/* 通过临时 .ps1 文件执行 PowerShell，避免命令行转义问题 */
static int run_powershell(const char *script, const char *tag)
{
    char temp_dir[MAX_PATH] = { 0 };
    char script_path[MAX_PATH] = { 0 };
    GetTempPathA(MAX_PATH, temp_dir);
    snprintf(script_path, sizeof(script_path), "%sdsh_%s_%lu.ps1",
             temp_dir, tag, (unsigned long) GetCurrentProcessId());

    FILE *fp = fopen(script_path, "wb");
    if (fp == NULL) {
        print_fail("无法创建临时脚本文件");
        return -1;
    }
    /* UTF-8 BOM，避免中文乱码 */
    fwrite("\xEF\xBB\xBF", 1, 3, fp);
    fwrite(script, 1, strlen(script), fp);
    fclose(fp);

    char cmd[MAX_PATH * 3];
    snprintf(cmd, sizeof(cmd),
             "powershell -NoProfile -ExecutionPolicy Bypass -File \"%s\"",
             script_path);
    int rc = system(cmd);
    remove(script_path);
    return rc;
}

/*
 * 使用 .NET HttpWebRequest 流式下载，每读 64KB 重绘一次进度条。
 * 进度条字符只用 ASCII（# 和 .），避免编码问题。
 */
static int install_node_windows_portable(void)
{
    print_info("使用官方 ZIP 便携版（无需管理员权限）");

    static const char *script =
        "$ErrorActionPreference = 'Stop'\n"
        "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12\n"

        /* 重绘进度条 */
        "function Show-Bar($done, $total) {\n"
        "  if ($total -le 0) { return }\n"
        "  $pct = [int](($done * 100) / $total)\n"
        "  $w   = 40\n"
        "  $f   = [int]($pct * $w / 100)\n"
        "  $e   = $w - $f\n"
        "  $bar = ('#' * $f) + ('.' * $e)\n"
        "  $mb1 = [math]::Round($done / 1MB, 1)\n"
        "  $mb2 = [math]::Round($total / 1MB, 1)\n"
        "  [Console]::Write(\"`r  [$bar] $pct%  ${mb1}MB / ${mb2}MB   \")\n"
        "}\n"

        "try {\n"
        /* --- 第 1 步：查询版本 --- */
        "  Write-Host '[1/3] 查询 Node.js LTS 版本...'\n"
        "  $index = Invoke-RestMethod -Uri 'https://nodejs.org/dist/index.json' "
        "           -UseBasicParsing -TimeoutSec 60\n"
        "  $lts = $index | Where-Object { $_.lts } | Select-Object -First 1\n"
        "  if (-not $lts) { throw '无法确定 LTS 版本' }\n"
        "  $ver = $lts.version\n"
        "  Write-Host \"      LTS 版本：$ver\"\n"

        "  if ([Environment]::Is64BitOperatingSystem) { $arch = 'win-x64' }\n"
        "  else                                        { $arch = 'win-x86' }\n"
        "  $name = \"node-$ver-$arch\"\n"
        "  $url  = \"https://nodejs.org/dist/$ver/$name.zip\"\n"
        "  $root = Join-Path $env:LOCALAPPDATA 'Programs'\n"
        "  New-Item -ItemType Directory -Force -Path $root | Out-Null\n"
        "  $zip  = Join-Path $env:TEMP \"$name.zip\"\n"

        /* --- 第 2 步：带进度条下载 --- */
        "  Write-Host \"[2/3] 下载 $url\"\n"
        "  $req = [System.Net.HttpWebRequest]::Create($url)\n"
        "  $req.Timeout           = 60000\n"
        "  $req.ReadWriteTimeout  = 900000\n"
        "  $resp  = $req.GetResponse()\n"
        "  $total = $resp.ContentLength\n"
        "  $in    = $resp.GetResponseStream()\n"
        "  $out   = [System.IO.File]::Create($zip)\n"
        "  $buf   = New-Object byte[] 65536\n"
        "  $sum   = 0\n"
        "  while (($n = $in.Read($buf, 0, $buf.Length)) -gt 0) {\n"
        "    $out.Write($buf, 0, $n)\n"
        "    $sum += $n\n"
        "    Show-Bar $sum $total\n"
        "  }\n"
        "  $out.Close(); $in.Close(); $resp.Close()\n"
        "  [Console]::WriteLine()\n"

        /* --- 第 3 步：解压 --- */
        "  Write-Host '[3/3] 解压安装包...'\n"
        "  $dest  = Join-Path $root $name\n"
        "  $final = Join-Path $root 'nodejs'\n"
        "  if (Test-Path $dest)  { Remove-Item $dest  -Recurse -Force }\n"
        "  if (Test-Path $final) { Remove-Item $final -Recurse -Force }\n"
        "  Expand-Archive -Path $zip -DestinationPath $root -Force\n"
        "  Move-Item $dest $final\n"
        "  Remove-Item $zip -ErrorAction SilentlyContinue\n"
        "  Write-Host \"      安装路径：$final\"\n"
        "  exit 0\n"
        "} catch {\n"
        "  Write-Host \"  错误：$_\"\n"
        "  exit 1\n"
        "}\n";

    return run_powershell(script, "node");
}

#endif /* _WIN32 */

/* ================================================================
 * 安装 Node.js — macOS
 * ================================================================ */
#ifdef __APPLE__

/* 定位 Homebrew 可执行文件（不依赖 PATH） */
static const char *find_brew(void)
{
    static const char *candidates[] = {
        "/opt/homebrew/bin/brew",   /* Apple Silicon */
        "/usr/local/bin/brew",      /* Intel */
        NULL
    };
    for (int i = 0; candidates[i] != NULL; i++) {
        if (access(candidates[i], X_OK) == 0) return candidates[i];
    }
    return NULL;
}

/*
 * 下载官方 tarball 到 $HOME/.nodejs。
 * curl 使用 --progress-bar 输出原生进度条。
 */
static int install_node_macos_tarball(void)
{
    if (!command_exists("curl")) {
        print_fail("系统缺少 curl，无法下载 Node.js");
        return -1;
    }

    static const char *script =
        "set -e\n"
        "TARGET=\"$HOME/.nodejs\"\n"
        "mkdir -p \"$TARGET\"\n"

        "ARCH=$(uname -m)\n"
        "case \"$ARCH\" in\n"
        "  arm64)  NODE_ARCH=darwin-arm64 ;;\n"
        "  x86_64) NODE_ARCH=darwin-x64 ;;\n"
        "  *) echo \"不支持的架构: $ARCH\" >&2; exit 1 ;;\n"
        "esac\n"

        "echo '[1/3] 查询 Node.js LTS 版本...'\n"
        "URL_BASE=$(curl -fsSL --connect-timeout 30 -o /dev/null -w '%{url_effective}' "
        "https://nodejs.org/dist/latest-lts/)\n"
        "VER=$(echo \"$URL_BASE\" | sed 's|.*/dist/||; s|/$||')\n"
        "if [ -z \"$VER\" ]; then echo '无法获取版本号' >&2; exit 1; fi\n"
        "echo \"      LTS 版本: $VER\"\n"

        "TARBALL=\"node-${VER}-${NODE_ARCH}.tar.gz\"\n"
        "URL=\"https://nodejs.org/dist/${VER}/${TARBALL}\"\n"
        "TMP=$(mktemp -d)\n"
        "trap 'rm -rf \"$TMP\"' EXIT\n"

        /* --progress-bar 显示带百分比的单行进度条 */
        "echo \"[2/3] 下载 $URL\"\n"
        "curl -fL --progress-bar --connect-timeout 30 "
        "\"$URL\" -o \"$TMP/$TARBALL\"\n"

        "echo '[3/3] 解压安装包...'\n"
        "tar -xzf \"$TMP/$TARBALL\" -C \"$TARGET\" --strip-components=1\n"
        "echo \"      安装路径: $TARGET\"\n";

    const char *path = "/tmp/dsh_install_node.sh";
    FILE *fp = fopen(path, "w");
    if (fp == NULL) return -1;
    fputs(script, fp);
    fclose(fp);

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "sh %s", path);
    int rc = system(cmd);
    remove(path);
    return rc;
}

#endif /* __APPLE__ */

/* ================================================================
 * 安装 Node.js — 统一入口
 * ================================================================ */
static int install_node(void)
{
    print_wait("正在安装 Node.js");

#ifdef _WIN32
    /* 方案 A：winget */
    if (command_exists("winget")) {
        print_info("检测到 winget，优先使用");
        int rc = system("winget install OpenJS.NodeJS.LTS -e "
                        "--accept-source-agreements --accept-package-agreements "
                        "--disable-interactivity");
        if (rc == 0) {
            print_ok("Node.js 安装完成（winget）");
            return 0;
        }
        print_warn("winget 安装失败，尝试便携版...");
    } else {
        print_info("未检测到 winget，使用便携版安装");
    }
    /* 方案 B：官方 ZIP 便携版 */
    if (install_node_windows_portable() == 0) {
        print_ok("Node.js 安装完成（便携版）");
        return 0;
    }

#elif __APPLE__
    /* 方案 A：Homebrew */
    const char *brew = find_brew();
    if (brew != NULL) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "\"%s\" install node", brew);
        print_info("使用 Homebrew 安装");
        print_cmd(cmd);
        if (system(cmd) == 0) {
            print_ok("Node.js 安装完成（Homebrew）");
            return 0;
        }
        print_warn("Homebrew 安装失败，尝试官方二进制包...");
    } else {
        print_info("未检测到 Homebrew，使用官方二进制包");
    }
    /* 方案 B：nodejs.org 官方 tarball */
    if (install_node_macos_tarball() == 0) {
        print_ok("Node.js 安装完成");
        return 0;
    }

#else
    /* Linux：按可用包管理器 */
    int rc = -1;
    if (command_exists("apt-get")) {
        rc = system("sudo apt-get update && sudo apt-get install -y nodejs npm");
    } else if (command_exists("dnf")) {
        rc = system("sudo dnf install -y nodejs npm");
    } else if (command_exists("pacman")) {
        rc = system("sudo pacman -S --noconfirm nodejs npm");
    } else if (command_exists("brew")) {
        rc = system("brew install node");
    }
    if (rc == 0) {
        print_ok("Node.js 安装完成");
        return 0;
    }
#endif

    print_fail("自动安装失败，请手动安装 Node.js（https://nodejs.org/）");
    return -1;
}

/* ================================================================
 * 主程序
 * ================================================================ */
int main(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    set_title("DeepSeek Harness Launcher");
    hide_cursor();

    /* 大标题 */
    printf("%s", CYAN);
    printf("  ██████╗ ███████╗███████╗██████╗ ███████╗███████╗ ██████╗ ██╗  ██╗\n");
    printf("  ██╔══██╗██╔════╝██╔════╝██╔══██╗██╔════╝██╔════╝██╔═══██╗██║ ██╔╝\n");
    printf("  ██║  ██║█████╗  █████╗  ██████╔╝█████╗  ███████╗██║   ██║█████╔╝ \n");
    printf("  ██║  ██║██╔══╝  ██╔══╝  ██╔═══╝ ██╔══╝  ╚════██║██║   ██║██╔═██╗ \n");
    printf("  ██████╔╝███████╗███████╗██║     ███████╗███████║╚██████╔╝██║  ██╗\n");
    printf("  ╚═════╝ ╚══════╝╚══════╝╚═╝     ╚══════╝╚══════╝ ╚═════╝ ╚═╝  ╚═╝\n");
    printf("%s", RESET);
    printf("%s  DeepSeek Harness Launcher%s  版本 1.4.0\n", BOLD, RESET);
    print_line();
    print_warn("此窗口显示运行过程，请勿关闭！");
    printf("\n");

    /* 先补齐 PATH，避免"装了却找不到" */
    ensure_node_in_path();

    /* ---- 步骤 1：检测 Node.js ---- */
    print_step(1, "检测 Node.js 环境");
    if (command_exists("node")) {
        print_ok("Node.js 已安装");
    } else {
        print_fail("未检测到 Node.js，准备自动安装");
        if (install_node() != 0) {
            printf("\n%s请手动安装 Node.js 后重新运行。%s\n", RED, RESET);
            show_cursor();
            wait_for_key();
            return 1;
        }
        ensure_node_in_path();
        if (!command_exists("node")) {
            print_fail("安装完成但仍找不到 node，请重启终端后重试");
            show_cursor();
            wait_for_key();
            return 1;
        }
        print_ok("Node.js 已就绪");
    }

    /* ---- 步骤 2：检查版本 ---- */
    print_step(2, "检查 Node.js 版本");
    print_cmd("node -v");
    system("node -v");
    printf("\n");

    /* ---- 步骤 3：启动 DeepSeek Harness ---- */
    print_step(3, "启动 DeepSeek Harness");
    if (!command_exists("npx")) {
        print_fail("未检测到 npx，Node.js 安装可能不完整");
        show_cursor();
        wait_for_key();
        return 1;
    }

    /* --yes 跳过 "Ok to proceed?" 交互；
     * --progress 强制显示 npm 下载进度条（默认已开启） */
    const char *launch_cmd =
        "npx --yes --progress @deepseek-ai/dsh web";

    print_cmd(launch_cmd);
    print_line();
    print_warn("首次运行需下载依赖，请耐心等待（约 1-3 分钟）");
    print_info("npm 会显示下载进度条，看到 100% 后即将启动");
    print_info("浏览器若未自动打开，请访问 http://127.0.0.1:3080");
    print_info("要停止服务，请直接关闭此窗口或按 Ctrl+C");
    printf("\n");

    /* 启动前短暂等待动画，让用户知道程序在工作 */
    progress_bar(1200);
    print_wait("正在启动 DeepSeek Harness");
    printf("\n\n");

    /* 执行 npx，其自身输出（含进度条）实时透传到当前终端 */
    int rc = system(launch_cmd);
    if (rc != 0) {
        printf("\n%s  [异常]%s DeepSeek 进程退出（代码 %d）\n", RED, RESET, rc);
    } else {
        printf("\n%s  [正常]%s DeepSeek 已停止\n", GREEN, RESET);
    }

    printf("\n");
    show_cursor();
    wait_for_key();
    return 0;
}