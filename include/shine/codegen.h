#pragma once
#include <memory>
#include <unordered_map>
#include <vector>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include "shine/ast.h"

namespace shine {

class CodeGen {
public:
    CodeGen();
    std::unique_ptr<llvm::Module> generate(const Module& mod);

private:
    struct VarInfo {
        llvm::AllocaInst* value = nullptr;
        bool isMutable = false;
        Type type;
    };

    struct LoopCtx {
        llvm::BasicBlock* continueBB = nullptr;
        llvm::BasicBlock* breakBB = nullptr;
    };

    void declareFn(const FunctionDecl& fn);
    void defineFn(const FunctionDecl& fn);
    void genStmt(const Stmt& s);
    void genStmtList(const std::vector<StmtPtr>& stmts);
    void genIf(const IfStmt& s);
    void genLoop(const LoopStmt& s);
    void genBreak(const BreakStmt& s);
    void genContinue(const ContinueStmt& s);
    llvm::Value* toBool(llvm::Value* v);
    llvm::Value* genExpr(const Expr& e);
    // Evaluates an expression with a known target type. Array literals need
    // this: their element type (and so the concrete LLVM array type) can only
    // be known from context, e.g. `[1, 2, 3]` against a [3]i64 variable.
    llvm::Value* genExprAs(const Expr& e, const Type& want);
    llvm::Value* genIdentifier(const IdentifierExpr& i);
    llvm::Value* genBinary(const BinaryExpr& e);
    llvm::Value* genCall(const CallExpr& c);
    llvm::Value* genWrite(const CallExpr& c);
    llvm::Value* genTerminalPause(const CallExpr& c);
    llvm::Value* genUserInput(const CallExpr& c);
    llvm::Value* genAddressOf(const AddressOfExpr& e);
    llvm::Value* genDeref(const DerefExpr& e);
    struct PtrInfo { llvm::Value* ptr; Type pointeeType; };
    // Evaluates any pointer-valued expression (identifier, or a deref that
    // itself yields a pointer, e.g. the `ppx` in `**ppx`) down to the
    // concrete address it points to plus that address's pointee type.
    PtrInfo genPointerExpr(const Expr& e);
    struct AddrInfo { llvm::Value* ptr; Type type; };
    // Resolves any lvalue-shaped expression (identifier, `*p`, `a[i]`, or a
    // chain of `.field` accesses over those) down to the concrete storage
    // address holding that value, plus its type. Used for both reads and
    // writes so they share one layout lookup.
    AddrInfo genLValueAddr(const Expr& e);
    llvm::Value* genFieldAccess(const FieldAccessExpr& e);
    llvm::Value* genStructLiteral(const StructLiteralExpr& e);
    llvm::Value* genIndex(const IndexExpr& e);
    llvm::Value* genArrayLiteral(const ArrayLiteralExpr& e, const Type& elemType);
    llvm::Value* castToType(llvm::Value* v, llvm::Type* target, bool isSigned);
    llvm::Type* llvmType(const Type& t);
    llvm::Type* mapType(const TypeRef& t);
    llvm::AllocaInst* createAlloca(llvm::Function* f, llvm::Type* ty, const std::string& name);
    llvm::Function* putsFn();
    llvm::Function* getcharFn();
    llvm::Function* printfFn();
    llvm::Function* scanfFn();
    std::vector<std::string> varNames() const;
    std::vector<std::string> fnNames() const;

    std::unique_ptr<llvm::LLVMContext> ctx_;
    std::unique_ptr<llvm::Module> mod_;
    std::unique_ptr<llvm::IRBuilder<>> b_;
    std::unordered_map<std::string, llvm::Function*> fns_;
    std::unordered_map<std::string, const FunctionDecl*> fnDecls_;
    std::unordered_map<std::string, VarInfo> vars_;
    std::unordered_map<std::string, const StructDecl*> structs_;
    std::unordered_map<std::string, llvm::StructType*> structTys_;
    std::vector<LoopCtx> loopStack_;
    const FunctionDecl* currentFnDecl_ = nullptr;
    llvm::Function* puts_ = nullptr;
    llvm::Function* getchar_ = nullptr;
    llvm::Function* printf_ = nullptr;
    llvm::Function* scanf_ = nullptr;
};

}