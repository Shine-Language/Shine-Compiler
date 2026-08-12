#pragma once
#include <memory>
#include <string>
#include <vector>
#include "shine/token.h"
#include "shine/type.h"

namespace shine {

struct TypeRef {
    std::string name;
    Type type;
    SourceLoc loc;
};

struct Expr { virtual ~Expr() = default; SourceLoc loc; };
using ExprPtr = std::unique_ptr<Expr>;

struct IntLiteralExpr : Expr { int64_t value = 0; };
struct StringLiteralExpr : Expr { std::string value; };
struct IdentifierExpr : Expr { std::string name; };
struct CallExpr : Expr { std::string callee; std::vector<ExprPtr> args; };
struct BinaryExpr : Expr { std::string op; ExprPtr left; ExprPtr right; };

struct Stmt { virtual ~Stmt() = default; SourceLoc loc; };
using StmtPtr = std::unique_ptr<Stmt>;

struct ReturnStmt : Stmt { ExprPtr value; };
struct ExprStmt : Stmt { ExprPtr expr; };
struct VarDeclStmt : Stmt {
    bool isMutable = false;
    std::string name;
    TypeRef type;
    ExprPtr value;
};
struct AssignStmt : Stmt { std::string name; ExprPtr value; };
struct IfStmt : Stmt {
    ExprPtr cond;
    std::vector<StmtPtr> thenBody;
    std::vector<StmtPtr> elseBody;
};
struct LoopStmt : Stmt { ExprPtr cond; std::vector<StmtPtr> body; };
struct BreakStmt : Stmt {};
struct ContinueStmt : Stmt {};

struct Param { std::string name; TypeRef type; };

struct FunctionDecl {
    std::string name;
    std::vector<Param> params;
    TypeRef returnType;
    std::vector<StmtPtr> body;
    SourceLoc loc;
};

struct Module {
    std::string file;
    std::vector<FunctionDecl> functions;
};

}