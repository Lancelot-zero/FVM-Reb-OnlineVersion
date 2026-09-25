package main

import (
	"os"
	"strings"
	"testing"
)

// lintDiag 便捷断言：检查指定行是否存在包含关键字的 error/warning
func hasDiag(diags []Diagnostic, line int, severity, contains string) bool {
	for _, d := range diags {
		if d.Line == line && d.Severity == severity && strings.Contains(d.Message, contains) {
			return true
		}
	}
	return false
}

func TestValidScript(t *testing.T) {
	code := `// 完整示例核心片段
_VM_ROOM_READY_ENTRY {
    VM_BanCard("small_fire")
    VM_SetCardLevelCap(10)
    VM_SetMaxSlots(10)
    VM_SetTerrain(-1, -1, "obstacle")
}

_VM_BATTLE_START {
    VM_SetFlame(3000)
    a_cnt = 0
    a = VM_CreatePlatform(0, 0, 4, 5, 0, 0, 0, "spr_raft_platform")
    b1 = VM_CreatePlatform(6, 0, 5, 1, 1, 1, 480, "spr_rainbow_1")
    VM_SpawnEnemy("normal_mouse", 0, 100)
}

_VM_CARD_CREATED {
    id = VM_GetLastCreatedCard()
    name = VM_GetProp(id, "plant_id")
    if (name == "egg_boiler_pult") {
        VM_SetProp(id, "atk", VM_GetProp(id, "atk") * 2)
    } elif (name == "coffee_pot") {
        VM_SetProp(id, "hp", VM_GetProp(id, "hp") + 500)
    } else {
        VM_ShellPrint("其他卡片", name)
    }
}

_VM_PLATFORM_IDLE_END {
    if (VM_GetLastIdlePlatform() == a) {
        if (a_cnt == 0) { VM_SetPlatformParams(a, 0, 2, 200, 1) }
        a_cnt = (a_cnt + 1) % 4
    }
    halt
}
`
	diags := lintScript(code)
	for _, d := range diags {
		if d.Severity == "error" {
			t.Errorf("合法脚本不应有错误诊断: 行%d [%s] %s", d.Line, d.Severity, d.Message)
		}
	}
}

func TestUnknownFunction(t *testing.T) {
	code := `_VM_BATTLE_START {
    VM_NoSuchFunc(1, 2)
}
`
	diags := lintScript(code)
	if !hasDiag(diags, 2, "error", "未知函数 VM_NoSuchFunc") {
		t.Errorf("应报未知函数，得到 %+v", diags)
	}
}

func TestArgCount(t *testing.T) {
	cases := []struct {
		code string
		line int
		msg  string
	}{
		{"_VM_BATTLE_START {\n    VM_SetFlame(1, 2)\n}\n", 2, "最多 1 个参数"},
		{"_VM_BATTLE_START {\n    VM_CreatePlatform(0, 0)\n}\n", 2, "至少需要 8 个参数"},
		{"_VM_BATTLE_START {\n    VM_GetWave(1)\n}\n", 2, "最多 0 个参数"},
	}
	for _, c := range cases {
		diags := lintScript(c.code)
		if !hasDiag(diags, c.line, "error", c.msg) {
			t.Errorf("代码 %q 应报 %q，得到 %+v", c.code, c.msg, diags)
		}
	}
}

func TestStringEnum(t *testing.T) {
	// 编译器实测强制的枚举 → error
	errorCases := []struct {
		code string
		msg  string
	}{
		{`_VM_BATTLE_START {
    VM_SpawnEnemy("no_such_enemy", 0, 100)
}
`, "未知敌人ID"},
		{`_VM_ROOM_READY_ENTRY {
    VM_BanCard("no_such_card")
}
`, "未知卡片ID"},
		{`_VM_BATTLE_START {
    VM_SpawnObject("not_an_object", 0, 0)
}
`, "未知物件名"},
		{`_VM_BATTLE_START {
    VM_SpawnPlant("no_such_card", 0, 0, 0, 0, 0)
}
`, "未知卡片ID"},
	}
	for _, c := range errorCases {
		diags := lintScript(c.code)
		if !hasDiag(diags, 2, "error", c.msg) {
			t.Errorf("代码 %q 应报 error %q，得到 %+v", c.code, c.msg, diags)
		}
	}
	// 编译器不校验、仅文档约定的枚举 → warning
	warnCases := []struct {
		code string
		msg  string
	}{
		{`_VM_ROOM_READY_ENTRY {
    VM_SetTerrain(-1, -1, "lava_terrain")
}
`, "未知地形类型"},
		{`_VM_ROOM_READY_ENTRY {
    VM_BanGem("not_a_gem")
}
`, "未知宝石名"},
	}
	for _, c := range warnCases {
		diags := lintScript(c.code)
		if !hasDiag(diags, 2, "warning", c.msg) {
			t.Errorf("代码 %q 应报 warning %q，得到 %+v", c.code, c.msg, diags)
		}
	}
	// 合法值不报错
	ok := `_VM_BATTLE_START {
    VM_SpawnBoss("arno", 2, 80000)
}
`
	for _, d := range lintScript(ok) {
		if d.Severity == "error" {
			t.Errorf("arno 是合法 BOSS，不应报错: %+v", d)
		}
	}
}

func TestKeyNames(t *testing.T) {
	code := `_VM_FRAME {
    if (VM_GetKeyDown("SPACE")) {
        VM_ShellPrint("space")
    }
    if (VM_GetKeyPressed("nonsense")) {
        VM_ShellPrint("bad")
    }
}
`
	diags := lintScript(code)
	// 编译器不校验键名，仅文档约定 → warning
	if !hasDiag(diags, 5, "warning", "未知键名") {
		t.Errorf("应警告未知键名，得到 %+v", diags)
	}
	// "SPACE" 不区分大小写，合法
	for _, d := range diags {
		if d.Line == 2 && d.Severity == "error" {
			t.Errorf("SPACE 是合法键名: %+v", d)
		}
	}
}

// TestNoPlacementRestrictions 函数放置不设限（编译器实测不强制，help.md 放置表描述不正确）
func TestNoPlacementRestrictions(t *testing.T) {
	code := `_VM_BATTLE_START {
    VM_BanCard("small_fire")
    VM_BanGem("laser_gem")
    VM_SetCardLevelCap(10)
    VM_SetMaxSlots(10)
    VM_SpawnCats(1)
    VM_SetTerrain(-1, -1, "obstacle")
    VM_CreatePlatform(0, 0, 4, 5, 0, 0, 0, "spr_x")
    VM_SetPlatformParams(0, 0, 2, 200, 1)
    VM_SetMapBackground("bg", 0.02)
    VM_SetEventEnabled(1)
    VM_CreateButton(0, 0, "spr_btn", 1.0, 0, 1, 2)
    VM_SpawnObject("obstacle", 0, 0)
    VM_SpawnPlant("small_fire", 0, 0, 0, 0, 0)
    VM_SpawnEnemy("normal_mouse", 0, 100)
    VM_SpawnBoss("arno", 2, 80000)
}
`
	for _, d := range lintScript(code) {
		if d.Severity == "error" {
			t.Errorf("函数放置不应报错: 行%d [%s] %s", d.Line, d.Severity, d.Message)
		}
		if strings.Contains(d.Message, "应放在") {
			t.Errorf("不应再有放置警告: 行%d %s", d.Line, d.Message)
		}
	}
}

func TestOutsideBlock(t *testing.T) {
	code := `x = 1
VM_SetFlame(100)
_VM_BATTLE_START {
}
`
	diags := lintScript(code)
	if !hasDiag(diags, 1, "error", "事件块") {
		t.Errorf("块外赋值应报错，得到 %+v", diags)
	}
	if !hasDiag(diags, 2, "error", "事件块") {
		t.Errorf("块外调用应报错，得到 %+v", diags)
	}
}

func TestDanglingElifElse(t *testing.T) {
	cases := []struct {
		code string
		line int
	}{
		{`_VM_BATTLE_START {
    x = 1
    elif (x == 2) {
    }
}
`, 3},
		{`_VM_BATTLE_START {
    if (x == 1) {
    }
}
_VM_WAVE_START {
    else {
    }
}
`, 6},
		// if 之后 else 合法
		{`_VM_BATTLE_START {
    if (x == 1) {
        VM_SetFlame(1)
    } else {
        VM_SetFlame(2)
    }
}
`, -1},
		// elif 链合法
		{`_VM_BATTLE_START {
    if (x == 1) {
        VM_SetFlame(1)
    } elif (x == 2) {
        VM_SetFlame(2)
    } else {
        VM_SetFlame(3)
    }
}
`, -1},
		// else if 形式合法（与 elif 等价）
		{`_VM_BATTLE_START {
    if (x == 1) {
        VM_SetFlame(1)
    } else if (x == 2) {
        VM_SetFlame(2)
    }
}
`, -1},
		// else if 后可以接 else（编译器实测）
		{`_VM_BATTLE_START {
    w = 1
    if (w % 2 == 0) {
        VM_SetFlame(VM_GetFlame() + 500)
        VM_ShellPrint("偶数波奖励火苗")
    } else if (w % 3 == 0) {
        VM_ShowNotice("三的倍数波")
    } else {
        VM_ShellPrint("奇数波: ", w)
    }
}
`, -1},
		// else if 后接 elif 混用（编译器实测，可与 elif 混用）
		{`_VM_BATTLE_START {
    if (x == 1) {
    } else if (x == 2) {
    } elif (x == 3) {
    }
}
`, -1},
		// else 之后链条终结：再跟 else 报错
		{`_VM_BATTLE_START {
    if (x == 1) {
    } else {
    } else {
    }
}
`, 4},
		// else 之后链条终结：再跟 elif 报错
		{`_VM_BATTLE_START {
    if (x == 1) {
    } else {
    } elif (x == 3) {
    }
}
`, 4},
	}
	for _, c := range cases {
		diags := lintScript(c.code)
		if c.line > 0 {
			if !hasDiag(diags, c.line, "error", "游离") {
				t.Errorf("代码 %q 应报游离 elif/else，得到 %+v", c.code, diags)
			}
		} else {
			for _, d := range diags {
				if strings.Contains(d.Message, "游离") {
					t.Errorf("代码 %q 不应报游离错误，得到 %+v", c.code, diags)
				}
			}
		}
	}
}

// TestWhileLoop while/break/continue 支持（与编译器实现同步）
func TestWhileLoop(t *testing.T) {
	// 合法：基础 while + 嵌套 + break/continue + 条件内函数调用
	ok := `_VM_BATTLE_START {
    i = 0
    while (i < 3) {
        i = i + 1
        if (i == 2) {
            continue
        }
        j = 0
        while (j < 10) {
            j = j + 1
            if (j == 2) { break }
        }
    }
    while (VM_GetEnemyCount() > 0) {
        break
    }
}
`
	for _, d := range lintScript(ok) {
		if d.Severity == "error" {
			t.Errorf("合法 while 不应报错: 行%d [%s] %s", d.Line, d.Severity, d.Message)
		}
	}
	// 非法用例
	bad := []struct {
		code string
		line int
		msg  string
	}{
		{`_VM_BATTLE_START {
    break
}
`, 2, "while 循环内"},
		{`_VM_BATTLE_START {
    continue
}
`, 2, "while 循环内"},
		{`_VM_BATTLE_START {
    while (x < 3)
    x = x + 1
}
`, 2, "while 后必须跟 {"},
		{`_VM_BATTLE_START {
    while () {
    }
}
`, 2, "条件不能为空"},
		{`_VM_BATTLE_START {
    while = 1
}
`, 2, "保留字"},
		{`_VM_BATTLE_START {
    break = 1
}
`, 2, "保留字"},
		// while 闭合后不能接 elif/else
		{`_VM_BATTLE_START {
    while (x < 3) {
    }
    elif (x == 1) {
    }
}
`, 4, "游离"},
		// 循环不能写在事件块外
		{`while (x < 3) {
}
`, 1, "事件块"},
		{`_VM_BATTLE_START {
    while (x < 3
}
`, 2, "右括号"},
	}
	for _, c := range bad {
		diags := lintScript(c.code)
		if !hasDiag(diags, c.line, "error", c.msg) {
			t.Errorf("代码 %q 应报 %q（第 %d 行），得到 %+v", c.code, c.msg, c.line, diags)
		}
	}
}

func TestReservedWords(t *testing.T) {
	code := `_VM_BATTLE_START {
    if = 1
    halt = 2
}
`
	diags := lintScript(code)
	if !hasDiag(diags, 2, "error", "保留字") {
		t.Errorf("if 用作变量名应报错，得到 %+v", diags)
	}
	if !hasDiag(diags, 3, "error", "保留字") {
		t.Errorf("halt 用作变量名应报错，得到 %+v", diags)
	}
}

func TestBraceMismatch(t *testing.T) {
	// 未闭合
	code := `_VM_BATTLE_START {
    VM_SetFlame(1)
`
	diags := lintScript(code)
	if !hasDiag(diags, 1, "error", "缺少右花括号") {
		t.Errorf("未闭合块应报错，得到 %+v", diags)
	}
	// 多余右括号
	code2 := `_VM_BATTLE_START {
    VM_SetFlame(1)
}
}
`
	diags2 := lintScript(code2)
	if !hasDiag(diags2, 4, "error", "多余") {
		t.Errorf("多余 } 应报错，得到 %+v", diags2)
	}
}

func TestSemicolonAsNewline(t *testing.T) {
	code := `_VM_BATTLE_START {
    x = 1; y = 2
    VM_SetFlame(1); VM_SetFlame(2)
}
`
	diags := lintScript(code)
	for _, d := range diags {
		t.Errorf("; 视作换行，不应有诊断: 行%d [%s] %s", d.Line, d.Severity, d.Message)
	}
}

func TestCommentAndString(t *testing.T) {
	code := `// VM_NoSuchFunc(1) 注释里的不算
_VM_BATTLE_START {
    // 行尾注释 VM_NoSuchFunc(2)
    VM_ShellPrint("a;b // 不是注释也不是分号")
    VM_SetFlame(100) // 行尾注释
}
`
	diags := lintScript(code)
	for _, d := range diags {
		t.Errorf("注释与字符串处理错误: 行%d [%s] %s", d.Line, d.Severity, d.Message)
	}
}

// TestLogicalOperators 编译器支持 && ||（含嵌套括号）；单个 & | 仍应报错
func TestLogicalOperators(t *testing.T) {
	ok := []string{
		"_VM_BATTLE_START {\n    if (1 == 1 && 2 == 2) { VM_ShellPrint(\"x\") }\n}\n",
		"_VM_BATTLE_START {\n    if (1 == 1 || 2 == 2) { VM_ShellPrint(\"x\") }\n}\n",
		"_VM_BATTLE_START {\n    if ((1 == 1 && 2 == 2) || (3 == 3 && 4 == 4)) { VM_ShellPrint(\"x\") }\n}\n",
		"_VM_BATTLE_START {\n    if (1 == 1 || 2 == 2 && 3 == 3) { VM_ShellPrint(\"x\") }\n}\n",
		"_VM_BATTLE_START {\n    VM_ShellPrint(\"a && b | c\")\n}\n",                         // 字符串内的不受影响
		"_VM_BATTLE_START {\n    if (1 == 1) { if (2 == 2) { VM_ShellPrint(\"嵌套\") } }\n}\n", // 嵌套 if 仍然合法
	}
	for _, code := range ok {
		diags := lintScript(code)
		for _, d := range diags {
			if strings.Contains(d.Message, "不支持") {
				t.Errorf("代码 %q 不应报不支持运算符，得到 %+v", code, diags)
			}
		}
	}

	bad := []string{
		"_VM_BATTLE_START {\n    x = 1 & 2\n}\n",
		"_VM_BATTLE_START {\n    x = 1 | 2\n}\n",
	}
	for _, code := range bad {
		diags := lintScript(code)
		if !hasDiag(diags, 2, "error", "不支持单个") {
			t.Errorf("代码 %q 应报不支持单个运算符，得到 %+v", code, diags)
		}
	}
}

func TestInlineBraceAndOneLineIf(t *testing.T) {
	code := `_VM_BATTLE_START { VM_SetFlame(3000) }
_VM_FRAME {
    if (x == 0) { VM_SetFlame(1) } elif (x == 1) { VM_SetFlame(2) } else { VM_SetFlame(3) }
}
`
	diags := lintScript(code)
	for _, d := range diags {
		t.Errorf("行内花括号写法应合法: 行%d [%s] %s", d.Line, d.Severity, d.Message)
	}
}

// TestOneLineIfAssignHalt 单行 if 内含赋值 / halt / exit（用户报告：报"缺少右花括号"误报）
func TestOneLineIfAssignHalt(t *testing.T) {
	// 用户的实际代码
	userCode := `_VM_BUTTON_CLICKED {
    b = VM_GetLastClickedButton()
    if (b >= 0) {
        phase = phase + 1
        if (phase > 2) { phase = 0 }
        VM_ShellPrint("按钮: ", b, " phase: ", phase)
    }
}
`
	for _, d := range lintScript(userCode) {
		if d.Severity == "error" {
			t.Errorf("用户代码不应报错: 行%d [%s] %s", d.Line, d.Severity, d.Message)
		}
	}
	// 单行 if 内各种语句形式
	cases := []string{
		`_VM_BATTLE_START {
    if (x == 1) { y = 2 }
    if (x == 2) { halt }
    if (x == 3) { exit }
    if (x == 4) { y = VM_Random(1, 10) }
    x = "}"
}
`,
	}
	for _, code := range cases {
		for _, d := range lintScript(code) {
			if d.Severity == "error" {
				t.Errorf("单行 if 写法应合法: 行%d [%s] %s", d.Line, d.Severity, d.Message)
			}
		}
	}
}

func TestUnknownEventBlock(t *testing.T) {
	code := `_VM_NOT_A_BLOCK {
    VM_SetFlame(1)
}
`
	diags := lintScript(code)
	if !hasDiag(diags, 1, "error", "未知事件块") {
		t.Errorf("未知事件块应报错，得到 %+v", diags)
	}
}

// TestTemplateLintsClean 新建模板（GetTemplate）必须无语法错误
func TestTemplateLintsClean(t *testing.T) {
	tpl := NewApp().GetTemplate()
	for _, d := range lintScript(tpl) {
		if d.Severity == "error" {
			t.Errorf("模板不应有语法错误: 行%d %s", d.Line, d.Message)
		}
	}
	// 模板应覆盖全部 34 个事件块
	for name := range eventBlocks {
		if !strings.Contains(tpl, name+" {") {
			t.Errorf("模板缺少事件块 %s", name)
		}
	}
}

// TestExplorerExe explorer.exe 定位兜底（PATH 里可能没有）
func TestExplorerExe(t *testing.T) {
	p := explorerExe()
	if _, err := os.Stat(p); err != nil {
		t.Errorf("explorer.exe 无法定位: %s (%v)", p, err)
	}
}

// TestFuncDescsComplete 每个函数都有中文描述（悬停提示/补全用）
func TestFuncDescsComplete(t *testing.T) {
	for name := range funcTable {
		if funcDescs[name] == "" {
			t.Errorf("函数 %s 缺少中文描述", name)
		}
	}
	if len(funcDescs) != len(funcTable) {
		t.Errorf("funcDescs(%d) 与 funcTable(%d) 数量不一致", len(funcDescs), len(funcTable))
	}
}

// TestEnclosingEventAt 光标所在块的定位（状态栏"块提示"用）
func TestEnclosingEventAt(t *testing.T) {
	code := `// 头部注释
_VM_BATTLE_START {
    if (x == 1) {
        VM_SetFlame(1)
    } else {
        VM_SetFlame(2)
    }
}
_VM_CARD_CREATED {
    VM_GetLastCreatedCard()
}
`
	cases := []struct {
		line int
		want string // 期望块名；"" = 块外
	}{
		{1, ""},                  // 块外注释
		{2, "_VM_BATTLE_START"},  // 块开行
		{3, "_VM_BATTLE_START"},  // if 行
		{4, "_VM_BATTLE_START"},  // if 体内
		{5, "_VM_BATTLE_START"},  // } else { 行（截断后仍在块内）
		{7, "_VM_BATTLE_START"},  // if 链收尾 } 行
		{8, "_VM_BATTLE_START"},  // 事件块收尾 } 行（截断后仍算块内）
		{9, "_VM_CARD_CREATED"},  // 下一块开行
		{10, "_VM_CARD_CREATED"}, // 块内调用
		{11, "_VM_CARD_CREATED"}, // 收尾 } 行
		{99, ""},                 // 越界
	}
	for _, c := range cases {
		ev := enclosingEventAt(code, c.line)
		got := ""
		if ev != nil {
			got = ev.name
		}
		if got != c.want {
			t.Errorf("第 %d 行期望块 %q，得到 %q", c.line, c.want, got)
		}
	}
	// 块提示数据完整（help.md 事件块表 25 个全有）
	if len(blockHints) != len(eventBlocks) {
		t.Errorf("blockHints(%d) 与 eventBlocks(%d) 数量不一致", len(blockHints), len(eventBlocks))
	}
}

func TestFrameWarning(t *testing.T) {
	code := `_VM_FRAME {
    VM_SpawnEnemy("normal_mouse", 0, 100)
}
`
	diags := lintScript(code)
	if !hasDiag(diags, 2, "warning", "_VM_FRAME") {
		t.Errorf("_VM_FRAME 中生成敌人应给警告，得到 %+v", diags)
	}
}

func TestUnclosedString(t *testing.T) {
	code := "_VM_BATTLE_START {\n    VM_ShellPrint(\"abc)\n}\n"
	diags := lintScript(code)
	if !hasDiag(diags, 2, "error", "结束引号") {
		t.Errorf("未闭合字符串应报错，得到 %+v", diags)
	}
}

// TestHelpExampleLintsClean 用 help.md 的完整示例做回归（编译器实测该示例可编译通过）
func TestHelpExampleLintsClean(t *testing.T) {
	idx := strings.Index(helpMarkdown, "## 完整示例")
	if idx < 0 {
		t.Skip("help.md 中未找到完整示例")
	}
	rest := helpMarkdown[idx:]
	fenceStart := strings.Index(rest, "```")
	if fenceStart < 0 {
		t.Skip("未找到代码块")
	}
	rest = rest[fenceStart+3:]
	fenceEnd := strings.Index(rest, "```")
	if fenceEnd < 0 {
		t.Skip("代码块未闭合")
	}
	code := rest[:fenceEnd]
	diags := lintScript(code)
	for _, d := range diags {
		if d.Severity == "error" {
			t.Errorf("help.md 完整示例不应有错误诊断: 行%d %s", d.Line, d.Message)
		}
	}
}

// TestTypeCheck 参数类型检查（compiler_defs.h 的 param_types，编译器实测强制）
func TestTypeCheck(t *testing.T) {
	bad := []struct {
		code string
		msg  string
	}{
		{"_VM_BATTLE_START {\n    VM_SetFlame(\"x\")\n}\n", "应为 int，得到 string"},
		{"_VM_BATTLE_START {\n    VM_SetFlame(1.5)\n}\n", "应为 int，得到 float"},
		{"_VM_BATTLE_START {\n    VM_PlaySound(1)\n}\n", "应为 string，得到 int"},
		{"_VM_BATTLE_START {\n    VM_ClearMapObjects(0, 0, 123)\n}\n", "应为 string，得到 int"},
		// SpawnPlantsRandom 卡片槽位是 string 类型（编译器实测数字 -1 会报错）
		{"_VM_BATTLE_START {\n    VM_SpawnPlantsRandom(0,0,1,1,0,0,0, \"small_fire\", -1,-1,-1,-1,-1,-1,-1,-1)\n}\n", "应为 string，得到 int"},
	}
	for _, c := range bad {
		diags := lintScript(c.code)
		if !hasDiag(diags, 2, "error", c.msg) {
			t.Errorf("代码 %q 应报 %q，得到 %+v", c.code, c.msg, diags)
		}
	}
	// 合法：int→float 自动提升；字符串槽传 "-1" 字符串；表达式跳过
	ok := `_VM_BATTLE_START {
    VM_SetMapBackground("bg", 2)
    VM_SetMapBackground("bg", 0.02)
    VM_SpawnPlantsRandom(0,0,1,1,0,0,0, "small_fire", "-1","-1","-1","-1","-1","-1","-1","-1")
    id = VM_GetLastCreatedCard()
    VM_SetProp(id, "atk", VM_GetProp(id, "atk") * 2)
}
`
	for _, d := range lintScript(ok) {
		if d.Severity == "error" {
			t.Errorf("类型合法代码不应报错: 行%d [%s] %s", d.Line, d.Severity, d.Message)
		}
	}
}

// TestVarargs 变长参数函数（compiler_defs.h 中 param_count=-1）
func TestVarargs(t *testing.T) {
	// VM_ShellPrint 游戏 VM 实测上限 16 个参数（编译期不拦，运行时截断）
	ok := `_VM_BATTLE_START {
    VM_ShellPrint()
    VM_ShellPrint(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16)
    VM_ShowNotice()
    VM_ShowNoticeDur("msg", 30)
    VM_ShowNoticeDur("msg")
}
`
	for _, d := range lintScript(ok) {
		if d.Severity == "error" {
			t.Errorf("变长参数函数合法用法不应报错: 行%d [%s] %s", d.Line, d.Severity, d.Message)
		}
	}
	bad := `_VM_BATTLE_START {
    VM_ShellPrint(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17)
}
`
	if diags := lintScript(bad); !hasDiag(diags, 2, "error", "最多 16 个参数") {
		t.Errorf("VM_ShellPrint 超过 16 个参数应报错，得到 %+v", diags)
	}
}

// TestCompilerExtraFunctions help.md 未收录但编译器支持的新函数（从编译器二进制提取，签名实测）
func TestCompilerExtraFunctions(t *testing.T) {
	code := `_VM_BATTLE_START {
    VM_ClearMapObjects(0, 0, "obstacle")
    VM_ClearMapObjects(-1, -1, "all")
    VM_FreeSpritePerm("spr_raft_platform")
    VM_LoadSpritePerm("spr_rainbow_1", 4)
    VM_RefreshPlatformSnapshots()
}
`
	diags := lintScript(code)
	for _, d := range diags {
		if d.Severity == "error" {
			t.Errorf("新函数应合法: 行%d [%s] %s", d.Line, d.Severity, d.Message)
		}
	}
	// 参数个数错误仍要报
	bad := `_VM_BATTLE_START {
    VM_ClearMapObjects(0, 0)
}
`
	if diags := lintScript(bad); !hasDiag(diags, 2, "error", "至少需要 3 个参数") {
		t.Errorf("VM_ClearMapObjects 参数个数应校验，得到 %+v", diags)
	}
}

func TestCrossBlockVariableOk(t *testing.T) {
	code := `_VM_BATTLE_START {
    cnt = 0
}
_VM_PLATFORM_IDLE_END {
    cnt = cnt + 1
}
`
	diags := lintScript(code)
	for _, d := range diags {
		t.Errorf("跨块变量应合法: 行%d [%s] %s", d.Line, d.Severity, d.Message)
	}
}

// TestCustomBlocks 自定义块 _DEFINE_BLOCK_xxx：定义 / 调用 / 各种非法写法
func TestCustomBlocks(t *testing.T) {
	// 定义 + 调用 + exit：合法（允许先调用后定义）
	ok := `_VM_BATTLE_START {
    _DEFINE_BLOCK_add()
}

_DEFINE_BLOCK_add {
    VM_SetFlame(VM_GetFlame() + 100)
    exit
}
`
	for _, d := range lintScript(ok) {
		if d.Severity == "error" {
			t.Errorf("自定义块合法脚本报错: 行%d [%s] %s", d.Line, d.Severity, d.Message)
		}
	}

	// 调用没定义的块
	undef := `_VM_BATTLE_START {
    _DEFINE_BLOCK_nope()
}
`
	if diags := lintScript(undef); !hasDiag(diags, 2, "error", "_DEFINE_BLOCK_nope") {
		t.Errorf("未定义的自定义块没报错: %+v", diags)
	}

	// 自定义块调用不能带参数
	withArgs := `_VM_BATTLE_START {
    _DEFINE_BLOCK_p(1)
}
_DEFINE_BLOCK_p {
    VM_SetFlame(1)
}
`
	if diags := lintScript(withArgs); !hasDiag(diags, 2, "error", "_DEFINE_BLOCK_p") {
		t.Errorf("带参数的自定义块调用没报错: %+v", diags)
	}

	// 自定义块里不能再调块
	nested := `_VM_BATTLE_START {
    _DEFINE_BLOCK_p()
}
_DEFINE_BLOCK_p {
    _DEFINE_BLOCK_q()
}
_DEFINE_BLOCK_q {
    VM_SetFlame(1)
}
`
	if diags := lintScript(nested); !hasDiag(diags, 5, "error", "_DEFINE_BLOCK_q") {
		t.Errorf("自定义块里调块没报错: %+v", diags)
	}

	// 定义必须写最外层
	inner := `_VM_BATTLE_START {
    _DEFINE_BLOCK_p {
        VM_SetFlame(1)
    }
}
`
	if diags := lintScript(inner); !hasDiag(diags, 2, "error", "_DEFINE_BLOCK_p") {
		t.Errorf("块内定义自定义块没报错: %+v", diags)
	}
}
