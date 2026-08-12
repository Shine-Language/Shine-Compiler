#include "shine/typecheck.h"
#include "shine/error.h"

namespace shine {

std::vector<std::string> TypeChecker::varNames() const {
    std::vector<std::string> names;
    for (auto& [k, v] : vars_) names.push_back(k);
    return names;
}

std::vector<std::string> TypeChecker::fnNames() const {
    std::vector<std::string> names;
    for (auto& [k, v] : fns_) names.push_back(k);
    return names;
}

void TypeChecker::expectType(const Type& got, const Type& want, const SourceLoc& loc) {
    if (!got.equals(want))
        throw CompileError(loc, Err::TypeMismatch, {want.canonicalName(), got.canonicalName()});
}

void TypeChecker::checkAssignable(const Expr& src, const Type& want, const SourceLoc& loc) {
    Type got = inferExpr(src);
    if (got.equals(want)) return;
    if (want.isInt() && got.isInt() && dynamic_cast<const IntLiteralExpr*>(&src)) return;
    throw CompileError(loc, Err::TypeMismatch, {want.canonicalName(), got.canonicalName()});
}

Type TypeChecker::inferWrite(const CallExpr& c) {
    if (c.args.size() != 1) throw CompileError(c.loc, Err::WriteArgCount);
    if (dynamic_cast<const StringLiteralExpr*>(c.args[0].get())) return Type::makeVoid();
    Type t = inferExpr(*c.args[0]);
    if (!t.isInt()) throw CompileError(c.args[0]->loc, Err::WriteArgType);
    return Type::makeVoid();
}

Type TypeChecker::inferUserInput(const CallExpr& c) {
    if (c.args.size() != 1) throw CompileError(c.loc, Err::UserInputArgCount);
    if (!dynamic_cast<const StringLiteralExpr*>(c.args[0].get()))
        throw CompileError(c.args[0]->loc, Err::UserInputArgType);
    return Type::makeInt(32, true);
}

Type TypeChecker::inferTerminalPause(const CallExpr& c) {
    if (c.args.size() > 1) throw CompileError(c.loc, Err::TerminalPauseArgCount);
    if (c.args.size() == 1 && !dynamic_cast<const IdentifierExpr*>(c.args[0].get()))
        throw CompileError(c.args[0]->loc, Err::TerminalPauseArgType);
    return Type::makeInt(32, true);
}

Type TypeChecker::inferCall(const CallExpr& c) {
    if (c.callee == "write") return inferWrite(c);
    if (c.callee == "terminal.pause") return inferTerminalPause(c);
    if (c.callee == "user_input") return inferUserInput(c);

    auto it = fns_.find(c.callee);
    if (it == fns_.end())
        throw CompileError(c.loc, Err::UndeclaredFunction, {c.callee, suggestClosest(c.callee, fnNames())});

    const FunctionDecl& fn = *it->second;
    if (c.args.size() != fn.params.size())
        throw CompileError(c.loc, Err::ArgCountMismatch,
                            {c.callee, std::to_string(fn.params.size()), std::to_string(c.args.size())});

    for (size_t i = 0; i < c.args.size(); i++)
        checkAssignable(*c.args[i], fn.params[i].type.type, c.args[i]->loc);

    return fn.returnType.type;
}

Type TypeChecker::inferExpr(const Expr& e) {
    if (dynamic_cast<const IntLiteralExpr*>(&e)) return Type::makeInt(32, true);
    if (dynamic_cast<const StringLiteralExpr*>(&e)) return Type::makePointer(Type::makeInt(8, false));
    if (auto* id = dynamic_cast<const IdentifierExpr*>(&e)) {
        auto it = vars_.find(id->name);
        if (it == vars_.end())
            throw CompileError(id->loc, Err::UndeclaredIdentifier, {id->name, suggestClosest(id->name, varNames())});
        return it->second.type;
    }
    if (auto* bin = dynamic_cast<const BinaryExpr*>(&e)) {
        Type l = inferExpr(*bin->left);
        Type r = inferExpr(*bin->right);
        if (!l.isInt() || !r.isInt()) throw CompileError(bin->loc, Err::VoidExpression);
        return Type::makeInt(32, true);
    }
    if (auto* c = dynamic_cast<const CallExpr*>(&e)) return inferCall(*c);
    if (auto* ao = dynamic_cast<const AddressOfExpr*>(&e)) {
        auto* id = dynamic_cast<const IdentifierExpr*>(ao->operand.get());
        if (!id) throw CompileError(ao->loc, Err::AddressOfNonVariable);
        auto it = vars_.find(id->name);
        if (it == vars_.end())
            throw CompileError(id->loc, Err::UndeclaredIdentifier, {id->name, suggestClosest(id->name, varNames())});
        return Type::makePointer(it->second.type);
    }
    if (auto* de = dynamic_cast<const DerefExpr*>(&e)) {
        Type t = inferExpr(*de->operand);
        if (!t.isPointer()) throw CompileError(de->loc, Err::DerefNonPointer, {t.canonicalName()});
        return *t.pointee;
    }
    throw CompileError(e.loc, Err::UnhandledExpression);
}

void TypeChecker::checkStmt(const Stmt& s) {
    if (auto* r = dynamic_cast<const ReturnStmt*>(&s)) {
        const Type& retTy = currentFn_->returnType.type;
        if (r->value) expectType(inferExpr(*r->value), retTy, r->loc);
        else if (!retTy.isVoid())
            throw CompileError(r->loc, Err::ReturnTypeMismatch, {currentFn_->name, retTy.canonicalName(), "void"});
        return;
    }
    if (auto* v = dynamic_cast<const VarDeclStmt*>(&s)) {
        if (vars_.find(v->name) != vars_.end())
            throw CompileError(v->loc, Err::VariableRedeclared, {v->name});
        if (v->type.type.isVoid()) throw CompileError(v->type.loc, Err::VoidVariable);
        checkAssignable(*v->value, v->type.type, v->value->loc);
        vars_[v->name] = {v->type.type, v->isMutable};
        return;
    }
    if (auto* a = dynamic_cast<const AssignStmt*>(&s)) {
        auto it = vars_.find(a->name);
        if (it == vars_.end())
            throw CompileError(a->loc, Err::UndeclaredIdentifier, {a->name, suggestClosest(a->name, varNames())});
        if (!it->second.isMutable) throw CompileError(a->loc, Err::ImmutableAssign, {a->name});
        checkAssignable(*a->value, it->second.type, a->value->loc);
        return;
    }
    if (auto* e = dynamic_cast<const ExprStmt*>(&s)) { inferExpr(*e->expr); return; }
    if (auto* i = dynamic_cast<const IfStmt*>(&s)) { checkIf(*i); return; }
    if (auto* l = dynamic_cast<const LoopStmt*>(&s)) { checkLoop(*l); return; }
    if (dynamic_cast<const BreakStmt*>(&s)) {
        if (loopDepth_ == 0) throw CompileError(s.loc, Err::BreakOutsideLoop);
        return;
    }
    if (dynamic_cast<const ContinueStmt*>(&s)) {
        if (loopDepth_ == 0) throw CompileError(s.loc, Err::ContinueOutsideLoop);
        return;
    }
    throw CompileError(s.loc, Err::UnhandledStatement);
}

void TypeChecker::checkStmtList(const std::vector<StmtPtr>& stmts) {
    for (auto& st : stmts) checkStmt(*st);
}

void TypeChecker::checkIf(const IfStmt& s) {
    inferExpr(*s.cond);
    checkStmtList(s.thenBody);
    checkStmtList(s.elseBody);
}

void TypeChecker::checkLoop(const LoopStmt& s) {
    inferExpr(*s.cond);
    loopDepth_++;
    checkStmtList(s.body);
    loopDepth_--;
}

void TypeChecker::checkFn(const FunctionDecl& fn) {
    currentFn_ = &fn;
    vars_.clear();
    for (auto& p : fn.params) vars_[p.name] = {p.type.type, false};
    checkStmtList(fn.body);
}

void TypeChecker::check(const Module& mod) {
    for (auto& fn : mod.functions) fns_[fn.name] = &fn;
    for (auto& fn : mod.functions) checkFn(fn);
}

}