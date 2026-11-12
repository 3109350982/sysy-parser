/* 【ai生成】声明表接口；核心数据结构分工负责人：宋坤远（2411241）。 */
#ifndef SYSY_DECL_INDEX_H
#define SYSY_DECL_INDEX_H

#include "ast.h"

/* 语法级的声明记录，不把同名词素误当成同一条声明。
 * 指针借用 AST 节点；索引释放后才能释放 AST。 */
typedef struct {
    const Ast *name, *type, *owner;
    const char *role;
    size_t scope, rank, arity;
} DeclEntry;

typedef struct {
    size_t parent;             /* 全局作用域无父项，输出 '-'。 */
    const char *label;
    const Ast *owner;
} ScopeEntry;

typedef struct {
    DeclEntry *entries;
    ScopeEntry *scopes;
    size_t count, capacity, scope_count, scope_capacity;
} DeclIndex;

void decl_index_build(DeclIndex *index, Ast *root);
void decl_index_print(FILE *out, const DeclIndex *index);
void decl_index_destroy(DeclIndex *index);
#endif
