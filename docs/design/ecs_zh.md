# ECS 设计规则（灵感来源：守望先锋）

ECS 框架遵循以下规则：System 无状态；Component 不包含函数；System 之间不得直接调用（允许共享工具函数）；System 迭代方式为"主 Component + 其兄弟 Component"；允许单例 Component。
