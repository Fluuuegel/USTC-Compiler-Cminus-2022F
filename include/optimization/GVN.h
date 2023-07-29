#pragma once
#include "BasicBlock.h"
#include "Constant.h"
#include "DeadCode.h"
#include "FuncInfo.h"
#include "Function.h"
#include "Instruction.h"
#include "Module.h"
#include "PassManager.hpp"
#include "Value.h"
#include "Type.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace GVNExpression {

// fold the constant value
class ConstFolder {
  public:
    ConstFolder(Module *m) : module_(m) {}
    Constant *compute(Instruction *instr, Constant *value1, Constant *value2);
    Constant *compute(Instruction *instr, Constant *value1);
    Constant *compute(Instruction::OpID op, Constant *value1, Constant *value2);
  private:
    Module *module_;
};

/**
 * for constructor of class derived from `Expression`, we make it public
 * because `std::make_shared` needs the constructor to be publicly available,
 * but you should call the static factory method `create` instead the constructor itself to get the desired data
 */
class Expression {
  public:
    // TODO: you need to extend expression types according to testcases
    enum gvn_expr_t { e_constant, e_bin, e_phi, e_vn, e_cmp, e_gep, e_cast, e_call};
    Expression(gvn_expr_t t) : expr_type(t) {}
    virtual ~Expression() = default;
    virtual std::string print() = 0;
    gvn_expr_t get_expr_type() const { return expr_type; }

  private:
    gvn_expr_t expr_type;
};

bool operator==(const std::shared_ptr<Expression> &lhs, const std::shared_ptr<Expression> &rhs);
bool operator==(const GVNExpression::Expression &lhs, const GVNExpression::Expression &rhs);

class ConstantExpression : public Expression {
  public:
    static std::shared_ptr<ConstantExpression> create(Constant *c) { return std::make_shared<ConstantExpression>(c); }
    virtual std::string print() { return c_->print(); }
    // we leverage the fact that constants in lightIR have unique addresses
    bool equiv(const ConstantExpression *other) const { return c_ == other->c_; }
    ConstantExpression(Constant *c) : Expression(e_constant), c_(c) {}

    Constant *retC() {
      return c_;
    }
    
  private:
    Constant *c_;
};

// arithmetic expression
class BinaryExpression : public Expression {
  public:
    static std::shared_ptr<BinaryExpression> create(Instruction::OpID op,
                                                    std::shared_ptr<Expression> lhs,
                                                    std::shared_ptr<Expression> rhs) {
        return std::make_shared<BinaryExpression>(op, lhs, rhs);
    }
    virtual std::string print() {
        return "(" + Instruction::get_instr_op_name(op_) + " " + lhs_->print() + " " + rhs_->print() + ")";
    }

    bool equiv(const BinaryExpression *other) const {
        if (op_ == other->op_ and *lhs_ == *other->lhs_ and *rhs_ == *other->rhs_)
            return true;
        else
            return false;
    }

    std::shared_ptr<Expression> retLhs() {
      return lhs_;
    }

    std::shared_ptr<Expression> retRhs() {
      return rhs_;
    }

    Instruction::OpID retOp() {
      return op_;
    }
    
    BinaryExpression(Instruction::OpID op, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
        : Expression(e_bin), op_(op), lhs_(lhs), rhs_(rhs) {}

  private:
    Instruction::OpID op_;
    std::shared_ptr<Expression> lhs_, rhs_;
};

class PhiExpression : public Expression {
  public:
    static std::shared_ptr<PhiExpression> create(std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs, BasicBlock *lbb, BasicBlock *rbb) {
        return std::make_shared<PhiExpression>(lhs, rhs, lbb, rbb);
    }
    virtual std::string print() { return "(phi " + lhs_->print() + " " + rhs_->print() + ")"; }
    bool equiv(const PhiExpression *other) const {
        if (*lhs_ == *other->lhs_ and *rhs_ == *other->rhs_ and lbb_ == other->lbb_ and rbb_ == other->rbb_)
            return true;
        else
            return false;
    }

    std::shared_ptr<Expression> retLhs(){
      return lhs_;
    }

    std::shared_ptr<Expression> retRhs(){
      return rhs_;
    }
    
    BasicBlock *retLbb(){
      return lbb_;
    }

    BasicBlock *retRbb(){
      return rbb_;
    }

    PhiExpression(std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs, BasicBlock *lbb, BasicBlock *rbb)
        : Expression(e_phi), lhs_(lhs), rhs_(rhs), lbb_(lbb), rbb_(rbb) {}

  private:
    std::shared_ptr<Expression> lhs_, rhs_;
    BasicBlock *lbb_, *rbb_; 
};

class VNExpression : public Expression { //any local variable(non array variable) would be load it at most once, and never be stored 
  public:
    static std::shared_ptr<VNExpression> create(size_t value_number) {
        return std::make_shared<VNExpression>(value_number);
    }
    virtual std::string print() { return "(value_number " + std::to_string(value_number_) + ")"; }
    bool equiv(const VNExpression *other) const { return value_number_ == other->value_number_; }
    VNExpression(size_t value_number)
        : Expression(e_vn), value_number_(value_number) {}

    size_t retVN(){
      return value_number_;
    }

  private:
    size_t value_number_;
};

class CmpExpression : public Expression {
  public:
    static std::shared_ptr<CmpExpression> create(Instruction::OpID op,
                                                    unsigned int cmpop,
                                                    std::shared_ptr<Expression> lhs,
                                                    std::shared_ptr<Expression> rhs) {
        return std::make_shared<CmpExpression>(op, cmpop, lhs, rhs);
    }
    virtual std::string print() {
        return "(" + Instruction::get_instr_op_name(op_) + " " +lhs_->print() + " " + rhs_->print() + ")";  //dont know how to print Cmpop
    }
    bool equiv(const CmpExpression *other) const {
        return (op_ == other->op_ and cmpop_ == other->cmpop_ and *lhs_ == *(other->lhs_) and *rhs_ == *(other->rhs_));
    }
    CmpExpression(Instruction::OpID op, unsigned int cmpop, std::shared_ptr<Expression> lhs, std::shared_ptr<Expression> rhs)
        : Expression(e_cmp), op_(op), cmpop_(cmpop), lhs_(lhs), rhs_(rhs) {}

  private:
    Instruction::OpID op_;
    unsigned int cmpop_;
    std::shared_ptr<Expression> lhs_, rhs_;
};

class GepExpression : public Expression {
  public:
    static std::shared_ptr<GepExpression> create(Value *val, std::vector<std::shared_ptr<Expression>> idxs) {
      return std::make_shared<GepExpression>(val, idxs);
    }
    virtual std::string print() {return " Gep ";}
    bool equiv(const GepExpression *other) const {
      return (val_ == (other->val_) and (idxs_) == (other->idxs_)) ;
    }
    GepExpression(Value *val, std::vector<std::shared_ptr<Expression>> idxs) : Expression(e_gep), val_(val), idxs_(idxs) {}

  private:
    Value *val_;
    std::vector<std::shared_ptr<Expression>> idxs_;
};


class CastExpression : public Expression {
  public:
    static std::shared_ptr<CastExpression> create(Instruction::OpID op, std::shared_ptr<Expression> val) {
      return std::make_shared<CastExpression>(op, val);
    }
    virtual std::string print() {return " Cast ";}
    bool equiv(const CastExpression *other) const {
      return (op_ == other->op_ and val_ == other->val_ );
    }
    CastExpression(Instruction::OpID op, std::shared_ptr<Expression> val) : Expression(e_cast), op_(op), val_(val) {}

  private:
    Instruction::OpID op_;
    std::shared_ptr<Expression> val_;
};

class CallExpression : public Expression {
  public:
    static std::shared_ptr<CallExpression> create(Value *func, std::vector<std::shared_ptr<Expression>> args) {
      return std::make_shared<CallExpression>(func, args);
    }
    virtual std::string print() {return " Call ";}
    bool equiv(const CallExpression *other) const {
      return (func_ == other->func_ && args_ == other->args_);
    }
    CallExpression(Value *func, std::vector<std::shared_ptr<Expression>> args) : Expression(e_call), func_(func), args_(args) {}
    Value *retFun() {return func_;}

  private:
    Value *func_;
    std::vector<std::shared_ptr<Expression>> args_;
};



} // namespace GVNExpression

/**
 * Congruence class in each partitions
 * note: for constant propagation, you might need to add other fields
 * and for load/store redundancy detection, you most certainly need to modify the class
 */
struct CongruenceClass {
    size_t index_;
    // representative of the congruence class, used to replace all the members (except itself) when analysis is done
    Value *leader_;
    // value expression in congruence class
    std::shared_ptr<GVNExpression::Expression> value_expr_;
    // value φ-function is an annotation of the congruence class
    std::shared_ptr<GVNExpression::PhiExpression> value_phi_;
    // equivalent variables in one congruence class
    std::set<Value *> members_;
    //constant
    Constant *c_;

    bool phi_;

    CongruenceClass(size_t index) : index_(index), c_{}, leader_{}, value_expr_{}, value_phi_{}, members_{}, phi_(false){}

    bool operator<(const CongruenceClass &other) const { return this->index_ < other.index_; }
    bool operator==(const CongruenceClass &other) const;

};

bool operator==(const std::shared_ptr<CongruenceClass> &lhs, const std::shared_ptr<CongruenceClass> &rhs);

namespace std {
template <>
// overload std::less for std::shared_ptr<CongruenceClass>, i.e. how to sort the congruence classes
struct less<std::shared_ptr<CongruenceClass>> {
    bool operator()(const std::shared_ptr<CongruenceClass> &a, const std::shared_ptr<CongruenceClass> &b) const {
        // nullptrs should never appear in partitions, so we just dereference it
        return *a < *b;
    }
};
} // namespace std

class GVN : public Pass {
  public:
    using partitions = std::set<std::shared_ptr<CongruenceClass>>;
    GVN(Module *m, bool dump_json) : Pass(m), dump_json_(dump_json) {}
    // pass start
    void run() override;
    // init for pass metadata;
    void initPerFunction();

    // fill the following functions according to Pseudocode, **you might need to add more arguments**
    void detectEquivalences();
    partitions join(const partitions &P1, const partitions &P2, BasicBlock *lbb, BasicBlock *rbb);
    std::shared_ptr<CongruenceClass> intersect(std::shared_ptr<CongruenceClass> Ci, std::shared_ptr<CongruenceClass> Cj, BasicBlock *lbb, BasicBlock *rbb);
    partitions transferFunction(Instruction *x, Value *e, partitions pin, BasicBlock &bb);
    std::shared_ptr<GVNExpression::PhiExpression> valuePhiFunc(std::shared_ptr<GVNExpression::Expression> ve,
                                                               const partitions &P);
    std::shared_ptr<GVNExpression::Expression> valueExpr(Instruction *instr, const partitions &PIN, Constant **constant);

    std::shared_ptr<GVNExpression::Expression> getVN(const partitions &pout,
                                                     std::shared_ptr<GVNExpression::Expression> ve, int type);

    std::shared_ptr<GVNExpression::Expression> getVE(Constant **con, Value *val, partitions pin, int type);

    std::shared_ptr<GVNExpression::PhiExpression> getVP(std::shared_ptr<GVNExpression::VNExpression> vn, partitions pin);

    partitions cpStmt(Instruction *x, Value *e, partitions pin, BasicBlock &bb);

    // replace cc members with leader
    void replace_cc_members();

    // note: be careful when to use copy constructor or clone
    partitions clone(const partitions &p);

    // create congruence class helper
    std::shared_ptr<CongruenceClass> createCongruenceClass(size_t index = 0) {
        return std::make_shared<CongruenceClass>(index);
    }

  private:
    bool dump_json_;
    std::uint64_t next_value_number_ = 1;
    Function *func_;
    std::map<BasicBlock *, partitions> pin_, pout_;
    std::map<BasicBlock* , int> bb_reached;
    std::unique_ptr<FuncInfo> func_info_;
    std::unique_ptr<GVNExpression::ConstFolder> folder_;
    std::unique_ptr<DeadCode> dce_;
};

