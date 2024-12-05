#define VM_MAX_INSTRS 10000 // @Temporary

typedef u64 Vm_Addr;

#define FOREACH_OP_TYPE(OP)    \
  OP(Op_Noop),                 \
  OP(Op_MovLit64),             \
  OP(Op_Push64),               \
  OP(Op_Pop64),                \
  OP(Op_MovCur),               \
  OP(Op_Add64),                \
  OP(Op_CmpEq),                \
  OP(Op_CmpLe),                \
  OP(Op_JmpLit),               \
  OP(Op_JmpIfLit),             \
  OP(Op_Debug_PrintReg),       \
  OP(Op_Die),                  \
  OP(Op_COUNT)

typedef enum {
  FOREACH_OP_TYPE(GEN_ENUM)
} Vm_Op_Type;

internal
const char *const g_Vm_Op_Type_str[] = {
  FOREACH_OP_TYPE(GEN_ENUM_STR)
};

internal
const char *vm_op_type_name(Vm_Op_Type ty)
{
  return g_Vm_Op_Type_str[ty];
}

#define FOREACH_REG_ID(OP) \
  OP(Reg_INVALID), \
  OP(Reg_A),       \
  OP(Reg_B),       \
  OP(Reg_C),       \
  OP(Reg_D),       \
  OP(Reg_Cmp),     \
  OP(Reg_COUNT)

typedef enum {
  FOREACH_REG_ID(GEN_ENUM)
} Vm_Reg_Id;

internal
const char *const g_Vm_Reg_Id_str[] = {
  FOREACH_REG_ID(GEN_ENUM_STR)
};

internal
const char *vm_reg_id_name(Vm_Reg_Id id)
{
  return g_Vm_Reg_Id_str[id];
}

typedef struct {
  u64 data;
} Vm_Reg;

typedef struct Vm_Instr {
  Vm_Op_Type type;

  Vm_Reg_Id reg1;
  Vm_Reg_Id reg2;
  union {
    u8 lit_u8;
    u16 lit_u16;
    u32 lit_u32;
    u64 lit_u64;

    i8 lit_i8;
    i16 lit_i16;
    i32 lit_i32;
    i64 lit_i64;

    f32 lit_f32;
    f32 lit_f64;
  };

} Vm_Instr;

internal
String8 vm_pretty_print_instr(Arena *arena, Vm_Instr i)
{
  switch (i.type) {
  case Op_Noop: return str8("noop");
  case Op_MovLit64: return push_str8f(arena, "movlit64 %" PRIu64 " -> %s", i.lit_u64, vm_reg_id_name(i.reg1)); 
  case Op_Push64: return push_str8f(arena, "push64 %s", vm_reg_id_name(i.reg1));             
  case Op_Pop64: return  push_str8f(arena, "pop64 %s", vm_reg_id_name(i.reg1));              
  case Op_MovCur: return  push_str8f(arena, "movcur %" PRIu64, i.lit_u64);              
  case Op_Add64: return str8("(todo)");              
  case Op_CmpEq: return str8("(todo)");              
  case Op_CmpLe: return str8("(todo)");              
  case Op_JmpLit: return str8("(todo)");             
  case Op_JmpIfLit: return str8("(todo)");           
  case Op_Debug_PrintReg: return str8("(todo)");     
  case Op_Die: return str8("die");                
  case Op_COUNT: return str8("(count)");
  }
  return str8("");
}

typedef struct {
  Arena *arena;
  u8 *memory;
  u64 mem_cur;

  // registers
  Vm_Reg regs[Reg_COUNT - 1];

  Vm_Instr *instrs;
  u64 n_instrs;
  u64 cur_instr;
  u64 steps_done;

} Fae_Vm;

internal
Fae_Vm vm_create(u64 mem_size)
{
  // just to make our life easier when dealing with registers.
  // maybe could be relaxed if needed, but unlikely.
  assert(mem_size % 8 == 0);

  Fae_Vm vm = {};
  vm.arena = arena_alloc();
  vm.instrs = arena_push_array_nozero(Vm_Instr, vm.arena, VM_MAX_INSTRS);
  vm.memory = arena_push_array(u8, vm.arena, mem_size);
  return vm;
}

internal
void vm_destroy(Fae_Vm *vm)
{
  arena_release(vm->arena);
  vm->memory = NULL;
}

internal
u64 *vm_access_reg(Fae_Vm *vm, Vm_Reg_Id reg_id)
{
  assert(reg_id != Reg_INVALID && reg_id != Reg_COUNT);
  return &vm->regs[reg_id - 1].data;
}

internal
b8 vm_exec_instr(Fae_Vm *vm, Vm_Instr *instr)
{
  #define vm_reg(id) vm_access_reg(vm, (id))

  switch (instr->type) {
  default:
    assert_always(!"Unknown op type!");

  case Op_Noop:
    break;

  case Op_MovLit64: // mov lit64 into reg1
    memcpy(vm_reg(instr->reg1), &instr->lit_u64, sizeof(instr->lit_u64));
    break;

  case Op_Push64: // push reg1 into memory
    memcpy(&vm->memory[vm->mem_cur], vm_reg(instr->reg1), sizeof(u64));
    break;

  case Op_Pop64: // pop memory into reg1
    memcpy(vm_reg(instr->reg1), &vm->memory[vm->mem_cur], sizeof(u64));
    break;

  case Op_MovCur: // moves memory cursor to lit_u64
    vm->mem_cur = instr->lit_u64;
    break;

  case Op_Add64: // adds reg2 to reg1
    *vm_reg(instr->reg1) += *vm_reg(instr->reg2);
    break;

  case Op_CmpEq: // checks if reg1 == reg2 and sets Reg_Cmp accordingly
    *vm_reg(Reg_Cmp) = !!(*vm_reg(instr->reg1) == *vm_reg(instr->reg2));
    break;

  case Op_CmpLe: // checks if reg1 <= reg2 and sets Reg_Cmp accordingly
    *vm_reg(Reg_Cmp) = !!(*vm_reg(instr->reg1) <= *vm_reg(instr->reg2));
    break;

  case Op_JmpLit: // jump to instruction (cur + lit_i64)
    assert(instr->lit_i64 >= -(i64)vm->cur_instr);
    vm->cur_instr += instr->lit_i64;
    return true;

  case Op_JmpIfLit: // jump to instruction (cur + lit_i64) if reg1 is non-zero
    if (*vm_reg(instr->reg1)) {
      assert(instr->lit_i64 >= -(i64)vm->cur_instr);
      vm->cur_instr += instr->lit_i64;
      return true;
    }
    break;

  case Op_Debug_PrintReg: // print contents of reg1
    DEBUG_TAG("Vm", "Reg %s: %lu (0x%lX)", vm_reg_id_name(instr->reg1), *vm_reg(instr->reg1), *vm_reg(instr->reg1));
    break;

  case Op_Die:
    return false;
  }
  ++vm->cur_instr;

  return true;
}

internal
b8 vm_step(Fae_Vm *vm)
{
  Vm_Instr instr = vm->instrs[vm->cur_instr];
  DEBUG_TAG("Vm", "[%lu] Executing [%lu] %s", vm->steps_done, vm->cur_instr, vm_op_type_name(instr.type));
  b8 res = vm_exec_instr(vm, &instr);
  ++vm->steps_done;
  return res;
}

internal
void vm_push_instr(Fae_Vm *vm, Vm_Instr instr)
{
  assert_always(vm->n_instrs < VM_MAX_INSTRS);
  vm->instrs[vm->n_instrs] = instr;
  ++vm->n_instrs;
}

#ifdef FAE_TESTING
void test_vm()
{
  Fae_Vm vm = vm_create(16 * 1024);
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_MovLit64, .lit_u64 = 42, .reg1 = Reg_A });
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_Push64, .reg1 = Reg_A });
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_MovLit64, .lit_u64 = 99, .reg1 = Reg_A });
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_Pop64, .reg1 = Reg_A });
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_MovLit64, .lit_u64 = 20, .reg1 = Reg_B });
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_Add64, .reg1 = Reg_A, .reg2 = Reg_B });
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_Die });
  while (vm_step(&vm)) ;
  assert(*vm_access_reg(&vm, Reg_A) == 62);
  vm_destroy(&vm);
}

void test_vm_loop()
{
  Fae_Vm vm = vm_create(16 * 1024);
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_MovLit64, .lit_u64 = 42, .reg1 = Reg_A });
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_Push64, .reg1 = Reg_A });
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_MovLit64, .lit_u64 = 99, .reg1 = Reg_A });
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_Pop64, .reg1 = Reg_A });
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_MovLit64, .lit_u64 = 20, .reg1 = Reg_B });
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_MovLit64, .lit_u64 = 1000, .reg1 = Reg_C });
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_Add64, .reg1 = Reg_A, .reg2 = Reg_B });
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_CmpLe, .reg1 = Reg_A, .reg2 = Reg_C });
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_JmpIfLit, .lit_i64 = -2, .reg1 = Reg_Cmp });
  vm_push_instr(&vm, (Vm_Instr){ .type = Op_Die });
  while (vm_step(&vm)) ;
  assert(*vm_access_reg(&vm, Reg_A) == 1002);
  vm_destroy(&vm);
}
#endif
