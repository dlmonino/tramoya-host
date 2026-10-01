[English](README.md) · **简体中文**

# PS5 WKAL ESP32-S3 主机

一块 ESP32-S3 固件，把 [itsPLK/ps5-webkit-autoloader](https://github.com/itsPLK/ps5-webkit-autoloader)
的 **PC host** 流程和 **U 盘**合并成一个设备——直接插在 PS5 上：

1. **USB**：ESP32 枚举为 U 盘（TinyUSB MSC，跑在磨损均衡的 FAT 分区上）。
   PS5 从盘上的 `ps5_autoloader/` 读取 payload；PC 也可以直接改写
   `frontend.zip`、放入新 payload。
2. **WiFi**：ESP32 开放热点（`PS5 WKAL`）。DHCP 把热点自己的 IP 下发为
   DNS 服务器；DNS :53 只应答 `manuals.playstation.net`（指向热点 IP），
   其余域名一律 NXDOMAIN。
3. **HTTPS :443 / HTTP :80** 从**同一块 FAT 分区**流式提供前端页面
   （tinfl 环形缓冲逐条目解压）——zip 既不进固件镜像，也不进 RAM。

两个角色同时工作：PS5 用 USB 给设备供电、挂载 U 盘、连上热点、打开
用户指南，漏洞链直接从盘上加载 payload。更新流程：从 PS5 上拔下 →
插到 PC 上改文件 → 拔下 → 重新插回 PS5（重新上电即生效）。

## 硬件目标

支持所有 ESP32-S3 闪存/PSRAM 组合——用 `--target`（或
`make build TARGET=...`）选择板型；每种板型有独立的分区表、sdkconfig
和构建目录：

| target          | 闪存  | PSRAM | PSRAM 模式 | 存储分区   |
|-----------------|-------|-------|------------|------------|
| `esp32s3-n4r2`  | 4 MB  | 2 MB  | quad       | 2.44 MB    |
| `esp32s3-n8r2`  | 8 MB  | 2 MB  | quad       | 5.81 MB    |
| `esp32s3-n8r8`  | 8 MB  | 8 MB  | octal      | 5.81 MB    |
| `esp32s3-n16r2` | 16 MB | 2 MB  | quad       | 13.81 MB   |
| `esp32s3-n16r8` | 16 MB | 8 MB  | octal      | 13.81 MB   |
| `esp32s3-n32r8` | 32 MB | 8 MB  | octal      | 29.87 MB   |

`python3 tools/build.py targets` 输出 `targets/boards.json` 里的实时列表。

## U 盘：目录结构与 payload 自动加载

暴露出来的卷就是一块普通 FAT 盘，插到 PC 上会看到：

```
frontend.zip            热点提供的网页前端（见下文）
ps5_autoloader/         PS5 侧读取的 payload 投放目录
  README.txt            盘上的简短说明
  autoload.txt          （可选）加载清单，格式见下文
  etaHEN.elf            你自己放进来的 payload ELF
  kstuff.elf
  ...
```

漏洞链完成破解后会加载 **unified autoloader**（作为
`payloads/payload.elf` 打包在前端 zip 里）。autoloader 会等 ELF 加载器
就绪，然后按优先级从高到低查找 `autoload.txt`：

1. U 盘上的 `ps5_autoloader_<app>/autoload.txt` —— `<app>` 是蓝光
   破解的 `bdjb`，或入口应用的 Title ID（如 YouTube 的 `PPSA01650`）
2. PS5 内部 `/data` 的 `ps5_autoloader_<app>/autoload.txt`
3. U 盘上的 `ps5_autoloader/autoload.txt` —— **本设备通常用这个**
4. PS5 内部 `/data` 的 `ps5_autoloader/autoload.txt`

找到配置就按顺序逐个启动列出的 payload；一个都找不到时，autoloader
启动自带的 **Payload Manager**，改在主机上手动选择 payload。

### `autoload.txt` 格式

```
# 以 '#' 开头的行为注释
etaHEN.elf              # 相对本文件所在目录解析
!1500                   # 下一行执行前先睡 1500 毫秒
kstuff.elf
@sync                   # 可选，见下文
```

- 一行一个条目；空行和 `#` 注释行会被忽略
- 文件名相对 `autoload.txt` 所在目录解析；以 `/` 开头的绝对路径按原样使用
- `!<毫秒>` 在处理下一行前先休眠——时序敏感的 payload（比如
  kstuff 紧跟 etaHEN）通常需要停顿一下
- `@sync`（建议放最后一行）：当所有 payload 都零错误加载完成后，把
  整个生效目录复制到 PS5 内部 `/data` 并逐字节校验，然后从 U 盘删除，
  之后开机就不再需要 U 盘。注意：在本设备上 `@sync` 是**从 PS5 侧
  写入模拟 U 盘**，不属于受支持的更新流程，请当作实验性功能。受支持
  的流程是：设备从 PS5 拔下后，在 PC 上直接编辑盘内文件。

payload ELF 本身不在仓库里——把你用到的（etaHEN、kstuff 等）从各自
项目下载后放进来即可。想在刷机时就把 payload 烧进存储镜像，用
`make storage PAYLOAD_DIR=<目录>`（或 `build.py --payload-dir <目录>`）。

更新流程：从 PS5 拔下 → 插到 PC → 改文件 → 弹出 → 插回 PS5。固件会
缓存 FAT/zip 索引直到下一次断电重启，所以在运行中改的文件要重插后才生效。

## 前端从上游构建——仓库不固化任何版本

本仓库**不**内置 `frontend.zip` 或上游漏洞前端。打包时构建流程会：

1. 解析上游 `ps5-webkit-autoloader` 项目的**最新 Release Tag**（走
   GitHub API，失败时回退 git tag；用 `--tag v0.5.2` 或 `UPSTREAM_TAG=`
   可钉住版本），
2. 带子模块克隆该 Tag 到 `.deps/upstream/<tag>/`（缓存复用，Tag 指向
   的 commit 不变就不重新克隆），
3. 执行上游自己的准备脚本（`download_deps.sh`、
   `apply_relapse_patch.sh`、`apply_slopkit_patch.sh`、
   `apply_umtx2_patch.sh`），
4. 把 `frontend/autoloader` + `pc-host/overrides` 打成 `frontend.zip`
   （DEFLATE、上游路径过滤、生成图标、`[[…PLACEHOLDER]]` 版本/构建
   时间替换，以及不支持的浏览器提示修复）。

每个产物都用 `frontend.zip-source.json` 记录前端的确切来源（Tag、
commit、SHA256），同时写入 `firmware-manifest.json`。

### 只想要 U 盘用的 frontend.zip？

不需要 ESP-IDF——只要 Python 3、git 和 bash：

```sh
python3 tools/build_frontend_zip.py
# → frontend.zip（+ frontend.zip-source.json），默认按 esp32s3-n4r2
#   的存储容量检查；8 MB 板加 --target esp32s3-n8r8，installer 流程
#   加 --payload installer.elf。
```

然后把 ESP32-S3 插到 PC，把 `frontend.zip` 拷到暴露的盘上（替换原有
文件），弹出、拔下、插回 PS5。

## 构建固件

```sh
# 一次性：把 ESP-IDF v5.4.2 放在本仓库旁边
git clone -b v5.4.2 --recursive --shallow-submodules \
    https://github.com/espressif/esp-idf.git esp-idf
cd esp-idf && ./install.sh esp32s3

# 构建（先激活 IDF 环境：. ./esp-idf/export.sh）
python3 tools/build.py package --target esp32s3-n4r2
```

或者用包装器：

```sh
make targets                 # 列出板型
make build  TARGET=esp32s3-n8r8
make package TARGET=esp32s3-n8r8
make zip     UPSTREAM_TAG=v0.5.2   # 只打 frontend.zip，输出到 main/
make storage FRONTEND_ZIP=my.zip  # 只重建 FAT 镜像
```

`package` 产出 `build/<target>/ps5_wkal_host-full.bin`——按板型闪存
大小生成的完整首刷镜像（bootloader、分区表、应用、存储）。写入位置是
**0x0**。它会重置 NVS 并覆盖存储内容——适合首次刷写；之后的更新都是
U 盘上的文件复制，不需要重刷（设计上没有 OTA）。

刷机必须显式指定串口——不会自动选择设备：

```sh
make flash  TARGET=esp32s3-n4r2 PORT=/dev/ttyACM0
make monitor TARGET=esp32s3-n4r2 PORT=/dev/ttyACM0
make flash-all ... --erase-storage   # 连存储分区一起重写（会清空盘内文件）
```

## CI（GitHub Actions）

`.github/workflows/build.yml`：

- **Run → Build firmware**（workflow_dispatch）：构建全部 6 种板型
  （或用 `targets` 指定逗号分隔子集），每种板型都打包来自上游最新
  Release 的前端（可用 `upstream_tag` 钉版本）。产物：`frontend`
  （zip + 溯源）和每板型的 `firmware-<target>`（完整镜像、manifest、
  存储镜像）。
- **推送 `v*` tag**：同样矩阵，外加创建 GitHub Release，包含每板型的
  `ps5-wkal-<tag>-<board>-full.bin` 和 `frontend.zip`。
- 每次运行都会执行宿主测试套件（runner 上跑单元 + ASan，ESP-IDF
  v5.4.2 容器里跑含 FAT 回读的全套）。

## 测试

```sh
python3 tools/build.py test         # 需要 make + gcc（+ zlib）
python3 tools/build.py test-asan
```

测试套件（`tools/tests/`）在宿主机上编译固件实际运行的那几个源文件
（`main/wk_*.c`、`main/http_server.c`、`main/miniz.c`）：

- **DNS 差分**——同样的线路查询喂给固件的 `wk_dns_handle_packet()` 和
  参考 `pc-host/host.py` 的应答构造，两者输出必须逐字节一致。
- **路径映射差分**——`wk_zip_map_path()` 与参考 `_relative_path()`
  在自动生成的 URI 用例上对比（query 参数、`/app/`、`/document/.../ps5/`、
  `%2e%2e` 折叠、`..` 链）。
- **zip 索引**——手工构造的 STORE+DEFLATE 小归档，加垃圾数据拒绝。
- **HTTP 核心**——`wk_http_serve_get()` 走内存 sink：真实归档的每个
  条目都解压到声明大小且 CRC32 一致。
- **payload.elf zlib 交叉校验**——固件 tinfl 循环与独立 zlib 解压对比。
- **HTTP handler**——通过 mock `esp_http_server` 驱动真实的
  `wk_httpd_handle_get`。
- **FAT 回读**（需要 ESP-IDF）——`storagegen.py` 生成的镜像用 IDF 的
  `fatfsparse` 读回，逐字节一致，再经过固件 zip 索引提供一次。

改了 `wk_*.c` 或 `http_server.c` 就跑一遍。

## 容量预算（4 MB 板，实测）

| 项目 | 大小 |
|------|------|
| 应用（TLS、DNS、MSC/CDC——不含前端） | ~894 KiB / 1408 KiB factory |
| 崩溃记录分区 | 128 KiB |
| 存储分区（FAT，磨损均衡） | 2.4375 MB |
| 上游 Release 打出的 frontend.zip（DEFLATE） | ~1.95 MB |
| 盘上剩余空间 | ~0.5 MB |

installer-ELF 前端变体（约 2.9 MB）装不进 4 MB 闪存——请用 8 MB 以上
板型。RAM：zip 索引（约 30 KB）和每连接 48 KB 缓冲；归档从 FAT 经
tinfl 环形缓冲流出，从不整体驻留 RAM。

## 注意事项

- 前端（漏洞链、补丁、图标）是上游 GPL-3.0 作品；本仓库只负责打包和
  提供服务。payload 在构建时从上游钉住的 Release 拉取——这里不提交
  任何漏洞相关内容。
- 内置的 miniz 对 ROM 提供的符号使用私有命名：ESP32-S3 ROM 自带的
  `tinfl_decompressor` 布局不兼容，解析到 ROM 版本会破坏堆。
  `build.py` 每次构建都会检查最终链接的实现和 HTTP 目标文件的引用。
- 证书以 `EMBED_TXTFILES` 嵌入（mbedTLS 要求 PEM 长度包含结尾 NUL）；
  `build.py` 会在链接产物中校验证书/私钥字节和公钥匹配。首次构建时
  自动生成。
- `auto_mount_off=1` 加链接器包装的 `tud_msc_test_unit_ready_cb`，让
  本地 FAT 挂载与 MSC 对介质的通告共存。主机和应用只读；只有 PC 侧
  写入。
- 应用在服务时如果 PC 改写了 FAT，应用的缓存要等下一次断电重启才
  刷新——所以要"拔下 → 改文件 → 重插"。
- HTTPS **和** HTTP 同时运行；PS5 走 443，浏览器可以在 80 上测试。
  本地可达性应答覆盖 `/netstart/icst|ps4|ps5`、`/connecttest.txt`、
  微软连通性测试域名和网络测速流，让主机的 Wi-Fi 检查在热点上通过。
- `CONFIG_SPIRAM_MODE_QUAD`/`OCT` 按板型匹配模组；PSRAM 与表中不同的
  板需要在 `targets/` 里加自己的条目。
- 无 OTA：factory 单槽是设计决定（更新就是拷文件）。
- `esp_tinyusb`/`tinyusb` 是受管组件，首次构建时从 Espressif registry
  下载（需要一次网络）。

## 许可

`main/` 和 `tools/` 中的固件源码：GPL-3.0-or-later，与本主机打包的
上游项目一致（见 `LICENSE`）。miniz 是公有领域；上游前端保留其自身的
GPL-3.0 许可，从钉住的 Release Tag 原样获取。
