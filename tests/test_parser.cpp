#include "shine/error.h"
#include "shine/lexer.h"
#include "shine/parser.h"
#include "test_framework.h"

using namespace shine;

static Module parse(const std::string& src) {
    Lexer l(src, "t.shine");
    Parser p(l.tokenize());
    return p.parseModule("t.shine");
}

TEST(empty_void_function) {
    Module m = parse("fn void main() { }");
    ASSERT_EQ(m.functions.size(), 1u);
    ASSERT_EQ(m.functions[0].name, "main");
}

TEST(return_int_literal) {
    Module m = parse("fn int main() { r/0; }");
    auto* r = dynamic_cast<ReturnStmt*>(m.functions[0].body[0].get());
    ASSERT_TRUE(r != nullptr);
    auto* i = dynamic_cast<IntLiteralExpr*>(r->value.get());
    ASSERT_TRUE(i != nullptr);
    ASSERT_EQ(i->value, 0);
}

TEST(call_statement) {
    Module m = parse(R"(fn void main() { write("hi"); })");
    auto* es = dynamic_cast<ExprStmt*>(m.functions[0].body[0].get());
    auto* c = dynamic_cast<CallExpr*>(es->expr.get());
    ASSERT_TRUE(c != nullptr);
    ASSERT_EQ(c->callee, "write");
}

TEST(let_decl_uses_parenthesized_type) {
    Module m = parse("fn int main() { let(int) a = 10; r/a; }");
    auto* v = dynamic_cast<VarDeclStmt*>(m.functions[0].body[0].get());
    ASSERT_TRUE(v != nullptr);
    ASSERT_TRUE(!v->isMutable);
    ASSERT_EQ(v->type.name, "int");
    ASSERT_EQ(v->name, "a");
}

TEST(var_decl_and_assignment) {
    Module m = parse("fn int main() { var(int) a = 10; a = a + 1; r/a; }");
    auto* v = dynamic_cast<VarDeclStmt*>(m.functions[0].body[0].get());
    auto* a = dynamic_cast<AssignStmt*>(m.functions[0].body[1].get());
    ASSERT_TRUE(v != nullptr);
    ASSERT_TRUE(v->isMutable);
    ASSERT_TRUE(a != nullptr);
    ASSERT_EQ(a->name, "a");
}

TEST(multiply_binds_before_add) {
    Module m = parse("fn int main() { r/1 + 2 * 3; }");
    auto* r = dynamic_cast<ReturnStmt*>(m.functions[0].body[0].get());
    auto* add = dynamic_cast<BinaryExpr*>(r->value.get());
    ASSERT_TRUE(add != nullptr);
    ASSERT_EQ(add->op, "+");
    auto* mul = dynamic_cast<BinaryExpr*>(add->right.get());
    ASSERT_TRUE(mul != nullptr);
    ASSERT_EQ(mul->op, "*");
}

TEST(grouped_expression) {
    Module m = parse("fn int main() { r/(1 + 2) * 3; }");
    auto* r = dynamic_cast<ReturnStmt*>(m.functions[0].body[0].get());
    auto* mul = dynamic_cast<BinaryExpr*>(r->value.get());
    ASSERT_TRUE(mul != nullptr);
    ASSERT_EQ(mul->op, "*");
    auto* add = dynamic_cast<BinaryExpr*>(mul->left.get());
    ASSERT_TRUE(add != nullptr);
    ASSERT_EQ(add->op, "+");
}

TEST(multiple_functions) {
    Module m = parse("fn void a() { } fn void b() { }");
    ASSERT_EQ(m.functions.size(), 2u);
}

TEST(missing_return_type_throws) {
    bool threw = false;
    try { parse("fn main() { }"); }
    catch (const CompileError&) { threw = true; }
    ASSERT_TRUE(threw);
}

TEST(unterminated_block_throws) {
    bool threw = false;
    try { parse("fn void main() {"); }
    catch (const CompileError&) { threw = true; }
    ASSERT_TRUE(threw);
}

TEST(if_else_parses) {
    Module m = parse("fn int main() { if (1) { r/1; } else { r/0; } }");
    auto* i = dynamic_cast<IfStmt*>(m.functions[0].body[0].get());
    ASSERT_TRUE(i != nullptr);
    ASSERT_EQ(i->thenBody.size(), 1u);
    ASSERT_EQ(i->elseBody.size(), 1u);
}

TEST(else_if_chain_nests_as_if) {
    Module m = parse("fn int main() { if (1) { } else if (2) { } else { } }");
    auto* i = dynamic_cast<IfStmt*>(m.functions[0].body[0].get());
    ASSERT_TRUE(i != nullptr);
    ASSERT_EQ(i->elseBody.size(), 1u);
    auto* nested = dynamic_cast<IfStmt*>(i->elseBody[0].get());
    ASSERT_TRUE(nested != nullptr);
    ASSERT_EQ(nested->elseBody.size(), 0u);
}

TEST(loop_with_break_and_continue) {
    Module m = parse("fn int main() { loop (1) { stop; cont; } r/0; }");
    auto* l = dynamic_cast<LoopStmt*>(m.functions[0].body[0].get());
    ASSERT_TRUE(l != nullptr);
    ASSERT_EQ(l->body.size(), 2u);
    ASSERT_TRUE(dynamic_cast<BreakStmt*>(l->body[0].get()) != nullptr);
    ASSERT_TRUE(dynamic_cast<ContinueStmt*>(l->body[1].get()) != nullptr);
}

TEST(array_type_and_literal_decl) {
    Module m = parse("fn int main() { var([5]i32) a = [1, 2, 3, 4, 5]; r/a[0]; }");
    auto* v = dynamic_cast<VarDeclStmt*>(m.functions[0].body[0].get());
    ASSERT_TRUE(v != nullptr);
    ASSERT_TRUE(v->type.type.isArray());
    ASSERT_EQ(v->type.type.length, 5);
    ASSERT_TRUE(v->type.type.element->isInt());
    auto* al = dynamic_cast<ArrayLiteralExpr*>(v->value.get());
    ASSERT_TRUE(al != nullptr);
    ASSERT_EQ(al->elements.size(), 5u);
}

TEST(array_type_nested) {
    Module m = parse("fn int main() { var([2][3]i32) g = [[1, 2, 3], [4, 5, 6]]; r/0; }");
    auto* v = dynamic_cast<VarDeclStmt*>(m.functions[0].body[0].get());
    ASSERT_TRUE(v != nullptr);
    ASSERT_TRUE(v->type.type.isArray());
    ASSERT_EQ(v->type.type.length, 2);
    ASSERT_TRUE(v->type.type.element->isArray());
    ASSERT_EQ(v->type.type.element->length, 3);
}

TEST(index_expression) {
    Module m = parse("fn int main() { var([3]i32) a = [1, 2, 3]; r/a[i + 1]; }");
    auto* r = dynamic_cast<ReturnStmt*>(m.functions[0].body[1].get());
    auto* ix = dynamic_cast<IndexExpr*>(r->value.get());
    ASSERT_TRUE(ix != nullptr);
    auto* bin = dynamic_cast<BinaryExpr*>(ix->index.get());
    ASSERT_TRUE(bin != nullptr);
    ASSERT_EQ(bin->op, "+");
}

TEST(index_assignment_statement) {
    Module m = parse("fn int main() { var([3]i32) a = [1, 2, 3]; a[1] = 42; r/0; }");
    auto* ia = dynamic_cast<IndexAssignStmt*>(m.functions[0].body[1].get());
    ASSERT_TRUE(ia != nullptr);
    ASSERT_TRUE(dynamic_cast<IdentifierExpr*>(ia->target.get()) != nullptr);
    ASSERT_TRUE(dynamic_cast<IntLiteralExpr*>(ia->index.get()) != nullptr);
    ASSERT_TRUE(dynamic_cast<IntLiteralExpr*>(ia->value.get()) != nullptr);
}

TEST(array_pointer_type_spellings) {
    Module m = parse("fn void f(a: *[5]i32, b: [5]*i32) { }");
    ASSERT_TRUE(m.functions[0].params[0].type.type.isPointer());
    ASSERT_TRUE(m.functions[0].params[0].type.type.pointee->isArray());
    ASSERT_TRUE(m.functions[0].params[1].type.type.isArray());
    ASSERT_TRUE(m.functions[0].params[1].type.type.element->isPointer());
}

TEST(zero_length_array_rejected) {
    bool threw = false;
    try { parse("fn int main() { var([0]i32) a = [1]; r/0; }"); }
    catch (const CompileError&) { threw = true; }
    ASSERT_TRUE(threw);
}

TEST(non_int_array_length_rejected) {
    bool threw = false;
    try { parse("fn int main() { var([x]i32) a = [1]; r/0; }"); }
    catch (const CompileError&) { threw = true; }
    ASSERT_TRUE(threw);
}
