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
    Module m;
    m.file = std::move(file);
    while (!atEnd()) {
        if (check(TokenKind::KwStruct)) m.structs.push_back(structDecl());
        else m.functions.push_back(function());
    }
    return m;
}

StructDecl Parser::structDecl() {
    const Token& kw = advance(); // 'struct'
    const Token& name = expect(TokenKind::Identifier, "as struct name");
    StructDecl d;
    d.name = name.text;
    d.loc = kw.loc;
    expect(TokenKind::LBrace, "to start struct body");
    while (!check(TokenKind::RBrace)) {
        const Token& fname = expect(TokenKind::Identifier, "as field name");
        expect(TokenKind::Colon, "after field name");
        d.fields.push_back({fname.text, type()});
        if (!match(TokenKind::Comma)) break; // trailing comma is optional
    }
    expect(TokenKind::RBrace, "to close struct body");
    return d;
}

TypeRef Parser::type() {
    // Prefix modifiers, read right-to-left: '*' means pointer and '[N]'
    // means fixed-size array, so *[5]i32 is "pointer to [5]i32" and
    // [5]*i32 is "[5] pointers to i32".
    struct Mod { enum { Star, Array } kind; int64_t length; };
    std::vector<Mod> mods;
    while (check(TokenKind::Star) || check(TokenKind::LBracket)) {
        if (match(TokenKind::Star)) {
            mods.push_back({Mod::Star, 0});
        } else {
            advance(); // consume '['
            const Token& n = expect(TokenKind::IntLiteral, "as array length");
            if (n.intValue < 1) err(n, Err::ArrayLengthZero, {std::to_string(n.intValue)});
            expect(TokenKind::RBracket, "to close array length");
            mods.push_back({Mod::Array, n.intValue});
        }
    }

    std::string name;
    SourceLoc loc;
    if (check(TokenKind::KwInt) || check(TokenKind::KwVoid)) {
        const Token& t = advance();
        name = t.kind == TokenKind::KwInt ? "int" : "void";
        loc = t.loc;
    } else if (check(TokenKind::Identifier)) {
        const Token& t = advance();
        name = t.text;
        loc = t.loc;
    } else {
        err(peek(), Err::ExpectedType);
    }

    TypeRef ref{name, {}, loc};
    if (!resolveTypeName(name, ref.type)) {
        // Not a built-in spelling -- treat it as a reference to a struct
        // type by name; the type-checker resolves whether it actually exists.
        ref.type = Type::makeStruct(name);
    }
    for (auto it = mods.rbegin(); it != mods.rend(); ++it) {
        if (it->kind == Mod::Star) {
            ref.type = Type::makePointer(ref.type);
            ref.name = "*" + ref.name;
        } else {
            ref.type = Type::makeArray(ref.type, it->length);
            ref.name = "[" + std::to_string(it->length) + "]" + ref.name;
        }
    }
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

    SourceLoc loc = peek().loc;
    ExprPtr e = expr();

    if (match(TokenKind::Equal)) {
        ExprPtr value = expr();
        expect(TokenKind::Semicolon, "after assignment");

        if (auto* id = dynamic_cast<IdentifierExpr*>(e.get())) {
            auto s = std::make_unique<AssignStmt>();
            s->loc = loc;
            s->name = id->name;
            s->value = std::move(value);
            return s;
        }
        if (auto* de = dynamic_cast<DerefExpr*>(e.get())) {
            auto s = std::make_unique<DerefAssignStmt>();
            s->loc = loc;
            s->target = std::move(de->operand);
            s->value = std::move(value);
            return s;
        }
        if (auto* fa = dynamic_cast<FieldAccessExpr*>(e.get())) {
            auto s = std::make_unique<FieldAssignStmt>();
            s->loc = loc;
            s->target = std::move(fa->target);
            s->field = fa->field;
            s->value = std::move(value);
            return s;
        }
        if (auto* ix = dynamic_cast<IndexExpr*>(e.get())) {
            auto s = std::make_unique<IndexAssignStmt>();
            s->loc = loc;
            s->target = std::move(ix->target);
            s->index = std::move(ix->index);
            s->value = std::move(value);
            return s;
        }
        err(peek(-1), Err::ExpectedExpression);
    }

    auto s = std::make_unique<ExprStmt>();
    s->loc = loc;
    s->expr = std::move(e);
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
    return postfix();
}

ExprPtr Parser::postfix() {
    ExprPtr e = primary();
    for (;;) {
        if (check(TokenKind::Dot)) {
            advance();
            const Token& field = expect(TokenKind::Identifier, "after '.'");
            auto fa = std::make_unique<FieldAccessExpr>();
            fa->loc = field.loc;
            fa->target = std::move(e);
            fa->field = field.text;
            e = std::move(fa);
        } else if (check(TokenKind::LBracket)) {
            const Token& lb = advance();
            auto ix = std::make_unique<IndexExpr>();
            ix->loc = lb.loc;
            ix->target = std::move(e);
            ix->index = expr();
            expect(TokenKind::RBracket, "to close index");
            e = std::move(ix);
        } else {
            break;
        }
    }
    return e;
}

ExprPtr Parser::primary() {
    if (match(TokenKind::LParen)) {
        auto e = expr();
        expect(TokenKind::RParen, "to close grouped expression");
        return e;
    }
    if (check(TokenKind::LBracket)) return arrayLiteral();
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

        // Struct literal: Name { field: expr, ... }
        if (check(TokenKind::LBrace)) return structLiteral(name);

        std::string callee = name.text;
        // The only namespaced builtin is terminal.pause(); ordinary dotted
        // access (struct.field, chains, etc.) is handled by postfix().
        if (callee == "terminal" && check(TokenKind::Dot) &&
            peek(1).kind == TokenKind::Identifier && peek(1).text == "pause") {
            advance();
            callee += "." + advance().text;
        }

        if (!check(TokenKind::LParen)) {
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

ExprPtr Parser::structLiteral(const Token& name) {
    auto lit = std::make_unique<StructLiteralExpr>();
    lit->loc = name.loc;
    lit->structName = name.text;
    expect(TokenKind::LBrace, "to start struct literal");
    while (!check(TokenKind::RBrace)) {
        const Token& fname = expect(TokenKind::Identifier, "as field name");
        expect(TokenKind::Colon, "after field name");
        lit->fields.emplace_back(fname.text, expr());
        if (!match(TokenKind::Comma)) break; // trailing comma is optional
    }
    expect(TokenKind::RBrace, "to close struct literal");
    return lit;
}

ExprPtr Parser::arrayLiteral() {
    const Token& lb = advance();
    auto lit = std::make_unique<ArrayLiteralExpr>();
    lit->loc = lb.loc;
    if (!check(TokenKind::RBracket)) {
        lit->elements.push_back(expr());
        while (match(TokenKind::Comma)) lit->elements.push_back(expr());
    }
    expect(TokenKind::RBracket, "to close array literal");
    return lit;
}

}