#include "BasicBlock.h"
#include "Constant.h"
#include "Function.h"
#include "IRBuilder.h"
#include "Module.h"
#include "Type.h"

#include <iostream>
#include <memory>

#ifdef DEBUG // 用于调试信息,大家可以在编译过程中通过" -DDEBUG"来开启这一选项
#define DEBUG_OUTPUT std::cout << __LINE__ << std::endl; // 输出行号的简单示例
#else
#define DEBUG_OUTPUT
#endif

#define CONST_INT(num) ConstantInt::get(num, module)

#define CONST_FP(num) ConstantFP::get(num, module) // 得到常数值的表示,方便后面多次用到

int main() {
    auto module = new Module("Cminus code");
    auto builder = new IRBuilder(nullptr, module);
    Type *Int32Type = Type::get_int32_type(module);

    auto mainFun = Function::create(FunctionType::get(Int32Type, {}),
                                    "main", module);
    auto bb = BasicBlock::create(module, "entry", mainFun);
    builder->set_insert_point(bb);

    auto retAlloca = builder->create_alloca(Int32Type);
    builder->create_store(CONST_INT(0), retAlloca);

    auto aAlloca = builder->create_alloca(Int32Type);
    auto iAlloca = builder->create_alloca(Int32Type);
    builder->create_store(CONST_INT(10), aAlloca);
    builder->create_store(CONST_INT(0), iAlloca);

    auto itCond = BasicBlock::create(module, "itCond", mainFun);
    auto itEntry = BasicBlock::create(module, "itEntry", mainFun);
    auto itEnd = BasicBlock::create(module, "itEnd", mainFun);

    builder->create_br(itCond);

    // iteration conditon
    builder->set_insert_point(itCond);

    auto iLoad = builder->create_load(iAlloca);
    auto aLoad = builder->create_load(aAlloca);
    auto icmp = builder->create_icmp_lt(iLoad, CONST_INT(10));
    builder->create_cond_br(icmp, itEntry, itEnd);

    // iteration entry
    builder->set_insert_point(itEntry);

    iLoad = builder->create_load(iAlloca);
    auto iAdd = builder->create_iadd(iLoad, CONST_INT(1));
    builder->create_store(iAdd, iAlloca);
    aLoad = builder->create_load(aAlloca);
    iLoad = builder->create_load(iAlloca);
    auto aAdd = builder->create_iadd(aLoad, iLoad);
    builder->create_store(aAdd, aAlloca);
    builder->create_br(itCond);

    // iteration end
    builder->set_insert_point(itEnd);

    aLoad = builder->create_load(aAlloca);
    builder->create_store(aLoad, retAlloca);
    auto retLoad = builder->create_load(retAlloca);
    builder->create_ret(retLoad);

    std::cout << module->print();
    delete module;
    return 0;
}