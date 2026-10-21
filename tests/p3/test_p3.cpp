#include "../mini_test.h"
#include "../test_helpers.h"
#include "tools/calculator.h"
#include "tools/tool_registry.h"
#include "tools/workspace.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
namespace fs = std::filesystem;
namespace {
class NamedTool : public Tool {
public:
    explicit NamedTool(std::string n) : n_(std::move(n)), schema_(JsonValue::object()) {}
    std::string name() const override { return n_; }
    std::string description() const override { return "test tool"; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue&, ToolContext&) override { return ToolResult::success(n_); }
private:
    std::string n_;
    JsonValue   schema_;
};
class CountedTool : public NamedTool {
public:
    CountedTool(std::string n, int* count) : NamedTool(std::move(n)), count_(count) {}
    ~CountedTool() override { ++*count_; }
private:
    int* count_;
};
bool eval(const std::string& expr, double* value, std::string* error) {
    std::vector<CalcToken> tokens;
    if (!calc_tokenize(expr, &tokens, error)) return false;
    return calc_evaluate(tokens, value, error);
}
}
TEST(reg_add_and_find) {
    ToolRegistry r;
    auto t = std::make_unique<NamedTool>("alpha");
    Tool* raw = t.get();
    REQUIRE(r.add(std::move(t)));
    CHECK(r.size() == 1);
    CHECK(r.find("alpha") == raw);
    CHECK(r.find("beta") == nullptr);
}
TEST(reg_duplicate_keeps_original) {
    ToolRegistry r;
    auto first = std::make_unique<NamedTool>("alpha");
    Tool* original = first.get();
    REQUIRE(r.add(std::move(first)));
    CHECK(!r.add(std::make_unique<NamedTool>("alpha")));
    CHECK(r.find("alpha") == original);
    CHECK(r.size() == 1);
}
TEST(ws_rejects_parent_escape) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("notes/todo.txt", &out, &reason));
    CHECK(out == ws.root() / "notes" / "todo.txt");
    CHECK(!ws.resolve("../etc/passwd", &out, &reason));
    CHECK(!reason.empty());
}
TEST(calc_precedence) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 + 3 * 4", &v, &err));
    CHECK(v == 14);
}
TEST(calc_division_by_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / 0", &v, &err));
    CHECK(err == "division by zero");
}
TEST(reg_empty_and_initial_buckets) {
    ToolRegistry r;
    CHECK(r.size() == 0);
    CHECK(r.bucket_count() >= 1 && r.bucket_count() <= 64);
    CHECK(r.load_factor() == 0.0);
    CHECK(r.max_probe_length() == 0);
}
TEST(reg_null_rejected) {
    ToolRegistry r;
    std::unique_ptr<Tool> none;
    CHECK(!r.add(std::move(none)));
    CHECK(r.size() == 0);
}
TEST(reg_for_each_visits_each_once) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("a")));
    REQUIRE(r.add(std::make_unique<NamedTool>("b")));
    REQUIRE(r.add(std::make_unique<NamedTool>("c")));
    std::vector<std::string> names;
    r.for_each([&](const Tool& t) { names.push_back(t.name()); });
    CHECK(names.size() == 3);
    for (const char* n : {"a", "b", "c"}) {
        CHECK(std::count(names.begin(), names.end(), n) == 1);
    }
}
TEST(reg_owned_tools_destroyed_once) {
    int destroyed = 0;
    {
        ToolRegistry r;
        REQUIRE(r.add(std::make_unique<CountedTool>("owned", &destroyed)));
        CHECK(!r.add(std::make_unique<CountedTool>("owned", &destroyed)));
        CHECK(destroyed == 1);
    }
    CHECK(destroyed == 2);
}
TEST(reg_hash_uses_order) {
    CHECK(ToolRegistry::hash("ab") != ToolRegistry::hash("ba"));
    CHECK(ToolRegistry::hash("tool_1234") != ToolRegistry::hash("tool_1243"));
}
TEST(reg_resize_preserves_tool_pointer) {
    ToolRegistry r;
    REQUIRE(r.add(std::make_unique<NamedTool>("stable")));
    Tool* before = r.find("stable");
    for (int i = 0; i < 100; ++i)
        REQUIRE(r.add(std::make_unique<NamedTool>("tool_" + std::to_string(i))));
    CHECK(r.find("stable") == before);
}
TEST(reg_10000_tools) {
    ToolRegistry r;
    Tool* first = nullptr;
    for (int i = 0; i < 10000; ++i) {
        const std::string name = "tool_" + std::to_string(i);
        auto tool = std::make_unique<NamedTool>(name);
        if (i == 0) first = tool.get();
        REQUIRE(r.add(std::move(tool)));
        CHECK(r.load_factor() <= 1.0);
    }
    CHECK(r.size() == 10000);
    CHECK(r.find("tool_0") == first);
    for (int i = 0; i < 10000; ++i)
        CHECK(r.find("tool_" + std::to_string(i)) != nullptr);
    CHECK(r.max_probe_length() <= 64);
    std::cout << "10,000 tools: buckets=" << r.bucket_count()
              << " load_factor=" << r.load_factor()
              << " max_chain=" << r.max_probe_length() << '\n';

    std::vector<std::size_t> counts(r.bucket_count(), 0);
    for (int i = 0; i < 10000; ++i) {
        const std::string name = "tool_" + std::to_string(i);
        const std::size_t weak = static_cast<unsigned char>(name[0]) + name.size();
        ++counts[weak % r.bucket_count()];
    }
    const std::size_t longest = *std::max_element(counts.begin(), counts.end());
    CHECK(longest == 9000);
    std::cout << "weak s[0]+length hash max_chain=" << longest << '\n';
}
TEST(ws_normalizes_relative_paths) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("./notes//todo.txt", &out, &reason));
    CHECK(out == ws.root() / "notes" / "todo.txt");
    REQUIRE(ws.resolve("a/../b", &out, &reason));
    CHECK(out == ws.root() / "b");
}
TEST(ws_dot_resolves_to_root) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve(".", &out, &reason));
    CHECK(out == ws.root());
    REQUIRE(ws.resolve("a/..", &out, &reason));
    CHECK(out == ws.root());
}
TEST(ws_accepts_nonexistent_and_double_dot_name) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    REQUIRE(ws.resolve("new/dir/file.txt", &out, &reason));
    CHECK(out == ws.root() / "new" / "dir" / "file.txt");
    REQUIRE(ws.resolve("my..notes.txt", &out, &reason));
    CHECK(out == ws.root() / "my..notes.txt");
}
TEST(ws_rejects_empty_absolute_and_nul_paths) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    CHECK(!ws.resolve("", &out, &reason));
    CHECK(reason == "empty path");
    CHECK(!ws.resolve("/etc/passwd", &out, &reason));
    CHECK(!ws.resolve(std::string("notes") + '\0' + "/x", &out, &reason));
    CHECK(!reason.empty());
}
TEST(ws_rejects_paths_that_climb_above_root_then_return) {
    TempDir d;
    Workspace ws(d.path());
    fs::path out;
    std::string reason;
    CHECK(!ws.resolve("..", &out, &reason));
    CHECK(!ws.resolve("a/../../b", &out, &reason));
    CHECK(!ws.resolve("notes/../../ws/notes/todo.txt", &out, &reason));
}
TEST(calc_left_associative_subtraction_and_division) {
    double v = 0;
    std::string err;
    REQUIRE(eval("10 - 4 - 3", &v, &err));
    CHECK(v == 3);
    REQUIRE(eval("100 / 10 / 5", &v, &err));
    CHECK(v == 2);
}
TEST(calc_parentheses) {
    double v = 0;
    std::string err;
    REQUIRE(eval("2 * (3 + 4) - 5", &v, &err));
    CHECK(v == 9);
    REQUIRE(eval("(2 + 3) * 4", &v, &err));
    CHECK(v == 20);
}
TEST(calc_negative_numbers_and_decimals) {
    double v = 0;
    std::string err;
    REQUIRE(eval("-3 * -3", &v, &err));
    CHECK(v == 9);
    REQUIRE(eval("7 / 2", &v, &err));
    CHECK(v == 3.5);
    REQUIRE(eval("0.25 + 1.5", &v, &err));
    CHECK(std::fabs(v - 1.75) < 1e-12);
}
TEST(calc_division_by_parenthesized_zero) {
    double v = 0;
    std::string err;
    CHECK(!eval("1 / (2 - 2)", &v, &err));
    CHECK(err == "division by zero");
}
TEST(calc_long_expression_iterative) {
    std::string expression = "1";
    for (int i = 0; i < 20000; ++i) expression += "+1";
    double v = 0;
    std::string err;
    REQUIRE(eval(expression, &v, &err));
    CHECK(v == 20001);
}
TEST(calc_deep_parentheses_iterative) {
    std::string expression(5000, '(');
    expression += "7";
    expression.append(5000, ')');
    double v = 0;
    std::string err;
    REQUIRE(eval(expression, &v, &err));
    CHECK(v == 7);
}
int main(int argc, char** argv) { return mt::run_all(argc, argv); }
