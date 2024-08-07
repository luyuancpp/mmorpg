python 读excel表跳过前8行, 以//分组,没行有唯一id

把每组文件生成proto文件，每行的唯一id 是proto的枚举变量，每组生成到自己组名的proto文件里面


使用一个临时文件存现有的id,如果名字变了id不变，后面的id叠加


使用保留字段：

Protocol Buffers支持保留字段来防止未来使用的字段ID被重复使用。可以在Proto文件中使用reserved关键字来保留未来可能需要的ID范围。
protobuf
复制代码
message MyMessage {
    // Existing fields
    optional int32 existing_field = 1;

    // Reserved range for future use
    reserved 2, 3, 4 to 10;
    
    // New fields can be added after the reserved range
    optional int32 new_field = 11;
}
这样做可以确保在添加新字段时，不会意外地分配到现有字段的ID。
文档化和版本控制：

维护Proto文件时，文档化和版本控制是非常重要的。确保团队中的每个人都了解字段ID的分配策略，并记录每个字段ID的使用情况和保留策略。
避免删除字段或重构ID：

在修改Proto文件时，尽量避免删除字段或者重新分配现有字段的ID。这可能会导致与现有数据兼容性问题。
使用Proto文件管理工具：

考虑使用专门的Proto文件管理工具，例如protoc编译器和相关插件，它们可以帮助管理Proto文件的版本和兼容性问题。