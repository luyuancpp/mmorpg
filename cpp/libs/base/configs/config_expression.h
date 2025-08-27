#pragma once

#include "exprtk/exprtk.hpp"
#include "base/core/type_define/type_define.h"

// 自定义随机函数
template<class T>
inline T customRandom()
{
    return static_cast<T>(rand()) / static_cast<T>(RAND_MAX);  // 返回一个介于 0 到 1 之间的随机数
}

template<class T>
class ExcelExpression
{
public:
    using SymbolTableType = exprtk::symbol_table<T>;
    using ExpressionType = exprtk::expression<T>;
    using ParserType = exprtk::parser<T>;
    using ParamListType = std::vector<T>;

    // 初始化函数，接受参数名和表达式字符串
    bool Init(const StringVector& paramNames)
    {
        paramList.clear();
        paramList.resize(paramNames.size());

        // 注册自定义函数
        symbolTable.add_function("random", customRandom<T>);
        
        // 注册变量名到符号表
        if (!RegisterVariables(paramNames)) {
            return false;
        }

        // 将符号表关联到表达式中
        expression.register_symbol_table(symbolTable);

        return true;
    }

    // 设置参数值
    void SetParam(std::size_t index, T value)
    {
        if (index >= paramList.size())
        {
            return;  // 检查索引有效性
        }
        paramList[index] = value;
    }

    void SetParam(const ParamListType& params)
    {
        if (params.size() != paramList.size())
        {
            return;  // 检查索引有效性
        }
        for (std::size_t i = 0; i < paramList.size(); ++i) {
            paramList[i] = params[i];
        }
    }

    // 返回表达式的值
    T Value( const std::string& expressionStr)
    {
        parser.compile(expressionStr, expression);
        return expression.value();
    }

private:
    // 注册变量到符号表
    bool RegisterVariables(const StringVector& paramNames){
        for (std::size_t i = 0; i < paramNames.size(); ++i) {
            symbolTable.add_variable(paramNames[i], paramList[i]);
        }
        return true;
    }

    ParamListType paramList;     // 参数列表
    ExpressionType expression;    // 表达式
    SymbolTableType symbolTable;  // 符号表
    ParserType parser;            // 解析器
};
