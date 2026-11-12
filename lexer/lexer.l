/* 【ai生成】沿用上次词法作业的实现与注释；原有分工标注保留。 */
/*
 * 【AI生成：Flex 扫描器框架及实现初稿】
 * 核心词法规则与算法分工负责人：林科融（2413121）。
 * 当前实现和中文注释由 AI 生成，姓名表示小组分工，供组员核实。
 * 文件分三段：正规定义和 C 辅助代码、匹配规则、附加 C 代码；以 %% 分隔。
 * Flex 根据这些规则生成扫描器，扫描器的内部自动机并非组员手写。
 */
/* noyywrap：单文件结束直接返回，不依赖额外的 yywrap 或 libfl。
 * noinput/nounput：不生成未用到的字符回退/读取接口。
 * nodefault：不隐式回显未识别字符，交给最后的错误规则处理。
 * never-interactive/nounistd：无需终端交互探测，减少平台依赖。
 * 8bit：按完整字节范围读取，便于报告 NUL 等异常字节。 */
%option noyywrap noinput nounput nodefault never-interactive nounistd 8bit
/* 独占状态：进入 COMMENT 后，只有显式带 COMMENT 的规则能够匹配。 */
%x COMMENT

%{
#include "token.h"
#include "symbol_table.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>

/* 【AI生成：扫描状态框架】token 是本次结果，其余变量保存跨匹配状态。 */
Token token;
const char *source_name = "<stdin>";
size_t lexical_errors;
static size_t next_line = 1, next_column = 1; /* 下一个未消费字节的位置。 */
static size_t comment_line, comment_column;  /* 未闭合注释应报告开头位置。 */
static int previous_cr; /* 上个字节是否为 CR，用于合并 CRLF 换行。 */

/*
 * 【核心算法：源位置跟踪；分工负责人：林科融（2413121）】
 * 每条规则执行其动作前，先记录本次匹配起点，再逐字节更新下次位置。
 * 不输出单词的空白、注释也会调用本函数，后续单词位置才不会偏移。
 * CR 单独推进一行；若随后是 LF，则不重复加行，兼容 CR/LF/CRLF。
 * previous_cr 跨匹配保留，即使 CR 与 LF 被不同匹配消费也能正确处理。
 * 时间 O(length)，不解析字符显示宽度；一个制表符按一个字节计列。
 */
static void locate(const char *text, size_t length) {
    token.line = next_line;
    token.column = next_column;
    token.lexeme = text; /* 临时借用 Flex 缓冲区，不在此处复制。 */
    token.length = length;
    for (size_t i = 0; i < length; ++i) {
        if (text[i] == '\r') {
            ++next_line;
            next_column = 1;
        } else if (text[i] == '\n') {
            if (!previous_cr) ++next_line;
            next_column = 1;
        } else {
            ++next_column;
        }
        previous_cr = text[i] == '\r';
    }
}

/* Flex 在普通规则动作前展开 YY_USER_ACTION；EOF 动作需自行设置位置。
 * EMIT 同时更新全局类别并返回，让 main 取走当前单词后再继续扫描。 */
#define YY_USER_ACTION locate(yytext, (size_t)yyleng);
#define EMIT(k) return (token.kind = (k))

/*
 * 【核心算法：错误报告与继续扫描；分工负责人：林科融（2413121）】
 * 当前错误片段已被 Flex 消费；返回非零 ERROR，不结束扫描循环。
 * 诊断写到 stderr，不混入 stdout 中用于核对的 TSV 单词流。
 */
static int error_token(const char *message) {
    fprintf(stderr, "%s:%zu:%zu: error: %s: ", source_name,
            token.line, token.column, message);
    print_escaped(stderr, token.lexeme, token.length);
    fputc('\n', stderr);
    ++lexical_errors;
    return (token.kind = TOK_ERROR);
}

/*
 * 【核心算法：整数属性转换；分工负责人：林科融（2413121）】
 * 正规式先验证格式；此处负责把词素转换成数值并检查表示范围。
 * base=0 使 strtoumax 按 0x、前导 0、其他前缀选择 16、8、10 进制。
 * ERANGE 检查转换器溢出，*end 检查是否完整消费，UINT32_MAX 限定属性宽度。
 * 允许 2147483648，以便将 -2147483648 拆成负号和非负数值部分。
 */
static int integer_token(void) {
    char *end;
    errno = 0; /* 必须先清零，防止误读此前库函数留下的错误状态。 */
    uintmax_t value = strtoumax(token.lexeme, &end, 0);
    if (errno == ERANGE || *end || value > UINT32_MAX)
        return error_token("integer literal outside uint32 range");
    token.attribute.integer = (uint32_t)value;
    return (token.kind = TOK_NUMBER);
}

/*
 * 【核心算法：浮点属性转换；分工负责人：林科融（2413121）】
 * strtof 直接转换成 float；进制与指数格式已由 DFLOAT/HFLOAT 规则约束。
 * 拒绝溢出、库报告的下溢、不完整转换和非有限值，避免输出无效属性。
 */
static int float_token(void) {
    char *end;
    errno = 0;
    float value = strtof(token.lexeme, &end);
    if (errno == ERANGE || *end || !isfinite(value))
        return error_token("floating literal outside supported float range");
    token.attribute.real = value;
    return (token.kind = TOK_FLOAT_NUMBER);
}
%}

/*
 * 【核心规则：正规定义；分工负责人：林科融（2413121）】
 * D/H 分别表示十进制/十六进制数字；ID 允许字母或下划线开头。
 * INT 覆盖十六进制、八进制和十进制；09 会交给 BADDEC 报错。
 * EXP 是十进制指数；FRAC 支持 1.、1.25、.5；DFLOAT 还接受 1e2。
 * HFLOAT 使用 p/P 指数且必须带指数，0x1.8p+1 的值为 3。
 * BADHEX/BADDEC 尽量整体消费错误数字，防止把 123abc 拆成正常单词。
 * 二者分别处理 p 与 e 的有符号指数，使 0x1e+2 仍可拆为整数、加号、整数。
 */
D           [0-9]
H           [0-9a-fA-F]
ID          [A-Za-z_][A-Za-z0-9_]*
INT         (0[xX]{H}+|0[0-7]*|[1-9]{D}*)
EXP         [eE][+-]?{D}+
FRAC        ({D}+\.{D}*|\.{D}+)
DFLOAT      ({FRAC}{EXP}?|{D}+{EXP})
HFRAC       ({H}+\.{H}*|\.{H}+|{H}+)
HFLOAT      0[xX]{HFRAC}[pP][+-]?{D}+
BADHEX      0[xX][A-Za-z0-9_.]*([pP][+-]?[A-Za-z0-9_.]*)?
BADDEC      ({D}+(\.{D}*)?|\.{D}+)([eE][+-]?[A-Za-z0-9_.]*)?[A-Za-z0-9_.]*

%%

    /* 【AI生成：输入编码兼容框架；分工负责人：林科融（2413121）】
     * 只允许文件开头的 UTF-8 BOM；它不属于源码单词，不计入有效列数。
     * locate 已消费三个字节，因此将下一列恢复为 1。 */
\xEF\xBB\xBF    {
                    if (token.line != 1 || token.column != 1)
                        return error_token("UTF-8 BOM is only allowed at file start");
                    next_column = 1;
                }
[ \t\v\f\r\n]+ { /* 跳过空白；位置已由 YY_USER_ACTION 更新。 */ }
"//"[^\r\n]*    { /* 跳过行注释；没有末尾换行时也能正常匹配。 */ }
    /* 【核心算法：块注释状态转换；分工负责人：林科融（2413121）】
     * 起始状态读到注释开头后，保存其位置并进入 COMMENT。
     * COMMENT 中不识别关键字或运算符，直到第一个注释结束符才返回 INITIAL。
     * 将普通内容成块跳过，同时分别处理星号和换行，避免遗漏输入。 */
"/*"            { comment_line = token.line; comment_column = token.column; BEGIN(COMMENT); }
<COMMENT>"*/"   { BEGIN(INITIAL); }
<COMMENT>[^*\r\n]+ { /* 一次跳过连续的非星号、非换行内容。 */ }
<COMMENT>"*"    { /* 单个星号；若能匹配结束符，最长匹配会优先选上面的规则。 */ }
<COMMENT>(\r\n|[\r\n]) { /* 跳过换行，locate 仍正常更新行列。 */ }
<COMMENT><<EOF>> {
                    /* EOF 没有新词素，将结果替换成注释开头并报告起始位置。 */
                    token.lexeme = "/*"; token.length = 2;
                    token.line = comment_line; token.column = comment_column;
                    /* 先退出状态；下一次 yylex 才会正常返回 EOF，避免重复报错。 */
                    BEGIN(INITIAL);
                    return error_token("unterminated block comment");
                }

    /* 【核心算法：数字匹配优先级；分工负责人：林科融（2413121）】
     * Flex 先选匹配最长的规则；同长度时选写在前面的规则。
     * 因此合法数字排在错误兜底前；更长的错误词素仍会被整体识别为 ERROR。
     * 减号和加号没有并入数字，仅在指数内部允许出现，避免吞掉表达式运算符。 */
{HFLOAT}        { return float_token(); }
{DFLOAT}        { return float_token(); }
{INT}           { return integer_token(); }
{BADHEX}        { return error_token("malformed numeric literal"); }
{BADDEC}        { return error_token("malformed numeric literal"); }
{ID}            {
                    /* 【核心算法：标识符分类；分工负责人：林科融（2413121）】
                     * 完整匹配后查保留字表；只有普通 ID 才登记符号表。
                     * symbol_intern 由宋坤远负责的模块提供，重复词素返回旧下标。 */
                    TokenKind kind = keyword_kind(yytext);
                    if (kind == TOK_ID)
                        token.attribute.symbol_id = symbol_intern(yytext, token.line, token.column);
                    EMIT(kind);
                }

    /* 【核心规则：运算符及界符；分工负责人：林科融（2413121）】
     * 多字符运算符整体匹配；各动作只返回类别，无额外数值属性。
     * main 根据类别输出 '-'，不会读取联合体里上一个单词遗留的值。 */
"=="            { EMIT(TOK_EQ); }
"!="            { EMIT(TOK_NE); }
"<="            { EMIT(TOK_LE); }
">="            { EMIT(TOK_GE); }
"&&"            { EMIT(TOK_AND); }
"||"            { EMIT(TOK_OR); }
"+"             { EMIT(TOK_PLUS); }
"-"             { EMIT(TOK_MINUS); }
"*"             { EMIT(TOK_STAR); }
"/"             { EMIT(TOK_SLASH); }
"%"             { EMIT(TOK_PERCENT); }
"="             { EMIT(TOK_ASSIGN); }
"<"             { EMIT(TOK_LT); }
">"             { EMIT(TOK_GT); }
"!"             { EMIT(TOK_NOT); }
"("             { EMIT(TOK_LPAREN); }
")"             { EMIT(TOK_RPAREN); }
"{"             { EMIT(TOK_LBRACE); }
"}"             { EMIT(TOK_RBRACE); }
"["             { EMIT(TOK_LBRACKET); }
"]"             { EMIT(TOK_RBRACKET); }
","             { EMIT(TOK_COMMA); }
";"             { EMIT(TOK_SEMICOLON); }
    /* 普通状态下的最后兜底：消费一个未知字节并报错；换行已由空白规则覆盖。 */
.               { return error_token("unexpected character"); }

    /* 【ai生成：本次接入】由林科融负责的 EOF 位置接口。
     * EOF 不走 YY_USER_ACTION；记录真实结束位置供 Bison 缺符号诊断使用。 */
<INITIAL><<EOF>> { token.line = next_line; token.column = next_column; return 0; }

%%
