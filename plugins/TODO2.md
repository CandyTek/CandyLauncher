别人做出的dll里使用了联网权限，或者使用扫盘等破坏行为，这种情况怎么应对？我应该提供权限机制以限制dll 或者是 交代用户请从信任的地方获取dll插件？


短答案：**两条腿一起走**——既要“源头把关”（签名、商店、信任机制），也要“技术兜底”（沙箱、权限与资源限制、监控与隔离）。单靠“提醒用户只装可信 DLL”远远不够；而**让未知 DLL 直接 in-process 运行**，你就**无法真正禁止它联网/读写磁盘**。正确做法是把不受信任插件**搬到进程外**，通过**受限令牌 + 作业对象(Job) +（最好）AppContainer** + “代理/Broker”接口来做所有敏感操作。

下面给你一份**安全 TODO 清单**（按落地优先级排好）：

# 1) 插件信任分级（从入口把关）

* 定义三类插件：
  **Trusted（可信，in-proc）**：你自己或审核通过、强签名的。
  **Restricted（受限，out-of-proc）**：第三方/未知来源，默认走沙箱。
  **Blocked（封禁）**：命中黑名单或违规。
* 要求 DLL **必须有 Authenticode 签名**；加载前做：签名链校验、证书吊销检查、时间戳有效性、发布者白名单匹配。
* 维护**线上撤销列表/黑名单**（按插件 ID/发布者证书指纹/文件哈希），**启动和每日**拉取更新；命中即阻止加载并提示。

# 2) 能力声明 + 权限授予（最小权限）

* 让插件在 `plugin_config.json` 里**声明能力**（capabilities），例如：

  ```json
  {
    "capabilities": {
      "network": { "allow": false, "allowlist": ["api.example.com:443"] },
      "fs_read": ["%APPDATA%/CandyLauncher/plugins/PluginX/**"],
      "fs_write": ["%LOCALAPPDATA%/CandyLauncher/plugins/PluginX/data/**"],
      "registry_read": [],
      "registry_write": [],
      "spawn_process": false,
      "gpu_compute": false
    }
  }
  ```
* 首次安装/首次使用时弹**权限对话框**（可持久化），默认全部拒绝；细粒度到“域名/目录/动作”。

# 3) 执行模型：**未知插件一律进程外运行**

> in-proc DLL 与主程序共享权限，**无法**从 OS 层只针对它“禁网/禁写”。要安全，就 out-of-proc。

* 为 Restricted 插件创建**沙箱进程**，通过 IPC（命名管道/Unix 域套接字/COM/gRPC）与宿主通信。
* 沙箱进程启动时应用：

  * **受限令牌**：`CreateRestrictedToken` 去除管理员/特权 SID，设为 **Low Integrity**。
  * **作业对象 JobObject**：限制 CPU（硬帽或权重）、内存、句柄数、线程数；启用 **KillOnJobClose**；必要时 IO/网络速率控制。
  * **AppContainer（推荐）**：为每个插件创建 AppContainer 配置文件/低盒 SID，仅允许声明的能力；把插件数据目录 ACL 授权给该 SID（只读/读写）。
  * **工作目录**仅指向插件的专属数据目录；环境变量白名单。
  * **禁 GPU**（不允许加载 OpenCL/D3D12 CUDA 等模块；或通过策略由 Broker 决定是否允许）。
* in-proc 只保留给 Trusted 插件，并且仍走**宿主 API**（见下一条），避免直接摸内部结构。

# 4) 代理化 I/O（Broker）——**所有敏感能力走宿主**

* 在 IPluginHost 里提供**受控 API**：`HttpGet/HttpPost`、受限文件读写、注册表访问、子进程启动等。
* **插件沙箱进程**禁止直连 WinHTTP/Winsock 等；需要联网时，**只能调用宿主的 HTTP Broker**，Broker 根据**域名白名单/速率/超时/数据量**执行，并完整审计。
* 文件系统也同理：只允许访问**插件数据根**，路径必须通过 Broker 校验（支持只读/只写/配额）。
* UI/剪贴板/热键等也走 Broker，做到**一处管控一处审计**。

# 5) 资源与行为监控（运行时防挖）

* **基线 + 阈值**：若插件在前台无任务时 **CPU > X% 持续 Y 秒**、或 **内存/线程数异常**，自动节流→告警→隔离→终止。
* **网络行为**：记录域名/IP/端口，默认拒绝常见矿池端口（如 3333/4444 等）和已知矿池域名；超出声明清单即阻断 & 记黑。
* **GPU/加速库加载**：发现加载 `opencl.dll/nvopencl.dll/cudart64_xx.dll` 等且未授权→立即隔离。
* **ETW/性能计数器** 辅助：长时间高算力 + 稳定外联特征 → 标记“疑似挖矿”，进入隔离与上报流程。
* **安全响应**：提供“**隔离/卸载/上报**”一键操作与自动化策略（多次触发直接封禁并加入撤销列表）。

# 6) 分发渠道与声誉系统

* 建一个**官方插件仓库/商店**：要求签名、自动静态分析、病毒扫描、人工审核（重点看网络/执行/自启动行为）。
* **声誉分**：下载量、崩溃率、权限请求次数、用户举报，影响“信任等级”。低分插件强制 Restricted 模式。
* 支持“**离线安装但强提示**”：未知来源 → 红色警告 + 明确列出所需能力 + 默认禁用联网/写盘。

# 7) 启动/升级/应急

* **安全模式启动**：只加载 First-party 与上次稳定集；用户可在崩溃后快速复原。
* **热补丁封禁**：你的程序定期拉取撤销列表，对已安装插件做**即时卸载与禁用**。
* **遥测与合规**：所有决策可导出报告（不含敏感内容），便于用户与企业审计。

# 8) Windows 上可直接用的技术点（给实现时对照）

* **受限进程**：`CreateRestrictedToken` + `SetTokenInformation(TokenIntegrityLevel=Low)` + `CreateProcessAsUser`。
* **AppContainer**（首选）：`CreateAppContainerProfile` / `DeriveAppContainerSidFromAppContainerName`，为插件数据目录设 ACL 给该 SID，仅它可读写。
* **Job Object**：`SetInformationJobObject(JobObjectCpuRateControlInformation, JobObjectExtendedLimitInformation, JobObjectLimitInformation)` 设 CPU/内存/进程数/句柄/“KillOnJobClose”。
* **Firewall / WFP**：简单场景用 `INetFwPolicy2` 给沙箱进程/路径建阻断规则；进阶用 **Windows Filtering Platform** 绑定到进程/安全标识做细粒度出站过滤。
* **审计**：ETW（Winsock, HTTP, Process, Thread, ImageLoad）管道，必要时只对沙箱进程开启。

# 9) 你的 settings.json 的落地改造点

* 在主配置里新增：

  * `plugins.trustPolicy`（严格/宽松）
  * `plugins.defaultMode = "restricted"`
  * `plugins.revocationListUrl` / `updateInterval`
* 为每个插件生成/合并一个 **权限页**（从其 `capabilities` 自动渲染），用户可以勾选域名白名单、目录白名单；变更立刻更新沙箱策略。

# 10) 关于“让 DLL 直接改 allActions 列表？”

* **不要暴露**内部容器。in-proc 也只提供**受控的 Add/Update/Remove** 接口（带速率限制与审计）；
* 对 out-of-proc 插件，**只允许通过 IPC 提交候选条目**，由宿主做去重、限频、过滤（防刷屏/钓鱼）。

---

## 快速决策表（给你设默认值）

* 默认插件模式：**Restricted / out-of-proc / AppContainer**
* 默认网络：**禁用**；需显式声明域名白名单并经用户授权
* 默认文件写：**仅私有数据目录**
* 默认 GPU：**禁用**
* CPU 限制：前台 25–40% 上限，后台 10–15% 上限；连续 30s 触发告警
* 未签名或签名无效：**拒绝加载**
* 来自未知来源安装：**红色警告 + 默认禁网禁写 + 受限模式**

---

## 你问“要不要只靠提醒用户从可信来源获取？”

要提醒，但这只是**最后一道软约束**。**硬约束**（沙箱/权限/监控/封禁）必须有，尤其是你允许第三方 DLL 放到同目录即加载的场景。按上面的 TODO 做完，你就能：

1. 即便用户装了恶意插件，也**联网不了、写盘越不了界、吃不了你整机的 CPU/GPU**；
2. 真出现可疑行为，**自动隔离 + 记录证据 + 一键卸载**；
3. 通过签名与仓库，把 90% 风险挡在门外。

需要我把**受限进程 + Job + AppContainer + Broker**的最小可运行样例骨架（C++/Win32）也写出来吗？我可以直接给一份“沙箱化插件进程启动器 + 受控 HTTP Broker”的示例代码，方便你对接现在的 IPluginHost。
