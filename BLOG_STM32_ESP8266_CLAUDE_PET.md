# 用 STM32H743、ESP8266 和 Claude Code 做一个桌面 AI 状态宠物

最近做了一个挺有意思的小项目：把一块 STM32H743 开发板、一块 240×240 的 IPS 小屏、一个 ESP8266 模块和 Claude Code 连接起来，做成一个可以显示 Claude 当前状态的桌面电子宠物。

它现在可以根据 Claude Code 的状态自动变化：

- 等待时显示 `Idle`
- 思考时显示 `Thinking`
- 回复时显示 `Answering`
- 忙碌或通知时显示 `Busy`

屏幕上的小宠物也会根据状态切换不同动作，比如待机、等待、挥手、跑动等。

## 最终效果

整个系统的数据流大概是这样：

```text
Claude Code hooks
        ↓ HTTP 请求
ESP8266 / ESP-12F
        ↓ UART 串口
STM32H743
        ↓ SPI + SD 卡
240×240 IPS 屏幕显示宠物和状态
```

现在我在 Claude Code 里发消息时，屏幕会自动切到 `Thinking`；Claude 回复结束后，又会回到 `Idle`。它不再只是一个普通小屏幕，而是一个能感知 AI 工作状态的小桌面伙伴。

## 硬件组成

这次用到的硬件有：

- STM32H743VGT6 开发板
- 240×240 IPS SPI 屏幕
- SD 卡模块 / 板载 SDIO
- ESP8266MOD / ESP-12F
- ST-Link V2
- 杜邦线若干

屏幕通过 SPI 显示图像，宠物动作帧存放在 SD 卡里。ESP8266 负责联网，STM32 负责显示。

## 整体技术方案

这个项目没有让 STM32 直接联网，而是把网络通信和屏幕显示拆成两个部分：

```text
电脑 / Claude Code
        |
        |  HTTP GET
        v
ESP8266 / ESP-12F
        |
        |  UART: STATE:THINKING\r\n
        v
STM32H743
        |
        |  FAT32 读 SD 卡 + LCD 显示
        v
240×240 IPS 屏幕
```

这样分工比较清楚：

- ESP8266 只负责 WiFi 和 HTTP；
- STM32 只负责串口接收、SD 卡读帧、LCD 显示；
- Claude Code `只需要通过` hook 请求一个 URL，不需要知道底层串口和屏幕细节。

我选择 HTTP 而不是 MQTT/WebSocket，是因为调试最简单。浏览器直接访问一个地址，就能验证 ESP8266 是否工作；Claude Code hook 里也只需要执行一次 `Invoke-WebRequest`。

状态协议也故意设计得很简单：

```text
STATE:IDLE\r\n
STATE:THINKING\r\n
STATE:ANSWERING\r\n
STATE:BUSY\r\n
```

STM32 端只要判断接收到的字符串中是否包含 `IDLE`、`THINK`、`ANSWER`、`BUSY`，就可以更新状态。这样即使前面带了 `STATE:`，或者后面有换行，也不影响解析。

## STM32 端技术实现

STM32 端主要做四件事：

1. 初始化屏幕、按键、SD 卡、USART1；
2. 从 SD 卡读取 RGB565 `.BIN` 文件并显示到屏幕；
3. 通过 USART1 接收 ESP8266 发来的状态字符串；
4. 根据状态切换右侧文字和宠物动作。

### 1. 串口初始化

ESP8266 和 STM32 使用 `USART1` 通信，波特率设置为 9600，因为我手上的 NodeMCU Lua 固件串口交互就是 9600。

STM32 初始化时调用：

```c
uart_init(9600);
```

对应引脚是：

```text
PA9  -> USART1_TX
PA10 -> USART1_RX
```

### 2. 接收状态字符串

原工程的 USART 接收逻辑会把一帧数据放到 `USART_RX_BUF` 中，并用 `USART_RX_STA` 标记接收状态。主循环里轮询检查：

```c
static void check_uart_state(void)
{
    u16 len = USART_RX_STA & 0x3FFF;

    if (len > 0 || (USART_RX_STA & 0x8000))
    {
        USART_RX_BUF[len] = 0;

        if (strstr((char *)USART_RX_BUF, "THINK") != 0)
        {
            set_pet_state(PET_STATE_THINKING, "Thinking");
            USART_RX_STA = 0;
        }
        else if (strstr((char *)USART_RX_BUF, "ANSWER") != 0)
        {
            set_pet_state(PET_STATE_ANSWERING, "Answering");
            USART_RX_STA = 0;
        }
        else if (strstr((char *)USART_RX_BUF, "IDLE") != 0)
        {
            set_pet_state(PET_STATE_IDLE, "Idle");
            USART_RX_STA = 0;
        }
        else if (strstr((char *)USART_RX_BUF, "BUSY") != 0)
        {
            set_pet_state(PET_STATE_BUSY, "Busy");
            USART_RX_STA = 0;
        }
    }
}
```

这里没有严格解析完整协议，而是用 `strstr` 查关键字。这样做比较宽容，调试时不容易因为换行符或前缀问题导致状态无法识别。

### 3. 动态切换宠物动作

宠物状态用一个枚举表示：

```c
typedef enum
{
    PET_STATE_IDLE = 0,
    PET_STATE_THINKING,
    PET_STATE_ANSWERING,
    PET_STATE_BUSY
} PetState;
```

不同状态对应不同动作目录和帧数：

```c
static void set_pet_state(PetState state, const char *text)
{
    pet_state = state;
    pet_frame = 0;
    pet_dir = 0;

    if (state == PET_STATE_THINKING)
    {
        strcpy(pet_prefix, "WT");
        pet_frame_count = 5;
    }
    else if (state == PET_STATE_ANSWERING)
    {
        strcpy(pet_prefix, "WV");
        pet_frame_count = 4;
    }
    else if (state == PET_STATE_BUSY)
    {
        strcpy(pet_prefix, "RN");
        pet_frame_count = 6;
    }
    else
    {
        strcpy(pet_prefix, "I");
        pet_frame_count = 6;
    }

    set_status_text(text);
}
```

路径拼接时根据 `pet_prefix` 生成文件名：

```c
static void make_pet_path(u8 frame)
{
    if (pet_state == PET_STATE_THINKING && frame >= 4) frame++;

    path_buf[0] = 0;
    strcpy(path_buf, "0:/INNI050/");
    strcat(path_buf, pet_prefix);
    strcat(path_buf, "/");
    strcat(path_buf, pet_prefix);
    path_buf[strlen(path_buf) + 1] = 0;
    path_buf[strlen(path_buf)] = '0' + (frame / 10);
    path_buf[strlen(path_buf) + 1] = 0;
    path_buf[strlen(path_buf)] = '0' + (frame % 10);
    strcat(path_buf, ".BIN");
}
```

这里有一个小细节：Thinking 状态跳过了 `WT04`，所以内部帧 `4` 会映射到实际文件 `WT05`。

### 4. 避免 UI 闪烁

一开始我每帧都重画整个下方 UI，导致文字闪烁。后来把 UI 分成三部分：

- 宠物帧区域：按动画刷新；
- 状态文字：只有状态变化时刷新；
- 时间数字：单独清除数字区域并重画。

时间刷新代码类似：

```c
LCD_Fill(170, 58, 238, 73, BLACK);
LCD_ShowNum(170, 58, h, 2, 16, YELLOW);
LCD_ShowString(186, 58, 8, 16, 16, YELLOW, (u8 *)":");
LCD_ShowNum(194, 58, m, 2, 16, YELLOW);
LCD_ShowString(210, 58, 8, 16, 16, YELLOW, (u8 *)":");
LCD_ShowNum(218, 58, s, 2, 16, YELLOW);
```

这样只有很小的一块区域会被刷新，视觉上稳定很多。

## 屏幕和宠物显示

小宠物的素材被处理成 RGB565 的 `.BIN` 文件，放在 SD 卡中。目录结构类似：

```text
INNI050/
├─ I/
│  ├─ I00.BIN
│  ├─ I01.BIN
│  └─ ...
├─ WT/
│  ├─ WT00.BIN
│  └─ ...
├─ WV/
│  ├─ WV00.BIN
│  └─ ...
├─ RN/
│  ├─ RN00.BIN
│  └─ ...
└─ PETMANI.JSON
```

其中 `PETMANI.JSON` 记录了动作信息，例如：

- `I`：idle，待机动作
- `WT`：waiting，用作 Thinking 状态
- `WV`：waving，用作 Answering 状态
- `RN`：running，用作 Busy 状态

STM32 端会根据当前状态动态拼接路径，例如：

```c
0:/INNI050/I/I00.BIN
0:/INNI050/WT/WT00.BIN
0:/INNI050/WV/WV00.BIN
0:/INNI050/RN/RN00.BIN
```

这样就可以在不同状态下播放不同动作，而不是只改变文字。

## ESP8266 和 STM32 通信

ESP8266 和 STM32 使用 UART 通信。最终接线是：

```text
ESP8266 / ESP-12F          STM32H743
-----------------          ---------
TX / U0TXD        ------->  PA10 / USART1_RX
RX / U0RXD        <-------  PA9  / USART1_TX
GND               -------   GND
3V3 / VCC         -------   3.3V 稳定电源
EN / CH_PD        -------   3.3V，上拉使能
RST               -------   3.3V，上拉；需要复位时短接 GND
GPIO0             -------   3.3V，正常启动；烧录固件时接 GND
```

如果用的是带 USB 口的 NodeMCU 开发板，供电和 EN/RST/GPIO0 通常已经在板子上处理好了，只需要关心 `TX`、`RX`、`GND`。如果用的是裸 ESP-12F 模组，则一定要注意 3.3V 供电能力，ESP8266 发 WiFi 时电流峰值比较大，建议 3.3V 至少能提供几百 mA。

调试时踩过一个坑：如果 ESP8266 同时连接 USB-TTL 和 STM32，ESP 的 RX 可能会被两个 TX 同时驱动，导致 Lua 命令没有响应。所以早期调试建议只做单向通信：

```text
ESP TX -> STM32 PA10
GND    -> GND
```

等确认通信没问题之后，再接完整的 TX/RX。

STM32 接收的状态字符串很简单：

```text
STATE:IDLE

STATE:THINKING

STATE:ANSWERING

STATE:BUSY

```

收到不同字符串后，屏幕右侧文字和宠物动作都会改变。

## ESP8266 / ESP-12F 配置过程

我手上的 ESP8266 模块启动后串口输出是：

```text
NodeMCU 0.9.6 build 20150704  powered by Lua 5.1.4
lua: cannot open init.lua
>
```

这说明它里面烧的是 NodeMCU Lua 固件，而不是 AT 固件。`lua: cannot open init.lua` 不是错误，只是表示模块里还没有开机自动执行的 `init.lua` 文件。

### 1. 串口连接和基础测试

先用 USB-TTL 连接 ESP8266：

```text
USB-TTL TX -> ESP RX
USB-TTL RX -> ESP TX
USB-TTL GND -> ESP GND
```

串口参数我这里是：

```text
9600 baud
8 data bits
no parity
1 stop bit
```

进入 Lua 交互后，可以测试：

```lua
print("hello")
```

如果返回：

```text
hello
>
```

说明串口通信正常。

再测试芯片 ID：

```lua
print(node.chipid())
```

### 2. 连接 WiFi

NodeMCU Lua 交互模式下一定要一行一行输入，等出现 `>` 再输入下一行。一次粘贴多行容易导致 Lua 解析错误。

连接 WiFi 的命令是：

```lua
wifi.setmode(wifi.STATION)
wifi.sta.config("你的WiFi名", "你的WiFi密码")
wifi.sta.connect()
```

等几秒后查看 IP：

```lua
print(wifi.sta.getip())
```

成功后会返回类似：

```text
10.254.201.130    255.255.255.0    10.254.201.73
```

也可以查看连接状态：

```lua
print(wifi.sta.status())
```

我这里状态码 `5` 表示已经成功连接并拿到 IP。

需要注意：ESP8266 只能连接 2.4GHz WiFi。如果一直连不上，可以先用手机开一个 2.4GHz 热点测试。

### 3. 手动启动一个最小 HTTP 服务

在写完整脚本之前，我先手动启动了一个最小 HTTP 服务，用来确认浏览器到 ESP8266 的链路是通的。

一行一行输入：

```lua
srv=net.createServer(net.TCP)
```

```lua
resp="HTTP/1.0 200 OK\r\n\r\nOK THINKING\n"
```

```lua
function r(c,p) uart.write(0,"STATE:THINKING\r\n") c:send(resp) end
```

```lua
function s(c) c:close() end
```

```lua
srv:listen(80,function(c)c:on("receive",r)c:on("sent",s)end)
```

然后在浏览器访问：

```text
http://10.254.201.130/
```

如果浏览器返回：

```text
OK THINKING
```

同时串口输出：

```text
STATE:THINKING
```

就说明 HTTP 服务和 UART 输出都正常。

### 4. 编写完整 init.lua

完整版本的 `init.lua` 做了几件事：

1. 设置 UART 为 9600；
2. 自动连接 WiFi；
3. 获取 IP 后启动 HTTP server；
4. 解析 URL 中的状态；
5. 通过 UART 发给 STM32。

核心状态发送函数如下：

```lua
local function send_state(state)
    state = string.upper(state or "")

    if state == "THINKING" or state == "THINK" then
        uart.write(0, "STATE:THINKING\r\n")
        return "THINKING"
    elseif state == "ANSWERING" or state == "ANSWER" then
        uart.write(0, "STATE:ANSWERING\r\n")
        return "ANSWERING"
    elseif state == "IDLE" then
        uart.write(0, "STATE:IDLE\r\n")
        return "IDLE"
    elseif state == "BUSY" then
        uart.write(0, "STATE:BUSY\r\n")
        return "BUSY"
    end

    return nil
end
```

URL 解析函数如下：

```lua
local function get_query_value(payload)
    local v = string.match(payload, "[?&]value=([A-Za-z]+)")
    if v == nil then
        v = string.match(payload, "GET /([A-Za-z]+)")
    end
    return v
end
```

所以它既支持：

```text
/state?value=thinking
```

也支持：

```text
/thinking
```

HTTP 收到请求后，根据解析出来的状态调用 `send_state()`，再返回文本响应。

### 5. 上传 init.lua 到 ESP8266

因为我的电脑上安装 `nodemcu-tool` 遇到了 Node.js 原生依赖问题，所以最后用了 Python + pyserial 写了一个简单上传脚本，把 Lua 文件逐行写入 ESP8266 的文件系统。

上传逻辑本质上就是通过串口执行这些 Lua 命令：

```lua
file.remove('init.lua')
file.open('init.lua','w+')
file.write('...')
file.close()
node.restart()
```

上传完成后，查看文件列表能看到：

```text
init.lua    2285
```

重启后 ESP8266 会自动执行 `init.lua`，串口输出：

```text
Connecting WiFi...
WiFi waiting...
WiFi connected: 10.254.201.130
HTTP server started
Try: http://10.254.201.130/state?value=thinking
```

到这里，ESP8266 配置就完成了。之后它每次上电都会自动连 WiFi 并启动 HTTP 服务。

## ESP8266 HTTP 服务

ESP8266 运行的是 NodeMCU Lua 固件。上电后会自动连接 WiFi，并启动一个 HTTP 服务。

核心逻辑是：浏览器访问某个 URL，ESP8266 就通过串口发状态给 STM32。

例如：

```text
http://10.254.201.130/state?value=thinking
```

ESP8266 收到后发送：

```text
STATE:THINKING

```

然后 STM32 屏幕显示 `Thinking`，宠物切换到等待/思考动作。

支持的 URL 有：

```text
/state?value=thinking
/state?value=answering
/state?value=idle
/state?value=busy
```

也支持简写：

```text
/thinking
/answering
/idle
/busy
```

ESP8266 启动成功时串口输出类似：

```text
NodeMCU 0.9.6 build 20150704
Connecting WiFi...
WiFi waiting...
WiFi connected: 10.254.201.130
HTTP server started
Try: http://10.254.201.130/state?value=thinking
```

看到这些输出的时候，就说明 ESP8266 已经变成了一个小型 HTTP 状态控制器。

## Claude Code hooks 自动同步状态

最后一步是让 Claude Code 自动通知 ESP8266。

Claude Code 的 hooks 可以在一些事件发生时自动执行命令。这里用到三个事件：

- `UserPromptSubmit`：用户提交消息时触发；
- `Stop`：Claude 本轮回复结束时触发；
- `Notification`：Claude Code 产生通知时触发。

我先写了一个 PowerShell 脚本：

```powershell
$ErrorActionPreference = 'SilentlyContinue'
$Base = 'http://10.254.201.130/state?value='
$State = if ($args.Count -gt 0) { $args[0] } else { 'idle' }
try {
    Invoke-WebRequest -UseBasicParsing -TimeoutSec 1 -Uri ($Base + $State) | Out-Null
} catch {
}
```

保存为：

```text
C:\Users\13683\.claude\esp-state.ps1
```

然后在 Claude Code 的 `settings.json` 中增加 hooks。配置文件位置是：

```text
C:\Users\13683\.claude\settings.json
```

核心配置类似：

```json
{
  "hooks": {
    "UserPromptSubmit": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "pwsh -NoProfile -ExecutionPolicy Bypass -File \"C:\\Users\\13683\\.claude\\esp-state.ps1\" thinking",
            "timeout": 2,
            "statusMessage": "Sending Thinking to STM32"
          }
        ]
      }
    ],
    "Stop": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "pwsh -NoProfile -ExecutionPolicy Bypass -File \"C:\\Users\\13683\\.claude\\esp-state.ps1\" idle",
            "timeout": 2,
            "statusMessage": "Sending Idle to STM32"
          }
        ]
      }
    ],
    "Notification": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "pwsh -NoProfile -ExecutionPolicy Bypass -File \"C:\\Users\\13683\\.claude\\esp-state.ps1\" busy",
            "timeout": 2,
            "statusMessage": "Sending Busy to STM32"
          }
        ]
      }
    ]
  }
}
```

配置完成后，可以在 Claude Code 里打开：

```text
/hooks
```

检查 hooks 是否被加载。如果当前会话没有立刻生效，重启 Claude Code 后就会重新读取配置。

最终效果就是：

```text
我发消息
  -> UserPromptSubmit hook 触发
  -> 请求 /state?value=thinking
  -> ESP8266 通过 UART 发 STATE:THINKING
  -> STM32 屏幕显示 Thinking
  -> 宠物切换动作

Claude 回复完成
  -> Stop hook 触发
  -> 请求 /state?value=idle
  -> ESP8266 通过 UART 发 STATE:IDLE
  -> STM32 显示 Idle
  -> 宠物回到待机动作
```

这里有一个限制：Claude Code hooks 里没有一个非常精确的“开始输出文字”事件，所以 `Answering` 这个状态目前主要可以通过手动 URL 或后续扩展脚本触发。当前自动流程主要是 `Thinking -> Idle`。

## 调试过程中遇到的问题

### 1. STM32 编译报 undefined symbol

一开始有一个链接错误：

```text
Undefined symbol draw_pet_ui
```

原因是之前把 UI 绘制拆成了：

```c
static void draw_pet_ui_static(void)
static void draw_clock_only(void)
```

但调用处还在调用旧的 `draw_pet_ui()`。把调用改成新的两个函数后解决。

### 2. 底部文字闪烁

最早每一帧都会重绘整个 UI 区域，导致底部文字闪烁。后来改成：

- 静态文字只画一次；
- 时间数字单独刷新；
- 宠物帧单独刷新。

这样显示稳定了很多。

### 3. ESP8266 Lua 命令不执行

当 ESP8266 的 RX 同时接了 USB-TTL TX 和 STM32 PA9 时，Lua 交互会失效。拔掉 STM32 PA9 后恢复正常。

这个问题提醒我：调试阶段不要让两个输出脚同时驱动同一个输入脚。

### 4. PA10 接错线

一开始 STM32 一直收不到 ESP8266 数据，甚至把所谓的 PA10 接地，屏幕仍然显示高电平。后来才发现是线插错了。

嵌入式调试里，软件问题和硬件接线问题经常交织在一起。最有效的方法还是一步步拆分验证：先确认 ESP 输出，再确认 STM32 输入，再确认串口协议。

## 当前状态动作映射

现在的动作映射是：

```text
Idle      -> I   待机动作
Thinking  -> WT  等待/思考动作
Answering -> WV  挥手动作
Busy      -> RN  跑动动作
```

Thinking 状态里还单独跳过了 `WT04` 这一帧，因为实际效果不太满意，所以播放序列变成：

```text
WT00 -> WT01 -> WT02 -> WT03 -> WT05
```

这个小细节让我感觉它更像真的在“思考”。

## 总结

这个项目本质上是一个很小的“AI 状态外设”，但做完之后意外地有生命感。

它把几个原本独立的东西连了起来：

- STM32 负责稳定显示；
- SD 卡负责存储动画帧；
- ESP8266 负责网络通信；
- Claude Code hooks 负责把软件状态同步到硬件；
- 小宠物负责把这些状态变得可爱一点。

最后，它不只是一个屏幕，也不是一个简单的串口显示器，而是一个能跟着 Claude Code 工作状态变化的小桌面伙伴。

后续还可以继续扩展：

- 显示当前会话标题；
- 显示今日提问次数；
- 支持自定义短文本显示；
- 根据空闲时间切换更多动作；
- 加外壳，做成真正的桌面电子宠物。

这次最大的感受是：当软件状态被映射到一个真实的小屏幕和小动画上时，交互会变得很有趣。AI 不再只是终端里的文字，它也可以在桌面上拥有一个小小的身体。