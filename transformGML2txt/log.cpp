#include "log.h"
#include <cstdio>

void vm_error_print(const Vm& vm) {
    switch (vm.last_err) {
    case VM_OK: return;
    case VM_E_BLOCK_NOT_FOUND:
        printf("[vm] block not found: %s / %s\n", str_at(vm.obj).c_str(), str_at(vm.last_err_a).c_str());
        return;
    case VM_E_UNKNOWN_OPCODE:
        printf("[vm] unknown opcode %d (block %s)\n", vm.last_err_a, str_at(vm.last_err_b).c_str());
        return;
    case VM_E_UNKNOWN_NATIVE:
        printf("[vm] unknown native function id = %d\n", vm.last_err_a);
        return;
    case VM_E_UNKNOWN_GAME_FN:
        printf("[vm] unknown game function id = %d\n", vm.last_err_a);
        return;
    case VM_E_NONINT_OPERAND:
        printf("[vm] mod with a non-integer operand (block %s)\n", str_at(vm.last_err_a).c_str());
        return;
    case VM_E_BIN_TOO_MANY_STRS:
        printf("[vm] string pool exceeds %d (u16 of OP_ASSIGN)\n", vm.last_err_a);
        return;
    default:
        printf("[vm] error code %d (a=%d b=%d)\n", vm.last_err, vm.last_err_a, vm.last_err_b);
        return;
    }
}
