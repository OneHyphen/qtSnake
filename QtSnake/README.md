# QtSnake — Qt 6 贪吃蛇

一个使用 **Qt 6 + C++17 + Qt Widgets + qmake** 开发的桌面版贪吃蛇游戏。
界面通过 `QPainter` 纯代码绘制，不使用 Qt Designer、QML 或第三方库。

## 开发环境

- Qt 6.x（含 Qt Widgets、Qt Multimedia 模块）
- C++17
- qmake（不使用 CMake）
- 编译器：MinGW / MSVC 均可（随 Qt Kit 提供）

## 编译

### 方式一：Qt Creator

1. 打开 Qt Creator。
2. `文件 → 打开文件或项目…`，选择 `QtSnake.pro`。
3. 选择已配置好的 Qt 6 Kit。
4. 点击构建（`Ctrl+B`），再点击运行（`Ctrl+R`）。

### 方式二：命令行 qmake

```bash
cd QtSnake
qmake QtSnake.pro
make          # Linux/macOS
mingw32-make  # Windows (MinGW)
# 或使用 nmake / jom（取决于所用 Kit）
```

Windows 示例（假设 Qt 安装在 `C:\Qt\6.x.x\mingw_64`）：

```bash
set PATH=C:\Qt\6.x.x\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;%PATH%
cd QtSnake
qmake QtSnake.pro
mingw32-make
```

生成的可执行文件为 `QtSnake`（Windows 下为 `QtSnake.exe`）。

## 操作

| 按键 | 功能 |
|------|------|
| ↑ / W | 向上移动 |
| ↓ / S | 向下移动 |
| ← / A | 向左移动 |
| → / D | 向右移动 |
| Shift（按住） | 加速冲刺 |
| Space | 开始 / 暂停 / 继续 |
| R | 重新开始 |

界面按钮：开始、暂停（继续）、重新开始、难度选择、声音开关。

## 游戏规则

- 使用方向键或 WASD 控制蛇在 **30 × 20** 的网格中移动。
- 吃到食物：**+10 分**、蛇身增长、食物重新生成、速度加快。
- 撞墙或撞到自己身体：**游戏结束**。
- 最高分通过 `QSettings` 保存到本地，重新打开程序后仍然保留。

## 难度

| 难度 | 初始速度 | 说明 |
|------|---------|------|
| 简单 | 340 ms/步 | 较慢，适合新手 |
| 普通 | 260 ms/步 | 适中 |
| 困难 | 180 ms/步 | 较快，富有挑战 |

吃到食物后每步间隔递减（每次 -5 ms），最低不低于 50 ms。
按住 **Shift** 可将当前间隔缩小到 1/3（同样不低于 50 ms）实现加速冲刺。

## 项目结构

```
QtSnake/
├── QtSnake.pro        # qmake 工程文件（widgets + multimedia + c++17 + 图标）
├── main.cpp           # 程序入口：先显示 SplashScreen，结束后创建 MainWindow
├── mainwindow.h/.cpp  # 主窗口：顶部信息栏、按钮、难度、背景图片、窗口图标
├── gamewidget.h/.cpp  # 游戏核心：绘制、QTimer 主循环、状态机、碰撞、计分、键盘
├── snake.h/.cpp       # 蛇：deque 身体数据、方向、移动、增长、自撞检测
├── food.h/.cpp        # 食物：多食物位置管理与随机生成
├── gameconfig.h       # 难度枚举 Difficulty 与三档参数 DifficultyConfig
├── soundmanager.h/.cpp# 音频：背景音乐与吃食物/结束/点击音效
├── splashscreen.h/.cpp# 启动画面：logo 展示、品牌语音、淡入淡出
├── resources.qrc      # 资源清单（嵌入音乐、音效、背景图、图标）
├── res/
│   ├── game_bgm.mp3 / menu_song.mp3  # 背景音乐（游戏 / 菜单）
│   ├── eat.wav / death.wav / click.wav         # 音效
│   ├── yuzusoft_voice.wav / yuzusoft_logo.png  # 启动语音与 logo
│   ├── background_menu.jpg        # 窗口背景图片
│   └── snake.ico                  # 窗口/可执行文件图标
└── README.md          # 本说明文件
```

> `debug/`、`release/`、`Makefile*`、`.qmake.stash`、`moc_*`、`*.o`、`qrc_resources.cpp` 等
> 均为构建产物，由 `.gitignore` 排除，克隆后重新执行 `qmake` + `make` 即可生成。

### 类职责

- `Snake`：管理蛇身 `deque<QPoint>`（头在前）、移动方向、移动与增长、自身碰撞检测，不依赖任何界面框架。
- `Food`：在网格内随机生成多种食物，避开蛇身与障碍，采用 `QSet` 汇总占用格以将冲突判断降为 `O(1)`。
- `GameWidget`：继承 `QWidget`，使用 `QTimer` 驱动渲染、`QElapsedTimer` 驱动逻辑移动，负责碰撞检测、计分、难度调速、键盘输入与全部绘制。
- `SoundManager`：继承 `QObject`，使用 `QMediaPlayer` 循环播放背景音乐，使用 `QSoundEffect` 播放吃食物、游戏结束与按钮点击音效，并统一控制静音。
- `SplashScreen`：无边框置顶启动画面，语音结束、最短停留与超时三条件与逻辑保证不会卡住。
- `MainWindow`：继承 `QMainWindow`，用 `QStackedWidget` 组织开始菜单页与游戏页，绘制窗口背景图片、设置窗口图标，管理分数显示、难度选择、按钮与声音开关。
