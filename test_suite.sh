#!/bin/bash

# PawLang v0.2.2 完整测试套件
# 测试所有核心功能

set -e  # 遇到错误立即退出

COMPILER="./build/pawc"
PASSED=0
FAILED=0
TOTAL=0

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║    🧪 PawLang v0.2.2 完整测试套件                           ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# 测试函数
test_file() {
    local file=$1
    local desc=$2
    
    TOTAL=$((TOTAL + 1))
    
    if [ ! -f "$file" ]; then
        echo "❌ $desc"
        echo "   文件不存在: $file"
        FAILED=$((FAILED + 1))
        return 1
    fi
    
    echo -n "测试 $TOTAL: $desc ... "
    
    # 编译
    if $COMPILER "$file" > /tmp/paw_test.log 2>&1; then
        # 运行（如果退出码为0或期望的值）
        ./a.out > /tmp/paw_output.log 2>&1
        exit_code=$?
        
        if [ $exit_code -eq 0 ] || [ $exit_code -eq 60 ]; then
            echo "✅ PASS"
            PASSED=$((PASSED + 1))
            rm -f a.out
            return 0
        else
            echo "❌ FAIL (退出码 $exit_code)"
            cat /tmp/paw_output.log
            FAILED=$((FAILED + 1))
            rm -f a.out
            return 1
        fi
    else
        echo "❌ FAIL (编译失败)"
        tail -20 /tmp/paw_test.log
        FAILED=$((FAILED + 1))
        return 1
    fi
}

# 1. 基本类型测试
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 1. 基本功能测试"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

test_file "examples/hello.paw" "Hello World"
test_file "examples/arithmetic.paw" "算术运算"
test_file "examples/char_basic.paw" "字符类型"
test_file "examples/string_complete.paw" "字符串操作"

# 2. 数组和切片测试
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 2. 数组和切片测试"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

test_file "examples/array_test.paw" "数组操作"
test_file "examples/slice_enum_test.paw" "切片枚举"

# 3. 元组测试
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 3. 元组测试"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# 创建简单的元组测试
if [ ! -f "examples/tuple_basic.paw" ]; then
    cat > examples/tuple_basic.paw << 'EOF'
// 基本元组测试
fn main() -> i32 {
    let t = (10, 20, 30);
    let val = t.1;
    assert(val == 20);
    println("✅ 元组访问成功");
    
    let (a, b, c) = (1, 2, 3);
    assert(a == 1 && b == 2 && c == 3);
    println("✅ 元组解构成功");
    
    return 0;
}
EOF
fi

test_file "examples/tuple_basic.paw" "元组基本操作"

# 4. Struct测试
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 4. Struct测试"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

test_file "examples/struct_test.paw" "Struct操作"
test_file "examples/nested_struct_test.paw" "嵌套Struct"

# 5. 枚举测试
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 5. 枚举测试"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

test_file "examples/enum_test.paw" "枚举操作"
test_file "examples/return_enum_test.paw" "返回枚举"

# 6. 引用系统测试
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 6. 引用系统测试 (v0.2.2) 🆕"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

test_file "examples/reference_type_check.paw" "引用类型检查"
test_file "examples/struct_ref_final.paw" "Struct引用"

# 7. 类型推断测试
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 7. 类型推断测试 (v0.2.2) 🆕"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

test_file "examples/infer_complete_test.paw" "完整类型推断"
test_file "examples/infer_enum_complete.paw" "枚举类型推断"

# 8. 标准库测试
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 8. 标准库测试"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

test_file "examples/math_api_test.paw" "数学API"
test_file "examples/string_test.paw" "字符串库"

# 9. 错误处理测试
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 9. 错误处理测试"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

test_file "examples/error_handling_complete.paw" "错误处理"

# 10. 泛型测试
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "📦 10. 泛型测试"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

test_file "examples/generic_test.paw" "泛型函数"

# 总结
echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║                     📊 测试结果汇总                           ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""
echo "总测试数: $TOTAL"
echo "✅ 通过: $PASSED"
echo "❌ 失败: $FAILED"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "🎊🎊 所有测试通过！🎊🎊"
    exit 0
else
    echo "⚠️  有测试失败，请检查日志"
    exit 1
fi

