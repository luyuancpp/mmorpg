#ifndef COMMON_SRC_MYSQL_CLIENT_MYSQL_RESULT_H_
#define COMMON_SRC_MYSQL_CLIENT_MYSQL_RESULT_H_

#include <vector>

namespace common
{
    using Length = unsigned long;
    using RowLength = Length *;

    class ResultRow {
    public:
        ResultRow(MYSQL_ROW row, RowLength length, MYSQL_RES* res, size_t size)
            : row_{ row }, length_{ length }, res_(res),size_(size)  {}
        ~ResultRow() { mysql_free_result(res_); }
        size_t size() const { return size_; }
        const char* const&  operator[](size_t i) const { return row_[i]; }
        Length length(size_t i) const { return length_[i]; }
    private:
        MYSQL_ROW row_;
        RowLength length_;
        MYSQL_RES* res_;
        size_t size_{0};
    };
}//namespace common
#endif//COMMON_SRC_MYSQL_CLIENT_MYSQL_RESULT_H_