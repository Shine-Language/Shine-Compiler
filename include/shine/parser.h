#pragma once
#include <vector>
#include "shine/ast.h"
#include "shine/error.h"
#include "shine/token.h"

namespace shine {

class Parser {
public:
    explicit Parser(std::vector<Token> toks);
    Module parseModule(std::string file);

private:
    const Token& peek(int off = 0) const;
    const Token& advance();
    bool check(TokenKind k) const;
    bool match(TokenKind k);
    const Token& expect(TokenKind k, const std::string& ctx);
    bool atEnd() const;

    FunctionDecl function();
    Param param();
    TypeRef type();
    StmtPtr stmt();
    StmtPtr returnStmt();
    StmtPtr varDecl();
    StmtPtr assignStmt();
    StmtPtr derefAssignStmt();
    StmtPtr ifStmt();
    StmtPtr loopStmt();
    StmtPtr breakStmt();
    StmtPtr contStmt();
    std::vector<StmtPtr> block();
    ExprPtr expr();
    ExprPtr equality();
    ExprPtr comparison();
    ExprPtr term();
    ExprPtr factor();
    ExprPtr unary();
    ExprPtr primary();

    [[noreturn]] void err(const Token& t, const std::string& msg) const;
    [[noreturn]] void err(const Token& t, Err code, const std::vector<std::string>& args = {}) const;

    std::vector<Token> toks_;
    size_t pos_ = 0;
};

}