/* 【ai生成】语法级声明索引框架及实现初稿。
 * 【核心算法·分工负责人：宋坤远（2411241）】从 AST 提取声明与作用域，不改写词法表。 */
#include "decl_index.h"

#include <stdlib.h>

static void *grow(void *data, size_t *capacity, size_t element_size) {
    /* 追加数组翻倍扩容，先检查乘法上界，再以临时指针接收 realloc。 */
    if (*capacity <= SIZE_MAX / element_size / 2 && SIZE_MAX / element_size >= 16) {
        size_t n = *capacity ? *capacity * 2 : 16;
        void *new_data = realloc(data, n * element_size);
        if (new_data) { *capacity = n; return new_data; }
    }
    fputs("fatal: cannot allocate declaration index\n", stderr);
    exit(2);
}

static size_t add_scope(DeclIndex *idx, size_t parent, const char *label, const Ast *owner) {
    if (idx->scope_count == idx->scope_capacity)
        idx->scopes = grow(idx->scopes, &idx->scope_capacity, sizeof *idx->scopes);
    size_t id = idx->scope_count++;
    idx->scopes[id] = (ScopeEntry){parent, label, owner};
    return id;
}

static void add_decl(DeclIndex *idx, DeclEntry entry) {
    if (idx->count == idx->capacity)
        idx->entries = grow(idx->entries, &idx->capacity, sizeof *idx->entries);
    idx->entries[idx->count++] = entry;
}

/* 先序保证 parent 的 scope 已确定。函数名登记在外围作用域，形参与函数体共享
 * 新的函数作用域；内部块另建作用域。同一个 lex_id 可以对应多个 DeclEntry。
 * 这里只保留声明事实，不检查同名重定义、绑定引用、类型或常量表达式。 */
void decl_index_build(DeclIndex *idx, Ast *root) {
    add_scope(idx, 0, "global", root);
    size_t depth = 0;
    for (Ast *n = root; n; n = ast_next_preorder(n, &depth)) {
        n->scope = n->parent ? n->parent->scope : 0;
        if (n->kind == AST_FUNC) {
            const Ast *type = n->first, *name = type->next, *params = name->next;
            add_decl(idx, (DeclEntry){name,type,n,"function",n->scope,0,ast_child_count(params)});
            n->scope = add_scope(idx, n->scope, name->text, n);
        } else if (n->kind == AST_BLOCK && n->parent->kind != AST_FUNC) {
            n->scope = add_scope(idx, n->scope, "block", n);
        } else if (n->kind == AST_PARAM) {
            const Ast *type = n->first, *name = type->next;
            add_decl(idx, (DeclEntry){name,type,n,"parameter",n->scope,ast_child_count(name->next),0});
        } else if (n->kind == AST_DEF) {
            const Ast *name = n->first, *type = n->parent->first;
            const char *role = n->parent->kind == AST_CONST_DECL ? "const" : "variable";
            add_decl(idx, (DeclEntry){name,type,n,role,n->scope,ast_child_count(name->next),0});
        }
    }
}

void decl_index_print(FILE *out, const DeclIndex *idx) {
    fputs("\n# SCOPES\nSCOPE\tPARENT\tNAME\tAST_NODE\n", out);
    for (size_t i = 0; i < idx->scope_count; ++i) {
        const ScopeEntry *s = &idx->scopes[i];
        fprintf(out, "%zu\t", i);
        if (i) fprintf(out, "%zu", s->parent); else fputc('-', out);
        fprintf(out, "\t%s\t%zu\n", s->label, s->owner->number);
    }
    fputs("\n# DECLARATIONS\nNAME\tLEX_ID\tKIND\tTYPE\tSCOPE\tRANK\tARITY\tLINE\tCOLUMN\tAST_NODE\n", out);
    for (size_t i = 0; i < idx->count; ++i) {
        const DeclEntry *d = &idx->entries[i];
        fprintf(out, "%s\t%zu\t%s\t%s\t%zu\t%zu\t", d->name->text, d->name->symbol_id,
                d->role, d->type->text, d->scope, d->rank);
        if (d->owner->kind == AST_FUNC) fprintf(out, "%zu", d->arity); else fputc('-', out);
        fprintf(out, "\t%zu\t%zu\t%zu\n", d->name->line, d->name->column, d->owner->number);
    }
}

void decl_index_destroy(DeclIndex *idx) {
    free(idx->entries);
    free(idx->scopes);
    *idx = (DeclIndex){0};
}
