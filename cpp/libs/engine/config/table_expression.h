#pragma once

#include "exprtk/exprtk.hpp"
#include "engine/core/type_define/type_define.h"

// Custom random function returning a value in [0, 1]
template<class T>
inline T customRandom()
{
    return static_cast<T>(rand()) / static_cast<T>(RAND_MAX);
}

template<class T>
class ExcelExpression
{
public:
    using SymbolTableType = exprtk::symbol_table<T>;
    using ExpressionType = exprtk::expression<T>;
    using ParserType = exprtk::parser<T>;
    using ParamListType = std::vector<T>;

    bool Init(const StringVector& paramNames)
    {
        paramList.clear();
        paramList.resize(paramNames.size());

        symbolTable.add_function("random", customRandom<T>);
        
        // Register variables into symbol table
        if (!RegisterVariables(paramNames)) {
            return false;
        }

        expression.register_symbol_table(symbolTable);

        return true;
    }

    void SetParam(std::size_t index, T value)
    {
        if (index >= paramList.size())
        {
            return;
        }
        paramList[index] = value;
    }

    void SetParam(const ParamListType& params)
    {
        if (params.size() != paramList.size())
        {
            return;
        }
        for (std::size_t i = 0; i < paramList.size(); ++i) {
            paramList[i] = params[i];
        }
    }

    T Value( const std::string& expressionStr)
    {
        parser.compile(expressionStr, expression);
        return expression.value();
    }

private:
    bool RegisterVariables(const StringVector& paramNames) {
        if (paramNames.size() != paramList.size()) {
            return false;
        }
        for (std::size_t i = 0; i < paramNames.size(); ++i) {
            symbolTable.add_variable(paramNames[i], paramList[i]);
        }
        return true;
    }

    ParamListType paramList;
    ExpressionType expression;
    SymbolTableType symbolTable;
    ParserType parser;
};
