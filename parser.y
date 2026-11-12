/* 【ai生成】Yacc/Bison 程序框架、CFG 及翻译动作初稿。
 * 【核心算法·分工负责人：林科融（2413121）】
 * 标注的是小组任务归属；实际完成情况由组员据实核对。
 * Bison 生成 LALR(1) 分析表；本文件设计产生式和归约时的 AST 构造。 */
%require "3.8"
%define api.prefix {sy}
%define api.pure full
%define parse.error detailed
%define parse.lac full
%define api.location.type {SourceSpan}
%locations
%expect 0
%parse-param {ParseContext *ctx}
%lex-param {ParseContext *ctx}
%initial-action { @$ = (SourceSpan){1, 1, 1, 1}; }

%code requires {
    #include "parse_context.h"
}
%code provides {
    int sylex(SYSTYPE *value, SYLTYPE *location, ParseContext *ctx);
    void syerror(const SYLTYPE *location, ParseContext *ctx, const char *message);
}
%code {
    #include <string.h>

    /* 【ai生成】归约动作辅助。传入空孩子时略去该项，保留其他孩子顺序。 */
    static Ast *new_node(ParseContext *ctx, AstKind kind, SourceSpan loc,
                         const char *text, Ast *a, Ast *b, Ast *c, Ast *d) {
        Ast *node = ast_new(&ctx->arena, kind, loc.first_line, loc.first_column, text);
        ast_add(node, a); ast_add(node, b); ast_add(node, c); ast_add(node, d);
        return node;
    }
    static Ast *new_list(ParseContext *ctx, AstKind kind, SourceSpan loc, AstList children) {
        Ast *node = new_node(ctx, kind, loc, NULL, NULL, NULL, NULL, NULL);
        ast_adopt(node, children);
        return node;
    }
    static Ast *operator_node(ParseContext *ctx, SourceSpan loc, const char *op, Ast *a, Ast *b) {
        return new_node(ctx, b ? AST_BINARY : AST_UNARY, loc, op, a, b, NULL, NULL);
    }
    static Ast *declaration(ParseContext *ctx, AstKind kind, SourceSpan loc, Ast *type, AstList defs) {
        Ast *node = new_node(ctx, kind, loc, NULL, type, NULL, NULL, NULL);
        ast_adopt(node, defs); /* 先是共同类型，其后按书写顺序排列各声明项。 */
        return node;
    }
}

%union { Ast *node; AstList list; }
%token <node> ID "identifier" INTEGER "integer literal" REAL "float literal"
%token T_CONST "const" T_INT "int" T_FLOAT "float" T_VOID "void"
%token T_IF "if" T_ELSE "else" T_WHILE "while" T_BREAK "break"
%token T_CONTINUE "continue" T_RETURN "return"
%token T_EQ "==" T_NE "!=" T_LE "<=" T_GE ">=" T_AND "&&" T_OR "||"

/* 【核心算法：悬挂 else·分工负责人：林科融（2413121）】
 * 无 else 的 if 规则优先级较低，遇到 else 选择移进，让它属于最近未匹配的 if。 */
%precedence IF_WITHOUT_ELSE
%precedence T_ELSE
%type <node> unit top decl btype func block block_item stmt var_def const_def init param
%type <node> exp_opt cond add mul unary primary lval rel eq land lor
%type <list> items var_defs const_defs dims param_dims init_items init_seq params_opt params
%type <list> block_items args_opt args indices
%start unit

%%

/* 【核心算法：编译单元、声明与列表·分工负责人：林科融（2413121）】
 * [] 和 {} 的 EBNF 记法改写为空产生式及左递归列表，$n 是子项属性，@n 是位置。 */
unit : items { ctx->root = $$ = new_list(ctx, AST_UNIT, @1, $1); } ;
items : top { $$ = ast_append((AstList){0}, $1); }
      | items top { $$ = ast_append($1, $2); } ;
top : decl { $$ = $1; } | func { $$ = $1; } ;

btype : T_INT { $$ = new_node(ctx, AST_TYPE, @1, "int", NULL,NULL,NULL,NULL); }
      | T_FLOAT { $$ = new_node(ctx, AST_TYPE, @1, "float", NULL,NULL,NULL,NULL); } ;
decl : T_CONST btype const_defs ';' { $$ = declaration(ctx, AST_CONST_DECL, @1, $2, $3); }
     | btype var_defs ';' { $$ = declaration(ctx, AST_VAR_DECL, @1, $1, $2); } ;
const_defs : const_def { $$ = ast_append((AstList){0}, $1); }
           | const_defs ',' const_def { $$ = ast_append($1, $3); } ;
var_defs : var_def { $$ = ast_append((AstList){0}, $1); }
         | var_defs ',' var_def { $$ = ast_append($1, $3); } ;
const_def : ID dims '=' init {
    $$ = new_node(ctx, AST_DEF, @1, NULL, $1, new_list(ctx, AST_DIMS, @2, $2), $4, NULL);
} ;
var_def : ID dims {
    $$ = new_node(ctx, AST_DEF, @1, NULL, $1, new_list(ctx, AST_DIMS, @2, $2), NULL, NULL);
} | ID dims '=' init {
    $$ = new_node(ctx, AST_DEF, @1, NULL, $1, new_list(ctx, AST_DIMS, @2, $2), $4, NULL);
} ;
dims : %empty { $$ = (AstList){0}; }
     | dims '[' add ']' { $$ = ast_append($1, new_node(ctx, AST_DIM, @2, NULL, $3,NULL,NULL,NULL)); } ;

/* ConstExp 与 Exp 在 CFG 中都是 AddExp，所以常量与普通初值可共用语法。
 * 常量表达式中的名称是否为 const、数组维数是否为正数，需由后续语义检查。 */
init : add { $$ = $1; }
     | '{' init_items '}' { $$ = new_list(ctx, AST_INIT_LIST, @1, $2); } ;
init_items : %empty { $$ = (AstList){0}; } | init_seq { $$ = $1; } ;
init_seq : init { $$ = ast_append((AstList){0}, $1); }
         | init_seq ',' init { $$ = ast_append($1, $3); } ;

/* 共用 btype 作为非 void 函数和变量的类型；读到 ID 后，'(' 区分函数定义。
 * 不先独立归约 FuncType -> btype，避免在还未看到 '(' 时引入选择冲突。 */
func : btype ID '(' params_opt ')' block {
    $$ = new_node(ctx, AST_FUNC, @1, NULL, $1, $2, new_list(ctx, AST_PARAMS, @3, $4), $6);
} | T_VOID ID '(' params_opt ')' block {
    Ast *type = new_node(ctx, AST_TYPE, @1, "void", NULL,NULL,NULL,NULL);
    $$ = new_node(ctx, AST_FUNC, @1, NULL, type, $2, new_list(ctx, AST_PARAMS, @3, $4), $6);
} | ID '(' ')' block {
    /* 课件例外：仅 main() 无参数且省略 int 时兼容，不放宽其他函数。 */
    if (ctx->strict || strcmp($1->text, "main") != 0) {
        syerror(&@1, ctx, "function definition requires an explicit return type");
        YYABORT;
    }
    Ast *type = new_node(ctx, AST_TYPE, @1, "int (implicit)", NULL,NULL,NULL,NULL);
    $$ = new_node(ctx, AST_FUNC, @1, NULL, type, $1,
                  new_list(ctx, AST_PARAMS, @2, (AstList){0}), $4);
} ;
params_opt : %empty { $$ = (AstList){0}; } | params { $$ = $1; } ;
params : param { $$ = ast_append((AstList){0}, $1); }
       | params ',' param { $$ = ast_append($1, $3); } ;
param : btype ID param_dims {
    $$ = new_node(ctx, AST_PARAM, @1, NULL, $1, $2, new_list(ctx, AST_DIMS, @3, $3), NULL);
} ;
param_dims : %empty { $$ = (AstList){0}; }
           | '[' ']' indices {
    /* 数组形参第一维必须省略长度。空维节点仍参与维数统计。 */
    Ast *omitted = new_node(ctx, AST_DIM, @1, "unsized", NULL,NULL,NULL,NULL);
    omitted->next = $3.first;
    $$ = (AstList){omitted, $3.last ? $3.last : omitted};
} ;

/* 【核心算法：语句翻译动作·分工负责人：林科融（2413121）】 */
block : '{' block_items '}' { $$ = new_list(ctx, AST_BLOCK, @1, $2); } ;
block_items : %empty { $$ = (AstList){0}; }
            | block_items block_item { $$ = ast_append($1, $2); } ;
block_item : decl { $$ = $1; } | stmt { $$ = $1; } ;
stmt : lval '=' add ';' { $$ = new_node(ctx, AST_ASSIGN, @2, "=", $1,$3,NULL,NULL); }
     | add ';' { $$ = new_node(ctx, AST_EXPR_STMT, @1, NULL, $1,NULL,NULL,NULL); }
     | ';' { $$ = new_node(ctx, AST_EMPTY, @1, NULL, NULL,NULL,NULL,NULL); }
     | block { $$ = $1; }
     | T_IF '(' cond ')' stmt %prec IF_WITHOUT_ELSE {
         $$ = new_node(ctx, AST_IF, @1, NULL, $3,$5,NULL,NULL);
     }
     | T_IF '(' cond ')' stmt T_ELSE stmt { $$ = new_node(ctx, AST_IF, @1, NULL, $3,$5,$7,NULL); }
     | T_WHILE '(' cond ')' stmt { $$ = new_node(ctx, AST_WHILE, @1, NULL, $3,$5,NULL,NULL); }
     | T_BREAK ';' { $$ = new_node(ctx, AST_BREAK, @1, NULL, NULL,NULL,NULL,NULL); }
     | T_CONTINUE ';' { $$ = new_node(ctx, AST_CONTINUE, @1, NULL, NULL,NULL,NULL,NULL); }
     | T_RETURN exp_opt ';' { $$ = new_node(ctx, AST_RETURN, @1, NULL, $2,NULL,NULL,NULL); } ;
exp_opt : %empty { $$ = NULL; } | add { $$ = $1; } ;

/* 【核心算法：表达式分层·分工负责人：林科融（2413121）】
 * 表达式从低到高为 ||、&&、相等、关系、加减、乘除模、一元、基本项。
 * 二元规则左递归，所以 a-b-c 归约成 (a-b)-c；一元规则递归向右。 */
cond : lor { $$ = $1; } ;
lor : land { $$ = $1; } | lor T_OR land { $$ = operator_node(ctx,@2,"||",$1,$3); } ;
land : eq { $$ = $1; } | land T_AND eq { $$ = operator_node(ctx,@2,"&&",$1,$3); } ;
eq : rel { $$ = $1; }
   | eq T_EQ rel { $$ = operator_node(ctx,@2,"==",$1,$3); }
   | eq T_NE rel { $$ = operator_node(ctx,@2,"!=",$1,$3); } ;
rel : add { $$ = $1; }
    | rel '<' add { $$ = operator_node(ctx,@2,"<",$1,$3); }
    | rel '>' add { $$ = operator_node(ctx,@2,">",$1,$3); }
    | rel T_LE add { $$ = operator_node(ctx,@2,"<=",$1,$3); }
    | rel T_GE add { $$ = operator_node(ctx,@2,">=",$1,$3); } ;
add : mul { $$ = $1; }
    | add '+' mul { $$ = operator_node(ctx,@2,"+",$1,$3); }
    | add '-' mul { $$ = operator_node(ctx,@2,"-",$1,$3); } ;
mul : unary { $$ = $1; }
    | mul '*' unary { $$ = operator_node(ctx,@2,"*",$1,$3); }
    | mul '/' unary { $$ = operator_node(ctx,@2,"/",$1,$3); }
    | mul '%' unary { $$ = operator_node(ctx,@2,"%",$1,$3); } ;
unary : primary { $$ = $1; }
      | ID '(' args_opt ')' {
          $$ = new_node(ctx, AST_CALL, @1, NULL, $1,NULL,NULL,NULL);
          ast_adopt($$, $3);
      }
      | '+' unary { $$ = operator_node(ctx,@1,"+",$2,NULL); }
      | '-' unary { $$ = operator_node(ctx,@1,"-",$2,NULL); }
      | '!' unary { $$ = operator_node(ctx,@1,"!",$2,NULL); } ;
primary : '(' add ')' { $$ = $2; } /* 结构已由归约表达，不为括号建节点。 */
        | lval { $$ = $1; } | INTEGER { $$ = $1; } | REAL { $$ = $1; } ;
lval : ID indices { $$ = new_node(ctx, AST_LVALUE, @1, NULL, $1,NULL,NULL,NULL); ast_adopt($$, $2); } ;
indices : %empty { $$ = (AstList){0}; }
        | indices '[' add ']' { $$ = ast_append($1, new_node(ctx, AST_DIM,@2,NULL,$3,NULL,NULL,NULL)); } ;
args_opt : %empty { $$ = (AstList){0}; } | args { $$ = $1; } ;
args : add { $$ = ast_append((AstList){0}, $1); }
     | args ',' add { $$ = ast_append($1, $3); } ;

%%

/* 【ai生成】位置信息与诊断入口；语法解析失败后不发布半棵 AST。 */
void syerror(const SYLTYPE *loc, ParseContext *ctx, const char *message) {
    /* Bison 已带英文错误前缀，去掉重复部分；自定义诊断仍使用统一格式。 */
    static const char bison_prefix[] = "syntax error, ";
    if (strncmp(message, bison_prefix, sizeof bison_prefix - 1) == 0)
        message += sizeof bison_prefix - 1;
    const char *category = strcmp(message, "memory exhausted") == 0 ? "fatal" : "syntax error";
    fprintf(stderr, "%s:%zu:%zu: %s: %s\n",
            ctx->filename, loc->first_line, loc->first_column, category, message);
    ++ctx->syntax_errors;
}
