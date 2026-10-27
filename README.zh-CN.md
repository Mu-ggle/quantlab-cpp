# QuantLab 中文使用说明与答辩准备

这是一个 C++17 金融时间序列分析小项目，包含简单收益率、移动平均、
年化样本波动率和最大回撤四个函数。作业的主要评分对象是工程流程，
因此项目把源码、测试、性能基准和文档分开放置，并通过 GitHub Actions
自动执行构建、测试、测速和文档发布。

- [公开仓库](https://github.com/Mu-ggle/quantlab-cpp)
- [CI 运行记录和性能输出](https://github.com/Mu-ggle/quantlab-cpp/actions/workflows/ci.yml)
- [Doxygen 在线文档](https://Mu-ggle.github.io/quantlab-cpp/)
- [面向老师的俄语 README](README.md)

## 本地运行

需要 CMake 3.22 或更新版本、支持 C++17 的编译器。首次配置会下载固定版本的 doctest
和 Google Benchmark，需要联网。文档生成另外需要 Doxygen。

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Windows 下如果使用 MinGW，建议在新的构建目录显式指定生成器，避免
CMake 默认选择 Visual Studio：

```powershell
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw --parallel
ctest --test-dir build-mingw --output-on-failure
```

部分旧版 MinGW 或 CMake 无法正确处理带中文、俄文的路径。遇到此类
问题时，使用纯 ASCII 项目路径，例如 `C:\dev\quantlab-cpp`，并重新
配置新的构建目录。已有构建目录不能直接换生成器。下面的程序路径
以 `build` 为例；如果采用 `build-mingw`，相应替换目录名。

使用 MinGW 时，可运行：

```powershell
.\build\quantlab_demo.exe
.\build\benchmarks\quantlab_benchmark.exe --benchmark_min_time=0.05s
```

使用 Visual Studio 生成器时，可执行文件多一层 `Release`：

```powershell
.\build\Release\quantlab_demo.exe
.\build\benchmarks\Release\quantlab_benchmark.exe --benchmark_min_time=0.05s
```

如果编译器尚未加入 PATH，需要先在对应的开发者终端中运行上述命令。
Linux 下把路径改为 `./build/quantlab_demo` 和
`./build/benchmarks/quantlab_benchmark`。

本地生成文档：

```powershell
cmake -S . -B build -DQUANTLAB_BUILD_DOCS=ON
cmake --build build --target docs
```

之后用浏览器打开 `build/docs/html/index.html`。

## 四个函数需要讲清楚什么

| 函数 | 可以这样解释 |
| :--- | :--- |
| `simple_returns` | 相邻价格的比例减 1。100 涨到 110 的收益率是 0.1，即 10%。n 个价格得到 n-1 个收益率。 |
| `moving_average` | 对每个完整窗口求平均。窗口大小为 w 时输出 n-w+1 个结果；通过移除旧值、加入新值把复杂度降到 O(n)。 |
| `sample_volatility` | 对中心化、缩放后的数据使用 Welford 算法计算样本方差，分母是 n-1，再恢复尺度并年化。默认一年 252 期，需要与数据频率一致。 |
| `maximum_drawdown` | 按时间顺序记录历史最高价格，计算当前价格相对该峰值的跌幅，并取最大值。输出 0.2 表示最大回撤 20%。 |

价格必须为正且有限；不接受 NaN 和无穷大。移动平均的窗口至少为 1，
且不能大于输入长度。样本波动率至少需要两个收益率，年化期数必须是
有限正数。无效参数会抛出 `std::invalid_argument`；无法用有限 `double`
表示的结果会抛出 `std::overflow_error`。

年化乘以平方根是模型约定，不表示真实收益一定独立、稳定，也不是预测。
项目没有交易接口，演示使用固定数据，不需要账户或密钥。

实现先减去参考值进行中心化，减少“大数附近的小幅变化”带来的精度
损失；再缩放后计算，降低直接平方导致中间值溢出的风险。最终返回的
数学量仍是样本标准差乘以 `sqrt(periods_per_year)`。这些措施改善数值
表现，不意味着浮点运算没有舍入误差。

## 工程部分的答辩要点

**为什么用 CMake？** 它描述目标、依赖关系和编译选项，生成平台对应的
构建系统。`cmake -S . -B build` 是配置；`cmake --build build` 才是编译。
库目标供 demo、测试和 benchmark 共同链接，避免复制实现。

**单元测试和 benchmark 有什么区别？** doctest 判断结果是否正确；
Google Benchmark 重复运行函数并估计时间。前者通过不能证明速度快，
后者速度快也不能证明结果正确。CTest 负责发现并启动测试程序。

**怎样避免 benchmark 被编译器删掉？** 使用 Google Benchmark 的
`DoNotOptimize` 保留对计算结果的使用。输入数据在计时循环之外生成。
返回向量的内存分配是函数本身的工作，因此包含在相应函数的耗时内。

**为什么在 Release 下测速？** 优化设置会显著影响性能；应记录构建模式。
共享 CI 机器的负载会变化，单次结果主要证明自动测速流程可用，不能把
不同机器的绝对时间直接当成算法改进幅度。

**Doxygen 做什么？** 它从头文件中的 API 注释和文档主页生成 HTML。
CI 自动生成后交给 GitHub Pages 发布；代码 push 后可以重新构建更新。
严格警告配置可以让缺失或错误的文档在检查阶段暴露。

**AGENTS.md 做什么？** 它给后续 AI 编程助手说明项目目的、目录、
构建和验证命令以及修改限制。它是协作规范，不是可执行程序。

## 提交前逐项验收

首次发布可以使用项目脚本。安装 GitHub CLI 后，在 PowerShell 7 中执行：

```powershell
gh auth login --hostname github.com --git-protocol https --web --scopes workflow
pwsh -File scripts/publish.ps1 -Owner YOUR_LOGIN
```

把 `YOUR_LOGIN` 换成自己的 GitHub 用户名。脚本用于创建公开的
`quantlab-cpp` 仓库、替换文档链接中的用户名、启用 Pages、推送项目并
检查远程 CI。执行发布脚本前，仓库与文档网站的链接是待替换模板。
只有实际成功的运行和可访问的网站才能作为已完成的证据。

| 分值 | 验收内容 | 向老师展示的证据 |
| :---: | :--- | :--- |
| 2 | 仓库公开，目录清楚，README 说明用途和构建 | 不登录也能打开的仓库首页 |
| 4 | CMake 构建在 push 后自动成功 | GitHub Actions 对应提交的构建日志 |
| 4 | 使用 C++ 测试框架且 CI 测试成功 | CTest 成功日志及 `tests/` 代码 |
| 4 | 使用 benchmark 工具且 CI 自动测速 | 日志中的函数、数据规模和耗时表格 |
| 4 | Doxygen 文档自动发布，README 有链接 | 成功部署记录和可打开的文档网站 |
| 2 | 根目录有 AI 代理配置 | `AGENTS.md` |

题目明确：第 2-5 项只有在远程 CI 实际成功运行时才给满分，否则只给
一半。不能只检查本地构建或 YAML 文件。首次启用 Pages 时，在仓库
**Settings → Pages → Build and deployment → Source** 选择
**GitHub Actions**。等待部署完成后，再从 README 点击在线文档链接验证。

如果 CI 失败，先打开失败步骤的日志，从第一条真实错误开始排查。修改
后再次 push，确认绿色状态属于准备提交的最新 commit。不要把旧提交
的成功结果当成新提交已通过。

## AI 使用说明

本项目按老师允许的方式使用 AI 辅助完成。答辩时应如实说明使用方式，
并能够解释函数、边界检查、测试和 CI 流程。上面的说明用于学习和复现，
不应把尚未发生的远程成功记录写成已经完成。
