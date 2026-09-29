// damage_enemy(inst, dmg, dmg_type) —— 给敌人结算一次伤害
//   照抄自 scripts/scr_command_VM/scr_command_VM.gml 的 damage_enemy（第 2755-2777 行）。
//   同一份语义的 C++ 版另见：transformGML2txt/vm_builtin.cpp 的 damage_enemy()（第 512-544 行）、
//   DLL_VM/outer/game_b.cpp 的 damage_enemy_local()（第 1153-1204 行）——都是"写实例变量 + 触发受击事件"。
//   语义：写 damage_amount / damage_type 两个实例变量 → event_user(0)（敌人自己的受击事件，
//         里面做闪白、护盾结算、掉血、死亡处理，各敌人可重写 Other_10）→ 有 hit_sound 就先播音效。
//   返回 1 = 已结算，0 = 实例不存在。dmg_type 缺省 / 空串 → "normal"。
//   拿不准的地方：无（三处实现口径一致，没自己加东西）。参考版里的
//   `if (variable_instance_exists(id, "hit_sound")) audio_play_sound(hit_sound, 0, 0);`
//   原样保留，没有改成"直接扣 hp"——那样会绕过护盾和自定义受击逻辑（参考注释里写明）。
function damage_enemy(inst, dmg, dmg_type) {
    var _type = dmg_type;
    if (is_undefined(_type) || _type == "") _type = "normal";
    if (!instance_exists(inst)) return 0;
    with (inst) {
        if (variable_instance_exists(id, "hit_sound")) audio_play_sound(hit_sound, 0, 0);
        damage_amount = dmg;
        damage_type   = _type;
        event_user(0);
    }
    return 1;
}
