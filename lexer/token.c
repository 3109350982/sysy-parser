/* 【ai生成】沿用上次词法作业的实现与注释；原有分工标注保留。 */
/*
 * 【AI生成：单词辅助模块框架及实现初稿】
 * 当前代码及中文注释由 AI 生成；下方姓名按小组分工标注负责人。
 * 本文件连接内部枚举与可读输出，同时将关键字和普通标识符分开。
 */
#include "token.h"

#include <string.h>

/* 【AI生成：类别名称输出；分工负责人：宋坤远（2411241）】 */
const char *token_name(TokenKind kind) {
    switch (kind) {
/* ## 拼接 TOK_ 和参数，# 将参数转成字符串。
 * 例如 NAME(ID) 会展开为 case TOK_ID: return "ID"，避免手写重复分支。 */
#define NAME(x) case TOK_##x: return #x
        NAME(ERROR); NAME(ID); NAME(NUMBER); NAME(FLOAT_NUMBER);
        NAME(CONST); NAME(INT); NAME(FLOAT); NAME(VOID); NAME(IF);
        NAME(ELSE); NAME(WHILE); NAME(BREAK); NAME(CONTINUE); NAME(RETURN);
        NAME(PLUS); NAME(MINUS); NAME(STAR); NAME(SLASH); NAME(PERCENT);
        NAME(ASSIGN); NAME(EQ); NAME(NE); NAME(LT); NAME(LE); NAME(GT); NAME(GE);
        NAME(NOT); NAME(AND); NAME(OR); NAME(LPAREN); NAME(RPAREN);
        NAME(LBRACE); NAME(RBRACE); NAME(LBRACKET); NAME(RBRACKET);
        NAME(COMMA); NAME(SEMICOLON);
#undef NAME
        default: return "UNKNOWN";
    }
}

/*
 * 【核心算法：保留字判定；分工负责人：林科融（2413121）】
 * 调用前已按完整 ID 正规式匹配词素，因此 ifx 不会误拆成 IF 和 ID。
 * 保留字数量固定且很少，顺序比较足够简单；这里只做分类，不插入符号表。
 * static const 保证整张映射表只保存一份，且不被本函数修改。
 */
TokenKind keyword_kind(const char *text) {
    static const struct { const char *word; TokenKind kind; } keywords[] = {
        {"const", TOK_CONST}, {"int", TOK_INT}, {"float", TOK_FLOAT},
        {"void", TOK_VOID}, {"if", TOK_IF}, {"else", TOK_ELSE},
        {"while", TOK_WHILE}, {"break", TOK_BREAK},
        {"continue", TOK_CONTINUE}, {"return", TOK_RETURN}
    };
    /* sizeof 比值计算数组元素个数，增加关键字时不必同步修改循环上限。 */
    for (size_t i = 0; i < sizeof keywords / sizeof keywords[0]; ++i)
        if (strcmp(text, keywords[i].word) == 0) return keywords[i].kind;
    return TOK_ID; /* 未命中任何保留字，才交给符号表处理。 */
}

/*
 * 【AI生成：输出辅助；分工负责人：宋坤远（2411241）】
 * 不能直接用 %s 输出任意错误词素：NUL 会截断，制表或换行会破坏 TSV。
 * 这里按 length 逐字节处理，将不可见字节写成可读的转义序列。
 */
void print_escaped(FILE *out, const char *text, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        /* 转为 unsigned char，避免高位字节在 char 有符号的平台变成负数。 */
        unsigned char c = (unsigned char)text[i];
        if (c == '\\') fputs("\\\\", out);
        else if (c == '\n') fputs("\\n", out);
        else if (c == '\r') fputs("\\r", out);
        else if (c == '\t') fputs("\\t", out);
        else if (c >= 32 && c <= 126) fputc(c, out);
        else fprintf(out, "\\x%02X", c);
    }
}
