#pragma once
#include "vm.h"
// outer layer: format the error code recorded by the inner layer
void vm_error_print(const Vm& vm);
