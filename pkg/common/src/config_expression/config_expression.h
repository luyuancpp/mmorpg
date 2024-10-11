#pragma once

#include "exprtk.hpp"

template<class T>
inline T myRandom()
{
    return 1;
}

template<class T>
class ExcelExpression
{
public:
    using SymbolTableType = exprtk::symbol_table<T>;
    using ExpressionType = exprtk::expression<T>;
    using ParserType =  exprtk::parser<T>;
    using IntParams =  std::vector<T>;
    using ParamNameList =  std::vector<std::string>;

    bool Init(const ParamNameList& paramsNameList, const std::string& expression)
    {
        intParamList.clear();
        intParamList.resize(paramsNameList.size());
        
        std::size_t nameIndex = 0;
        symbolTable.add_function("random", myRandom);
        for (auto& nane : paramsNameList)        {
            symbolTable.add_variable(nane, intParamList[nameIndex++]);
        }
        
        expression.register_symbol_table(symbolTable);

        if (!parser.compile(expression, expression))        {
            return false;
        }
        
        return true;
    }

    void SetParam(std::size_t index, T value)
    {
        if (index < 0 || index >= intParamList.size())
        {
            return;
        }
        intParamList[index] = value;
    }

    T Value()
    {
        return expression.value();
    }

private:
    IntParams intParamList;
    ExpressionType expression;
    SymbolTableType symbolTable;
    ParserType parser;
};
