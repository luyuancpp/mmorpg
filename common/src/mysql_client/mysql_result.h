#ifndef COMMON_SRC_MYSQL_CLIENT_MYSQL_RESULT_H_
#define COMMON_SRC_MYSQL_CLIENT_MYSQL_RESULT_H_

#include <vector>

namespace common
{
    using Row = std::vector<const char*>;
    using LengthType = unsigned long;
    using RowLength = std::vector<LengthType>;

    class ResultRow {
    public:
        ResultRow(Row row, RowLength length) : row_{ std::move(row) }, length_{ std::move(length) } {}
        virtual ~ResultRow() {}
        size_t size() const { return row_.size(); }
        const char* const&  operator[](size_t i) const { return row_[i]; }
        const char* const& operator[](int32_t i) { return row_[i]; }
        LengthType length(size_t i) const { return length_[i]; }
    private:
        Row row_;
        RowLength length_;
    };
}//namespace common
#endif//COMMON_SRC_MYSQL_CLIENT_MYSQL_RESULT_H_