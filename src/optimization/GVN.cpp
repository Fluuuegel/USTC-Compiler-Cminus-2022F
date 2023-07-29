#include "GVN.h"

#include "BasicBlock.h"
#include "Constant.h"
#include "DeadCode.h"
#include "FuncInfo.h"
#include "Function.h"
#include "Instruction.h"
#include "logging.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

using namespace GVNExpression;
using std::string_literals::operator""s;
using std::shared_ptr;

static auto get_const_int_value = [](Value *v) { return dynamic_cast<ConstantInt *>(v)->get_value(); };
static auto get_const_fp_value = [](Value *v) { return dynamic_cast<ConstantFP *>(v)->get_value(); };
// Constant Propagation helper, folders are done for you
Constant *ConstFolder::compute(Instruction *instr, Constant *value1, Constant *value2) {
    auto op = instr->get_instr_type();
    switch (op) {
    case Instruction::add: return ConstantInt::get(get_const_int_value(value1) + get_const_int_value(value2), module_);
    case Instruction::sub: return ConstantInt::get(get_const_int_value(value1) - get_const_int_value(value2), module_);
    case Instruction::mul: return ConstantInt::get(get_const_int_value(value1) * get_const_int_value(value2), module_);
    case Instruction::sdiv: return ConstantInt::get(get_const_int_value(value1) / get_const_int_value(value2), module_);
    case Instruction::fadd: return ConstantFP::get(get_const_fp_value(value1) + get_const_fp_value(value2), module_);
    case Instruction::fsub: return ConstantFP::get(get_const_fp_value(value1) - get_const_fp_value(value2), module_);
    case Instruction::fmul: return ConstantFP::get(get_const_fp_value(value1) * get_const_fp_value(value2), module_);
    case Instruction::fdiv: return ConstantFP::get(get_const_fp_value(value1) / get_const_fp_value(value2), module_);

    case Instruction::cmp:
        switch (dynamic_cast<CmpInst *>(instr)->get_cmp_op()) {
        case CmpInst::EQ: return ConstantInt::get(get_const_int_value(value1) == get_const_int_value(value2), module_);
        case CmpInst::NE: return ConstantInt::get(get_const_int_value(value1) != get_const_int_value(value2), module_);
        case CmpInst::GT: return ConstantInt::get(get_const_int_value(value1) > get_const_int_value(value2), module_);
        case CmpInst::GE: return ConstantInt::get(get_const_int_value(value1) >= get_const_int_value(value2), module_);
        case CmpInst::LT: return ConstantInt::get(get_const_int_value(value1) < get_const_int_value(value2), module_);
        case CmpInst::LE: return ConstantInt::get(get_const_int_value(value1) <= get_const_int_value(value2), module_);
        }
    case Instruction::fcmp:
        switch (dynamic_cast<FCmpInst *>(instr)->get_cmp_op()) {
        case FCmpInst::EQ: return ConstantInt::get(get_const_fp_value(value1) == get_const_fp_value(value2), module_);
        case FCmpInst::NE: return ConstantInt::get(get_const_fp_value(value1) != get_const_fp_value(value2), module_);
        case FCmpInst::GT: return ConstantInt::get(get_const_fp_value(value1) > get_const_fp_value(value2), module_);
        case FCmpInst::GE: return ConstantInt::get(get_const_fp_value(value1) >= get_const_fp_value(value2), module_);
        case FCmpInst::LT: return ConstantInt::get(get_const_fp_value(value1) < get_const_fp_value(value2), module_);
        case FCmpInst::LE: return ConstantInt::get(get_const_fp_value(value1) <= get_const_fp_value(value2), module_);
        }
    default: return nullptr;
    }
}

Constant *ConstFolder::compute(Instruction *instr, Constant *value1) {
    auto op = instr->get_instr_type();
    switch (op) {
    case Instruction::sitofp: return ConstantFP::get((float)get_const_int_value(value1), module_);
    case Instruction::fptosi: return ConstantInt::get((int)get_const_fp_value(value1), module_);
    case Instruction::zext: return ConstantInt::get((int)get_const_int_value(value1), module_);
    default: return nullptr;
    }
}

namespace utils {
static std::string print_congruence_class(const CongruenceClass &cc) {
    std::stringstream ss;
    if (cc.index_ == 0) {
        ss << "top class\n";
        return ss.str();
    }
    ss << "\nindex: " << cc.index_ << "\nleader: " << cc.leader_->print()
       << "\nvalue phi: " << (cc.value_phi_ ? cc.value_phi_->print() : "nullptr"s)
       << "\nvalue expr: " << (cc.value_expr_ ? cc.value_expr_->print() : "nullptr"s) << "\nmembers: {";
    for (auto &member : cc.members_)
        ss << member->print() << "; ";
    ss << "}\n";
    return ss.str();
}

static std::string dump_cc_json(const CongruenceClass &cc) {
    std::string json;
    json += "[";
    for (auto member : cc.members_) {
        if (auto c = dynamic_cast<Constant *>(member))
            json += member->print() + ", ";
        else
            json += "\"%" + member->get_name() + "\", ";
    }
    json += "]";
    return json;
}

static std::string dump_partition_json(const GVN::partitions &p) {
    std::string json;
    json += "[";
    for (auto cc : p)
        json += dump_cc_json(*cc) + ", ";
    json += "]";
    return json;
}

static std::string dump_bb2partition(const std::map<BasicBlock *, GVN::partitions> &map) {
    std::string json;
    json += "{";
    for (auto [bb, p] : map)
        json += "\"" + bb->get_name() + "\": " + dump_partition_json(p) + ",";
    json += "}";
    return json;
}

// logging utility for you
static void print_partitions(const GVN::partitions &p) {
    if (p.empty()) {
        LOG_DEBUG << "empty partitions\n";
        return;
    }
    std::string log;
    for (auto &cc : p)
        log += print_congruence_class(*cc);
    LOG_DEBUG << log; // please don't use std::cout
}
} // namespace utils

GVN::partitions GVN::join(const partitions &P1, const partitions &P2, BasicBlock *lbb, BasicBlock *rbb) {
    // TODO: do intersection pair-wise
    partitions P;
    for(auto ci : P1) {
        for(auto cj : P2) {
            auto Ck = intersect(ci, cj, lbb, rbb);
            if(Ck == nullptr) continue;
            P.insert(Ck);
        }
    }
    return P;
}

std::shared_ptr<CongruenceClass> GVN::intersect(std::shared_ptr<CongruenceClass> Ci,
                                                std::shared_ptr<CongruenceClass> Cj,
                                                BasicBlock *lbb, BasicBlock *rbb) {

    std::shared_ptr<CongruenceClass> Ck = createCongruenceClass();

    if(Ci->index_ == 0) return Cj;
    if(Cj->index_ == 0) return Ci;

    for (auto &i : Ci->members_) {
        for (auto &j : Cj->members_) {
            if (i == j) Ck->members_.insert(i);
        }
    }
    
    if(Ck->members_.size() == 0) return nullptr;

    if(Ci->leader_ == Cj->leader_) {
        Ck->leader_ = Ci->leader_;

        switch (bb_reached[lbb]) {
            case 1 : {
                Ck->index_ = Ci->index_;
                Ck->value_expr_ = Ci->value_expr_;
                Ck->value_phi_ = Ci->value_phi_;
                Ck->c_ = Ci->c_;
                break;
            } 
            
            default: {
                Ck->index_ = Cj->index_;
                Ck->value_expr_ = Cj->value_expr_;
                Ck->value_phi_ = Cj->value_phi_;
                Ck->c_ = Cj->c_;
                break;
            }
        }
        
        return Ck;
    }

    Ck->index_ = next_value_number_++;
    auto iter = Ck->members_.begin();
    shared_ptr<Expression> lhs;
    shared_ptr<Expression> rhs;

    if(Ci->phi_) lhs = ConstantExpression::create(Ci->c_);
    else lhs = VNExpression::create(Ci->index_);

    if(Cj->phi_) rhs = ConstantExpression::create(Cj->c_);
    else rhs = VNExpression::create(Cj->index_);

    Ck->leader_ = *iter;
    Ck->value_expr_ = nullptr;
    Ck->value_phi_ = PhiExpression::create(lhs, rhs, lbb, rbb);
    Ck->c_ = nullptr;

    return Ck;
}

void GVN::detectEquivalences() {

    bool changed = false;

    partitions P;

    //  set global value
    for (auto &gv : m_->get_global_variable()) {
        auto cc = createCongruenceClass(next_value_number_++);
        if(gv.is_const()){
            cc->leader_ = gv.get_init();
            cc->value_expr_ = ConstantExpression::create(gv.get_init());
            cc->members_.insert(&gv);
            cc->c_ = gv.get_init();
        }
        else{
            cc->leader_ = &gv;
            cc->members_.insert(&gv);
        }
        P.insert(cc);
    }
    
    // set args
    for (auto arg : func_->get_args()) {
        auto cc = createCongruenceClass(next_value_number_++);
        cc->leader_ = arg;
        cc->members_.insert(arg);
        P.insert(cc);
    }

    // get entry block
    auto entry = func_->get_entry_block();


    // initialize
    for(auto &instr : entry->get_instructions()) {
        if(! instr.is_phi())
        P = transferFunction(&instr, &instr, P, *entry);
    }
    for(auto successor : entry->get_succ_basic_blocks()) {
        for(auto &instr : successor->get_instructions()) {
            if(instr.is_phi())
                P = cpStmt(&instr, &instr, P, *entry);
        }
    }

    pout_[entry] = std::move(P);

    // set up top partition
    std::shared_ptr<CongruenceClass> top_ = createCongruenceClass();
    partitions top;
    top.insert(top_);

    // initialize pout with top
    for (auto &bb : func_->get_basic_blocks()) {
        if (!(&bb == entry)) {
            pout_[&bb] = clone(top);
        }
    }

    std::uint64_t start = next_value_number_;

    // iterate until converge
    // TODO:
    do {
        next_value_number_ = start;
        bb_reached[entry] = 1;
        for(auto &bb : func_->get_basic_blocks()) {
            if(&bb == entry) {
                continue;
            } else {
                bb_reached[&bb] = 0;
            }
        }
        // see the pseudo code in documentation
        for (auto &bb : func_->get_basic_blocks()) { // you might need to visit the blocks in depth-first order
            // get PIN of bb by predecessor(s)
            // iterate through all instructions in the block
            // and the phi instruction in all the successors

            if(&bb == entry) continue;
            auto pre_bb_list = bb.get_pre_basic_blocks();
            bb_reached[&bb] = 1;

            // 
            if(pre_bb_list.empty()) continue;
            if(pre_bb_list.size() == 2) {
                P = join(pout_[pre_bb_list.front()], pout_[pre_bb_list.back()], pre_bb_list.front(), pre_bb_list.back());
            } else {
                auto it = bb.get_pre_basic_blocks().begin();
                partitions p3 = pout_[*it];
                P = clone(p3);
            } 

            for(auto &instr : bb.get_instructions()) {
                if(instr.is_phi()) continue;
                P = transferFunction(&instr, &instr, P, bb);
            }

            // copy statement
            auto succ_bb_list = bb.get_succ_basic_blocks();
            for(auto succ_bb : succ_bb_list) {
                for (auto &instr : succ_bb->get_instructions()) {
                    if ((&instr)->is_phi()) {
                        //copy stmt
                        P = cpStmt(&instr, &instr, P, bb);
                    }
                }
            }

            // check changes in pout
            if(!(pout_[&bb] == P)) {
                changed = true;
            } else {
                changed = false;
            }
            pout_[&bb] = std::move(P);

        }
    } while (changed);
}

shared_ptr<Expression> GVN::valueExpr(Instruction *instr, const partitions &pin, Constant **con) {

    auto op = instr->get_instr_type();
    auto operands = instr->get_operands();

    if(instr->isBinary() || instr->is_cmp() || instr->is_fcmp()) {

        Value *lhs = operands[0];
        Value *rhs = operands[1];

        Constant *lhs_c = dynamic_cast<Constant *>(lhs);
        Constant *rhs_c = dynamic_cast<Constant *>(rhs);

        shared_ptr<Expression> lhs_ve;
        shared_ptr<Expression> rhs_ve;

        if(lhs_c and rhs_c) {
            *con = folder_->compute(instr, lhs_c, rhs_c);
            return ConstantExpression::create(*con);
        }
        
        if(lhs_c) lhs_ve = ConstantExpression::create(lhs_c);
        else lhs_ve = getVE(&lhs_c, lhs, pin, 0);

        if(rhs_c) rhs_ve = ConstantExpression::create(rhs_c);
        else  rhs_ve = getVE(&rhs_c, rhs, pin, 0);            

        if(lhs_c and rhs_c) *con = folder_->compute(instr, lhs_c, rhs_c);

        if(lhs_ve == nullptr or rhs_ve == nullptr) return nullptr;

        if(instr->isBinary()) return BinaryExpression::create(instr->get_instr_type(), lhs_ve, rhs_ve);
        else {
            auto icmp = dynamic_cast<CmpInst *>(instr);
            auto fcmp = dynamic_cast<FCmpInst *>(instr);
            unsigned int cmpop;
            if(icmp) cmpop = icmp->get_cmp_op();
            else cmpop = fcmp->get_cmp_op();
            return CmpExpression::create(op, cmpop, lhs_ve, rhs_ve);
        }
    } else if(instr->is_gep()) { 

        auto val = operands[0];
        std::vector<shared_ptr<Expression>> idxs;
        Constant *is_con;
        for (int i = 1; i < operands.size(); i++) {
            is_con = dynamic_cast<Constant *>(operands[i]);
            if(is_con) {
                idxs.push_back(ConstantExpression::create(is_con));
                continue;
            }
            int flag = 0;
            for(auto cc : pin) {
                if(cc->members_.find(operands[i]) != cc->members_.end()){
                    idxs.push_back(VNExpression::create(cc->index_));
                    flag = 114514;
                    break;
                }
            }
            if(flag == 0) {
                for(auto &[_bb, pin] : pout_) {
                    for(auto cc : pin) {
                        if(cc->members_.find(operands[i]) != cc->members_.end()) {
                            idxs.push_back(VNExpression::create(cc->index_));
                            flag = 114514;
                            break;
                        }
                    }
                    if(flag == 114514) break;
                }
            }
        }
        return GepExpression::create(val, idxs);

    } else if(instr->is_zext() || instr->is_fp2si() || instr->is_si2fp()) {
        Constant *const1 = nullptr;
        auto val = operands[0];
        auto val_ve = getVE(&const1, val, pin, 0);
        if(val == nullptr) return nullptr;
        if(const1) *con = folder_->compute(instr, const1);
        return CastExpression::create(op, val_ve);

    } else if(instr->is_call()) {   //pure function

        if(! func_info_->is_pure_function(dynamic_cast<Function *>(instr->get_operand(0)))) {
            return nullptr;
        } else {
            Value *func = operands[0];
            std::vector<shared_ptr<Expression>> args;
            Constant *is_con;
            for (int i = 1; i < operands.size(); i++) {
                is_con = dynamic_cast<Constant *>(operands[i]);
                if(is_con) {
                    args.push_back(ConstantExpression::create(is_con));
                    continue;
                }
                int flag = 0;
                for(auto cc : pin) {
                    if(cc->members_.find(operands[i]) != cc->members_.end()){
                        args.push_back(VNExpression::create(cc->index_));
                        flag = 114514;
                        break;
                    }
                }
                if(flag == 0) {
                    for(auto &[_bb, pin] : pout_) {
                        for(auto cc : pin) {
                            if(cc->members_.find(instr->get_operand(i)) != cc->members_.end()) {
                                args.push_back(VNExpression::create(cc->index_));
                                flag = 114514;
                                break;
                            }
                        }
                        if(flag == 114514) break;
                    }
                }
            }
            return CallExpression::create(func, args);
        }
    } else {
        return nullptr;
    }
}

// instruction of the form `x = e`, mostly x is just e (SSA), but for copy stmt x is a phi instruction in the
// successor. Phi values (not copy stmt) should be handled in detectEquiv
/// \param bb basic block in which the transfer function is called
GVN::partitions GVN::transferFunction(Instruction *x, Value *e, partitions pin, BasicBlock &bb) {
    // TODO: get different ValueExpr by Instruction::OpID, modify pout
    partitions pout = clone(pin);

    if(x->is_void()) return pout;

    Constant *constant = nullptr;
    shared_ptr<Expression> ve = valueExpr(x, pin, &constant);

    if(ve == nullptr or x->is_alloca() or x->is_load()) {
        for(auto cc : pout){
            if(cc->members_.find(x) != cc->members_.end())
                return pout;
        }
        auto cc = createCongruenceClass(next_value_number_++);
        cc->leader_ = x;
        cc->members_.insert(x);
        pout.insert(cc);
        return pout;
    }

    shared_ptr<PhiExpression> vpf = valuePhiFunc(ve, pin);
    for(auto cc : pout ){
        if(cc->value_expr_ == ve or 
           ( vpf and (std::static_pointer_cast<Expression>((cc)->value_phi_) == std::static_pointer_cast<Expression>(vpf)))) {
                if(constant){
                    cc->leader_ = constant;
                }
                cc->members_.insert(x);
                cc->value_expr_ = ve;
                cc->c_ = constant;
                return pout;
        }
    }

    if(constant){
        e = constant;
    }
    auto cc = createCongruenceClass(next_value_number_++);
    cc->leader_ = e;
    cc->value_expr_ = ve;
    cc->value_phi_= vpf;
    cc->members_.insert(x);
    cc->c_ = constant;
    pout.insert(cc);

    return pout;
}

//note : parameter P is not used
shared_ptr<PhiExpression> GVN::valuePhiFunc(shared_ptr<Expression> ve, const partitions &P) {
    shared_ptr<Expression> vi_, vj_;
    shared_ptr<PhiExpression> phik;
    if(ve->get_expr_type() == Expression::e_bin) {
        auto bin = std::dynamic_pointer_cast<BinaryExpression>(ve);
        auto op = bin->retOp();
        auto lhs = bin->retLhs();
        auto rhs = bin->retRhs();
        shared_ptr<VNExpression> lhs_vn = std::dynamic_pointer_cast<VNExpression>(lhs);
        shared_ptr<VNExpression> rhs_vn = std::dynamic_pointer_cast<VNExpression>(rhs);
        auto lhs_phi = getVP(lhs_vn, P);
        auto rhs_phi = getVP(rhs_vn, P);

        if(lhs_phi and rhs_phi) {

            if(lhs_phi->retLbb() != rhs_phi->retLbb()) return nullptr;
            if(lhs_phi->retRbb() != rhs_phi->retRbb()) return nullptr;

            auto poutkl = pout_[lhs_phi->retLbb()];
            auto poutkr = pout_[lhs_phi->retRbb()];

            auto v1_con = std::dynamic_pointer_cast<ConstantExpression>(lhs_phi->retLhs());
            auto v2_con = std::dynamic_pointer_cast<ConstantExpression>(rhs_phi->retLhs());
            shared_ptr<Expression> vi12;
            int is_constant = 514;
            if(v1_con or v2_con) {
                is_constant = 114;
            }

            switch (is_constant) {
            case 114:
                return nullptr;
                break;
            case 514:
                vi12 = BinaryExpression::create(bin->retOp(), lhs_phi->retLhs(), rhs_phi->retLhs());
                   break;
            default:
                break;
            }
            if(!vi12) return nullptr;

            vi_ = getVN(poutkl, vi12, 0);
            if(vi_ == nullptr){
                auto vi_phi = valuePhiFunc(vi12, poutkl);
                if(vi_phi){
                    bool flag = false;
                    for(auto cc : poutkl){
                        if(cc->value_phi_ == vi_phi){
                            vi_ = VNExpression::create(cc->index_);
                            flag = true;
                            break;
                        }
                    }
                }
            }

            auto v3_con = std::dynamic_pointer_cast<ConstantExpression>(lhs_phi->retRhs());
            auto v4_con = std::dynamic_pointer_cast<ConstantExpression>(rhs_phi->retRhs());
            shared_ptr<Expression> vj12;
            is_constant = 514;
            if(v3_con or v4_con) {
                is_constant = 114;
            }

            switch (is_constant) {
            case 114:
                return nullptr;
                break;
            case 514:
                vj12 = BinaryExpression::create(bin->retOp(), lhs_phi->retRhs(), rhs_phi->retRhs());
            default:
                break;
            }
            if(!vj12) return nullptr;

            vj_ = getVN(poutkr, vj12, 0);
            if(vj_ == nullptr) {
                auto vj_phi = valuePhiFunc(vj12, poutkr);
                if(vj_phi) {
                    bool flag = false;
                    for(auto cc : poutkr){
                        if(cc->value_phi_ == vj_phi){
                            vi_ = VNExpression::create(cc->index_);
                            flag = true;
                            break;
                        }
                    }
                }
            }   
        }

        if(vi_ && vj_) {
            phik = PhiExpression::create(vi_, vj_, lhs_phi->retLbb(), rhs_phi->retRbb());
        }
    }
    return phik;

}

GVN::partitions GVN::cpStmt(Instruction *x, Value *e, partitions pin, BasicBlock &bb) {

    partitions pout = clone(pin);

    if(x->is_void()) return pout;

    Constant *con = nullptr;

    auto operands = x->get_operands();
    for(int i = 0; i < operands.size(); i = i + 2){
        if(operands[i + 1] == static_cast<Value*>(&bb)){
            e = operands[i];
            con = dynamic_cast<Constant*> (e);
            break;
        }
    }

    //erase all concurrence of x in pout except the one where e is in
    for(auto cc = pout.begin(); cc != pout.end();) {
        bool erase_cc = false;
        if((*cc)->members_.find(e) == (*cc)->members_.end()){
            if((*cc)->members_.erase(x)){
                if((*cc)->members_.empty())
                    erase_cc = true;
                else if(static_cast<Value*>(x) == (*cc)->leader_)
                    (*cc)->leader_ = *((*cc)->members_.begin());
            }
        }
        if(erase_cc)
            cc = pout.erase(cc);
        else 
            cc++;
    }

    for(auto it : pout){
        if(con and (con == it->c_)) {
            it->members_.insert(x);
            return pout;
        }
        if(it->members_.find(e) != it->members_.end()){
            it->members_.insert(x);
            return pout;
        }
    }

    // case that x is different from e
    auto cc = createCongruenceClass(next_value_number_++);
    cc->leader_ = e;
    cc->members_.insert(x);
    if(con) {
        cc->value_expr_ = ConstantExpression::create(con);
        cc->c_ = con;
        cc->phi_ = true;
    }
    else cc->members_.insert(e);
    pout.insert(cc); 

    return pout;
}

shared_ptr<Expression> GVN::getVN(const partitions &pout, shared_ptr<Expression> ve, int type) {
    // TODO: return what?

    if(type == 0) {
        for(auto cc : pout) {
            if(ve and cc->value_expr_ == ve) {
                return VNExpression::create(cc->index_);
            }
        }
    } else if(type == 1) { //FIXME:
        std::string n_ve, n_member;
        if(type == 1) {
            for (auto cc = pout.begin(); cc != pout.end(); cc++) {
                for(auto &member : (*cc)->members_) {
                    continue;
                }
                if (((*cc)->value_expr_ and *(*cc)->value_expr_ == *ve)) {
                    return VNExpression::create((*cc)->index_);
                }
                    
            }
        }   
    }

    return nullptr;
}

shared_ptr<Expression> GVN::getVE(Constant **con, Value *val, partitions pin, int type) {

    shared_ptr<Expression> ve = nullptr;

    for(auto cc : pin) {
        if(cc->members_.find(val) != cc->members_.end()) {
            *con = cc->c_;
            ve = VNExpression::create(cc->index_);
            return ve;
        }
    }
    
    for(auto &[_bb, p] : pout_) {
        for(auto cc : p) {
            if(cc->members_.find(val) != cc->members_.end()) {
                *con = cc->c_;
                ve = VNExpression::create(cc->index_);
                return ve;
            }
        }
    }

    if(type == 1) { //FIXME:
        std::string name_ve;

        for (auto it = pin.begin(); it != pin.end(); it++) {
            std::string name_it;
            for (auto &member : (*it)->members_) {
                name_it = member->get_name();
                if ((name_ve != "") and (name_ve == name_it))
                    continue;
            }
        }
    }

    return ve;
}

shared_ptr<PhiExpression> GVN::getVP(shared_ptr<VNExpression> vn, partitions pin) {
    if(vn == nullptr) return nullptr;
    for(auto it : pin) {
        if(it->index_ == vn->retVN()) {
            return it->value_phi_;
        }
    }
    return nullptr;
}

void GVN::initPerFunction() {
    next_value_number_ = 1;
    pin_.clear();
    pout_.clear();
}

//using so called "leader" to replace any other value is the set
void GVN::replace_cc_members() {
    for (auto &[_bb, part] : pout_) {
        auto bb = _bb; // workaround: structured bindings can't be captured in C++17
        for (auto &cc : part) {
            if (cc->index_ == 0)
                continue;
            // if you are planning to do constant propagation, leaders should be set to constant at some point
            for (auto &member : cc->members_) {
                bool member_is_phi = dynamic_cast<PhiInst *>(member);
                bool value_phi = cc->value_phi_ != nullptr;
                if (member != cc->leader_ and (value_phi or !member_is_phi)) {
                    // only replace the members if users are in the same block as bb or the value is used by Phi instruction in the next block
                    member->replace_use_with_when(cc->leader_, [bb](User *user) {
                        if (auto instr = dynamic_cast<Instruction *>(user)) {
                            auto parent = instr->get_parent();
                            auto &bb_pre = parent->get_pre_basic_blocks();
                            if (instr->is_phi()) // as copy stmt, the phi belongs to this block
                                return std::find(bb_pre.begin(), bb_pre.end(), bb) != bb_pre.end();
                            else
                                return parent == bb;
                        }
                        return false;
                    });
                }
            }
        }
    }
    return;
}

// top-level function, done for you
void GVN::run() {
    std::ofstream gvn_json;
    if (dump_json_) {
        gvn_json.open("gvn.json", std::ios::out);
        gvn_json << "[";
    }

    folder_ = std::make_unique<ConstFolder>(m_);
    func_info_ = std::make_unique<FuncInfo>(m_);
    func_info_->run();
    dce_ = std::make_unique<DeadCode>(m_);
    dce_->run(); // let dce take care of some dead phis with undef

    for (auto &f : m_->get_functions()) {
        if (f.get_basic_blocks().empty())
            continue;
        func_ = &f;
        initPerFunction();
        LOG_INFO << "Processing " << f.get_name();
        detectEquivalences();
        LOG_INFO << "===============pin=========================\n";
        for (auto &[bb, part] : pin_) {
            LOG_INFO << "\n===============bb: " << bb->get_name() << "=========================\npartitionIn: ";
            for (auto &cc : part)
                LOG_INFO << utils::print_congruence_class(*cc);
        }
        LOG_INFO << "\n===============pout=========================\n";
        for (auto &[bb, part] : pout_) {
            LOG_INFO << "\n=====bb: " << bb->get_name() << "=====\npartitionOut: ";
            for (auto &cc : part)
                LOG_INFO << utils::print_congruence_class(*cc);
        }
        if (dump_json_) {
            gvn_json << "{\n\"function\": ";
            gvn_json << "\"" << f.get_name() << "\", ";
            gvn_json << "\n\"pout\": " << utils::dump_bb2partition(pout_);
            gvn_json << "},";
        }
        replace_cc_members(); // don't delete instructions, just replace them
    }
    dce_->run(); // let dce do that for us
    if (dump_json_)
        gvn_json << "]";
}

template <typename T>
static bool equiv_as(const Expression &lhs, const Expression &rhs) {
    // we use static_cast because we are very sure that both operands are actually T, not other types.
    return static_cast<const T *>(&lhs)->equiv(static_cast<const T *>(&rhs));
}

bool GVNExpression::operator==(const Expression &lhs, const Expression &rhs) {
    if (lhs.get_expr_type() != rhs.get_expr_type())
        return false;
    switch (lhs.get_expr_type()) {
    case Expression::e_constant: return equiv_as<ConstantExpression>(lhs, rhs);
    case Expression::e_bin: return equiv_as<BinaryExpression>(lhs, rhs);
    case Expression::e_phi: return equiv_as<PhiExpression>(lhs, rhs);
    case Expression::e_vn: return equiv_as<VNExpression>(lhs, rhs);
    case Expression::e_cmp: return equiv_as<CmpExpression>(lhs, rhs);
    case Expression::e_gep: return equiv_as<GepExpression>(lhs, rhs);
    case Expression::e_cast: return equiv_as<CastExpression>(lhs, rhs);
    case Expression::e_call: return equiv_as<CallExpression>(lhs, rhs);
    }
}

bool GVNExpression::operator==(const shared_ptr<Expression> &lhs, const shared_ptr<Expression> &rhs) {
    if (lhs == nullptr and rhs == nullptr) // is the nullptr check necessary here?
        return true;
    return lhs and rhs and *lhs == *rhs;
}

bool operator==(const std::shared_ptr<CongruenceClass> &lhs, const std::shared_ptr<CongruenceClass> &rhs){
    if ((lhs == nullptr && rhs == nullptr) || (lhs && rhs && (*lhs) == (*rhs)) )
        return true;
    return false;
}

GVN::partitions GVN::clone(const partitions &p) {
    partitions data;
    for (auto &cc : p) {
        data.insert(std::make_shared<CongruenceClass>(*cc));
    }
    return data;
}

bool CongruenceClass::operator==(const CongruenceClass &other) const {
    if( !(value_expr_ == other.value_expr_) || 
        !(std::static_pointer_cast<Expression> (value_phi_) == std::static_pointer_cast<Expression> (other.value_phi_)) || 
        !(c_ == other.c_) || 
        (members_ != other.members_)) {
        return false;
    }
    return true;
}
