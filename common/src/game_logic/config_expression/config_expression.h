#pragma once

#include "src/game_logic/exprtk/exprtk.hpp"

template<class T>
inline T randu()
{
    return 1;
}

template<class T>
class ExcelExpression
{
public:
    using symbol_table_type = exprtk::symbol_table<T>;
    using expression_type = exprtk::expression<T>;
    using parser_type =  exprtk::parser<T>;
    using int_params =  std::vector<T>;
    using value_list_type =  std::vector<std::string>;

    bool Init(const value_list_type& params_name, const std::string& expression_string)
    {
        iparams_.clear();
        iparams_.resize(params_name.size());
        std::size_t value_index = 0;
        symbol_table_.add_function("random", randu);
        for (auto& nane : params_name)
        {
            symbol_table_.add_variable(nane, iparams_[value_index++]);
        }
        expression_.register_symbol_table(symbol_table_);

        if (!parser_.compile(expression_string, expression_))
        {
            return false;
        }
        return true;
    }

    void SetParam(std::size_t vec_index, T value)
    {
        if (vec_index < 0 || vec_index >= iparams_.size())
        {
            return;
        }
        iparams_[vec_index] = value;
    }

    T Value()
    {
        return expression_.value();
    }

private:
    int_params iparams_;
    expression_type expression_;
    symbol_table_type symbol_table_;
    parser_type parser_;
};
