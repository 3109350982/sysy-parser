# 【ai生成】构建与测试框架。分工负责人：宋坤远（2411241）。
CC = gcc
BISON = bison
FLEX = flex
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS = -I. -Ibuild -Ilexer
TARGET = build/sysy-parser
SOURCES = main.c ast.c decl_index.c lexer_bridge.c lexer/token.c lexer/symbol_table.c
HEADERS = ast.h decl_index.h parse_context.h lexer/token.h lexer/symbol_table.h

.PHONY: all demo test clean
all: $(TARGET)

build:
	mkdir -p $@

# Bison 同时生成 C、头文件和状态报告。-Werror 防止误交带冲突的文法。
# &: 为 GNU Make 4.3 的分组目标：一次命令同时更新所有产物。
build/parser.tab.c build/parser.tab.h build/parser.output &: parser.y | build
	$(BISON) -Wall -Werror -d -v -o build/parser.tab.c $<

build/lex.yy.c: lexer/lexer.l | build
	$(FLEX) -o $@ $<

$(TARGET): build/parser.tab.c build/parser.tab.h build/lex.yy.c $(SOURCES) $(HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) build/parser.tab.c build/lex.yy.c $(SOURCES) -o $@

demo: $(TARGET)
	./$(TARGET) --numbered --symbols examples/ppt.sy

test: $(TARGET)
	python3 tests/test_parser.py ./$(TARGET)

clean:
	rm -f build/parser.tab.c build/parser.tab.h build/parser.output build/lex.yy.c $(TARGET)
