"""【ai生成】Windows 原生构建框架。分工负责人：宋坤远（2411241）。"""
from pathlib import Path
from shutil import which
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
PORTABLE = ROOT.parent / '.environment' / 'parser-tools'
OUT = ROOT / 'build' / 'windows'


def find_tool(*names):
    """优先使用已配置的 PATH，其次使用本工作区的便携工具目录。"""
    for name in names:
        found = which(name)
        if found:
            return found
        candidate = PORTABLE / name
        if candidate.is_file():
            return str(candidate)
    raise SystemExit(f'Tool missing: {names[0]}; see README.md')


def run(*args):
    """按参数列表运行并原样返回失败码，不经 shell 拼接命令。"""
    result = subprocess.run(args, cwd=ROOT)
    if result.returncode:
        raise SystemExit(result.returncode)


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    # Bison 和 Flex 输出位置与 Linux 构建分开，避免 Windows ELF/EXE 混用。
    run(find_tool('win_bison.exe', 'bison.exe'), '-Wall', '-Werror', '-d', '-v',
        '-o', 'build/windows/parser.tab.c', 'parser.y')
    run(find_tool('win_flex.exe', 'flex.exe'), '-o', 'build/windows/lex.yy.c', 'lexer/lexer.l')
    run(find_tool('gcc.exe'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Wpedantic',
        '-D__USE_MINGW_ANSI_STDIO=1', '-I.', '-Ibuild/windows', '-Ilexer',
        'build/windows/parser.tab.c', 'build/windows/lex.yy.c', 'main.c', 'ast.c',
        'decl_index.c', 'lexer_bridge.c', 'lexer/token.c', 'lexer/symbol_table.c',
        '-o', 'build/windows/sysy-parser.exe')


def main():
    action = sys.argv[1] if len(sys.argv) > 1 else 'build'
    if action not in ('build', 'demo', 'test') or len(sys.argv) > 2:
        raise SystemExit('Usage: python build_windows.py [build|demo|test]')
    build()
    exe = str(OUT / 'sysy-parser.exe')
    if action == 'demo':
        run(exe, '--numbered', '--symbols', 'examples/ppt.sy')
    elif action == 'test':
        run(sys.executable, '-X', 'utf8', 'tests/test_parser.py', exe)
    else:
        print('Built: build/windows/sysy-parser.exe')


if __name__ == '__main__':
    main()
