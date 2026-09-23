// 把 datafiles/mod开发文档/*.md 转成同目录的 .html（和 md 并排）
// 用法：node tools/md2html.js
// 依赖：编辑器前端的 marked（不联网）。找不到时用 MARKED_PATH=<marked 目录> 覆盖。
const fs = require('fs');
const path = require('path');

const DIR = path.join(__dirname, '..', 'datafiles', 'mod开发文档');
const MARKED_PATHS = [
  process.env.MARKED_PATH,
  'E:/project/MapEditCreator/frontend/node_modules/marked',
  'marked',
].filter(Boolean);

let marked = null, lastErr = '';
for (const p of MARKED_PATHS) {
  try { marked = require(p); break; } catch (e) { lastErr = e.message; }
}
if (!marked) {
  console.error('找不到 marked 模块（最后错误：' + lastErr + '）');
  console.error('可以 set MARKED_PATH=<marked 目录> 后再跑');
  process.exit(1);
}
const parse = (md) => (marked && typeof marked.parse === 'function') ? marked.parse(md) : marked(md);

const CSS = `
:root { color-scheme: light; }
* { box-sizing: border-box; }
body { margin:0; background:#f6f7f9; color:#24292f;
  font:16px/1.75 -apple-system, "Segoe UI", "Microsoft YaHei", "PingFang SC", sans-serif; }
main { max-width:920px; margin:0 auto; padding:36px 28px 90px; background:#fff; min-height:100vh;
  box-shadow:0 0 24px rgba(0,0,0,.06); }
.nav { font-size:.88em; margin:0 0 1.4em; padding-bottom:.6em; border-bottom:1px dashed #e5e7eb; color:#6b7280; }
.nav a { text-decoration:none; }
h1,h2,h3,h4 { line-height:1.3; margin:1.6em 0 .6em; }
h1 { font-size:1.8em; border-bottom:2px solid #e5e7eb; padding-bottom:.3em; margin-top:.2em; }
h2 { font-size:1.35em; border-bottom:1px solid #eceef1; padding-bottom:.25em; }
h3 { font-size:1.12em; }
p { margin:.7em 0; }
code { background:#f2f3f5; padding:.15em .38em; border-radius:4px;
  font-family:Consolas,"Courier New",monospace; font-size:.9em; }
pre { background:#1f2430; color:#e8e8e8; padding:14px 16px; border-radius:8px; overflow:auto; }
pre code { background:none; color:inherit; padding:0; font-size:.88em; }
table { border-collapse:collapse; width:100%; margin:1em 0; font-size:.93em; display:block; overflow-x:auto; }
th,td { border:1px solid #dfe3e8; padding:6px 10px; text-align:left; vertical-align:top; }
th { background:#f3f5f7; white-space:nowrap; }
blockquote { margin:1em 0; padding:.5em 1em; border-left:4px solid #cbd5e1; background:#f8fafc; color:#475569; }
blockquote p { margin:.3em 0; }
a { color:#0969da; }
ul,ol { padding-left:1.7em; margin:.7em 0; }
li { margin:.2em 0; }
hr { border:none; border-top:1px solid #e5e7eb; margin:2em 0; }
.idx { list-style:none; padding-left:0; }
.idx li { margin:.35em 0; }
`;

const page = (title, body) => `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${title}</title>
<style>${CSS}</style>
</head>
<body>
<main>
<div class="nav"><a href="index.html">&larr; 文档目录</a></div>
${body}
</main>
</body>
</html>
`;

// index 的顺序 + 一句话说明（和 mod开发工具说明.md 里那张索引表一致）
const INDEX = [
  ['mod开发工具说明', '先看这个：能做哪几种 mod、通用流程、块名与贴图约定、实例字段（x/y/image_*）、常见坑'],
  ['mod卡片', '卡片：JSON 字段（17 档数值）、动画、块执行时机、demo'],
  ['mod武器', '武器：三种槽位、JSON 字段、动画四属性、demo'],
  ['mod宝石', '宝石：被动/主动、点击与冷却契约、图标两道静默失败'],
  ['mod敌人', '敌人：JSON 字段、行为要自己写'],
  ['mod子弹', '子弹：两步创建、vx/vy、碰撞自己写、更省的两个弹幕接口'],
  ['mod特效', '特效：跟随实例、自动销毁'],
  ['mod时装', '时装：只有 json 的纯外观'],
  ['mod地图', '地图：world.json 与关卡（实验室格式 + 关卡脚本）'],
  ['help', 'VM 脚本语言：语法、事件块、全部 VM 函数（查函数用这份）'],
];

if (!fs.existsSync(DIR)) {
  console.error('目录不存在：' + DIR);
  process.exit(1);
}

const mds = fs.readdirSync(DIR).filter((f) => f.toLowerCase().endsWith('.md')).sort();
let n = 0;
for (const f of mds) {
  const md = fs.readFileSync(path.join(DIR, f), 'utf8');
  let html = parse(md);
  // 相对链接 .md -> .html（保留锚点）
  html = html.replace(/href="([^"]*?)\.md(#[^"]*)?"/g, 'href="$1.html$2"');
  const out = path.join(DIR, f.replace(/\.md$/i, '.html'));
  fs.writeFileSync(out, page(f.replace(/\.md$/i, ''), html), 'utf8');
  n++;
  console.log('  生成 ' + path.basename(out));
}

const listed = INDEX.map((x) => x[0] + '.md');
const extra = mds.filter((f) => listed.indexOf(f) === -1);
const body = INDEX
  .filter((x) => mds.indexOf(x[0] + '.md') !== -1)
  .concat(extra.map((f) => [f.replace(/\.md$/i, ''), '']))
  .map((x) => `  <li><a href="${x[0]}.html">${x[0]}</a>${x[1] ? ' —— ' + x[1] : ''}</li>`)
  .join('\n');
fs.writeFileSync(path.join(DIR, 'index.html'),
  page('Mod 开发文档', `<h1>Mod 开发文档</h1>\n<ul class="idx">\n${body}\n</ul>`), 'utf8');
n++;
console.log('  生成 index.html');
console.log('共 ' + n + ' 个 html（目录：' + DIR + '）');
