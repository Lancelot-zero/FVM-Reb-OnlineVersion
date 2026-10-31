// 单例 hover 记录器每帧清空：Step 末尾各 mod 实例自判命中后重新维护
global._vm_hover_pick = noone;
// 有效开关（每帧折算一次）：房间开关有值以它为准，没值用全局默认
// 鼠标事件在本帧 Begin Step 之后才跑，所以这里算好后面全帧可用
global._vm_click_singleton_eff = !is_undefined(global._vm_room_click_singleton) ? global._vm_room_click_singleton : global._vm_mod_click_singleton;
