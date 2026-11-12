/* 【ai生成】AST 公共接口框架及中文注释。
 * 【核心数据结构·分工负责人：宋坤远（2411241）】
 * 姓名按小组分工标注，实际完成人及修改内容请组员据实核对。 */
#ifndef SYSY_AST_H
#define SYSY_AST_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* 只保留有意义的语法结构；分号、逗号、括号等不单独成为节点。 */
typedef enum {
    AST_UNIT, AST_CONST_DECL, AST_VAR_DECL, AST_DEF, AST_TYPE, AST_NAME,
    AST_DIMS, AST_DIM, AST_INIT_LIST, AST_FUNC, AST_PARAMS, AST_PARAM,
    AST_BLOCK, AST_ASSIGN, AST_EXPR_STMT, AST_EMPTY, AST_IF, AST_WHILE,
    AST_BREAK, AST_CONTINUE, AST_RETURN, AST_LVALUE, AST_CALL,
    AST_INT, AST_FLOAT, AST_UNARY, AST_BINARY
} AstKind;

typedef struct Ast Ast;
struct Ast {
    AstKind kind;
    size_t line, column;       /* 节点起点；运算节点指向操作符本身。 */
    size_t number;             /* 输出前按先序遍历分配；根节点为 0。 */
    size_t scope;              /* 声明索引遍历时分配，用于继承外围作用域。 */
    size_t symbol_id;          /* 仅 AST_NAME 使用：来自 Flex 词素表。 */
    union { uint32_t integer; float real; } value; /* 常量的数值属性。 */
    Ast *first, *last;          /* 首子节点、尾子节点；节点的孩子数不限。 */
    Ast *next, *parent;         /* 下一兄弟、父节点；树遍历无需递归。 */
    Ast *allocation_next;      /* 分配链独立于树边：释放失败解析产生的孤立节点。 */
    char text[];               /* 一次分配保存节点及独立词素副本，始终以 NUL 结尾。 */
};

/* 语法动作先以头尾指针收集列表，追加 O(1)，再挂到父节点。 */
typedef struct { Ast *first, *last; } AstList;
typedef struct { Ast *allocated; } AstArena;

Ast *ast_new(AstArena *arena, AstKind kind, size_t line, size_t column, const char *text);
AstList ast_append(AstList list, Ast *node);
void ast_adopt(Ast *parent, AstList list);
void ast_add(Ast *parent, Ast *node);
size_t ast_child_count(const Ast *node);
Ast *ast_next_preorder(Ast *node, size_t *depth);
void ast_print(FILE *out, Ast *root, int numbered);
void ast_destroy(AstArena *arena);

#endif
