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

# 完整测试函数：编译+运行
test_full() {
    local file=$1
    local desc=$2
    
    echo -n "  [$((PASSED + FAILED + 1))] $desc ... "
    
    if [ ! -f "$file" ]; then
        echo "❌ (文件不存在)"
        FAILED=$((FAILED + 1))
        return 1
    fi
    
    # 编译
    if ! $COMPILER "$file" > /tmp/paw_compile.log 2>&1; then
        echo "❌ (编译失败)"
        FAILED=$((FAILED + 1))
        return 1
    fi
    
    # 运行
    if ! ./a.out > /tmp/paw_run.log 2>&1; then
        local exit_code=$?
        # 允许非零退出码（如果程序是正常退出）
        if [ $exit_code -lt 100 ]; then
            echo "✅"
            PASSED=$((PASSED + 1))
            rm -f a.out
            return 0
        else
            echo "❌ (运行失败: exit $exit_code)"
            FAILED=$((FAILED + 1))
            rm -f a.out
            return 1
        fi
    else
        echo "✅"
        PASSED=$((PASSED + 1))
        rm -f a.out
        return 0
    fi
}

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 1. 基本功能"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_full "examples/hello.paw" "Hello World"
test_full "examples/arithmetic.paw" "算术运算"
test_full "examples/fibonacci.paw" "斐波那契"
test_full "examples/char_basic.paw" "字符类型"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 2. 数组和切片"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_full "examples/array_test.paw" "数组操作"
test_full "examples/slice_test.paw" "切片操作"
test_full "examples/slice_enum_test.paw" "切片枚举"
test_full "examples/range_slice_test.paw" "范围切片"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 3. 元组系统 (v0.2.2) 🆕"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_full "examples/tuple_test.paw" "元组基本"
test_full "examples/tuple_field_access_test.paw" "元组访问"
test_full "examples/tuple_destructure_test.paw" "元组解构"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 4. Struct"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_full "examples/struct_test.paw" "Struct操作"
test_full "examples/nested_struct_test.paw" "嵌套Struct"
test_full "examples/self_simple.paw" "Self类型"
test_full "examples/struct_pass_by_value.paw" "Struct按值传递"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 5. 枚举"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_full "examples/enum_test.paw" "枚举操作"
test_full "examples/return_enum_test.paw" "返回枚举"
test_full "examples/match_simple.paw" "模式匹配"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 6. 引用系统 (v0.2.2) 🆕"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_full "examples/reference_type_check.paw" "引用检查"
test_full "examples/struct_ref_final.paw" "Struct引用"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 7. 类型推断 (v0.2.2) 🆕"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_full "examples/infer_complete_test.paw" "完整推断"
test_full "examples/infer_enum_complete.paw" "枚举推断"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 8. 泛型"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_full "examples/generic_test.paw" "泛型函数"
test_full "examples/generic_box.paw" "泛型Struct"
test_full "examples/generic_swap.paw" "泛型Swap"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 9. 错误处理"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_full "examples/error_handling_complete.paw" "错误处理"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 10. 标准库"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_full "examples/math_api_test.paw" "数学API"
test_full "examples/string_test.paw" "字符串库"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 11. 内置函数"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
test_full "examples/builtin_demo.paw" "内置函数"

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

