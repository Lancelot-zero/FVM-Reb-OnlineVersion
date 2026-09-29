// ============================================================
// const_str.cpp — 名字表本体 + 灌池（清单在 const_str.h，这里只是展开）
//   这是全项目**唯一**该出现字符串字面量的地方（名单本身就是字符串）
// ============================================================
#include "const_str.h"
#include "vm_def.h"

static const char* kNames[] = {
#define NAME_STR(n) #n,
    NAME_LIST(NAME_STR)
#undef NAME_STR
};

static_assert((int)(sizeof(kNames) / sizeof(kNames[0])) == S_LAST_,
              "const_str.h 的清单和字符串表对不上");

void seed_names() {
    g_strings.reserve((size_t)S_LAST_ + 256);
    for (int i = 0; i < S_LAST_; i++) intern_str(kNames[i]);
}
