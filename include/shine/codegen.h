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
    llvm::Value* genIdentifier(const IdentifierExpr& i);
    llvm::Value* genBinary(const BinaryExpr& e);
    llvm::Value* genCall(const CallExpr& c);
    llvm::Value* genWrite(const CallExpr& c);
    llvm::Value* genTerminalPause(const CallExpr& c);
    llvm::Value* genUserInput(const CallExpr& c);
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
    std::unordered_map<std::string, VarInfo> vars_;
    std::vector<LoopCtx> loopStack_;
    llvm::Function* puts_ = nullptr;
    llvm::Function* getchar_ = nullptr;
    llvm::Function* printf_ = nullptr;
    llvm::Function* scanf_ = nullptr;
};

}