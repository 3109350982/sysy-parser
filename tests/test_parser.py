"""【ai生成】黑盒测试框架。分工负责人：宋坤远（2411241），预期树结构由两人共同核对。"""
from collections import Counter
import csv
import io
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXE = str(Path(sys.argv.pop(1)).resolve())


def run(text='', *options):
    """每次启动新进程，独立验证退出码、AST、符号表和错误输出。"""
    p = subprocess.run([EXE, *options], input=text.encode('utf-8'),
                       capture_output=True, timeout=10, cwd=ROOT)
    return p.returncode, p.stdout.decode('utf-8'), p.stderr.decode('utf-8', errors='replace')


def numbered(output):
    """按实际输出的孩子编号重建邻接表，用于断言父子结构。"""
    nodes = {}
    for row in output.split('\n\n', 1)[0].splitlines():
        node, desc, children = re.fullmatch(r'(\d+) (.+) @\d+:\d+ Children:(.*)', row).groups()
        nodes[int(node)] = (desc, [int(x) for x in children.split()])
    return nodes


def table(output, section):
    data = output.split(f'\n# {section}\n', 1)[1].split('\n\n', 1)[0]
    return list(csv.DictReader(io.StringIO(data), delimiter='\t'))


class ParserTests(unittest.TestCase):
    def valid(self, text='', *options):
        code, out, err = run(text, '--numbered', *options)
        self.assertEqual(code, 0, err)
        self.assertFalse(err)
        nodes = numbered(out.replace('\r\n', '\n'))
        self.assertEqual(nodes[0][0], 'CompUnit')
        return nodes, out.replace('\r\n', '\n')

    def invalid(self, text, *options):
        code, out, err = run(text, *options)
        self.assertEqual(code, 1, (out, err))
        self.assertFalse(out, 'No partial AST should be printed on failure')
        self.assertTrue(err)
        return err

    def test_ppt_tree(self):
        """核对课件样例的共同类型、两个声明项、if 条件与赋值。"""
        nodes, out = self.valid('', '--symbols', 'examples/ppt.sy')
        self.assertIn('Type int (implicit)', out)
        block = next(c for label, c in nodes.values() if label == 'Block')
        self.assertEqual([nodes[i][0] for i in block], ['VarDecl', 'If'])
        decl = nodes[block[0]][1]
        self.assertEqual([nodes[i][0] for i in decl], ['Type int', 'Declarator', 'Declarator'])
        cond, then = nodes[block[1]][1]
        self.assertEqual(nodes[cond][0], 'Binary ==')
        self.assertEqual(nodes[then][0], 'Assign =')
        self.assertEqual([d['NAME'] for d in table(out, 'DECLARATIONS')], ['main', 'a', 'b'])

    def test_full_file(self):
        """严格模式综合样例覆盖数组、浮点、控制流、嵌套初值与函数调用。"""
        _, out = self.valid('', '--strict', '--symbols', 'examples/full.sy')
        for needle in ['FloatLiteral 0x1.8p+1 [value=3]', 'IntLiteral 010 [value=8]',
                       'While', 'Break', 'Continue', 'InitList', 'Dimension unsized', 'Call']:
            self.assertIn(needle, out)

    def test_arithmetic_precedence_and_parentheses(self):
        for expr, root, child in [('1+2*3','Binary +','Binary *'),
                                  ('(1+2)*3','Binary *','Binary +')]:
            with self.subTest(expr=expr):
                nodes, _ = self.valid('int f(){return '+expr+';}')
                ret = next(c[0] for label, c in nodes.values() if label == 'Return')
                self.assertEqual(nodes[ret][0], root)
                self.assertIn(child, [nodes[i][0] for i in nodes[ret][1]])

    def test_left_association(self):
        nodes, _ = self.valid('int f(){return 9-3-1;}')
        ret = next(c[0] for label, c in nodes.values() if label == 'Return')
        left, right = nodes[ret][1]
        self.assertEqual(nodes[left][0], 'Binary -')
        self.assertEqual(nodes[right][0], 'IntLiteral 1 [value=1]')

    def test_unary_nesting(self):
        nodes, _ = self.valid('int f(){return -+1;}')
        ret = next(c[0] for label, c in nodes.values() if label == 'Return')
        self.assertEqual(nodes[ret][0], 'Unary -')
        self.assertEqual(nodes[nodes[ret][1][0]][0], 'Unary +')

    def test_logical_precedence(self):
        nodes, _ = self.valid('int f(){if(a==b||c<d&&e!=f)return 1;return 0;}')
        cond = next(c[0] for label, c in nodes.values() if label == 'If')
        self.assertEqual(nodes[cond][0], 'Binary ||')
        left, right = nodes[cond][1]
        self.assertEqual((nodes[left][0], nodes[right][0]), ('Binary ==','Binary &&'))

    def test_dangling_else(self):
        nodes, _ = self.valid('int f(){if(a)if(b)return 1;else return 2;}')
        children = [c for label, c in nodes.values() if label == 'If']
        self.assertEqual([len(c) for c in children], [2, 3])

    def test_scopes_declarations_and_lex_ids(self):
        _, out = self.valid('int a;float f(int x[][2]){int a;{float a;return a;}}', '--symbols')
        declarations = table(out, 'DECLARATIONS')
        a = [d for d in declarations if d['NAME'] == 'a']
        self.assertEqual([d['SCOPE'] for d in a], ['0','1','2'])
        self.assertEqual([d['TYPE'] for d in a], ['int','int','float'])
        self.assertEqual(len({d['LEX_ID'] for d in a}), 1)
        scopes = table(out, 'SCOPES')
        self.assertEqual([s['PARENT'] for s in scopes], ['-','0','1'])
        function = next(d for d in declarations if d['NAME'] == 'f')
        parameter = next(d for d in declarations if d['NAME'] == 'x')
        self.assertEqual((function['KIND'], function['ARITY']), ('function','1'))
        self.assertEqual((parameter['KIND'], parameter['RANK']), ('parameter','2'))

    def test_numbering_forms_single_tree(self):
        nodes, _ = self.valid('', 'examples/full.sy')
        self.assertEqual(list(nodes), list(range(len(nodes))))
        edges = Counter(i for _, children in nodes.values() for i in children)
        self.assertEqual(edges, Counter({i: 1 for i in nodes if i}))

    def test_indent_output(self):
        code, out, err = run('int main(){return 0;}')
        self.assertEqual((code, err), (0, ''))
        self.assertRegex(out, r'\n  1 Function ')
        self.assertNotIn('Children:', out)

    def test_empty_constructs_and_source_order(self):
        nodes, _ = self.valid('int x[1]={};void f(){} float y; int g(){;return 0;}')
        self.assertEqual([nodes[i][0] for i in nodes[0][1]], ['VarDecl','Function','VarDecl','Function'])
        self.assertIn(('InitList', []), nodes.values())
        self.assertIn(('Block', []), nodes.values())
        self.assertIn(('EmptyStmt', []), nodes.values())

    def test_strict_mode_and_compatibility_limit(self):
        self.assertIn('explicit return type', self.invalid('main(){}','--strict'))
        self.assertIn('explicit return type', self.invalid('other(){}'))
        self.valid('int main(){}','--strict')

    def test_lexical_error_is_not_repeated(self):
        err = self.invalid('int f(){return 09;}')
        self.assertIn('malformed numeric literal', err)
        self.assertNotIn('syntax error', err)

    def test_unterminated_comment(self):
        err = self.invalid('int f() {\r\n  /* bad')
        self.assertIn(':2:3: error: unterminated block comment', err)

    def test_syntax_error_position(self):
        self.assertIn(':3:5: syntax error:', self.invalid('', 'examples/syntax_error.sy'))

    def test_eof_position(self):
        self.assertIn(':3:1: syntax error:', self.invalid('int f(){\nreturn 0;\n'))

    def test_invalid_grammar_constructs(self):
        for text in ['','const int x;','void x;', 'int f(int a[2]){}',
                     'int f(){(a+1)=2;}', 'int f(){if(a<b)return 1;else}',
                     'int f(){return a<b;}', 'int f(){} int g();']:
            with self.subTest(text=text): self.invalid(text)

    def test_cli_file_with_spaces_and_missing_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            file = Path(tmp)/'case with spaces.sy'
            file.write_text('int f(){return 0;}',encoding='ascii')
            self.valid('',str(file))
            code, out, err = run('', str(file.with_name('absent.sy')))
            self.assertEqual((code, out), (2, ''))
            self.assertTrue(err)

    def test_long_left_recursive_list(self):
        """验证长声明列表的拼接次序、树遍历、索引扩容及编号连续性。"""
        names = [f'v{i}' for i in range(1500)]
        _, out = self.valid('int '+','.join(names)+';','--symbols')
        self.assertEqual([r['NAME'] for r in table(out,'DECLARATIONS')],names)


if __name__ == '__main__':
    unittest.main(verbosity=2)
