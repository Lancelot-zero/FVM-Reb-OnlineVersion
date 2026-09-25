// LabMap 脚本语言模式：高亮 + 自动补全
import { StreamLanguage, foldService } from "@codemirror/language";
import { completeFromList, snippet } from "@codemirror/autocomplete";

// 事件块（与后端 linter.go 一致）
export const EVENT_BLOCKS = [
  "_VM_ROOM_READY_ENTRY", "_VM_BATTLE_START", "_VM_WAVE_START", "_VM_WAVE_END",
  "_VM_SUBWAVE_START", "_VM_SUBWAVE_END", "_VM_CARD_CREATED", "_VM_CARD_DESTROYED",
  "_VM_CARD_DAMAGED", "_VM_CARD_PREVIEW_PICKED", "_VM_ENEMY_SPAWNED", "_VM_ENEMY_KILLED",
  "_VM_ENEMY_DAMAGED", "_VM_PLAYER_DAMAGED", "_VM_BOSS_STATE_CHANGE", "_VM_PLATFORM_IDLE_END",
  "_VM_MOUSE_LEFT", "_VM_MOUSE_RIGHT", "_VM_KEY_PRESSED", "_VM_BUTTON_CLICKED",
  "_VM_FRAME", "_VM_TIMER_5f", "_VM_TIMER_10f", "_VM_TIMER_15f", "_VM_TIMER_30f", "_VM_TIMER_60f",
  // mod 对象块（卡 / 武器 / 宝石 / 子弹 / 特效 / 敌人）
  "_OBJECT_CFG", "_OBJECT_CREATE", "_OBJECT_STEP", "_OBJECT_DRAW", "_OBJECT_DESTROY",
  "_OBJECT_MOUSE_ENTER", "_OBJECT_MOUSE_LEAVE", "_OBJECT_CLICK",
];

// 函数名（用于高亮与补全）
export const FUNC_NAMES = [
  "VM_BanCard", "VM_BanGem", "VM_SetCardLevelCap", "VM_SetMaxSlots", "VM_SpawnCats",
  "VM_SetTerrain", "VM_SetRowFeature", "VM_GetTerrain",
  "VM_CreatePlatform", "VM_SpawnPlant", "VM_SpawnEnemy", "VM_SpawnBoss", "VM_SpawnObject",
  "VM_SpawnPlantsRandom", "VM_CreateButton",
  "VM_GetProp", "VM_SetProp", "VM_SetCardProp", "VM_SetEnemyProp", "VM_ApplyPlantLevel",
  "VM_GetKilledProp",
  "VM_GetWave", "VM_GetSubwave", "VM_GetFlame", "VM_GetEnemyCount", "VM_GetPlantCount",
  "VM_GetPlantCountAt", "VM_GetPlantAt",
  "VM_GetLastBoss", "VM_GetLastCreatedEnemy", "VM_GetLastKilledEnemy",
  "VM_GetLastCreatedCard", "VM_GetLastDestroyedCard", "VM_GetLastIdlePlatform",
  "VM_GetLastClickedButton",
  "VM_GetMouseX", "VM_GetMouseY", "VM_GetMouseCol", "VM_GetMouseRow",
  "VM_GetMousePressed", "VM_GetKeyDown", "VM_GetKeyPressed",
  "VM_ClearMapObjects", "VM_ClearPlants", "VM_ClearPlantsByType", "VM_WakePlants",
  "VM_SwapPlants", "VM_SwapPlantRects", "VM_CompactColumn", "VM_CompactColumnRev",
  "VM_CompactRow", "VM_CompactRowRev",
  "VM_LoadSprite", "VM_LoadSpriteFrames", "VM_LoadSpriteFrames_Ex",
  "VM_LoadSpritePerm", "VM_FreeSpritePerm", "VM_GetLoadedSpriteName", "VM_AliasSprite",
  "VM_SetPlatformParams", "VM_RefreshPlatformSnapshots",
  "VM_SetMapBackground", "VM_SetDrawSlot_front", "VM_SetDrawSlot",
  "VM_Random", "VM_SetFlame", "VM_PlaySound", "VM_IsUndefined", "VM_IsDestroyed", "VM_LoadSound", "VM_Floor", "VM_SetEventEnabled",
  "VM_GameWin", "VM_GameLose",
  "VM_ShellPrint", "VM_ShowNotice", "VM_ShowNoticeDur",
  "VM_SetCardSlotProp", "VM_CalcCardSlotProp", "VM_GetCardSlotCount", "VM_GetPreviewCard",
  "VM_ArrayGet", "VM_ArraySet", "VM_ArrayDel", "VM_ArrayADD", "VM_ArraySize",
  "VM_ArrayClear", "VM_ArrayClearAll", "VM_SetNoticeStyle",
  "VM_conveyor_belt_able", "VM_Slot_add",
  "VM_BanAllCard", "VM_CannelBanCard", "VM_BanWeapon", "VM_BanSuperWeapon",
  "VM_BanShield", "VM_SetCardShapeCap", "VM_SetCardSkillCap",
  "VM_Ceil", "VM_SpawnBatMouse", "VM_GetTimeLimit", "VM_SetTimeLimit",
  "VM_SetDrawSlotEx", "VM_SetDrawSlotEx_front", "VM_SetWaveAuto", "VM_SetWave",
  "VM_GetCurCard", "VM_EnemyInRange", "VM_GetHomingTarget", "VM_GetInstancesInRange", "VM_CreateInstance", "VM_LoadSpritePerm_Ex", "VM_SetShovelFlameRate",
  "VM_BulletScreenAdd", "VM_DrawSpriteExt", "VM_RunStep", "VM_DestroyInstance", "VM_GetCardProp", "VM_CanPlace", "VM_CallFunc", "VM_FuncExists", "VM_FuncDesc",
  "VM_SpriteExists",
  "VM_AliasSpritePerm",
  "VM_ArrayExists", "VM_CellCount", "VM_CellItem", "VM_CellContains", "VM_ArrayContains", "VM_InstArrayExists", "VM_InstArraySize", "VM_InstArrayItem", "VM_InstArraySet", "VM_InstArrayAdd", "VM_InstArrayDel", "VM_InstArrayClear", "VM_InstArrayContains", "VM_HomingBulletAdd", "VM_DamageEnemy", "VM_DamageEnemyAsh",
  "VM_BulletScreenAdd_Ex",
  "VM_BulletScreenAdd_Exs",
];

const eventSet = new Set(EVENT_BLOCKS);
const funcSet = new Set(FUNC_NAMES);
const keywords = new Set(["if", "elif", "else", "halt", "exit", "while", "break", "continue"]);

// 自定义块 _DEFINE_BLOCK_xxx（xxx = 字母/数字/下划线，与后端 linter.go 一致）
const CUSTOM_BLOCK_PREFIX = "_DEFINE_BLOCK_";

export function isCustomBlockName(s) {
  if (!s.startsWith(CUSTOM_BLOCK_PREFIX) || s.length === CUSTOM_BLOCK_PREFIX.length) return false;
  return /^\w*$/.test(s.slice(CUSTOM_BLOCK_PREFIX.length));
}

// collectCustomBlocks 扫描文档收集自定义块定义，返回 名字 → 定义行号
function collectCustomBlocks(doc) {
  const defs = new Map();
  for (let i = 1; i <= doc.lines; i++) {
    const m = doc.line(i).text.match(/^\s*(_DEFINE_BLOCK_\w*)\s*\{/);
    if (m && isCustomBlockName(m[1]) && !defs.has(m[1])) defs.set(m[1], i);
  }
  return defs;
}

// customBlockComment 取某个自定义块"定义处上面那几行 // 注释"当说明（中间最多隔一个空行）
export function customBlockComment(doc, name) {
  const lineNo = collectCustomBlocks(doc).get(name);
  if (!lineNo) return "";
  const lines = [];
  let blank = 0;
  for (let n = lineNo - 1; n >= 1; n--) {
    const t = doc.line(n).text.trim();
    if (t.startsWith("//")) {
      lines.unshift(t.replace(/^\/\/\s?/, ""));
      blank = 0;
      continue;
    }
    if (t === "") {
      if (++blank > 1) break; // 注释和定义之间最多隔一个空行
      continue;
    }
    break;
  }
  return lines.join("\n");
}

// customBlockCompletions 自定义块补全（按文档内已定义的名字，前缀过滤）
export function customBlockCompletions(context) {
  const word = context.matchBefore(/[A-Za-z_]\w*/);
  if (!word || word.from === word.to) return null;
  const doc = context.state.doc;
  const defs = collectCustomBlocks(doc);
  const options = [];
  for (const [name, lineNo] of defs) {
    if (!name.startsWith(word.text) || name === word.text) continue;
    const cmt = customBlockComment(doc, name).split("\n")[0];
    options.push({
      label: name,
      type: "keyword",
      detail: "自定义块（第 " + lineNo + " 行定义）" + (cmt ? "：" + cmt : ""),
      info: customBlockComment(doc, name),
      apply: name + "()",
      boost: 4,
    });
  }
  if (!options.length) return null;
  return { from: word.from, options };
}

// collectVars 扫描文档收集已定义的变量（赋值语句左值），返回 变量名 → 定义行号
function collectVars(doc) {
  const vars = new Map();
  for (let i = 1; i <= doc.lines; i++) {
    const line = doc.line(i).text;
    // 去掉注释与字符串内容（替换成空格，避免误匹配）
    let s = "";
    let inStr = false;
    for (let j = 0; j < line.length; j++) {
      const c = line[j];
      if (c === '"') {
        inStr = !inStr;
        s += " ";
        continue;
      }
      if (inStr) {
        s += " ";
        continue;
      }
      if (c === "/" && line[j + 1] === "/") break;
      s += c;
    }
    // 赋值：标识符后面跟 =（排除 == 等比较；= 前面不能是 =!<>）
    const re = /(^|[^\w=!<>])([A-Za-z_]\w*)\s*=(?!=)/g;
    let m;
    while ((m = re.exec(s))) {
      const name = m[2];
      if (keywords.has(name)) continue;
      if (!vars.has(name)) vars.set(name, i);
    }
  }
  return vars;
}

// variableCompletions 变量名补全（按已定义的变量，前缀过滤）
export function variableCompletions(context) {
  const word = context.matchBefore(/[A-Za-z_]\w*/);
  if (!word || word.from === word.to) return null;
  const vars = collectVars(context.state.doc);
  const options = [];
  for (const [name, lineNo] of vars) {
    if (!name.startsWith(word.text)) continue; // 前缀过滤
    if (name === word.text) continue;
    options.push({
      label: name,
      type: "variable",
      detail: "变量（第 " + lineNo + " 行定义）",
      boost: 4, // 高于块/函数，变量匹配优先展示
    });
  }
  if (!options.length) return null;
  return { from: word.from, options };
}

// makeLabLanguage 创建语言模式；meta 为 Go 端 GetEditorMeta 返回的元信息
export function makeLabLanguage(meta = {}) {
  const blockHints = meta.blocks || {};
  const funcInfos = meta.funcs || {};

  const blockCompletions = EVENT_BLOCKS.map((b) => {
    const h = blockHints[b];
    let detail = "事件块";
    if (h) {
      detail = h.desc || "事件块";
      if (h.funcs && h.funcs.length) detail += " · 可用: " + h.funcs.join(", ");
    }
    return {
      label: b,
      type: "keyword",
      detail,
      // snippet：${} 是真实占位符，接受后光标落在块体内
      apply: snippet(b + " {\n\t${}\n}"),
      boost: 3,
    };
  });

  const funcCompletions = FUNC_NAMES.map((f) => {
    const info = funcInfos[f];
    let detail = "函数";
    let apply;
    if (!info) {
      detail = "函数";
      apply = snippet(f + "(${})");
    } else {
      detail = info.desc || "函数";
      if (info.args < 0) {
        // 变长参数：光标落在括号内
        apply = snippet(f + "(${})");
      } else if (info.args === 0) {
        apply = f + "()";
      } else {
        // 固定参数：补全所有逗号，${1}..${N} 占位，Tab 逐个跳转
        const placeholders = Array.from({ length: info.args }, (_, i) => `\${${i + 1}}`).join(", ");
        apply = snippet(f + "(" + placeholders + ")");
      }
    }
    return { label: f, type: "function", detail, apply, boost: 2 };
  });

  const kwCompletions = [...keywords].map((k) => {
    const c = { label: k, type: "keyword", boost: 1 };
    if (k === "while") c.apply = snippet("while (${}) {\n\t${}\n}");
    return c;
  });

  // 自定义块定义：敲 _DEF 就能补出骨架（名字自己取，字母/数字/下划线）
  const customSnippets = [
    {
      label: CUSTOM_BLOCK_PREFIX,
      displayLabel: CUSTOM_BLOCK_PREFIX + "名字 { }",
      type: "keyword",
      detail: "定义自定义块（只写一次，在事件块里用 " + CUSTOM_BLOCK_PREFIX + "名字() 调用）",
      apply: snippet(CUSTOM_BLOCK_PREFIX + "${1} {\n\t${}\n}"),
      boost: 5,
    },
    {
      label: "_DEFINE_BLOCK",
      displayLabel: "_DEFINE_BLOCK…",
      type: "keyword",
      detail: "同上（不用打全前缀）",
      apply: snippet(CUSTOM_BLOCK_PREFIX + "${1} {\n\t${}\n}"),
      boost: 4,
    },
  ];

  const lang = StreamLanguage.define({
    name: "labmap",
    token(stream) {
      if (stream.eatSpace()) return null;
      // 行注释
      if (stream.match("//")) {
        stream.skipToEnd();
        return "comment";
      }
      // 字符串
      if (stream.match(/"/)) {
        while (!stream.eol()) {
          if (stream.next() === '"') return "string";
        }
        return "string";
      }
      // 数字（含负数）
      if (stream.match(/^-?\d+(\.\d+)?/)) return "number";
      // 标识符
      if (stream.match(/[A-Za-z_]\w*/)) {
        const word = stream.current();
        if (eventSet.has(word) || keywords.has(word) || isCustomBlockName(word)) return "keyword";
        if (funcSet.has(word)) return "typeName";
        return null; // 普通变量保持默认颜色
      }
      // 运算符
      if (stream.match(/^(==|!=|>=|<=|&&|\|\|)/)) return "operator";
      if (stream.match(/[+\-*/%=<>!(){},;[\]]/)) return "operator";
      stream.next();
      return null;
    },
    languageData: {
      commentTokens: { line: "//" },
      // 静态补全（块/函数/关键字）+ 文档内已定义变量。
      // 注意：必须合并为单个源函数——数组形式的多个源在
      // 当前 CM 版本组合下会触发管线异常（实测定位）。
      autocomplete: combinedCompletions([
        completeFromList([...blockCompletions, ...funcCompletions, ...kwCompletions, ...customSnippets]),
        variableCompletions,
        customBlockCompletions,
      ]),
    },
  });
  return lang;
}

// combinedCompletions 把多个补全源合并为一个源（避免数组源触发管线 bug）
function combinedCompletions(sources) {
  return (context) => {
    let from = context.pos;
    let options = [];
    let validFor = null;
    for (const src of sources) {
      let r = null;
      try {
        r = src(context);
      } catch (e) {
        console.error("补全源异常:", e);
        continue;
      }
      if (!r || !r.options || !r.options.length) continue;
      if (r.from < from) from = r.from;
      if (r.validFor) validFor = r.validFor;
      options = options.concat(r.options);
    }
    if (!options.length) return null;
    return { from, options, validFor };
  };
}

export const labLanguage = makeLabLanguage();

// ============================================================
// 代码折叠
// ============================================================

// stripLineComment 去掉行内 // 注释（字符串里的 // 不算），供折叠扫描用
function stripLineComment(line) {
  let inStr = false;
  for (let i = 0; i < line.length; i++) {
    const c = line[i];
    if (c === '"') {
      inStr = !inStr;
      continue;
    }
    if (inStr) continue;
    if (c === "/" && line[i + 1] === "/") return line.slice(0, i);
  }
  return line;
}

// labFoldFn 计算某一行的可折叠范围：
//   · `... {` 花括号块 —— 折到闭合 } 所在行行首（} 留着，折叠后显示成 `{ ⋯ }`）
//   · 连续 // 注释段 —— 折到注释段最后一行行尾
// 返回 {from, to} 或 null；只需 state.doc，便于单测
export function labFoldFn(state, lineStart, lineEnd) {
  const doc = state.doc;
  const line = doc.lineAt(lineStart);
  const raw = doc.sliceString(lineStart, lineEnd);
  const code = stripLineComment(raw).trim();

  // 连续 // 注释段
  if (code === "" && raw.trimStart().startsWith("//")) {
    let last = line.number;
    for (let n = line.number + 1; n <= doc.lines; n++) {
      if (!doc.line(n).text.trimStart().startsWith("//")) break;
      last = n;
    }
    return last > line.number ? { from: lineEnd, to: doc.line(last).to } : null;
  }

  // 只有行尾是 { 才算块头（行内 `if (a) { x = 1 }` 不折）
  if (!code.endsWith("{")) return null;

  let depth = 0;
  for (let n = line.number; n <= doc.lines; n++) {
    const text = stripLineComment(doc.line(n).text);
    let inStr = false;
    for (let i = 0; i < text.length; i++) {
      const c = text[i];
      if (c === '"') {
        inStr = !inStr;
        continue;
      }
      if (inStr) continue;
      if (c === "{") depth++;
      else if (c === "}") {
        if (depth === 0) continue; // 行首的 }（如 `} else {`）是闭合外层块的，跳过
        depth--;
        if (depth === 0) {
          if (n === line.number) return null; // { } 同一行，没什么可折
          return { from: lineEnd, to: doc.line(n).from };
        }
      }
    }
  }
  return null;
}

export const labFoldService = foldService.of(labFoldFn);
