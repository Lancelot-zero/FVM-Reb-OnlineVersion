// VM_LineOf(vm, 块名, ip, _is_byte = false) → "文件名:行号"，拿不到就返回 ""
//
// 【为什么是这么一份瘦实现】
//   原来这份是**平台侧那台 GML VM 的调试件**：它调 VM_Decode 把块解码一遍，
//   建一张"指令下标 → 字节偏移"的表，再拿 ip 去查行号。
//   现在整台 VM 都是 C++（平台那套 VM_Decode / VM_Execute / vm_read_mem 已经删掉），
//   这边没有解码器可用 → 行号映射这一步**优雅退化**，返回空串。
//
//   调用方（mod_error_report）本来就按"拿不到行号"处理（会打"源码行：拿不到"），
//   所以报错功能不受影响，只是少一行"第几行"。
//
//   以后要在 C++ 侧出行号：内层本来就知道当前块和 ip，给 VM_LineOf 注册一个 C++ 实现
//   （VM_RegisterFunc）再把它接回来即可 —— native 表不用动。
function VM_LineOf(vm, name, ip, _is_byte = false) {
    if (!is_struct(vm)) return "";
    if (!variable_struct_exists(vm, "line_tab") || !variable_struct_exists(vm, "blocks")) {
        mod_error_log("[VM_LineOf] 没有 line_tab/blocks（bin 里没打包行号表？）");
        return "";
    }
    if (!ds_map_exists(vm.line_tab, name) || !ds_map_exists(vm.blocks, name)) {
        mod_error_log("[VM_LineOf] 行号表里没有块 " + string(name));
        return "";
    }
    return "";   // C++ 这版没有"解码块拿位置表"这一步（那是平台 VM 的活）→ 行号拿不到
}
