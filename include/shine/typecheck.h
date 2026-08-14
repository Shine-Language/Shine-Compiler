#pragma once
#include <unordered_map>
#include <vector>
#include "shine/ast.h"

namespace shine {

class TypeChecker {
public:
    void check(const Module& mod);

private:
    struct VarInfo {
        Type type;
        bool isMutable = false;
    };

    void checkFn(const FunctionDecl& fn);
    void checkStmt(const Stmt& s);
    void checkStmtList(const std::vector<StmtPtr>& stmts);
    void checkIf(const IfStmt& s);
    void checkLoop(const LoopStmt& s);
    Type inferExpr(const Expr& e);
    Type inferCall(const CallExpr& c);
    Type inferWrite(const CallExpr& c);
    Type inferUserInput(const CallExpr& c);
    Type inferTerminalPause(const CallExpr& c);
    void expectType(const Type& got, const Type& want, const SourceLoc& loc);
    void checkAssignable(const Expr& src, const Type& want, const SourceLoc& loc);
    const StructDecl& findStruct(const std::string& name, const SourceLoc& loc);
    const StructField& findField(const StructDecl& sd, const std::string& fieldName, const SourceLoc& loc);
    std::vector<std::string> varNames() const;
    std::vector<std::string> fnNames() const;

    std::unordered_map<std::string, const FunctionDecl*> fns_;
    std::unordered_map<std::string, const StructDecl*> structs_;
    std::unordered_map<std::string, VarInfo> vars_;
    const FunctionDecl* currentFn_ = nullptr;
    int loopDepth_ = 0;
};

}