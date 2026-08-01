# 通用 YAML 可视化浏览器设计方案

## 1. 产品目标

开发一个独立的 Go 桌面 GUI 工具，用于浏览和搜索任意结构化 YAML 文件。

工具只依赖 YAML 本身的结构，不依赖任何业务字段、固定目录、固定文件名或项目 schema。新增任意字段、层级、数组元素或自定义 tag 时，不需要修改工具代码。

首个版本定位为只读浏览器，核心能力包括：

- 解析单文档和多文档 YAML。
- 按 Mapping、Sequence、Scalar、Alias 层级展示。
- 将机器字段名转换为人类可读的显示名称。
- 对字段名、路径、值和 YAML 元数据进行模糊搜索。
- 查看节点的完整值、类型、路径、源文件位置和元数据。
- 打开、重新加载和拖拽导入 YAML 文件。

GUI 推荐使用 Fyne。它提供 Go 原生跨平台桌面能力和按需加载的多级 Tree 组件，适合展示大量层级节点。[Fyne Tree 官方文档](https://docs.fyne.io/collection/tree/)

YAML 解析使用 go.yaml.in/yaml/v3 的 yaml.Node，不将内容反序列化到固定 Go struct。该 API 提供 YAML 节点类型、标签、锚点、注释、行列位置和多文档 Decoder 等信息。[yaml.v3 API 文档](https://pkg.go.dev/go.yaml.in/yaml/v3)

## 2. 总体架构

~~~text
GUI
├── Toolbar / File Picker
├── Search Box
├── Hierarchy Tree
└── Node Inspector

Application State
├── Current Documents
├── Selected Node
├── Search Query
└── Filtered Tree State

Domain Model
├── YAML Document
├── Generic YAML Node
├── Display Name Formatter
└── Search Index

Infrastructure
├── YAML Decoder
├── File Loader
└── Fyne Bootstrap
~~~

建议目录结构：

~~~text
cmd/yamlviewer/main.go

internal/
  yamlmodel/
    document.go
    node.go
    decoder.go
    path.go

  display/
    humanize.go
    formatter.go

  search/
    normalize.go
    index.go
    matcher.go
    scorer.go

  appstate/
    state.go
    actions.go

  ui/
    window.go
    toolbar.go
    tree.go
    inspector.go
    dialogs.go

  fileio/
    loader.go
    recent.go
~~~

## 3. 通用 YAML 数据模型

不要使用 map[string]any 作为核心模型，因为它无法可靠保留节点顺序、重复键和 YAML 元数据。

内部模型应从 yaml.Node 转换为稳定的通用树节点：

~~~go
type NodeKind string

const (
    MappingNode NodeKind = "mapping"
    SequenceNode NodeKind = "sequence"
    ScalarNode NodeKind = "scalar"
    AliasNode NodeKind = "alias"
)

type Node struct {
    ID        string
    Kind      NodeKind
    Key       string
    Index     int
    Value     string
    Path      string
    Tag       string
    Style     string
    Anchor    string
    Alias     string
    Line      int
    Column    int
    Comments  Comments
    Children  []*Node
}
~~~

处理规则：

- Mapping 保留 YAML 源文件中的键顺序。
- Sequence 使用数组下标作为路径段。
- 重复键不合并，分别显示，并在详情面板中标记。
- 非字符串 Mapping key 使用格式化后的 YAML 文本作为显示名称。
- 空对象、空数组、空文档和 null 都作为合法节点展示。
- 多文档 YAML 在根节点下显示 Document 1、Document 2 等虚拟节点。
- Alias 不递归展开，避免循环引用；显示 Alias 指向的 Anchor 名称。
- 自定义 YAML tag 只作为元数据展示，不执行模板、环境变量、脚本或网络请求。
- 节点 ID 使用文档编号加子节点序号生成，不能只使用人类可读 path，因为重复键可能产生相同 path。

## 4. 字段名称显示格式化

### 4.1 显示目标

原始字段名只用于数据定位和复制；树节点中使用单独的 human-readable label。

| 原始字段名 | 显示名称 |
| --- | --- |
| tick_rate | Tick Rate |
| tick-rate | Tick Rate |
| tickRate | Tick Rate |
| TICK_RATE | TICK RATE |
| HTTPServer | HTTP Server |
| max2Dashes | Max 2 Dashes |
| already Humanized | Already Humanized |

### 4.2 Humanize 算法

HumanizeKey(raw string) string 必须是纯函数，不依赖任何业务字段表或人工配置。

处理顺序：

1. 将下划线、短横线和连续空白转换成分隔符。
2. 在 lowercase 到 Uppercase 之间插入分隔符：tickRate 变成 tick Rate。
3. 在缩写与普通单词边界插入分隔符：HTTPServer 变成 HTTP Server。
4. 在字母与数字边界插入分隔符：max2Dashes 变成 max 2 Dashes。
5. 清理重复分隔符和首尾空白。
6. 普通单词首字母大写。
7. 全大写缩写保持原样，不引入项目专用缩写字典。

原始 key、格式化后的 label 和标准化后的搜索文本必须同时保存在节点或索引中，避免显示层和搜索层互相依赖。

## 5. 统一模糊搜索设计

### 5.1 等价写法

以下查询应视为同一组搜索意图：

~~~text
tick_rate
Tick Rate
tick-rate
tickRate
TICK RATE
tickrate
~~~

搜索必须统一处理：

- 大小写差异。
- 下划线、短横线、空格和连续分隔符差异。
- snake_case、kebab-case、camelCase、PascalCase 和全大写写法。
- 查询中有空格、没有空格或使用下划线的情况。
- Unicode 字符的大小写转换。

### 5.2 标准化表示

为每个可搜索字段生成两种标准化形式：

~~~text
segmented: tick rate
compact:   tickrate
~~~

标准化流程：

1. 识别 snake_case、kebab-case、camelCase、PascalCase 和缩写边界。
2. 将下划线、短横线、空格、点号和其他非字母数字字符作为分隔符。
3. 使用 Unicode 大小写折叠。
4. 合并连续空白。
5. 生成带空格的 segmented 形式。
6. 移除分隔符生成 compact 形式。

因此：

~~~text
tick_rate  → segmented: tick rate, compact: tickrate
tickRate   → segmented: tick rate, compact: tickrate
Tick Rate  → segmented: tick rate, compact: tickrate
~~~

值字段也参与标准化，但必须保留原始值用于展示。对于值中的标点和数字，搜索层只做宽松匹配，不修改详情面板中的原始文本。

### 5.3 匹配算法

查询拆分成多个 token，并按 AND 关系匹配。每个 token 支持以下层级：

1. 完全匹配。
2. 前缀匹配。
3. 连续子串匹配。
4. 字符顺序匹配，即 fuzzy subsequence。

匹配同时尝试 segmented 和 compact 形式。例如输入 tick rate 时，除了按两个 token 匹配，也使用 tickrate 进行紧凑匹配，以便命中 tick_rate 和 tickRate。

建议评分权重：

~~~text
字段名精确匹配       最高
字段名前缀匹配       很高
完整 path 匹配       高
字段名模糊匹配       中高
标量值匹配           中
tag / anchor 匹配    低
comment 匹配         低
~~~

同分结果保持 YAML 原始顺序，确保搜索结果稳定。

### 5.4 搜索结果树

- 查询为空时显示完整树。
- 查询有结果时只保留匹配节点及其全部祖先。
- 自动展开包含匹配结果的路径。
- 显示匹配节点数量。
- 选中结果后，树滚动到对应节点，详情面板显示完整信息。
- 清空查询后恢复用户之前的展开状态。
- 使用 100～150ms 防抖，避免每次按键都立即刷新整棵树。
- 树节点展示 humanized label，同时保留 raw key 的轻量辅助信息，避免不同原始 key 被误认为相同。

建议树节点格式：

~~~text
Tick Rate: 30
~~~

详情面板中明确区分：

~~~text
Display name: Tick Rate
Raw key:      tick_rate
Path:         /settings/tick_rate
~~~

## 6. 主界面设计

~~~text
┌──────────────────────────────────────────────┐
│ Open  Reload       Search                    │
├──────────────────┬───────────────────────────┤
│ YAML hierarchy   │ Selected node details     │
│                  │                           │
│ root             │ Display name              │
│ ├─ Section       │ Raw key                   │
│ │  ├─ Tick Rate  │ Full path                 │
│ │  └─ Items      │ Type / Value              │
│ └─ Another       │ Tag / Anchor / Alias      │
│                  │ Line / Column             │
├──────────────────┴───────────────────────────┤
│ file name | document count | match count     │
└──────────────────────────────────────────────┘
~~~

树节点显示规则：

- Mapping：显示 humanized key 和子节点数量。
- Sequence：显示 [0]、[1] 等下标和节点摘要。
- Scalar：显示 humanized key、值摘要和 YAML 类型。
- Alias：显示别名名称。
- 长字符串截断显示，完整值放入详情面板。
- 搜索命中节点使用选中或高亮状态，不改变原始值。

详情区域显示：

- Display name。
- Raw key。
- 完整 YAML path。
- 节点类型。
- 完整值。
- YAML tag 和 scalar style。
- Anchor / Alias 信息。
- 源文件行号和列号。
- Head comment、line comment、foot comment。
- Mapping / Sequence 子节点数量。
- 复制 raw key、path、值和节点信息的按钮。

## 7. 文件加载与错误处理

支持：

- 文件选择器打开任意文本文件。
- 命令行参数直接打开文件：

  ~~~bash
  yamlviewer path/to/file.yaml
  ~~~

- 拖拽文件到窗口。
- 重新加载当前文件。
- 最近打开文件列表。
- Ctrl/Cmd + O 打开文件。
- Ctrl/Cmd + F 聚焦搜索框。
- Esc 清空搜索。

加载流程：

~~~text
打开文件
  ↓
后台读取和 YAML 解码
  ↓
构建通用 Node 模型
  ↓
生成 display label 和 search index
  ↓
切换到新的文档状态
  ↓
刷新 Tree 和 Inspector
~~~

解析失败时：

- 保留当前已经成功打开的文档。
- 显示错误面板。
- 显示解析器提供的行号和列号。
- 提供复制完整错误信息的操作。
- 错误文档不进入搜索索引。
- 空文件显示明确的 Empty Document 状态。

读取、解析和索引建立放入后台 goroutine；GUI 更新回到 Fyne UI 线程。每个加载和搜索任务使用递增 generation ID，丢弃过期结果，避免快速切换文件或输入搜索时发生状态覆盖。

## 8. 可扩展性原则

为了确保未来新增条目无需修改工具：

- 不使用固定字段白名单。
- 不使用业务专用 struct。
- 所有未知 YAML tag 都按照普通节点处理。
- 所有 Mapping、Sequence 和 Scalar 都由统一递归遍历处理。
- 显示格式化只基于字符串边界规则，不维护业务字段字典。
- 搜索索引只依赖通用节点属性。
- UI 通过 NodeKind 决定渲染方式，不通过 key 名决定渲染方式。
- Parser、Model、Search 和 UI 之间使用接口隔离。
- 后续增加编辑、schema 校验或自定义渲染时，不修改基础解析模型。

## 9. 测试计划

### YAML 解析测试

- 嵌套 Mapping。
- 嵌套 Sequence。
- Mapping 与 Sequence 混合。
- 字符串、整数、浮点数、布尔值和 null。
- 空对象和空数组。
- 多文档 YAML。
- 重复键。
- 非字符串 key。
- 自定义 tag。
- Anchor 和 Alias。
- 注释。
- 多语言和 Unicode。
- 非法缩进、未闭合字符串和非法文档分隔符。

### 字段显示测试

~~~text
tick_rate          → Tick Rate
tick-rate          → Tick Rate
tickRate           → Tick Rate
HTTPServer         → HTTP Server
max2Dashes         → Max 2 Dashes
already Humanized  → Already Humanized
~~~

验证 humanized label 不会覆盖 raw key 和原始 path。

### 搜索测试

验证以下查询都能命中同一字段：

~~~text
tick_rate
Tick Rate
tick-rate
tickRate
tickrate
TICK RATE
~~~

另外覆盖：

- 完全匹配、前缀匹配、子串匹配和字符顺序匹配。
- 多 token AND 查询。
- 大小写混合。
- Unicode 查询。
- path、值、tag、anchor 和 comment 查询。
- 父节点保留。
- 无结果状态。
- 重复 key 节点。
- 多文档节点。
- 搜索结果排序稳定性。

### 构建和性能检查

~~~bash
go test ./...
go vet ./...
go build ./cmd/yamlviewer
~~~

增加 benchmark 测试：

- 大量嵌套节点。
- 深层 path。
- 大量重复结构。
- 大量长文本值。
- 索引建立时间。
- 单次搜索耗时。

目标是解析或搜索期间界面保持可操作；具体阈值应在目标平台上使用基准文件测量，而不是把固定硬编码限制写进业务逻辑。

## 10. 版本边界

首版实现：

- YAML 解析。
- 多层级浏览。
- humanized 字段显示。
- 统一格式模糊搜索。
- 节点详情查看。
- 文件打开、拖拽和重新加载。

首版暂不实现：

- 修改 YAML。
- 保存 YAML。
- schema 校验。
- 自动格式化。
- 业务字段特殊渲染。
- YAML 合并。
- 插件系统。
- 实时文件监听。

后续增加编辑功能时，应在当前通用 Node 模型上增加独立的编辑状态和序列化层，不改变显示名称格式化和搜索接口。

## 11. 验收标准

- 任意新增 YAML key 不需要改代码即可显示。
- tick_rate 在树中显示为 Tick Rate。
- tick_rate、Tick Rate、tickRate、tick-rate 和 tickrate 可以互相搜索命中。
- 显示名称变化不会影响 raw key、path、复制内容和 YAML 原始值。
- 重复 key 不丢失。
- 多文档、Anchor、Alias、注释和自定义 tag 不导致崩溃或无限递归。
- 非法 YAML 能显示包含行号和列号的错误。
- 搜索结果只保留匹配节点及其祖先。
- 搜索清空后恢复完整树和之前的展开状态。
- go test ./...、go vet ./... 和 go build ./cmd/yamlviewer 通过。
