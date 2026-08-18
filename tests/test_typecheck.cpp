#include "shine/error.h"
#include "shine/lexer.h"
#include "shine/parser.h"
#include "shine/typecheck.h"
#include "test_framework.h"

using namespace shine;

static void check(const std::string& src) {
    Lexer l(src, "t.shine");
    Parser p(l.tokenize());
    Module m = p.parseModule("t.shine");
    TypeChecker().check(m);
}

TEST(array_decl_typechecks) {
    check("fn int main() { var([3]i32) a = [1, 2, 3]; a[1] = 42; r/a[1]; }");
}

TEST(array_index_out_of_bounds_throws) {
    bool threw = false;
    try { check("fn int main() { var([3]i32) a = [1, 2, 3]; r/a[3]; }"); }
    catch (const CompileError&) { threw = true; }
    ASSERT_TRUE(threw);
}

TEST(array_literal_count_mismatch_throws) {
    bool threw = false;
    try { check("fn int main() { var([3]i32) a = [1, 2]; r/0; }"); }
    catch (const CompileError&) { threw = true; }
    ASSERT_TRUE(threw);
}

TEST(array_literal_to_non_array_throws) {
    bool threw = false;
    try { check("fn int main() { var(int) a = [1, 2]; r/0; }"); }
    catch (const CompileError&) { threw = true; }
    ASSERT_TRUE(threw);
}

TEST(index_non_array_throws) {
    bool threw = false;
    try { check("fn int main() { var(int) a = 1; r/a[0]; }"); }
    catch (const CompileError&) { threw = true; }
    ASSERT_TRUE(threw);
}

TEST(index_non_int_throws) {
    bool threw = false;
    try { check("fn int main() { var([3]int) a = [1, 2, 3]; var(*int) p = 0; r/a[p]; }"); }
    catch (const CompileError&) { threw = true; }
    ASSERT_TRUE(threw);
}

TEST(array_elements_widen_to_target_type) {
    check("fn i64 main() { var([3]i64) a = [1, 2, 3]; r/a[0]; }");
}

TEST(array_element_of_struct_type) {
    check(R"(struct P { x: int, }
fn int main() {
    var([2]P) arr = [P { x: 1 }, P { x: 2 }];
    r/arr[0].x;
})");
}