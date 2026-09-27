// battle_kernel/battle_start.cpp —— 开战初始化（一场战斗只跑一次）
//
//   对着 GML 的 obj_event_manager/Step_0.gml 开头那一小段写：
//       if (!global._VM_battle_start_done) {
//           置位 + 上一次波次归零
//           VM_Execute(global.__vm, global._VM_BATTLE_START, "_VM_BATTLE_START")
//           vm_hook_run("_VM_BATTLE_START")
//       }
//   搬到 C++ 就是三件事：**门（只跑一次）+ 归零 + 跑地图脚本的 _VM_BATTLE_START**。
//
//   没搬的两件（GML 里有、C++ 这边还没有对应物，各一行，接上就补）：
//     mod_card_vm_deck_refresh()         卡片 mod 的卡组刷新
//     vm_hook_run("_VM_BATTLE_START")    mod 侧派发（挂载点表在 C++ 里还没建）
//   也不在这儿管"读关卡 bin 时那批 global._VM_last_* 归零"——那是装载 bin 的事，
//   不属于"开战"这一步。
//
//   调用方：宿主（把地图 .bin 装进 mapvm 的人）。函数定义在就能调，
//   等宿主接进来的时候，声明放进 battle_kernel 的头里就行。

#include "../vm.h"
#include "../hot/hot_common.h"

// 每场战斗只调一次的实际动作；重复调用会自己在门口挡掉。
//   mapvm  : 地图 / 关卡那个 .bin 的 VM
//   mapobj : 这个 bin 注册时用的对象名（vm.cpp 的 main 里是按文件名取的）
// 返回 false 只有一种情况：地图脚本的 _VM_BATTLE_START 执行失败。
// （已经跑过、或者地图根本就没这个块，都算 true —— 没出错。）
bool battle_start(Vm& mapvm, const std::string& mapobj) {
    if (gtrue(mapvm, "_VM_battle_start_done")) return true;      // 门：跑过就不再跑
    set_var(0, "_VM_battle_start_done", mk_num(1));              // inst = 0 → 写全局
    set_var(0, "_VM_prev_wave",         mk_num(-1));
    set_var(0, "_VM_prev_subwave",      mk_num(-1));

    if (mapvm.find_block("_VM_BATTLE_START"))                   // 地图脚本没这个块就跳过
        return mapvm.run_named("_VM_BATTLE_START");
    return true;
}
