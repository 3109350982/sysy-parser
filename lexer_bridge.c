/* 【ai生成】Flex → Bison 接口框架及实现初稿。
 * 【核心算法·分工负责人：林科融（2413121）】保留旧词法器，不依赖两套枚举编号相等。 */
#include "parser.tab.h"
#include "token.h"

int sylex(SYSTYPE *value, SYLTYPE *loc, ParseContext *ctx) {
    int kind = yylex(); /* 旧扫描器跳过空白和注释，并设置共享 token。 */
    *loc = (SourceSpan){token.line, token.column, token.line, token.column};
    value->node = NULL;
    if (!kind) return 0; /* 词法器的 EOF 动作已填充真实 EOF 行列。 */
    if (kind == TOK_ERROR) return SYerror; /* 已由 Flex 报错，不重复输出语法错误。 */
    if (token.length) loc->last_column += token.length - 1;

    /* Bison 可能先读取下一个单词再归约，故现在复制 yytext，而非在归约时读取它。
     * NUMBER 保留原始词素与已转换的数值；ID 保留旧词素表稳定编号。 */
    if (kind == TOK_ID || kind == TOK_NUMBER || kind == TOK_FLOAT_NUMBER) {
        AstKind leaf = kind == TOK_ID ? AST_NAME : kind == TOK_NUMBER ? AST_INT : AST_FLOAT;
        Ast *node = ast_new(&ctx->arena, leaf, token.line, token.column, token.lexeme);
        value->node = node;
        if (kind == TOK_ID) { node->symbol_id = token.attribute.symbol_id; return ID; }
        if (kind == TOK_NUMBER) { node->value.integer = token.attribute.integer; return INTEGER; }
        node->value.real = token.attribute.real;
        return REAL;
    }

    /* 单字符终结符可直接返回字符编码，多字符和关键字返回 parser.tab.h 的枚举。 */
    switch (kind) {
        case TOK_CONST: return T_CONST; case TOK_INT: return T_INT;
        case TOK_FLOAT: return T_FLOAT; case TOK_VOID: return T_VOID;
        case TOK_IF: return T_IF; case TOK_ELSE: return T_ELSE; case TOK_WHILE: return T_WHILE;
        case TOK_BREAK: return T_BREAK; case TOK_CONTINUE: return T_CONTINUE;
        case TOK_RETURN: return T_RETURN;
        case TOK_EQ: return T_EQ; case TOK_NE: return T_NE;
        case TOK_LE: return T_LE; case TOK_GE: return T_GE;
        case TOK_AND: return T_AND; case TOK_OR: return T_OR;
        case TOK_PLUS: return '+'; case TOK_MINUS: return '-'; case TOK_STAR: return '*';
        case TOK_SLASH: return '/'; case TOK_PERCENT: return '%'; case TOK_ASSIGN: return '=';
        case TOK_LT: return '<'; case TOK_GT: return '>'; case TOK_NOT: return '!';
        case TOK_LPAREN: return '('; case TOK_RPAREN: return ')';
        case TOK_LBRACE: return '{'; case TOK_RBRACE: return '}';
        case TOK_LBRACKET: return '['; case TOK_RBRACKET: return ']';
        case TOK_COMMA: return ','; case TOK_SEMICOLON: return ';';
        default: return SYUNDEF;
    }
}
