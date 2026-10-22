# C++ 文本模板库选型建议（2026-10-21）

状态：按负责人要求记录本次问答与选型建议。本次只写文档，未引入依赖、替换生成器或运行性能测试。

## 需求与结论

这里的模板指 Go `text/template`、Python Jinja2 一类文本模板引擎：把数据代入模板，支持变量、循环和条件，生成代码、配置或其他文本。

**需要在 C++ 工具中渲染此类模板时，优先评估 inja。** 它采用头文件集成，使用 `nlohmann/json` 承载数据，支持变量、循环、条件、include 和自定义回调，适合 handler、配置等源码生成。[官方说明](https://github.com/pantor/inja)

本建议不要求把现有 Go/Python 生成工具改写为 C++，也不表示已经决定在项目中接入该依赖。

## 候选库比较

| 库 | 能力与集成方式 | 选择条件 |
|---|---|---|
| [inja](https://github.com/pantor/inja) | 纯头文件，依赖 `nlohmann/json`；支持循环、条件、include、自定义函数等 | 通用代码、配置和文本生成的优先评估项 |
| [Jinja2Cpp](https://github.com/jinja2cpp/Jinja2Cpp) | 强调 Jinja2 兼容性，提供宏、过滤器、模板继承与缓存；需要构建和链接 | 已有复杂 Jinja2 模板需要复用时，核对具体语法兼容性及依赖成本 |
| [Mustache](https://github.com/kainjow/Mustache) | 纯头文件，无第三方依赖，支持变量、列表、分区与局部模板 | 模板逻辑简单，希望使用受限语法时 |
| [{fmt}](https://fmt.dev/latest/api/) | 字符串格式化，提供占位替换、格式说明与自定义类型格式化 | 只需拼接一条提示或格式化数值，无需模板文件、循环和条件时 |

inja 受 Jinja 启发，不能据此假设任意 Python Jinja2 模板都能直接迁移；Jinja2Cpp 也应按实际模板验证兼容范围。

## inja 使用示例

下面是展示 API 用法的完整示例，本次未编译或运行。包含路径按使用 `single_include/` 的目录布局书写，实际接入时以依赖提供的包含目录为准。

```cpp
#include <inja/inja.hpp>
#include <iostream>

int main() {
    inja::Environment env;

    // 同一模板解析一次，后续替换数据并重复渲染。
    const auto tmpl = env.parse(
        "{{ name }}: {% for task in tasks %}{{ task }} {% endfor %}");

    nlohmann::json data = {
        {"name", "generator"},
        {"tasks", {"handler", "config"}}
    };
    std::cout << env.render(tmpl, data) << '\n';

    data["name"] = "another_generator";
    std::cout << env.render(tmpl, data) << '\n';
}
```

模板来自文件时，可以用 `env.parse_template(path)` 读取并解析，再复用返回的 `inja::Template` 调用 `env.render(tmpl, data)`。[官方渲染示例](https://github.com/pantor/inja#template-rendering)

## 效率与使用边界

- **开发效率**：用模板表达文本排版、循环和条件；数据校验与业务规则放在生成器代码中，避免模板承担复杂业务决策。
- **重复渲染**：缓存已解析的模板，避免同一模板每次都走字符串解析或文件读取路径；“解析后复用”不等于生成了原生机器码。
- **性能结论**：本次没有本项目的基准数据，不声称任何候选库比 Go/Python 更快。确有吞吐要求时，用相同模板、输入和输出规模分别测量数据构造、模板解析、重复渲染及文件写入成本。

## 与当前仓库的关系

截至本次只读核对，已有工具使用以下模板引擎：

- proto 生成器的 [template.go](../../tools/proto_generator/protogen/internal/utils/template.go) 使用 Go `text/template`；其中 `RenderOrExit` 路径有解析结果缓存。
- 导表器的 [config_gen.py](../../tools/data_table_exporter/core/generators/config_gen.py) 使用 Python Jinja2，依赖声明见 [requirements.txt](../../tools/data_table_exporter/requirements.txt)。生成的是 C++ 源码，并不要求生成器自身也用 C++。

后续若确有 C++ 生成工具需求，再确定依赖版本、构建接入与实际模板验收范围。修改生成逻辑仍应改源模板/生成器并按正规流程重生成，遵守 [AGENTS.md](../../AGENTS.md) 的生成产物边界。

本次未修改客户端可见协议或新增服务器功能；Java 版无对应实现变更，不新增功能对账项。
