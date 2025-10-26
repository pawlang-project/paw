#!/bin/bash

# PawLang v0.2.2 快速测试套件
# 只测试核心功能，避免卡住

set -e

COMPILER="./build/pawc"
PASSED=0
FAILED=0

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║    🧪 PawLang v0.2.2 快速测试                               ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# 简单测试函数：只检查编译
test_compile() {
    local file=$1
    local desc=$2
    
    echo -n "  [$((PASSED + FAILED + 1))] $desc ... "
    
    if [ ! -f "$file" ]; then
        echo "❌ (文件不存在)"
        FAILED=$((FAILED + 1))
        return 1
    fi
    
    if $COMPILER "$file" > /dev/null 2>&1; then
        echo "✅"
        PASSED=$((PASSED + 1))
        rm -f a.out
        return 0
    else
        echo "❌"
        FAILED=$((FAILED + 1))
        return 1
    fi
}

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 1. 基本功能"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_compile "examples/hello.paw" "Hello World"
test_compile "examples/arithmetic.paw" "算术运算"
test_compile "examples/fibonacci.paw" "斐波那契"
test_compile "examples/char_basic.paw" "字符类型"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 2. 数组和切片"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_compile "examples/array_test.paw" "数组操作"
test_compile "examples/slice_test.paw" "切片操作"
test_compile "examples/slice_enum_test.paw" "切片枚举"
test_compile "examples/range_slice_test.paw" "范围切片"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 3. 元组系统 (v0.2.2) 🆕"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_compile "examples/tuple_test.paw" "元组基本"
test_compile "examples/tuple_field_access_test.paw" "元组访问"
test_compile "examples/tuple_destructure_test.paw" "元组解构"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 4. Struct"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_compile "examples/struct_test.paw" "Struct操作"
test_compile "examples/nested_struct_test.paw" "嵌套Struct"
test_compile "examples/self_simple.paw" "Self类型"
test_compile "examples/struct_pass_by_value.paw" "Struct按值传递"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 5. 枚举"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_compile "examples/enum_test.paw" "枚举操作"
test_compile "examples/return_enum_test.paw" "返回枚举"
test_compile "examples/match_simple.paw" "模式匹配"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 6. 引用系统 (v0.2.2) 🆕"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_compile "examples/reference_type_check.paw" "引用检查"
test_compile "examples/struct_ref_final.paw" "Struct引用"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 7. 类型推断 (v0.2.2) 🆕"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_compile "examples/infer_complete_test.paw" "完整推断"
test_compile "examples/infer_enum_complete.paw" "枚举推断"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 8. 泛型"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_compile "examples/generic_test.paw" "泛型函数"
test_compile "examples/generic_box.paw" "泛型Struct"
test_compile "examples/generic_swap.paw" "泛型Swap"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 9. 错误处理"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_compile "examples/error_handling_complete.paw" "错误处理"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 10. 标准库"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_compile "examples/math_api_test.paw" "数学API"
test_compile "examples/string_test.paw" "字符串库"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 11. 内置函数"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_compile "examples/builtin_demo.paw" "内置函数"

echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║                     📊 测试结果                               ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""
TOTAL=$((PASSED + FAILED))
echo "  总测试数: $TOTAL"
echo "  ✅ 通过: $PASSED"
echo "  ❌ 失败: $FAILED"
echo ""

if [ $FAILED -eq 0 ]; then
    PERCENT=100
    echo "  🎊 通过率: $PERCENT%"
    echo ""
    echo "  🎉🎉 所有测试通过！🎉🎉"
    exit 0
else
    PERCENT=$((PASSED * 100 / TOTAL))
    echo "  📊 通过率: $PERCENT%"
    echo ""
    echo "  ⚠️  有 $FAILED 个测试失败"
    exit 1
fi

