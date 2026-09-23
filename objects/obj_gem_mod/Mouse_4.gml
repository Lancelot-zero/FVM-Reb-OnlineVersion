// 主动宝石（插件在 _OBJECT_CREATE 里把 clickable 置 1）：
//   不在冷却时点一下 → 置 clicked 给插件放技能，并自动重新走冷却（同原版宝石的 Mouse_4）
// 被动宝石（clickable 保持 0，默认）：
//   点击不响应，cooldown_timer 由插件自己按自己的节奏写（比如神佑每次产火苗后自己设回 cycle*60）
if (clickable) {
	if (cooldown_timer <= 0) {
		clicked = true;
		cooldown_timer = cooldown;
	}
}
