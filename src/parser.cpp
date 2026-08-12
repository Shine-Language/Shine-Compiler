#include "shine/parser.h"
#include "shine/error.h"

namespace shine {

Parser::Parser(std::vector<Token> toks) : toks_(std::move(toks)) {}

bool Parser::atEnd() const { return peek().kind == TokenKind::EndOfFile; }

const Token& Parser::peek(int off) const {
    size_t i = pos_ + (size_t)off;
    return i < toks_.size() ? toks_[i] : toks_.back();
}

const Token& Parser::advance() {
    const Token& t = peek();
    if (!atEnd()) pos_++;
    return t;
}

bool Parser::check(TokenKind k) const { return peek().kind == k; }

bool Parser::match(TokenKind k) {
    if (!check(k)) return false;
    advance();
    return true;
}

const Token& Parser::expect(TokenKind k, const std::string& ctx) {
    if (!check(k)) {
        if (k == TokenKind::Semicolon)
            err(peek(-1), Err::ExpectedToken, {tokenName(k), ctx, tokenName(peek().kind)});
        err(peek(), Err::ExpectedToken, {tokenName(k), ctx, tokenName(peek().kind)});
    }
    return advance();
}

void Parser::err(const Token& t, const std::string& msg) const {
    throw CompileError(t.loc, msg);
}

void Parser::err(const Token& t, Err code, const std::vector<std::string>& args) const {
    throw CompileError(t.loc, code, args);
}

Module Parser::parseModule(std::string file) {
    Module m{std::move(file), {}};
    while (!atEnd()) m.functions.push_back(function());
    return m;
}

TypeRef Parser::type() {
    std::string name;
    SourceLoc loc;
    if (check(TokenKind::KwInt) || check(TokenKind::KwVoid)) {
        const Token& t = advance();
        name = t.kind == TokenKind::KwInt ? "int" : "void";
        loc = t.loc;
    } else if (check(TokenKind::Identifier)) {
        Type probe;
        if (!resolveTypeName(peek().text, probe)) err(peek(), Err::ExpectedType);
        const Token& t = advance();
        name = t.text;
        loc = t.loc;
    } else {
        err(peek(), Err::ExpectedType);
    }

    TypeRef ref{name, {}, loc};
    resolveTypeName(name, ref.type);
    while (match(TokenKind::Star)) {
        ref.type = Type::makePointer(ref.type);
        ref.name += "*";
    }
    return ref;
}

Param Parser::param() {
    const Token& name = expect(TokenKind::Identifier, "as param name");
    expect(TokenKind::Colon, "after param name");
    return {name.text, type()};
}

FunctionDecl Parser::function() {
    expect(TokenKind::KwFn, "to start function");
    TypeRef retType = type();
    const Token& name = expect(TokenKind::Identifier, "as function name");
    FunctionDecl fn{name.text, {}, retType, {}, name.loc};

    expect(TokenKind::LParen, "after function name");
    if (!check(TokenKind::RParen)) {
        fn.params.push_back(param());
        while (match(TokenKind::Comma)) fn.params.push_back(param());
    }
    expect(TokenKind::RParen, "to close params");

    expect(TokenKind::LBrace, "to start body");
    while (!check(TokenKind::RBrace)) {
        if (atEnd()) err(peek(), Err::UnexpectedEof, {"function body"});
        fn.body.push_back(stmt());
    }
    expect(TokenKind::RBrace, "to close body");
    return fn;
}

StmtPtr Parser::stmt() {
    if (check(TokenKind::KwReturn)) return returnStmt();
    if (check(TokenKind::KwLet) || check(TokenKind::KwVar)) return varDecl();
    if (check(TokenKind::KwIf)) return ifStmt();
    if (check(TokenKind::KwLoop)) return loopStmt();
    if (check(TokenKind::KwStop)) return breakStmt();
    if (check(TokenKind::KwCont)) return contStmt();
    if (check(TokenKind::Identifier) && peek(1).kind == TokenKind::Equal) return assignStmt();
    auto s = std::make_unique<ExprStmt>();
    s->loc = peek().loc;
    s->expr = expr();
    expect(TokenKind::Semicolon, "after expression");
    return s;
}

std::vector<StmtPtr> Parser::block() {
    expect(TokenKind::LBrace, "to start block");
    std::vector<StmtPtr> body;
    while (!check(TokenKind::RBrace)) {
        if (atEnd()) err(peek(), Err::UnexpectedEof, {"block"});
        body.push_back(stmt());
    }
    expect(TokenKind::RBrace, "to close block");
    return body;
}

StmtPtr Parser::ifStmt() {
    const Token& kw = advance();
    auto s = std::make_unique<IfStmt>();
    s->loc = kw.loc;
    expect(TokenKind::LParen, "after 'if'");
    s->cond = expr();
    expect(TokenKind::RParen, "after if condition");
    s->thenBody = block();
    if (match(TokenKind::KwElse)) {
        if (check(TokenKind::KwIf)) {
            s->elseBody.push_back(ifStmt());
        } else {
            s->elseBody = block();
        }
    }
    return s;
}

StmtPtr Parser::loopStmt() {
    const Token& kw = advance();
    auto s = std::make_unique<LoopStmt>();
    s->loc = kw.loc;
    expect(TokenKind::LParen, "after 'loop'");
    s->cond = expr();
    expect(TokenKind::RParen, "after loop condition");
    s->body = block();
    return s;
}

StmtPtr Parser::breakStmt() {
    const Token& kw = advance();
    auto s = std::make_unique<BreakStmt>();
    s->loc = kw.loc;
    expect(TokenKind::Semicolon, "after 'stop'");
    return s;
}

StmtPtr Parser::contStmt() {
    const Token& kw = advance();
    auto s = std::make_unique<ContinueStmt>();
    s->loc = kw.loc;
    expect(TokenKind::Semicolon, "after 'cont'");
    return s;
}

StmtPtr Parser::varDecl() {
    const Token& kw = advance();
    auto s = std::make_unique<VarDeclStmt>();
    s->loc = kw.loc;
    s->isMutable = kw.kind == TokenKind::KwVar;
    expect(TokenKind::LParen, "after variable keyword");
    s->type = type();
    expect(TokenKind::RParen, "after variable type");
    const Token& name = expect(TokenKind::Identifier, "as variable name");
    s->name = name.text;
    expect(TokenKind::Equal, "after variable name");
    s->value = expr();
    expect(TokenKind::Semicolon, "after variable declaration");
    return s;
}

StmtPtr Parser::assignStmt() {
    const Token& name = advance();
    auto s = std::make_unique<AssignStmt>();
    s->loc = name.loc;
    s->name = name.text;
    expect(TokenKind::Equal, "after variable name");
    s->value = expr();
    expect(TokenKind::Semicolon, "after assignment");
    return s;
}

StmtPtr Parser::returnStmt() {
    const Token& kw = advance();
    auto s = std::make_unique<ReturnStmt>();
    s->loc = kw.loc;
    if (!check(TokenKind::Semicolon)) s->value = expr();
    expect(TokenKind::Semicolon, "after r/ statement");
    return s;
}

static ExprPtr binaryExpr(const Token& op, ExprPtr left, ExprPtr right) {
    auto e = std::make_unique<BinaryExpr>();
    e->loc = op.loc;
    e->op = op.text;
    e->left = std::move(left);
    e->right = std::move(right);
    return e;
}

ExprPtr Parser::expr() { return equality(); }

ExprPtr Parser::equality() {
    auto left = comparison();
    while (check(TokenKind::EqualEqual) || check(TokenKind::BangEqual)) {
        const Token& op = advance();
        left = binaryExpr(op, std::move(left), comparison());
    }
    return left;
}

ExprPtr Parser::comparison() {
    auto left = term();
    while (check(TokenKind::Less) || check(TokenKind::LessEqual) ||
           check(TokenKind::Greater) || check(TokenKind::GreaterEqual)) {
        const Token& op = advance();
        left = binaryExpr(op, std::move(left), term());
    }
    return left;
}

ExprPtr Parser::term() {
    auto left = factor();
    while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
        const Token& op = advance();
        left = binaryExpr(op, std::move(left), factor());
    }
    return left;
}

ExprPtr Parser::factor() {
    auto left = unary();
    while (check(TokenKind::Star) || check(TokenKind::Slash)) {
        const Token& op = advance();
        left = binaryExpr(op, std::move(left), unary());
    }
    return left;
}

ExprPtr Parser::unary() {
    if (check(TokenKind::Amp)) {
        const Token& op = advance();
        auto e = std::make_unique<AddressOfExpr>();
        e->loc = op.loc;
        e->operand = unary();
        return e;
    }
    if (check(TokenKind::Star)) {
        const Token& op = advance();
        auto e = std::make_unique<DerefExpr>();
        e->loc = op.loc;
        e->operand = unary();
        return e;
    }
    return primary();
}

ExprPtr Parser::primary() {
    if (match(TokenKind::LParen)) {
        auto e = expr();
        expect(TokenKind::RParen, "to close grouped expression");
        return e;
    }
    if (check(TokenKind::IntLiteral)) {
        const Token& t = advance();
        auto e = std::make_unique<IntLiteralExpr>();
        e->loc = t.loc;
        e->value = t.intValue;
        return e;
    }
    if (check(TokenKind::StringLiteral)) {
        const Token& t = advance();
        auto e = std::make_unique<StringLiteralExpr>();
        e->loc = t.loc;
        e->value = t.text;
        return e;
    }
    if (check(TokenKind::Identifier)) {
        const Token& name = advance();
        std::string callee = name.text;
        while (check(TokenKind::Dot)) {
            advance();
            callee += "." + expect(TokenKind::Identifier, "after '.'").text;
        }
        if (!check(TokenKind::LParen)) {
            if (callee != name.text) err(name, Err::ExpectedToken, {"'('", "after '" + callee + "'", tokenName(peek().kind)});
            auto e = std::make_unique<IdentifierExpr>();
            e->loc = name.loc;
            e->name = callee;
            return e;
        }
        advance();
        auto call = std::make_unique<CallExpr>();
        call->loc = name.loc;
        call->callee = callee;
        if (!check(TokenKind::RParen)) {
            call->args.push_back(expr());
            while (match(TokenKind::Comma)) call->args.push_back(expr());
        }
        expect(TokenKind::RParen, "to close call");
        return call;
    }
    err(peek(), Err::ExpectedExpression);
}

}