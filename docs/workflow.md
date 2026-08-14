# 单人双机协作工作流（每日操作手册）

> **核心原则：Git 只在「已 push 的提交」之间同步。** 工作区里没提交的改动、`git stash` 里的东西，都「困」在当前机器上，另一台机器看不见。下面所有纪律都从这一句推出。

本文面向「一个人在 Windows 机器 A / 机器 B 之间交替开发」的场景。环境安装见 [`getting-started.md`](getting-started.md)，门禁标准见 [`golden-gates.md`](golden-gates.md)。

## 0. 一次性准备（两台机器各做一次）

- 统一 git 身份：`git config --global user.name` / `user.email` 两机一致，避免提交者信息混乱。
- GitHub 凭据用 PAT 或 SSH key（2021 后不支持密码 push）。
- 设上游：`git push -u origin main`（之后能简写 `git push` / `git pull`）。
- MSYS2 装在默认路径 `C:\msys64`（Makefile 硬编码了该路径）。
- `.gitignore` 已覆盖 `.tmp/`、`local-repo.git/`、`bin/`、`build/`、`output/`、summary JSON 等本机产物。

## 1. 每日开机仪式（先 pull，但不止 pull）

```bash
git fetch                        # 1. 拿远端最新
git status                       # 2. 先看本地干不干净（脏了先处理，见第 6 节，别带脏树 pull）
git pull --rebase origin main    # 3. 干净了才拉最新并 rebase
```

然后做环境自检（重开机后这些变量都丢了，要重设）：

```powershell
$env:PATH = 'C:\msys64\usr\bin;C:\msys64\ucrt64\bin;C:\Program Files\Microsoft MPI\Bin;' + $env:PATH
$env:TEMP = '<仓库目录>\.tmp'; $env:TMP=$env:TEMP; $env:TMPDIR=$env:TEMP
New-Item -ItemType Directory -Force -Path $env:TEMP | Out-Null
```

确认 p4est 已编译、求解器可构建：

```bash
test -f third_party/p4est/build/local/lib/libp4est.a   # p4est 就绪？
make -j8                                                 # 重新构建 exe
```

## 2. 工作循环（改代码 → 存档）

- 正常改代码、调试。
- 随时喊 **「存档」**：立即 WIP 提交 + push（只 stage 源码/文档，不碰 `.tmp`/`output` 等垃圾；message 从改动自动拟一句有意义的摘要）。想存几次存几次。
- 需要验证时 `make -j8` + 跑对应算例。

## 3. 门禁与正式提交

- 喊 **「正式提交」**：自动跑 G0（构建）→ G1（串行）→ G3（MPI），**全绿才** commit + push；任何一门不过就停下汇报，不提交。
- 核对 `serial_golden_summary.json` / `mpi_gate_summary.json` 的 `param_restored: true`。
- 门禁容差以 `python/gates_common.py` 的 `GATE_TOLERANCE` 为唯一来源（当前 `1e-6`）。

## 4. 每日收工仪式（离开任何机器前必做）

```bash
git status                       # 看有没有没提交的
git add <具体文件>               # 精确加，别 -A
git commit -m "WIP: ..."         # 哪怕没做完、编不过也提交（检查点，不是发布）
git push                         # 关键：推上去，工作才算"离开"这台机器
```

**永远不带未提交工作离开一台机器。**

## 5. 换机交接

- A 收工：commit + push。
- B 开工：`git fetch` + `git pull --rebase`。
- 活没干完要搬到另一台：在 A 上照样 commit + push（哪怕 WIP），到 B 拉下来继续。

## 6. 出岔子恢复（A 忘了 push，B 已 push）

```bash
git status              # 看 A 上有哪些游离改动
git stash               # 先收起来
git pull --rebase       # 把 A 追平到 B 的最新
git stash pop           # 放回改动，冲突则手动解
```

## 7. 禁忌与常见坑

1. **绝不用网盘 / OneDrive / Dropbox 同步工作目录**——会损坏 `.git`、带坏 build 产物、造成对象错乱。git 才是唯一的同步工具，工作目录必须纯本地，仓库不要放进云同步目录。
2. **不要把整机备份/镜像当同步手段**——会夹带 `.o`、旧 exe、output 等本机产物。
3. `build/`、`bin/`、`output/`、`.tmp/`、`third_party/p4est/build/` 是**每台机器独立**的，别提交、别同步；换机后第一件事是重新编译 p4est + `make`。
4. `reference/` 只在「门禁绿 + 有意的物理变化」时更新，并附「结果为何变了」的说明；纯环境舍入差异不要提交。
5. `param.ini` 别手动改完忘恢复；两个 runner 结束都会逐字节恢复它。
6. `serial_golden_summary.json` / `mpi_gate_summary.json` 是门禁证据，已 gitignore，留在本地即可，别提交。
7. 断网 / 需代理（GFW）时 push/pull 失败先测网络，别慌着删东西；本仓库是部分克隆（blobless），看老提交的 diff 等操作也需联网懒加载。
8. 两台机器时钟若不同步，`make` 的 mtime 判断会受影响——因为 build 目录每机独立，影响有限，但别对同一份工作目录跨机共享。
9. 忘了「哪台机器最新」→ 收工都 push 的话，答案永远是「GitHub 最新」；`git fetch` + `git status` 一眼看清 ahead/behind。
