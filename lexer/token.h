/* 【ai生成】沿用上次词法作业的实现与注释；原有分工标注保留。 */
/*
 * 【AI生成：公共接口与程序框架】
 * 代码来源：本文件的实现初稿及详细注释由 AI 生成。
 * 分工负责人：林科融（2413121）、宋坤远（2411241）共同负责接口设计。
 * 姓名表示小组任务归属，实际完成情况应由组员核实。
 *
 * 本文件约定扫描器与驱动程序之间的数据格式：
 *   yylex() 返回单词类别，同时将词素、位置和属性写入全局 token。
 *   main.c 消费当前 token 后，才能再次调用 yylex()。
 */
#ifndef SYSY_TOKEN_H
#define SYSY_TOKEN_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
 * 【核心数据结构：单词类别；分工负责人：林科融（2413121）】
 * 0 留给 Lex 的文件结束标志，因此具体单词从 256 开始编号。
 * ERROR 也是非零单词，驱动程序能输出错误后继续扫描，直至真正读到 EOF。
 * 枚举值只表示类别，不保存词素内容；词素和属性放在下方的 Token 中。
 */
typedef enum {
    /* 标识符、整数常量、浮点常量分别具有不同的属性类型。 */
    TOK_ERROR = 256, TOK_ID, TOK_NUMBER, TOK_FLOAT_NUMBER,
    /* 保留字独立成类，不能作为普通标识符写入符号表。 */
    TOK_CONST, TOK_INT, TOK_FLOAT, TOK_VOID, TOK_IF, TOK_ELSE,
    TOK_WHILE, TOK_BREAK, TOK_CONTINUE, TOK_RETURN,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_ASSIGN, TOK_EQ, TOK_NE, TOK_LT, TOK_LE, TOK_GT, TOK_GE,
    TOK_NOT, TOK_AND, TOK_OR,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_LBRACKET, TOK_RBRACKET, TOK_COMMA, TOK_SEMICOLON
} TokenKind;

/*
 * 【核心数据结构：单词及属性；分工负责人：两位组员共同负责】
 * 使用带类别标记的联合体：根据 kind 决定读取 attribute 的哪个成员。
 * 同一单词不会同时使用三种属性，union 可以复用这部分存储空间。
 */
typedef struct {
    TokenKind kind;      /* 当前单词所属类别，也是联合体属性的判别依据。 */
    const char *lexeme;  /* 借用 yytext，仅在下一次 yylex() 调用前有效。 */
    size_t length;       /* 显式保存字节数，错误词素中含 NUL 时也能完整输出。 */
    size_t line, column; /* 单词起始位置，从 1 开始；列数按源文件字节计。 */
    union {
        size_t symbol_id; /* ID：符号表的稳定下标，表扩容后仍有效。 */
        uint32_t integer; /* NUMBER：转换后的非负整数值，符号另作单词。 */
        float real;      /* FLOAT_NUMBER：转换后的单精度浮点数。 */
    } attribute;
} Token;

/* extern 只声明共享变量；实际存储由 lexer.l 中的定义提供。 */
extern Token token;
extern const char *source_name;
extern size_t lexical_errors;
extern FILE *yyin;

int yylex(void);                  /* Flex 生成：返回单词类别，EOF 时返回 0。 */
int yylex_destroy(void);          /* Flex 生成：释放扫描缓冲区等内部资源。 */
const char *token_name(TokenKind kind); /* 将类别转换为输出列中的名称。 */
TokenKind keyword_kind(const char *text); /* 查保留字；查不到则返回 ID。 */
void print_escaped(FILE *out, const char *text, size_t length); /* 转义输出词素。 */

#endif
