#ifndef __CONFIG_EXPRESSION_
#define __CONFIG_EXPRESSION_

#include "Exprtk/exprtk.hpp"

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
    using param_vector_type =  std::vector<T>;
    using value_list_type =  std::vector<std::string>;

    bool Init(const value_list_type& value_name_list, const std::string& expression_string)
    {
        param_vec_.clear();
        param_vec_.resize(value_name_list.size());
        std::size_t value_index = 0;
        symbol_table_.add_function("random", randu);
        for (auto& value_name : value_name_list)
        {
            symbol_table_.add_variable(value_name, param_vec_[value_index++]);
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
        if (vec_index < 0 || vec_index >= param_vec_.size())
        {
            return;
        }
        param_vec_[vec_index] = value;
    }

    T Value()
    {
        return expression_.value();
    }

private:
    param_vector_type param_vec_;
    expression_type expression_;
    symbol_table_type symbol_table_;
    parser_type parser_;
};

#endif // !__CONFIG_EXPRESSION_
