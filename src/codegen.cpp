#include "shine/codegen.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Verifier.h>
#include "shine/error.h"

namespace shine {

CodeGen::CodeGen()
    : ctx_(std::make_unique<llvm::LLVMContext>()),
      mod_(std::make_unique<llvm::Module>("shine_module", *ctx_)),
      b_(std::make_unique<llvm::IRBuilder<>>(*ctx_)) {}

llvm::Type* CodeGen::llvmType(const Type& t) {
    switch (t.kind) {
        case TypeKind::Void:    return llvm::Type::getVoidTy(*ctx_);
        case TypeKind::Int:     return llvm::Type::getIntNTy(*ctx_, t.bitWidth);
        case TypeKind::Pointer: return llvm::PointerType::getUnqual(*ctx_);
        case TypeKind::Struct:  return structTys_.at(t.structName);
        case TypeKind::Array:   return llvm::ArrayType::get(llvmType(*t.element), (uint64_t)t.length);
    }
    return nullptr;
}

llvm::Type* CodeGen::mapType(const TypeRef& t) {
    if (llvm::Type* ty = llvmType(t.type)) return ty;
    throw CompileError(t.loc, Err::UnknownType, {t.name});
}

llvm::Value* CodeGen::castToType(llvm::Value* v, llvm::Type* target, bool isSigned) {
    if (v->getType() == target) return v;
    if (v->getType()->isIntegerTy() && target->isIntegerTy())
        return b_->CreateIntCast(v, target, isSigned, "cast");
    // `0` used where a pointer is expected is the null-pointer idiom (see
    // TypeChecker::checkAssignable) -- give it the concrete null constant
    // instead of leaving it as an integer, which would fail store/arg types.
    if (target->isPointerTy() && v->getType()->isIntegerTy()) {
        if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(v))
            if (ci->isZero()) return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(target));
    }
    return v;
}

llvm::AllocaInst* CodeGen::createAlloca(llvm::Function* f, llvm::Type* ty, const std::string& name) {
    llvm::IRBuilder<> tmp(&f->getEntryBlock(), f->getEntryBlock().begin());
    return tmp.CreateAlloca(ty, nullptr, name);
}

llvm::Function* CodeGen::putsFn() {
    if (puts_) return puts_;
    auto* ty = llvm::FunctionType::get(llvm::Type::getInt32Ty(*ctx_),
                                        {llvm::PointerType::getUnqual(*ctx_)}, false);
    puts_ = llvm::Function::Create(ty, llvm::Function::ExternalLinkage, "puts", mod_.get());
    return puts_;
}

llvm::Function* CodeGen::getcharFn() {
    if (getchar_) return getchar_;
    auto* ty = llvm::FunctionType::get(llvm::Type::getInt32Ty(*ctx_), false);
    getchar_ = llvm::Function::Create(ty, llvm::Function::ExternalLinkage, "getchar", mod_.get());
    return getchar_;
}

llvm::Function* CodeGen::printfFn() {
    if (printf_) return printf_;
    auto* ty = llvm::FunctionType::get(llvm::Type::getInt32Ty(*ctx_),
                                        {llvm::PointerType::getUnqual(*ctx_)}, true);
    printf_ = llvm::Function::Create(ty, llvm::Function::ExternalLinkage, "printf", mod_.get());
    return printf_;
}

llvm::Function* CodeGen::scanfFn() {
    if (scanf_) return scanf_;
    auto* ty = llvm::FunctionType::get(llvm::Type::getInt32Ty(*ctx_),
                                        {llvm::PointerType::getUnqual(*ctx_)}, true);
    scanf_ = llvm::Function::Create(ty, llvm::Function::ExternalLinkage, "scanf", mod_.get());
    return scanf_;
}

void CodeGen::declareFn(const FunctionDecl& fn) {
    std::vector<llvm::Type*> paramTys;
    for (auto& p : fn.params) paramTys.push_back(mapType(p.type));
    auto* ty = llvm::FunctionType::get(mapType(fn.returnType), paramTys, false);
    fns_[fn.name] = llvm::Function::Create(ty, llvm::Function::ExternalLinkage, fn.name, mod_.get());
}

void CodeGen::defineFn(const FunctionDecl& fn) {
    currentFnDecl_ = &fn;
    llvm::Function* f = fns_.at(fn.name);
    b_->SetInsertPoint(llvm::BasicBlock::Create(*ctx_, "entry", f));

    vars_.clear();
    size_t i = 0;
    for (auto& arg : f->args()) {
        arg.setName(fn.params[i].name);
        auto* slot = createAlloca(f, arg.getType(), fn.params[i].name);
        b_->CreateStore(&arg, slot);
        vars_[fn.params[i].name] = {slot, false, fn.params[i].type.type};
        i++;
    }

    genStmtList(fn.body);

    if (!b_->GetInsertBlock()->getTerminator()) {
        if (f->getReturnType()->isVoidTy()) b_->CreateRetVoid();
        else b_->CreateRet(llvm::Constant::getNullValue(f->getReturnType()));
    }

    std::string errStr;
    llvm::raw_string_ostream os(errStr);
    if (llvm::verifyFunction(*f, &os))
        throw CompileError(fn.loc, Err::CodegenFailure, {fn.name, os.str()});
}

void CodeGen::genStmt(const Stmt& s) {
    if (auto* r = dynamic_cast<const ReturnStmt*>(&s)) {
        if (r->value) b_->CreateRet(genExprAs(*r->value, currentFnDecl_->returnType.type));
        else b_->CreateRetVoid();
        return;
    }
    if (auto* v = dynamic_cast<const VarDeclStmt*>(&s)) {
        if (vars_.find(v->name) != vars_.end())
            throw CompileError(v->loc, Err::VariableRedeclared, {v->name});
        if (v->type.type.isVoid()) throw CompileError(v->type.loc, Err::VoidVariable);
        llvm::Type* ty = mapType(v->type);
        auto* slot = createAlloca(b_->GetInsertBlock()->getParent(), ty, v->name);
        b_->CreateStore(genExprAs(*v->value, v->type.type), slot);
        vars_[v->name] = {slot, v->isMutable, v->type.type};
        return;
    }
    if (auto* a = dynamic_cast<const AssignStmt*>(&s)) {
        auto it = vars_.find(a->name);
        if (it == vars_.end())
            throw CompileError(a->loc, Err::UndeclaredIdentifier, {a->name, suggestClosest(a->name, varNames())});
        if (!it->second.isMutable) throw CompileError(a->loc, Err::ImmutableAssign, {a->name});
        llvm::Value* val = genExprAs(*a->value, it->second.type);
        b_->CreateStore(val, it->second.value);
        return;
    }
    if (auto* da = dynamic_cast<const DerefAssignStmt*>(&s)) {
        AddrInfo addr = genLValueAddr(*da->target);
        if (!addr.type.isPointer()) throw CompileError(da->loc, Err::DerefNonPointer, {addr.type.canonicalName()});
        llvm::Value* ptrVal = b_->CreateLoad(llvm::PointerType::getUnqual(*ctx_), addr.ptr, "derefslot");
        llvm::Value* val = genExprAs(*da->value, *addr.type.pointee);
        b_->CreateStore(val, ptrVal);
        return;
    }
    if (auto* fa = dynamic_cast<const FieldAssignStmt*>(&s)) {
        AddrInfo base = genLValueAddr(*fa->target);
        if (!base.type.isStruct()) throw CompileError(fa->loc, Err::FieldAccessNonStruct, {fa->field, base.type.canonicalName()});
        auto sdIt = structs_.find(base.type.structName);
        const StructDecl& sd = *sdIt->second;
        int idx = -1;
        for (size_t i = 0; i < sd.fields.size(); i++)
            if (sd.fields[i].name == fa->field) { idx = (int)i; break; }
        if (idx < 0) throw CompileError(fa->loc, Err::UnknownField, {sd.name, fa->field});
        const Type& fieldTy = sd.fields[idx].type.type;
        llvm::Value* addr = b_->CreateStructGEP(structTys_.at(sd.name), base.ptr, idx, fa->field);
        llvm::Value* val = genExprAs(*fa->value, fieldTy);
        b_->CreateStore(val, addr);
        return;
    }
    if (auto* ia = dynamic_cast<const IndexAssignStmt*>(&s)) {
        AddrInfo addr = genLValueAddr(*ia->target);
        if (!addr.type.isArray()) throw CompileError(ia->loc, Err::IndexNonArray, {addr.type.canonicalName()});
        llvm::Value* idx = b_->CreateIntCast(genExpr(*ia->index), llvm::Type::getInt64Ty(*ctx_), true, "idx");
        llvm::Value* elemAddr = b_->CreateGEP(llvmType(addr.type), addr.ptr, {b_->getInt32(0), idx}, "elem");
        llvm::Value* val = genExprAs(*ia->value, *addr.type.element);
        b_->CreateStore(val, elemAddr);
        return;
    }
    if (auto* e = dynamic_cast<const ExprStmt*>(&s)) { genExpr(*e->expr); return; }
    if (auto* i = dynamic_cast<const IfStmt*>(&s)) { genIf(*i); return; }
    if (auto* l = dynamic_cast<const LoopStmt*>(&s)) { genLoop(*l); return; }
    if (auto* br = dynamic_cast<const BreakStmt*>(&s)) { genBreak(*br); return; }
    if (auto* co = dynamic_cast<const ContinueStmt*>(&s)) { genContinue(*co); return; }
    throw CompileError(s.loc, Err::UnhandledStatement);
}

void CodeGen::genStmtList(const std::vector<StmtPtr>& stmts) {
    for (auto& st : stmts) {
        if (b_->GetInsertBlock()->getTerminator()) break;
        genStmt(*st);
    }
}

llvm::Value* CodeGen::toBool(llvm::Value* v) {
    return b_->CreateICmpNE(v, llvm::Constant::getNullValue(v->getType()), "cond");
}

void CodeGen::genIf(const IfStmt& s) {
    llvm::Function* f = b_->GetInsertBlock()->getParent();
    llvm::Value* cond = toBool(genExpr(*s.cond));

    auto* thenBB = llvm::BasicBlock::Create(*ctx_, "then", f);
    auto* elseBB = llvm::BasicBlock::Create(*ctx_, "else", f);
    auto* mergeBB = llvm::BasicBlock::Create(*ctx_, "ifcont", f);
    b_->CreateCondBr(cond, thenBB, elseBB);

    b_->SetInsertPoint(thenBB);
    genStmtList(s.thenBody);
    if (!b_->GetInsertBlock()->getTerminator()) b_->CreateBr(mergeBB);

    b_->SetInsertPoint(elseBB);
    genStmtList(s.elseBody);
    if (!b_->GetInsertBlock()->getTerminator()) b_->CreateBr(mergeBB);

    b_->SetInsertPoint(mergeBB);
}

void CodeGen::genLoop(const LoopStmt& s) {
    llvm::Function* f = b_->GetInsertBlock()->getParent();
    auto* condBB = llvm::BasicBlock::Create(*ctx_, "loopcond", f);
    auto* bodyBB = llvm::BasicBlock::Create(*ctx_, "loopbody", f);
    auto* afterBB = llvm::BasicBlock::Create(*ctx_, "loopend", f);

    b_->CreateBr(condBB);

    b_->SetInsertPoint(condBB);
    llvm::Value* cond = toBool(genExpr(*s.cond));
    b_->CreateCondBr(cond, bodyBB, afterBB);

    loopStack_.push_back({condBB, afterBB});
    b_->SetInsertPoint(bodyBB);
    genStmtList(s.body);
    if (!b_->GetInsertBlock()->getTerminator()) b_->CreateBr(condBB);
    loopStack_.pop_back();

    b_->SetInsertPoint(afterBB);
}

void CodeGen::genBreak(const BreakStmt& s) {
    if (loopStack_.empty()) throw CompileError(s.loc, Err::BreakOutsideLoop);
    b_->CreateBr(loopStack_.back().breakBB);
}

void CodeGen::genContinue(const ContinueStmt& s) {
    if (loopStack_.empty()) throw CompileError(s.loc, Err::ContinueOutsideLoop);
    b_->CreateBr(loopStack_.back().continueBB);
}

llvm::Value* CodeGen::genExpr(const Expr& e) {
    if (auto* i = dynamic_cast<const IntLiteralExpr*>(&e)) {
        bool fits32 = i->value >= INT32_MIN && i->value <= INT32_MAX;
        llvm::Type* ty = fits32 ? llvm::Type::getInt32Ty(*ctx_) : llvm::Type::getInt64Ty(*ctx_);
        return llvm::ConstantInt::get(ty, (uint64_t)i->value, true);
    }
    if (auto* s = dynamic_cast<const StringLiteralExpr*>(&e))
        return b_->CreateGlobalString(s->value, "str");
    if (auto* id = dynamic_cast<const IdentifierExpr*>(&e)) return genIdentifier(*id);
    if (auto* bin = dynamic_cast<const BinaryExpr*>(&e)) return genBinary(*bin);
    if (auto* c = dynamic_cast<const CallExpr*>(&e)) return genCall(*c);
    if (auto* ao = dynamic_cast<const AddressOfExpr*>(&e)) return genAddressOf(*ao);
    if (auto* de = dynamic_cast<const DerefExpr*>(&e)) return genDeref(*de);
    if (auto* fa = dynamic_cast<const FieldAccessExpr*>(&e)) return genFieldAccess(*fa);
    if (auto* ix = dynamic_cast<const IndexExpr*>(&e)) return genIndex(*ix);
    if (auto* sl = dynamic_cast<const StructLiteralExpr*>(&e)) return genStructLiteral(*sl);
    if (auto* al = dynamic_cast<const ArrayLiteralExpr*>(&e)) {
        // Bare context (e.g. `[1, 2, 3];` as a statement): element type is
        // inferred from the first element. Every context that stores or
        // passes an array value goes through genExprAs, which passes the
        // known element type instead.
        if (al->elements.empty()) throw CompileError(e.loc, Err::EmptyArrayLiteral);
        llvm::Value* first = genExpr(*al->elements[0]);
        llvm::Type* elemTy = first->getType();
        llvm::Type* arrTy = llvm::ArrayType::get(elemTy, al->elements.size());
        llvm::Function* f = b_->GetInsertBlock()->getParent();
        auto* slot = createAlloca(f, arrTy, "arr_lit");
        for (size_t i = 0; i < al->elements.size(); i++) {
            llvm::Value* val = genExpr(*al->elements[i]);
            if (val->getType()->isIntegerTy() && elemTy->isIntegerTy())
                val = castToType(val, elemTy, true);
            llvm::Value* addr = b_->CreateGEP(arrTy, slot, {b_->getInt32(0), b_->getInt32((int)i)}, "elemit");
            b_->CreateStore(val, addr);
        }
        return b_->CreateLoad(arrTy, slot, "arr_val");
    }
    throw CompileError(e.loc, Err::UnhandledExpression);
}

llvm::Value* CodeGen::genExprAs(const Expr& e, const Type& want) {
    if (auto* al = dynamic_cast<const ArrayLiteralExpr*>(&e))
        return genArrayLiteral(*al, *want.element);
    return castToType(genExpr(e), llvmType(want), want.isSigned);
}

llvm::Value* CodeGen::genAddressOf(const AddressOfExpr& e) {
    auto* id = dynamic_cast<const IdentifierExpr*>(e.operand.get());
    if (!id) throw CompileError(e.loc, Err::AddressOfNonVariable);
    auto it = vars_.find(id->name);
    if (it == vars_.end())
        throw CompileError(id->loc, Err::UndeclaredIdentifier, {id->name, suggestClosest(id->name, varNames())});
    return it->second.value;
}

CodeGen::PtrInfo CodeGen::genPointerExpr(const Expr& e) {
    if (auto* id = dynamic_cast<const IdentifierExpr*>(&e)) {
        auto it = vars_.find(id->name);
        if (it == vars_.end())
            throw CompileError(id->loc, Err::UndeclaredIdentifier, {id->name, suggestClosest(id->name, varNames())});
        if (!it->second.type.isPointer()) throw CompileError(id->loc, Err::DerefNonPointer, {id->name});
        llvm::Value* ptrVal = b_->CreateLoad(llvm::PointerType::getUnqual(*ctx_), it->second.value, id->name);
        return {ptrVal, *it->second.type.pointee};
    }
    if (auto* de = dynamic_cast<const DerefExpr*>(&e)) {
        PtrInfo inner = genPointerExpr(*de->operand);
        if (!inner.pointeeType.isPointer()) throw CompileError(de->loc, Err::DerefNonPointer, {inner.pointeeType.canonicalName()});
        llvm::Value* val = b_->CreateLoad(llvmType(inner.pointeeType), inner.ptr, "ptr");
        return {val, *inner.pointeeType.pointee};
    }
    if (auto* ix = dynamic_cast<const IndexExpr*>(&e)) {
        // `*a[i]`: the element of an array of pointers, e.g. *ptrs[0].
        AddrInfo base = genLValueAddr(*ix->target);
        if (!base.type.isArray()) throw CompileError(ix->loc, Err::IndexNonArray, {base.type.canonicalName()});
        if (!base.type.element->isPointer()) throw CompileError(ix->loc, Err::DerefNonPointer, {base.type.element->canonicalName()});
        llvm::Value* idx = b_->CreateIntCast(genExpr(*ix->index), llvm::Type::getInt64Ty(*ctx_), true, "idx");
        llvm::Value* elemAddr = b_->CreateGEP(llvmType(base.type), base.ptr, {b_->getInt32(0), idx}, "elem");
        llvm::Value* ptrVal = b_->CreateLoad(llvmType(*base.type.element), elemAddr, "ptr");
        return {ptrVal, *base.type.element->pointee};
    }
    throw CompileError(e.loc, Err::DerefNonVariable);
}

llvm::Value* CodeGen::genDeref(const DerefExpr& e) {
    PtrInfo info = genPointerExpr(*e.operand);
    llvm::Type* pointeeTy = llvmType(info.pointeeType);
    return b_->CreateLoad(pointeeTy, info.ptr, "deref");
}

CodeGen::AddrInfo CodeGen::genLValueAddr(const Expr& e) {
    if (auto* id = dynamic_cast<const IdentifierExpr*>(&e)) {
        auto it = vars_.find(id->name);
        if (it == vars_.end())
            throw CompileError(id->loc, Err::UndeclaredIdentifier, {id->name, suggestClosest(id->name, varNames())});
        return {it->second.value, it->second.type};
    }
    if (auto* de = dynamic_cast<const DerefExpr*>(&e)) {
        // `*p` as an lvalue: the address it refers to is whatever pointer
        // value `p` currently holds.
        PtrInfo info = genPointerExpr(*de->operand);
        return {info.ptr, info.pointeeType};
    }
    if (auto* fa = dynamic_cast<const FieldAccessExpr*>(&e)) {
        AddrInfo base = genLValueAddr(*fa->target);
        if (!base.type.isStruct()) throw CompileError(fa->loc, Err::FieldAccessNonStruct, {fa->field, base.type.canonicalName()});
        auto sdIt = structs_.find(base.type.structName);
        const StructDecl& sd = *sdIt->second;
        int idx = -1;
        for (size_t i = 0; i < sd.fields.size(); i++)
            if (sd.fields[i].name == fa->field) { idx = (int)i; break; }
        if (idx < 0) throw CompileError(fa->loc, Err::UnknownField, {sd.name, fa->field});
        llvm::StructType* sty = structTys_.at(sd.name);
        llvm::Value* addr = b_->CreateStructGEP(sty, base.ptr, idx, fa->field);
        return {addr, sd.fields[idx].type.type};
    }
    if (auto* ix = dynamic_cast<const IndexExpr*>(&e)) {
        AddrInfo base = genLValueAddr(*ix->target);
        if (!base.type.isArray()) throw CompileError(ix->loc, Err::IndexNonArray, {base.type.canonicalName()});
        llvm::Value* idx = b_->CreateIntCast(genExpr(*ix->index), llvm::Type::getInt64Ty(*ctx_), true, "idx");
        llvm::Value* addr = b_->CreateGEP(llvmType(base.type), base.ptr, {b_->getInt32(0), idx}, "elem");
        return {addr, *base.type.element};
    }
    throw CompileError(e.loc, Err::DerefNonVariable);
}

llvm::Value* CodeGen::genIndex(const IndexExpr& e) {
    AddrInfo addr = genLValueAddr(e);
    return b_->CreateLoad(llvmType(addr.type), addr.ptr, "elem");
}

llvm::Value* CodeGen::genFieldAccess(const FieldAccessExpr& e) {
    AddrInfo addr = genLValueAddr(e);
    return b_->CreateLoad(llvmType(addr.type), addr.ptr, e.field);
}

llvm::Value* CodeGen::genStructLiteral(const StructLiteralExpr& e) {
    auto sdIt = structs_.find(e.structName);
    if (sdIt == structs_.end()) throw CompileError(e.loc, Err::UnknownStruct, {e.structName});
    const StructDecl& sd = *sdIt->second;
    llvm::StructType* sty = structTys_.at(sd.name);

    llvm::Function* f = b_->GetInsertBlock()->getParent();
    llvm::AllocaInst* slot = createAlloca(f, sty, sd.name + "_lit");

    for (auto& [fname, fexpr] : e.fields) {
        int idx = -1;
        for (size_t i = 0; i < sd.fields.size(); i++)
            if (sd.fields[i].name == fname) { idx = (int)i; break; }
        if (idx < 0) throw CompileError(e.loc, Err::UnknownField, {sd.name, fname});
        const Type& fieldTy = sd.fields[idx].type.type;
        llvm::Value* addr = b_->CreateStructGEP(sty, slot, idx, fname);
        llvm::Value* val = genExprAs(*fexpr, fieldTy);
        b_->CreateStore(val, addr);
    }
    return b_->CreateLoad(sty, slot, sd.name + "_val");
}

llvm::Value* CodeGen::genArrayLiteral(const ArrayLiteralExpr& e, const Type& elemType) {
    if (e.elements.empty()) throw CompileError(e.loc, Err::EmptyArrayLiteral);
    llvm::Type* elemTy = llvmType(elemType);
    llvm::Type* arrTy = llvm::ArrayType::get(elemTy, e.elements.size());
    llvm::Function* f = b_->GetInsertBlock()->getParent();
    auto* slot = createAlloca(f, arrTy, "arr_lit");
    for (size_t i = 0; i < e.elements.size(); i++) {
        llvm::Value* val = genExprAs(*e.elements[i], elemType);
        llvm::Value* addr = b_->CreateGEP(arrTy, slot, {b_->getInt32(0), b_->getInt32((int)i)}, "elemit");
        b_->CreateStore(val, addr);
    }
    return b_->CreateLoad(arrTy, slot, "arr_val");
}

llvm::Value* CodeGen::genIdentifier(const IdentifierExpr& i) {
    auto it = vars_.find(i.name);
    if (it == vars_.end())
        throw CompileError(i.loc, Err::UndeclaredIdentifier, {i.name, suggestClosest(i.name, varNames())});
    return b_->CreateLoad(it->second.value->getAllocatedType(), it->second.value, i.name);
}

llvm::Value* CodeGen::genBinary(const BinaryExpr& e) {
    auto* left = genExpr(*e.left);
    auto* right = genExpr(*e.right);

    if (e.op == "+") return b_->CreateAdd(left, right, "addtmp");
    if (e.op == "-") return b_->CreateSub(left, right, "subtmp");
    if (e.op == "*") return b_->CreateMul(left, right, "multmp");
    if (e.op == "/") return b_->CreateSDiv(left, right, "divtmp");

    llvm::Value* cmp = nullptr;
    if (e.op == "==") cmp = b_->CreateICmpEQ(left, right, "eqtmp");
    else if (e.op == "!=") cmp = b_->CreateICmpNE(left, right, "netmp");
    else if (e.op == "<") cmp = b_->CreateICmpSLT(left, right, "lttmp");
    else if (e.op == "<=") cmp = b_->CreateICmpSLE(left, right, "letmp");
    else if (e.op == ">") cmp = b_->CreateICmpSGT(left, right, "gttmp");
    else if (e.op == ">=") cmp = b_->CreateICmpSGE(left, right, "getmp");

    if (cmp) return b_->CreateIntCast(cmp, llvm::Type::getInt32Ty(*ctx_), false, "booltmp");
    throw CompileError(e.loc, Err::UnknownBinaryOp, {e.op});
}

llvm::Value* CodeGen::genWrite(const CallExpr& c) {
    if (c.args.size() != 1) throw CompileError(c.loc, Err::WriteArgCount);

    // String literals keep the original puts() lowering.
    if (auto* s = dynamic_cast<const StringLiteralExpr*>(c.args[0].get()))
        return b_->CreateCall(putsFn(), {b_->CreateGlobalString(s->value, "str")});

    llvm::Value* val = genExpr(*c.args[0]);
    if (!val->getType()->isIntegerTy())
        throw CompileError(c.args[0]->loc, Err::WriteArgType);

    // Fixed-width ints wider than 32 bits must not be narrowed before
    // printing (that silently truncates values like i64) and need the
    // matching printf length modifier.
    if (val->getType()->getIntegerBitWidth() > 32) {
        val = castToType(val, llvm::Type::getInt64Ty(*ctx_), true);
        llvm::Value* fmt = b_->CreateGlobalString("%lld\n", "fmt");
        return b_->CreateCall(printfFn(), {fmt, val});
    }
    val = castToType(val, llvm::Type::getInt32Ty(*ctx_), true);
    llvm::Value* fmt = b_->CreateGlobalString("%d\n", "fmt");
    return b_->CreateCall(printfFn(), {fmt, val});
}

llvm::Value* CodeGen::genUserInput(const CallExpr& c) {
    if (c.args.size() != 1) throw CompileError(c.loc, Err::UserInputArgCount);
    auto* s = dynamic_cast<const StringLiteralExpr*>(c.args[0].get());
    if (!s) throw CompileError(c.args[0]->loc, Err::UserInputArgType);

    llvm::Value* promptFmt = b_->CreateGlobalString("%s", "fmt");
    b_->CreateCall(printfFn(), {promptFmt, b_->CreateGlobalString(s->value, "str")});

    llvm::Function* f = b_->GetInsertBlock()->getParent();
    llvm::Type* i32 = llvm::Type::getInt32Ty(*ctx_);
    llvm::AllocaInst* slot = createAlloca(f, i32, "input");

    b_->CreateStore(llvm::ConstantInt::get(i32, 0, true), slot);

    llvm::Value* scanFmt = b_->CreateGlobalString("%d", "fmt");
    b_->CreateCall(scanfFn(), {scanFmt, slot});

    b_->CreateCall(getcharFn(), {});

    return b_->CreateLoad(i32, slot, "input");
}

llvm::Value* CodeGen::genTerminalPause(const CallExpr& c) {
    if (c.args.size() > 1) throw CompileError(c.loc, Err::TerminalPauseArgCount);
    if (c.args.size() == 1 && !dynamic_cast<const IdentifierExpr*>(c.args[0].get()))
        throw CompileError(c.args[0]->loc, Err::TerminalPauseArgType);
    b_->CreateCall(putsFn(), {b_->CreateGlobalString("Press Enter to continue...", "str")});
    return b_->CreateCall(getcharFn(), {});
}

llvm::Value* CodeGen::genCall(const CallExpr& c) {
    if (c.callee == "write") return genWrite(c);
    if (c.callee == "terminal.pause") return genTerminalPause(c);
    if (c.callee == "user_input") return genUserInput(c);
    auto it = fns_.find(c.callee);
    if (it == fns_.end())
        throw CompileError(c.loc, Err::UndeclaredFunction, {c.callee, suggestClosest(c.callee, fnNames())});

    llvm::Function* f = it->second;
    if (c.args.size() != f->arg_size())
        throw CompileError(c.loc, Err::ArgCountMismatch,
                            {c.callee, std::to_string(f->arg_size()), std::to_string(c.args.size())});

    const FunctionDecl* decl = fnDecls_.at(c.callee);
    std::vector<llvm::Value*> args;
    for (size_t i = 0; i < c.args.size(); i++)
        args.push_back(genExprAs(*c.args[i], decl->params[i].type.type));
    return b_->CreateCall(f, args);
}

std::vector<std::string> CodeGen::varNames() const {
    std::vector<std::string> names;
    for (auto& [k, v] : vars_) names.push_back(k);
    return names;
}

std::vector<std::string> CodeGen::fnNames() const {
    std::vector<std::string> names;
    for (auto& [k, v] : fns_) names.push_back(k);
    return names;
}

std::unique_ptr<llvm::Module> CodeGen::generate(const Module& mod) {
    mod_->setSourceFileName(mod.file);

    // Two passes so struct fields can reference other structs (including
    // structs declared later in the file, or mutually via pointers).
    for (auto& sd : mod.structs) {
        structs_[sd.name] = &sd;
        structTys_[sd.name] = llvm::StructType::create(*ctx_, sd.name);
    }
    for (auto& sd : mod.structs) {
        std::vector<llvm::Type*> fieldTys;
        for (auto& f : sd.fields) fieldTys.push_back(llvmType(f.type.type));
        structTys_[sd.name]->setBody(fieldTys);
    }

    for (auto& fn : mod.functions) {
        fnDecls_[fn.name] = &fn;
        declareFn(fn);
    }
    for (auto& fn : mod.functions) defineFn(fn);
    return std::move(mod_);
}

}