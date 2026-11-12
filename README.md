# SysY 语法分析器

用 **Flex + GNU Bison（兼容 Yacc）+ C11** 把 SysY 源文件解析为抽象语法树（AST），支持缩进输出和编号父子关系输出。它复用上次作业的 Flex 扫描器，并可输出语法级的声明与作用域记录。

## 立即运行

当前电脑使用已验证的 Windows 原生工具链。双击 `运行演示.cmd`，将重新编译并输出课件样例的 AST、作用域表、声明表及词素表；双击 `运行测试.cmd` 可运行 19 项自动测试。

在 **PowerShell** 中也可以手动执行：

```powershell
Set-Location -LiteralPath 'D:\编译原理大作业\sysy_parser'
python -X utf8 build_windows.py build
.\build\windows\sysy-parser.exe --numbered --symbols examples/ppt.sy
$LASTEXITCODE
```

查看综合样例或错误示例：

```powershell
.\build\windows\sysy-parser.exe --strict --symbols examples/full.sy
.\build\windows\sysy-parser.exe examples/syntax_error.sy
.\build\windows\sysy-parser.exe examples/lexical_error.sy
```

当前机器的 Python、GCC 已在 PATH 中；WinFlexBison 位于工作区的 `.environment/parser-tools`，脚本也接受 PATH 中的工具。源码包不包含这些第三方工具。换电脑时，可配置 [WinFlexBison](https://github.com/lexxmark/winflexbison/releases)、GCC 和 Python，或者在装有 Bison 3.8+、Flex、GNU Make 4.3+ 和 GCC 的 Linux / WSL 中执行：

```bash
make
make demo
make test
./build/sysy-parser --numbered --symbols examples/ppt.sy
```

当前这次验证是在 Windows 原生环境完成的。`--numbered` 输出 `Children:` 后的孩子编号；不加时输出缩进树。`--symbols` 追加声明及词素相关表。无文件参数或使用 `-` 时读标准输入。退出码 `0` 表示成功，`1` 表示词法或语法失败，`2` 表示参数、I/O 或内存等运行失败。失败时只向标准错误输出诊断，不输出半棵树。

## 文法范围与课件样例

项目参考 [SysY 2022 语言定义](https://bdgit.educoder.net/prxpafhei/SysyCompiler_Arm/raw/branch/main/doc/sysy-2022-spec.pdf) 的语法部分，支持 `int`、`float`、`void`，常量与变量声明、多维数组、嵌套初始化、函数定义和调用、语句块、赋值、`if` / `else`、`while`、`break`、`continue`、`return`，以及算术和条件表达式。声明和表达式的具体规则及翻译动作均在 `parser.y`。

`examples/ppt.sy` 保留课件中的 `main()` 无显式返回类型写法。默认仅兼容这种无参数 `main()` 函数定义，并在树中标记 `Type int (implicit)`；`--strict` 关闭这项兼容。`--strict` 不等于完整语义检查。普通表达式使用 `add`，条件使用 `cond`；基本项括号包裹的是普通表达式。课件式分组演示可使用 `if (a == 0)`，不依赖非标准的括号嵌套条件扩展。

本项目的输入范围与复用的词法器一致，未扩展字符串、预处理等特性；没有做名称绑定、重定义、类型、常量表达式或循环上下文检查，也不执行输入程序。旧词法作业中能扫描的文件，不一定能通过这次语法分析。

## 从源文件到语法树

1. `main.c` 打开输入并赋给 `yyin`，调用一次 `syparse(&ctx)`。此后由 Bison 按需取前瞻符号。
2. `lexer_bridge.c` 的 `sylex` 调用上次作业的 `yylex`，将旧 `TOK_*` 映射到 Bison 终结符，并填充位置和语义值。标识符和常量的词素在此复制，归约时不再借用 `yytext`。
3. `parser.y` 用归约动作创建并挂接 AST 节点。`$1` 等指向产生式右部的属性，`$$` 是左部属性，`@1` 等提供位置。只保留有效语法结构，标点不单独输出。
4. 成功后 `ast.c` 按先序编号并输出。`--symbols` 再遍历树提取声明和作用域。无论成功还是失败，程序都会回收 AST 和词素表。

文法把加减、乘除模、一元运算及逻辑条件分层，左递归让 `a-b-c` 形成 `(a-b)-c`。例如 `a=b+1*2;` 的根为 `Assign =`，右子树根为 `Binary +`，乘法位于它的右孩子。悬挂 `else` 用优先级选择移进，使 `else` 归属最近可匹配的 `if`；构建时用 `%expect 0`、`-Werror` 检查未解决冲突。

## 数据结构与符号表

| 数据 | 内容及用途 |
| --- | --- |
| AST | 节点类型、位置、复制的词素、常量值与词素编号。首孩子、尾孩子、下一兄弟和父指针构成多叉树；独立分配链统一回收节点。 |
| 词素表 | 保留旧 Flex 哈希表：名称、首次位置、次数及 `LEX_ID`。同名词素复用一个编号。 |
| 声明索引 | 每个声明单独一行，记录名称、`LEX_ID`、声明类别、类型、作用域、数组维数、函数形参个数、位置和 AST 编号。 |
| 作用域表 | `0` 是全局作用域；函数参数和函数体共享函数作用域；内部块有独立编号和父作用域。 |

例如全局和局部同名变量可以共享 `LEX_ID`，但对应不同 `SCOPE` 的声明记录。表由 AST 在成功后提取，不会在出错的分析过程中留下半成品；它为后续语义绑定提供信息，当前不判定引用指向哪一条声明。

## 主要文件与展示顺序

| 文件 | 用途 |
| --- | --- |
| `parser.y` | CFG、Yacc/Bison 翻译动作、错误位置。 |
| `lexer_bridge.c`、`parse_context.h` | 终结符映射、位置信息与语义值接口。 |
| `ast.c`、`ast.h` | AST 多叉树、节点构造、先序输出、统一回收。 |
| `decl_index.c`、`decl_index.h` | 作用域和声明索引。 |
| `lexer/` | 上次 Flex 源码及词素表的独立副本；为本次接入补充 EOF 位置记录。 |
| `main.c`、`Makefile`、`build_windows.py` | 输入输出、编译及平台启动。 |
| `examples/`、`tests/test_parser.py` | 课件、综合、错误样例及黑盒测试。 |

展示建议：先运行课件样例，指出 `VarDecl` 的两个声明孩子、`If` 的 `Binary ==` 条件和 `Assign =` 分支；再展示综合样例中不同作用域的声明；最后运行缺分号样例，指出诊断行列。课件样例的实测输出见 `课件样例输出.txt`。

本次课件样例是 `examples/ppt.sy`。上次词法答辩的 `example.sy` 不适合当作这次的成功样例：`return a//**/xm;` 的分号属于行注释，下一行的 `return` 会在第 10 行第 5 列触发语法错误。这正好说明词法成功不等于语法正确。

提交时使用项目源文件（保留 `parser.y` 和 `lexer/lexer.l`）。课程若要求 Git 链接，可将源码包解压后的项目上传到自己的课程仓库，再提交链接。AI 来源及组员分工见 `小组分工说明.md`。
