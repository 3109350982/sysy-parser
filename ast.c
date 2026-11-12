/* 【ai生成】AST 构造、遍历与释放框架及实现初稿。
 * 【核心算法·分工负责人：宋坤远（2411241）】
 * 每个节点只在分配链中出现一次，树链接与内存所有权分开维护。 */
#include "ast.h"

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

Ast *ast_new(AstArena *arena, AstKind kind, size_t line, size_t column, const char *text) {
    /* 词素不能借用 Flex 的 yytext。分配柔性数组并把结尾 NUL 一并复制。 */
    size_t length = text ? strlen(text) : 0;
    Ast *node = length < SIZE_MAX - sizeof *node
        ? calloc(1, sizeof *node + length + 1) : NULL;
    if (!node) { fputs("fatal: cannot allocate AST node\n", stderr); exit(2); }
    node->kind = kind;
    node->line = line;
    node->column = column;
    if (text) memcpy(node->text, text, length + 1);
    /* 即使之后 Bison 因错误丢弃语义值，此节点仍可从分配链找到。 */
    node->allocation_next = arena->allocated;
    arena->allocated = node;
    return node;
}

/* 【核心算法：多叉树列表拼接·分工负责人：宋坤远（2411241）】 */
AstList ast_append(AstList list, Ast *node) {
    if (!node) return list;
    assert(!node->parent && !node->next);
    if (list.last) list.last->next = node;
    else list.first = node;
    list.last = node;
    return list;
}

void ast_adopt(Ast *parent, AstList list) {
    if (!list.first) return;
    assert(list.last && !list.last->next);
    if (parent->last) parent->last->next = list.first;
    else parent->first = list.first;
    parent->last = list.last;
    /* 每个孩子只在挂接时设置一次 parent，便于无递归遍历及作用域继承。 */
    for (Ast *p = list.first; p; p = p->next) {
        assert(!p->parent && p != parent);
        p->parent = parent;
    }
}

void ast_add(Ast *parent, Ast *node) {
    if (node) ast_adopt(parent, (AstList){node, node});
}

size_t ast_child_count(const Ast *node) {
    size_t count = 0;
    for (const Ast *p = node->first; p; p = p->next) ++count;
    return count;
}

/* 【核心算法：先序遍历·分工负责人：宋坤远（2411241）】
 * 有孩子先下探；没有孩子则沿父链回到具有下一兄弟的节点。
 * 每条树边至多上下各经过一次，因此总遍历时间 O(n)，辅助空间 O(1)。 */
Ast *ast_next_preorder(Ast *node, size_t *depth) {
    if (node->first) { ++*depth; return node->first; }
    while (node->parent && !node->next) { node = node->parent; --*depth; }
    return node->next;
}

static const char *kind_name(AstKind kind) {
    static const char *const names[] = {
        "CompUnit", "ConstDecl", "VarDecl", "Declarator", "Type", "Name",
        "Dimensions", "Dimension", "InitList", "Function", "Params", "Param",
        "Block", "Assign", "ExprStmt", "EmptyStmt", "If", "While",
        "Break", "Continue", "Return", "LValue", "Call",
        "IntLiteral", "FloatLiteral", "Unary", "Binary"
    };
    _Static_assert(sizeof names / sizeof names[0] == AST_BINARY + 1, "missing AST label");
    return names[kind];
}

void ast_print(FILE *out, Ast *root, int numbered) {
    size_t depth = 0, number = 0;
    for (Ast *n = root; n; n = ast_next_preorder(n, &depth)) n->number = number++;
    depth = 0;
    for (Ast *n = root; n; n = ast_next_preorder(n, &depth)) {
        if (!numbered) for (size_t i = 0; i < depth; ++i) fputs("  ", out);
        fprintf(out, "%zu %s", n->number, kind_name(n->kind));
        if (*n->text) fprintf(out, " %s", n->text);
        if (n->kind == AST_NAME) fprintf(out, " [lex=%zu]", n->symbol_id);
        if (n->kind == AST_INT) fprintf(out, " [value=%" PRIu32 "]", n->value.integer);
        if (n->kind == AST_FLOAT) fprintf(out, " [value=%.9g]", (double)n->value.real);
        fprintf(out, " @%zu:%zu", n->line, n->column);
        if (numbered) {
            fputs(" Children:", out);
            for (const Ast *p = n->first; p; p = p->next) fprintf(out, " %zu", p->number);
        }
        fputc('\n', out);
    }
}

void ast_destroy(AstArena *arena) {
    /* 不沿树递归释放：语法出错也能回收全部节点，深表达式不压 C 栈。 */
    while (arena->allocated) {
        Ast *node = arena->allocated;
        arena->allocated = node->allocation_next;
        free(node);
    }
}
