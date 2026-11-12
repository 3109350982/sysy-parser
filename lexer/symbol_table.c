/* 【ai生成】沿用上次词法作业的实现与注释；原有分工标注保留。 */
/*
 * 【AI生成：符号表模块框架及实现初稿】
 * 核心算法与数据结构分工负责人：宋坤远（2411241）。
 * 当前代码和注释由 AI 生成；成员标注依据小组分工，实际完成情况需核实。
 *
 * 结构：symbols 动态数组保存表项，buckets 数组保存各哈希桶的链首。
 * 冲突通过 Symbol.next 连接，所有链连接都用“下标 + 1”保存，不用指针。
 * 因此 symbols 重新分配内存时，已有 symbol_id 和冲突链仍然有效。
 */
#include "symbol_table.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* 【核心数据结构：符号表项；分工负责人：宋坤远（2411241）】 */
typedef struct {
    char *name; /* 独立分配的词素副本；由符号表持有并负责释放。 */
    size_t first_line, first_column, occurrences; /* 首次位置与累计出现次数。 */
    size_t next; /* 下一项的下标 + 1；0 表示链尾，避免与合法下标 0 冲突。 */
} Symbol;

/* 文件级 static 将这些状态限制在本模块内；首次使用前自动初始化为零。 */
static Symbol *symbols;
static size_t count, capacity; /* 已登记项数，以及 symbols 可容纳的项数。 */
static size_t *buckets;        /* 每个桶保存链首下标 + 1；空桶保存 0。 */
static size_t bucket_count;    /* 桶数，与符号数组的 capacity 分开维护。 */

/* 【AI生成：内存分配辅助】统一处理分配失败，防止后续解引用空指针。 */
static void *allocate(size_t n) {
    void *p = malloc(n);
    if (!p) { fputs("fatal: out of memory\n", stderr); exit(2); }
    return p;
}

/*
 * 【核心算法：FNV-1a 哈希；分工负责人：宋坤远（2411241）】
 * 从固定 64 位初值开始，对每个字节先异或、再乘固定素数。
 * uint64_t 的乘法按模 2^64 回绕，这是无符号运算的正常定义。
 * 返回哈希值后再按桶数取模；仍须用 strcmp 比较，哈希相同不代表词素相同。
 * 哈希计算为 O(L)，L 为词素长度。
 */
static size_t hash_name(const char *name) {
    uint64_t h = UINT64_C(14695981039346656037);
    for (const unsigned char *p = (const unsigned char *)name; *p; ++p) {
        h ^= *p;
        h *= UINT64_C(1099511628211);
    }
    return (size_t)h;
}

/*
 * 【核心算法：重新散列；分工负责人：宋坤远（2411241）】
 * 桶数变化后，同一哈希值的取模结果可能改变，必须重建每条冲突链。
 * 只改桶数组与 next，不改变 symbols 的排列，所以单词属性中的编号不变。
 * 时间为 O(n + 所有已有词素的总长度)，这里 n 是新桶数。
 */
static void rehash(size_t n) {
    size_t *fresh = allocate(n * sizeof *fresh);
    memset(fresh, 0, n * sizeof *fresh); /* 新桶全部置为空链。 */
    for (size_t i = 0; i < count; ++i) {
        size_t b = hash_name(symbols[i].name) % n;
        symbols[i].next = fresh[b]; /* 头插：新链首的后继是原链首。 */
        fresh[b] = i + 1;           /* 再让桶指向当前表项。 */
    }
    free(buckets);
    buckets = fresh;
    bucket_count = n;
}

/*
 * 【核心算法：查找或插入；分工负责人：宋坤远（2411241）】
 * 步骤：按词素找桶 → 沿冲突链查重 → 必要时扩容 → 保存新词素 → 头插。
 * 哈希分布均匀时，链长平均为常数；计入字符串处理后平均为 O(L)。
 * 最坏情况下需遍历全部同桶表项，不能保证每次查找都是常数时间。
 */
size_t symbol_intern(const char *name, size_t line, size_t column) {
    if (!bucket_count) rehash(64); /* 首次调用才创建桶数组。 */
    size_t b = hash_name(name) % bucket_count;
    /* link 为 0 时结束；访问数组之前必须减 1 还原真实下标。 */
    for (size_t link = buckets[b]; link; link = symbols[link - 1].next) {
        Symbol *entry = &symbols[link - 1];
        if (strcmp(entry->name, name) == 0) {
            ++entry->occurrences;
            return link - 1; /* 保留首次位置，复用原有编号。 */
        }
    }
    /* 插入前检查负载，达到 0.75 则桶数翻倍，降低平均冲突链长度。 */
    if (count >= bucket_count * 3 / 4) {
        rehash(bucket_count * 2);
        b = hash_name(name) % bucket_count; /* 桶数已改变，旧 b 不再可用。 */
    }
    /* 表项数组满时另行扩容，容量从 32 开始翻倍，摊还插入开销。 */
    if (count == capacity) {
        size_t n = capacity ? capacity * 2 : 32;
        Symbol *fresh = allocate(n * sizeof *fresh);
        /* 这里只复制表项，name 指向的词素内存不移动、不重复分配。 */
        if (count) memcpy(fresh, symbols, count * sizeof *fresh);
        free(symbols);
        symbols = fresh;
        capacity = n;
    }
    /* yytext 会在后续扫描中失效，必须连同末尾 NUL 一起复制词素。 */
    char *copy = allocate(strlen(name) + 1);
    strcpy(copy, name);
    /* 新表项追加到数组末尾，同时作为对应哈希桶的新链首。 */
    symbols[count] = (Symbol){copy, line, column, 1, buckets[b]};
    buckets[b] = count + 1;
    return count++; /* 先返回新项下标，再递增已登记项数。 */
}

/* 【AI生成：符号表展示框架；分工负责人：宋坤远（2411241）】 */
void symbol_table_print(FILE *out) {
    fputs("\n# SYMBOL_TABLE\nINDEX\tLEXEME\tFIRST_LINE\tFIRST_COLUMN\tOCCURRENCES\n", out);
    for (size_t i = 0; i < count; ++i)
        fprintf(out, "%zu\t%s\t%zu\t%zu\t%zu\n", i, symbols[i].name,
                symbols[i].first_line, symbols[i].first_column,
                symbols[i].occurrences);
}

/*
 * 【数据结构资源回收；分工负责人：宋坤远（2411241）】
 * 先释放每个表项拥有的词素，再释放表项数组和桶数组。
 * 置空并清零后，重复调用本函数也不会重复释放旧地址。
 */
void symbol_table_destroy(void) {
    for (size_t i = 0; i < count; ++i) free(symbols[i].name);
    free(symbols);
    free(buckets);
    symbols = NULL;
    buckets = NULL;
    count = capacity = bucket_count = 0;
}
