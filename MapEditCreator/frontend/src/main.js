import "./style.css";
import { EditorView, basicSetup } from "codemirror";
import { EditorState, Compartment } from "@codemirror/state";
import { linter, lintGutter, forceLinting, forEachDiagnostic, closeLintPanel } from "@codemirror/lint";
import { keymap, hoverTooltip } from "@codemirror/view";
import { indentMore, indentLess } from "@codemirror/commands";
import { completionStatus } from "@codemirror/autocomplete";
import { HighlightStyle, syntaxHighlighting, indentUnit, codeFolding } from "@codemirror/language";
import { tags } from "@lezer/highlight";
import { oneDark } from "@codemirror/theme-one-dark";
import { marked } from "marked";
import { makeLabLanguage, labLanguage, labFoldService, isCustomBlockName, customBlockComment } from "./lablang.js";

import {
  CheckCode, ReadFile, WriteFile, Compile, GetHelp, GetTemplate,
  GetCompilerInfo, SetDirty, OpenInExplorer, PickOpenFile, PickSaveFile,
  GetEditorMeta, GetBlockInfo, GetUpdateLog,
} from "../wailsjs/go/main/App";
import {
  EventsOn, OnFileDrop, WindowMinimise, WindowToggleMaximise, WindowIsMaximised, Quit,
} from "../wailsjs/runtime/runtime";

// ---------------- 全局状态 ----------------
let currentPath = ""; // 当前文件路径（空 = 未命名）
let dirty = false;
let fileDisplayName = "未命名.txt";
let lastBinPath = "";
let lintSeq = 0;
let editorMeta = { blocks: {}, funcs: {} }; // 启动时从 Go 端拉取

// 悬停提示：函数/事件块的中文描述
const wordHover = hoverTooltip((view, pos) => {
  const word = view.state.wordAt(pos);
  if (!word) return null;
  const text = view.state.doc.sliceString(word.from, word.to);
  const fm = editorMeta.funcs[text];
  const bh = editorMeta.blocks[text];
  if (!fm && !bh) {
    // 自定义块：说明取"定义处上面那几行 // 注释"
    if (!isCustomBlockName(text)) return null;
    const cmt = customBlockComment(view.state.doc, text);
    if (!cmt) return null;
    const dom = document.createElement("div");
    dom.className = "hover-tip";
    const name = document.createElement("div");
    name.className = "ht-name";
    name.textContent = text + "（自定义块）";
    const d = document.createElement("div");
    d.className = "ht-desc";
    d.textContent = cmt;
    dom.appendChild(name);
    dom.appendChild(d);
    return { pos: word.from, end: word.to, create: () => ({ dom }), above: true };
  }
  const dom = document.createElement("div");
  dom.className = "hover-tip";
  if (fm) {
    // 标题行：优先显示函数原型（来自 help.md 函数表），没有就退回“名字（N 个参数）”
    const name = document.createElement("div");
    name.className = "ht-name";
    const argcText = fm.args >= 0 ? `${fm.args} 个参数` : "变长参数";
    if (fm.sig) {
      name.textContent = fm.sig;
      const meta = document.createElement("div");
      meta.className = "ht-meta";
      meta.textContent = argcText;
      const d = document.createElement("div");
      d.className = "ht-desc";
      d.textContent = fm.desc || "";
      dom.appendChild(name);
      dom.appendChild(meta);
      dom.appendChild(d);
    } else {
      name.textContent = text + `（${argcText}）`;
      const d = document.createElement("div");
      d.className = "ht-desc";
      d.textContent = fm.desc || "";
      dom.appendChild(name);
      dom.appendChild(d);
    }
  } else {
    const name = document.createElement("div");
    name.className = "ht-name";
    name.textContent = text;
    const d = document.createElement("div");
    d.textContent = (bh.desc || "事件块") + (bh.funcs && bh.funcs.length ? " · 可用: " + bh.funcs.join(", ") : "");
    dom.appendChild(name);
    dom.appendChild(d);
  }
  return { pos: word.from, end: word.to, create: () => ({ dom }), above: true };
});

// ---------------- 语法高亮配色 ----------------
// 深色（one-dark 风格）
const labHighlight = HighlightStyle.define([
  { tag: tags.comment, color: "#5c6370", fontStyle: "italic" },
  { tag: tags.string, color: "#98c379" },
  { tag: tags.number, color: "#d19a66" },
  { tag: tags.keyword, color: "#c678dd" },
  { tag: tags.typeName, color: "#61afef" },
  { tag: tags.operator, color: "#56b6c2" },
]);

// 浅色（one-light 风格）
const labHighlightLight = HighlightStyle.define([
  { tag: tags.comment, color: "#a0a1a7", fontStyle: "italic" },
  { tag: tags.string, color: "#50a14f" },
  { tag: tags.number, color: "#986801" },
  { tag: tags.keyword, color: "#a626a4" },
  { tag: tags.typeName, color: "#4078f2" },
  { tag: tags.operator, color: "#383a42" },
]);

// ---------------- 主题（深色/浅色整体切换） ----------------
const lightEditorTheme = EditorView.theme(
  {
    "&": { backgroundColor: "#e9ebef" },
    ".cm-content": { caretColor: "#1a73e8" },
    ".cm-cursor, &.cm-focused .cm-cursor": { borderLeftColor: "#1a73e8" },
    ".cm-selectionBackground, &.cm-focused .cm-selectionBackground, ::selection": { backgroundColor: "#c3d4f2" },
    ".cm-matchingBracket": { backgroundColor: "#dde3ee", outline: "1px solid #c7d2fe" },
    ".cm-tooltip": {
      backgroundColor: "#f4f5f7",
      border: "1px solid #d5d9df",
      color: "#1e293b",
      boxShadow: "0 6px 18px rgba(15, 23, 42, 0.15)",
    },
    ".cm-panels": { backgroundColor: "#e7eaee", color: "#475569", borderColor: "#d5d9df" },
    ".cm-panels.cm-panels-bottom": { borderTop: "1px solid #d5d9df" },
  },
  { dark: false }
);

const darkThemeBundle = [oneDark, syntaxHighlighting(labHighlight)];
const lightThemeBundle = [lightEditorTheme, syntaxHighlighting(labHighlightLight)];
const themeCompartment = new Compartment();

// 读取上次主题（默认深色）
let isDark = localStorage.getItem("theme") !== "light";
document.body.classList.toggle("light-mode", !isDark);

function applyTheme(dark) {
  isDark = dark;
  document.body.classList.toggle("light-mode", !dark);
  view.dispatch({ effects: themeCompartment.reconfigure(dark ? darkThemeBundle : lightThemeBundle) });
  const btn = document.getElementById("btn-theme");
  if (btn) {
    btn.textContent = dark ? "🌞 白天" : "🌙 夜间";
    btn.title = dark ? "切换到白天模式" : "切换到夜间模式";
  }
  localStorage.setItem("theme", dark ? "dark" : "light");
}

// ---------------- 编辑器 ----------------
const doc = "";

// 诊断行号 → CM 位置
function mapDiag(d) {
  const doc = view.state.doc;
  const lineNo = Math.min(Math.max(1, d.line), doc.lines);
  const line = doc.line(lineNo);
  const text = line.text;
  const indentLen = text.length - text.trimStart().length;
  return {
    from: line.from + indentLen,
    to: line.to,
    severity: d.severity,
    message: d.message,
  };
}

// Go 后端语法检查（防抖由 linter 的 delay 选项控制）
const goLinter = linter(
  async (view) => {
    const seq = ++lintSeq;
    try {
      const diags = await CheckCode(view.state.doc.toString());
      if (seq !== lintSeq) return []; // 丢弃过期结果
      return diags.map(mapDiag);
    } catch (e) {
      console.error("语法检查失败:", e);
      return [];
    }
  },
  { delay: 350 }
);

// 应用快捷键
const appKeymap = keymap.of([
  { key: "Mod-s", run: () => { saveFile(); return true; }, preventDefault: true },
  { key: "Mod-o", run: () => { openFileDialog(); return true; }, preventDefault: true },
  { key: "Mod-n", run: () => { newFile(); return true; }, preventDefault: true },
  { key: "F5", run: () => { doCompile(); return true; }, preventDefault: true },
  { key: "Mod-b", run: () => { doCompile(); return true; }, preventDefault: true },
]);

// Tab 插入 \t（该语言没有缩进服务，CM 自带的 indentWithTab 会放弃处理导致焦点跳到按钮）
const tabKeymap = keymap.of([
  {
    key: "Tab",
    preventDefault: true,
    run: (view) => {
      // 补全面板打开时交给补全处理（Tab 接受候选）
      const st = completionStatus(view.state);
      if (st === "active" || st === "pending") return false;
      if (view.state.selection.ranges.some((r) => !r.empty)) {
        indentMore(view); // 有选区：缩进选中行
      } else {
        view.dispatch(view.state.replaceSelection("\t"));
      }
      return true;
    },
  },
  {
    key: "Shift-Tab",
    preventDefault: true,
    run: (view) => {
      const st = completionStatus(view.state);
      if (st === "active" || st === "pending") return false;
      return indentLess(view);
    },
  },
]);

// 语言模式放入 Compartment：启动时拿到 Go 端块提示后重配置（补全带触发时机/可用函数）
const langCompartment = new Compartment();

const view = new EditorView({
  state: EditorState.create({
    doc,
    extensions: [
      appKeymap,
      // snippet 占位符的 Tab 跳转由 basicSetup 的 autocompletion 内置（Prec.highest）
      tabKeymap,
      themeCompartment.of(isDark ? darkThemeBundle : lightThemeBundle),
      langCompartment.of(labLanguage),
      // 代码折叠：basicSetup 已带 foldGutter 和 foldKeymap，这里补上折叠状态与范围计算
      codeFolding(),
      labFoldService,
      EditorState.tabSize.of(4),
      indentUnit.of("\t"),
      basicSetup,
      lintGutter(),
      goLinter,
      wordHover,
      EditorView.updateListener.of((upd) => {
        if (upd.docChanged) markDirty(true);
        if (upd.docChanged || upd.selectionSet) updateStatus();
        if (upd.transactions.some((tr) => tr.effects.length > 0)) updateStatus();
      }),
      EditorView.theme({
        "&": { height: "100%", fontSize: "14px" },
        ".cm-scroller": { fontFamily: "Consolas, 'Courier New', ui-monospace, monospace" },
      }),
    ],
  }),
  parent: document.getElementById("editor-host"),
});

// ---------------- 状态栏 ----------------
let blockSeq = 0;
let blockDebounceTimer = null;

// 光标所在事件块的提示（触发时机 + 可用函数，来自 help.md 事件块表）
function updateBlockInfo() {
  const line = view.state.doc.lineAt(view.state.selection.main.head).number;
  clearTimeout(blockDebounceTimer);
  blockDebounceTimer = setTimeout(async () => {
    const seq = ++blockSeq;
    try {
      const info = await GetBlockInfo(view.state.doc.toString(), line);
      if (seq !== blockSeq) return; // 丢弃过期结果
      const el = document.getElementById("status-block");
      if (info.name) {
        let text = `${info.name} · ${info.desc || ""}`;
        if (info.funcs && info.funcs.length) text += ` · 可用: ${info.funcs.join(", ")}`;
        el.textContent = text;
        el.title = text;
        el.classList.remove("hidden");
      } else {
        el.classList.add("hidden");
      }
    } catch (e) {
      /* 忽略：文档过大或窗口关闭中 */
    }
  }, 250);
}

function updateStatus() {
  const head = view.state.selection.main.head;
  const line = view.state.doc.lineAt(head);
  document.getElementById("status-pos").textContent = `行 ${line.number}, 列 ${head - line.from + 1}`;
  let errs = 0;
  let warns = 0;
  forEachDiagnostic(view.state, (d) => {
    if (d.severity === "error") errs++;
    else if (d.severity === "warning") warns++;
  });
  const diagEl = document.getElementById("status-diags");
  if (errs + warns > 0) {
    let text = "";
    if (errs) text += `${errs} 个错误`;
    if (errs && warns) text += `, ${warns} 个警告`;
    else if (warns) text += `${warns} 个警告`;
    diagEl.textContent = text;
    diagEl.className = errs ? "diag-bad" : "diag-warn";
  } else {
    diagEl.className = "hidden";
    diagEl.textContent = "";
    closeLintPanel(view); // 无诊断时收起诊断面板，避免残留灰色文字
  }
  updateBlockInfo();
}

// ---------------- 脏状态 ----------------
function markDirty(d) {
  if (dirty === d) return;
  dirty = d;
  document.getElementById("dirty-dot").classList.toggle("hidden", !d);
  SetDirty(d).catch(() => {});
}

function updateTitle() {
  document.getElementById("file-name").textContent = fileDisplayName;
}

// ---------------- 模态框 / 提示 ----------------
function showModal(el) { el.classList.remove("hidden"); }
function hideModal(el) { el.classList.add("hidden"); }

function askConfirm(title, message, buttons) {
  return new Promise((resolve) => {
    document.getElementById("confirm-title").textContent = title;
    document.getElementById("confirm-message").textContent = message;
    const box = document.getElementById("confirm-buttons");
    box.innerHTML = "";
    for (const b of buttons) {
      const btn = document.createElement("button");
      btn.className = "btn " + (b.cls || "btn-outline");
      btn.textContent = b.label;
      btn.addEventListener("click", () => {
        hideModal(document.getElementById("confirm-modal"));
        resolve(b.id);
      });
      box.appendChild(btn);
    }
    showModal(document.getElementById("confirm-modal"));
  });
}

let toastTimer = null;
function showToast(msg, type = "info") {
  const t = document.getElementById("toast");
  t.textContent = msg;
  t.className = "toast show toast-" + type;
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { t.className = "toast hidden"; }, 3200);
}

// 有未保存修改时的确认（用于新建/打开/拖拽打开）
async function confirmDiscard() {
  if (!dirty) return true;
  const choice = await askConfirm("未保存的修改", "当前文件有未保存的修改，如何处理？", [
    { id: "save", label: "保存", cls: "btn-primary" },
    { id: "discard", label: "不保存", cls: "btn-outline-danger" },
    { id: "cancel", label: "取消", cls: "btn-outline" },
  ]);
  if (choice === "save") return await saveFile();
  return choice === "discard";
}

// ---------------- 文件操作 ----------------
async function newFile() {
  if (!(await confirmDiscard())) return;
  try {
    const tpl = await GetTemplate();
    view.dispatch({ changes: { from: 0, to: view.state.doc.length, insert: tpl } });
  } catch (e) {
    console.error("获取模板失败:", e);
  }
  currentPath = "";
  fileDisplayName = "未命名.txt";
  markDirty(false);
  updateTitle();
  view.focus();
  forceLinting(view);
}

async function loadFile(path) {
  const content = await ReadFile(path);
  view.dispatch({ changes: { from: 0, to: view.state.doc.length, insert: content } });
  currentPath = path;
  fileDisplayName = path.split(/[\\/]/).pop();
  markDirty(false);
  updateTitle();
  forceLinting(view);
  showToast("已打开: " + fileDisplayName, "success");
}

async function openFileDialog() {
  if (!(await confirmDiscard())) return;
  try {
    const path = await PickOpenFile();
    if (!path) return; // 用户取消
    await loadFile(path);
  } catch (e) {
    showToast("打开失败: " + e, "error");
  }
}

async function saveFile() {
  let path = currentPath;
  if (!path) {
    try {
      path = await PickSaveFile(fileDisplayName === "未命名.txt" ? "script.txt" : fileDisplayName);
    } catch (e) {
      showToast("保存失败: " + e, "error");
      return false;
    }
    if (!path) return false; // 用户取消
  }
  try {
    await WriteFile(path, view.state.doc.toString());
    currentPath = path;
    fileDisplayName = path.split(/[\\/]/).pop();
    markDirty(false);
    updateTitle();
    showToast("已保存: " + fileDisplayName, "success");
    return true;
  } catch (e) {
    showToast("保存失败: " + e, "error");
    return false;
  }
}

// ---------------- 编译 ----------------
function showOutputPanel() {
  document.getElementById("output-panel").classList.remove("hidden");
}

function jumpToLine(n) {
  const doc = view.state.doc;
  const lineNo = Math.min(Math.max(1, n), doc.lines);
  const line = doc.line(lineNo);
  view.dispatch({
    selection: { anchor: line.from },
    effects: EditorView.scrollIntoView(line.from, { y: "center" }),
  });
  view.focus();
}

function renderCompileResult(res) {
  const el = document.getElementById("compile-output");
  el.innerHTML = "";
  const lines = (res.output || "(无输出)").split(/\r?\n/);
  for (const ln of lines) {
    const div = document.createElement("div");
    div.className = "output-line";
    div.textContent = ln || " ";
    const m = /(?:line|行)\s*[:：]?\s*(\d+)/i.exec(ln);
    if (m) {
      div.classList.add("output-clickable");
      div.title = "点击跳转到第 " + m[1] + " 行";
      div.addEventListener("click", () => jumpToLine(parseInt(m[1], 10)));
    }
    if (/error|错误|失败/i.test(ln)) div.classList.add("output-error");
    else if (/warning|警告/i.test(ln)) div.classList.add("output-warn");
    el.appendChild(div);
  }
  // 结果汇总
  const summary = document.createElement("div");
  summary.className = "output-line output-summary";
  if (res.exitCode === 0) {
    summary.textContent = `✔ 编译成功（耗时 ${res.duration}）→ ${res.binPath || ""}`;
  } else {
    summary.textContent = `✘ 编译失败（退出码 ${res.exitCode}，耗时 ${res.duration}）`;
  }
  el.appendChild(summary);
  lastBinPath = res.binPath || "";
  showOutputPanel();
  if (res.exitCode === 0) showToast("编译成功", "success");
  else showToast("编译失败，详见输出面板", "error");
}

function renderCompileError(err) {
  const el = document.getElementById("compile-output");
  el.innerHTML = "";
  const div = document.createElement("div");
  div.className = "output-line output-error";
  div.textContent = "✘ " + err;
  el.appendChild(div);
  showOutputPanel();
  showToast("编译失败: " + err, "error");
}

async function doCompile() {
  // 编译前自动保存
  if (dirty && !(await saveFile())) {
    showToast("保存失败，已取消编译", "error");
    return;
  }
  if (!currentPath) {
    showToast("请先保存脚本文件再编译", "error");
    return;
  }
  renderCompiling();
  try {
    const res = await Compile(currentPath);
    renderCompileResult(res);
  } catch (e) {
    renderCompileError(e);
  }
}

function renderCompiling() {
  const el = document.getElementById("compile-output");
  el.innerHTML = "";
  const div = document.createElement("div");
  div.className = "output-line";
  div.textContent = "正在编译 " + fileDisplayName + " ...";
  el.appendChild(div);
  showOutputPanel();
}

// ---------------- 帮助 ----------------
let helpMdCache = null;
let helpHits = [];
let helpHitIdx = -1;

function escapeRegExp(s) {
  return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function helpSearchQuery() {
  return document.getElementById("help-search").value.trim();
}

function helpUpdateCount() {
  const el = document.getElementById("help-search-count");
  el.textContent = helpHits.length ? helpHitIdx + 1 + " / " + helpHits.length : "";
}

function helpGoToHit(i) {
  if (!helpHits.length) return;
  if (helpHitIdx >= 0 && helpHits[helpHitIdx]) helpHits[helpHitIdx].classList.remove("current");
  helpHitIdx = ((i % helpHits.length) + helpHits.length) % helpHits.length;
  const hit = helpHits[helpHitIdx];
  hit.classList.add("current");
  hit.scrollIntoView({ block: "center" });
  helpUpdateCount();
}

function helpSearchPrev() { if (helpHits.length) helpGoToHit(helpHitIdx - 1); }
function helpSearchNext() { if (helpHits.length) helpGoToHit(helpHitIdx + 1); }

function renderHelp() {
  if (helpMdCache === null) return;
  const content = document.getElementById("help-content");
  content.innerHTML = marked.parse(helpMdCache);
  helpHits = [];
  helpHitIdx = -1;
  const q = helpSearchQuery();
  if (!q) { helpUpdateCount(); return; }

  // 遍历文本节点，命中处包 <mark>（含代码块——函数名大多在代码里）
  const walker = document.createTreeWalker(content, NodeFilter.SHOW_TEXT, {
    acceptNode(n) { return n.nodeValue.trim() ? NodeFilter.FILTER_ACCEPT : NodeFilter.FILTER_SKIP; },
  });
  const textNodes = [];
  while (walker.nextNode()) textNodes.push(walker.currentNode);
  const re = new RegExp(escapeRegExp(q), "gi");
  for (const tn of textNodes) {
    const s = tn.nodeValue;
    re.lastIndex = 0;
    const frag = document.createDocumentFragment();
    let last = 0, m, found = false;
    while ((m = re.exec(s)) !== null) {
      found = true;
      if (m.index > last) frag.appendChild(document.createTextNode(s.slice(last, m.index)));
      const mark = document.createElement("mark");
      mark.className = "search-hit";
      mark.textContent = m[0];
      frag.appendChild(mark);
      last = m.index + m[0].length;
      if (m[0].length === 0) re.lastIndex++;
    }
    if (found) {
      frag.appendChild(document.createTextNode(s.slice(last)));
      tn.parentNode.replaceChild(frag, tn);
    }
  }
  helpHits = content.querySelectorAll(".search-hit");
  if (helpHits.length) helpGoToHit(0);
  else helpUpdateCount();
}

function setupHelpSearch() {
  const input = document.getElementById("help-search");
  input.addEventListener("input", renderHelp);
  input.addEventListener("keydown", (e) => {
    if (e.key === "Enter") {
      e.preventDefault();
      e.shiftKey ? helpSearchPrev() : helpSearchNext();
    } else if (e.key === "Escape") {
      e.stopPropagation(); // 第一次 Esc 清空搜索，第二次才关弹窗
      input.value = "";
      renderHelp();
    }
  });
  document.getElementById("btn-search-prev").addEventListener("click", helpSearchPrev);
  document.getElementById("btn-search-next").addEventListener("click", helpSearchNext);
}

async function openHelp() {
  try {
    if (helpMdCache === null) {
      const md = await GetHelp();
      // marked 的 GFM 单 ~ 删除线语法会吞掉范围写法（如 "A"~"Z"、0~1），
      // 先把单 ~ 换成 HTML 实体（渲染后字形不变），成对的 ~~ 不受影响
      helpMdCache = md.replace(/([^~])~([^~])/g, "$1&#126;$2");
    }
    renderHelp();
    showModal(document.getElementById("help-modal"));
  } catch (e) {
    showToast("加载帮助失败: " + e, "error");
  }
}

// ---------------- 事件绑定 ----------------
// 最大化/还原图标（Windows 11 风格）
const MAX_SVG =
  '<svg viewBox="0 0 12 12" width="12" height="12"><rect x="2.6" y="2.6" width="6.8" height="6.8" rx="1.4" stroke="currentColor" stroke-width="1.2" fill="none"/></svg>';
const RESTORE_SVG =
  '<svg viewBox="0 0 12 12" width="12" height="12"><rect x="4.4" y="2.6" width="5" height="5" rx="1.1" stroke="currentColor" stroke-width="1.2" fill="none"/><rect x="2.6" y="4.4" width="5" height="5" rx="1.1" stroke="currentColor" stroke-width="1.2" fill="none"/></svg>';

// 按窗口状态更新最大化/还原图标
async function updateMaxIcon() {
  try {
    const isMax = await WindowIsMaximised();
    document.getElementById("w-max-btn").innerHTML = isMax ? RESTORE_SVG : MAX_SVG;
  } catch (e) {
    /* 忽略 */
  }
}

function setupWindowControls() {
  document.getElementById("w-min-btn").addEventListener("click", WindowMinimise);
  document.getElementById("w-max-btn").addEventListener("click", () => {
    WindowToggleMaximise();
    setTimeout(updateMaxIcon, 200);
  });
  document.getElementById("w-close-btn").addEventListener("click", Quit);
  document.querySelector(".title-drag-region").addEventListener("dblclick", () => {
    WindowToggleMaximise();
    setTimeout(updateMaxIcon, 200);
  });
  updateMaxIcon();
}

function setupMenuButtons() {
  document.getElementById("btn-new").addEventListener("click", newFile);
  document.getElementById("btn-open").addEventListener("click", openFileDialog);
  document.getElementById("btn-save").addEventListener("click", saveFile);
  document.getElementById("btn-compile").addEventListener("click", doCompile);
  document.getElementById("btn-help").addEventListener("click", openHelp);
  document.getElementById("btn-theme").addEventListener("click", () => applyTheme(!isDark));
  document.getElementById("btn-output").addEventListener("click", () => {
    document.getElementById("output-panel").classList.toggle("hidden");
  });
  document.getElementById("btn-close-help").addEventListener("click", () => hideModal(document.getElementById("help-modal")));
  document.getElementById("btn-close-update").addEventListener("click", () => hideModal(document.getElementById("update-modal")));
  document.getElementById("btn-close-output").addEventListener("click", () => document.getElementById("output-panel").classList.add("hidden"));
  document.getElementById("btn-open-dir").addEventListener("click", () => {
    // 优先选中 .bin；未编译时打开当前脚本目录；都没有时打开程序目录
    const target = lastBinPath || currentPath || "";
    OpenInExplorer(target).catch((e) => showToast("打开失败: " + e, "error"));
  });
}

function setupWailsEvents() {
  // 关闭窗口前有未保存修改 → 弹确认
  EventsOn("confirm-close", () => {
    askConfirm("确认退出", "有未保存的修改，退出前要保存吗？", [
      { id: "save", label: "保存并退出", cls: "btn-primary" },
      { id: "discard", label: "直接退出", cls: "btn-outline-danger" },
      { id: "cancel", label: "取消", cls: "btn-outline" },
    ]).then(async (choice) => {
      if (choice === "cancel") return;
      if (choice === "save" && !(await saveFile())) return;
      await SetDirty(false);
      Quit();
    });
  });

  // 拖拽 .txt 文件打开
  OnFileDrop((x, y, paths) => {
    const p = (paths || []).find((f) => /\.txt$/i.test(f));
    if (!p) return;
    confirmDiscard().then((ok) => {
      if (!ok) return;
      loadFile(p).catch((e) => showToast("打开失败: " + e, "error"));
    });
  }, true);

  // 阻止浏览器默认拖拽行为
  window.addEventListener("dragover", (e) => e.preventDefault());
  window.addEventListener("drop", (e) => e.preventDefault());
}

function setupGlobalKeys() {
  document.addEventListener("keydown", (e) => {
    if (e.key === "Escape") {
      hideModal(document.getElementById("help-modal"));
      hideModal(document.getElementById("confirm-modal"));
      hideModal(document.getElementById("update-modal"));
    }
    // 帮助打开时 Ctrl+F 聚焦帮助搜索框（否则交给编辑器自己的搜索）
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === "f") {
      const helpModal = document.getElementById("help-modal");
      if (!helpModal.classList.contains("hidden")) {
        e.preventDefault();
        const input = document.getElementById("help-search");
        input.focus();
        input.select();
      }
    }
  });
}

// ---------------- 启动 ----------------
async function init() {
  setupWindowControls();
  setupMenuButtons();
  setupHelpSearch();
  setupWailsEvents();
  setupGlobalKeys();
  applyTheme(isDark); // 同步主题按钮文字

  // 初始模板
  try {
    const tpl = await GetTemplate();
    view.dispatch({ changes: { from: 0, to: view.state.doc.length, insert: tpl } });
  } catch (e) {
    console.error("获取模板失败:", e);
  }
  markDirty(false);
  updateStatus();
  forceLinting(view);
  view.focus();

  // 元数据（函数描述/参数个数 + 事件块提示）→ 补全详情与悬停提示
  try {
    editorMeta = await GetEditorMeta();
    view.dispatch({ effects: langCompartment.reconfigure(makeLabLanguage(editorMeta)) });
  } catch (e) {
    console.error("获取编辑器元数据失败:", e);
  }

  // 编译器信息
  try {
    const info = await GetCompilerInfo();
    const el = document.getElementById("status-compiler");
    el.textContent = "编译器: " + info;
    el.title = info;
  } catch (e) {
    console.error("获取编译器信息失败:", e);
  }

  // 更新内容弹窗
  try {
    const log = await GetUpdateLog();
    if (log && log.trim()) {
      const lines = log.split("\n");
      if (lines[0] && /【.*】/.test(lines[0])) lines.shift(); // 首行标题移入弹窗标题栏
      document.getElementById("update-content").innerHTML = marked.parse(lines.join("\n"));
      showModal(document.getElementById("update-modal"));
    }
  } catch (e) {
    console.error("获取更新日志失败:", e);
  }
}

document.addEventListener("DOMContentLoaded", init);
