/* 【ai生成】沿用上次词法作业的实现与注释；原有分工标注保留。 */
/*
 * 【AI生成：符号表接口框架及实现初稿】
 * 核心数据结构分工负责人：宋坤远（2411241）。
 * 当前实现及注释由 AI 生成，姓名表示小组任务归属，供组员核实。
 */
#ifndef SYSY_SYMBOL_TABLE_H
#define SYSY_SYMBOL_TABLE_H

#include <stddef.h>
#include <stdio.h>

/*
 * 查找或登记一个标识符，返回从 0 开始的稳定下标。
 * 已存在：增加出现次数，返回旧下标；不存在：复制词素并创建新项。
 * name 必须是以 NUL 结束的标识符；line/column 是本次出现的起始位置。
 * 表会复制 name，调用者无需延长 yytext 的生命周期。
 * 当前仅按词素去重，不区分作用域、声明和类型。
 */
size_t symbol_intern(const char *name, size_t line, size_t column);
/* 按首次出现顺序输出表项，编号与 ID 单词中的 symbol_id 一致。 */
void symbol_table_print(FILE *out);
/* 释放各词素与两个动态数组，并将模块状态恢复为空表。 */
void symbol_table_destroy(void);

#endif
