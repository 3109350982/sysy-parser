/* 【ai生成】Bison 与 Flex 接口框架。
 * 公共接口按林科融（2413121）、宋坤远（2411241）共同分工标注。 */
#ifndef SYSY_PARSE_CONTEXT_H
#define SYSY_PARSE_CONTEXT_H

#include "ast.h"

/* 保持与旧词法器的 size_t 行列一致，避免将长文件位置截断为 int。 */
typedef struct {
    size_t first_line, first_column, last_line, last_column;
} SourceSpan;

typedef struct {
    AstArena arena;            /* 拥有所有节点，成功和失败统一释放。 */
    Ast *root;                 /* 由开始符号归约后设置。 */
    const char *filename;      /* 借用 main 的输入文件名，用于诊断。 */
    int strict;                /* 禁止课件省略 int 的 main() 兼容写法。 */
    size_t syntax_errors;
} ParseContext;

#endif
