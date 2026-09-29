function VM_ClientWrapId(real_id) {
    if (global.network.mode != "client" || real_id < 0) return real_id;
    var _vmid = ds_map_find_value(global._VM_real_to_vm_id, real_id);
    if (is_undefined(_vmid)) return real_id;
    return -_vmid;
}
