/* 【ai生成】命令行、文件输入与资源回收框架及详细注释。
 * 分工负责人：宋坤远（2411241）；实际完成人及修改内容请据实核对。 */
#include "parser.tab.h"
#include "decl_index.h"
#include "symbol_table.h"
#include "token.h"

#include <string.h>

static void usage(const char *exe) {
    printf("Usage: %s [--numbered] [--symbols] [--strict] [FILE|-]\n"
           "  --numbered  show explicit child node numbers instead of indentation\n"
           "  --symbols   show scopes, declaration index, and the lexer word table\n"
           "  --strict    reject the slide sample's implicit int main() shorthand\n", exe);
}

int main(int argc, char **argv) {
    ParseContext ctx = {0};
    const char *path = NULL;
    int numbered = 0, symbols = 0;
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (!strcmp(arg, "--help")) { usage(argv[0]); return 0; }
        if (!strcmp(arg, "--numbered")) numbered = 1;
        else if (!strcmp(arg, "--symbols")) symbols = 1;
        else if (!strcmp(arg, "--strict")) ctx.strict = 1;
        else if (arg[0] == '-' && strcmp(arg, "-")) {
            fprintf(stderr, "unknown option: %s\n", arg); return 2;
        } else if (path) { fputs("expected only one input file\n", stderr); return 2; }
        else path = arg;
    }

    /* 保留原始换行字节，由复用的 Flex 规则统一统计行列。 */
    FILE *input = (!path || !strcmp(path, "-")) ? stdin : fopen(path, "rb");
    if (!input) { perror(path); return 2; }
    yyin = input;
    ctx.filename = source_name = input == stdin ? "<stdin>" : path;

    /* main 只调用一次 syparse。Bison 根据状态与前瞻按需调用 sylex，
     * 后者再调用旧 yylex；不是沿用词法作业里 main 的 while(yylex())。 */
    int result = syparse(&ctx);
    int read_failed = ferror(input);
    yylex_destroy();
    if (input != stdin) fclose(input);

    /* 词法或语法失败时不输出半棵树；stderr 保存带位置的诊断。 */
    int status = result == 2 || read_failed ? 2 : (result || ctx.syntax_errors || lexical_errors ? 1 : 0);
    if (read_failed) fprintf(stderr, "%s: input read failed\n", ctx.filename);
    if (status == 0) {
        ast_print(stdout, ctx.root, numbered);
        if (symbols) {
            DeclIndex index = {0};
            decl_index_build(&index, ctx.root);
            decl_index_print(stdout, &index);
            decl_index_destroy(&index); /* 表项借用 AST 中的名称和类型。 */
            symbol_table_print(stdout);
        }
        if (fflush(stdout) == EOF || ferror(stdout)) {
            fputs("output write failed\n", stderr); status = 2;
        }
    }
    symbol_table_destroy();
    ast_destroy(&ctx.arena); /* 失败时也能释放已分配但未挂接的节点。 */
    return status;
}
