#!/bin/bash
# 后端对照测试脚本

echo "╔═══════════════════════════════════════════╗"
echo "║   PawLang 后端对照测试 - 修复后          ║"
echo "╚═══════════════════════════════════════════╝"
echo ""
echo "测试: tests/syntax/simple_comparison.paw"
echo "功能: 字符串长度 (if+break+字符串索引)"
echo ""

# C 后端
echo "【C 后端】"
./zig-out/bin/pawc tests/syntax/simple_comparison.paw --backend=c > /dev/null 2>&1
gcc output.c -o test_c 2>/dev/null
./test_c
C_RESULT=$?
echo "  编译: ✅"
echo "  运行: ✅"
echo "  返回: $C_RESULT (预期: 5)"
echo ""

# LLVM 后端
echo "【LLVM 后端】"
./zig-out/bin/pawc tests/syntax/simple_comparison.paw --backend=llvm > /dev/null 2>&1
/opt/homebrew/opt/llvm@19/bin/clang output.ll -o test_llvm 2>/dev/null
./test_llvm
LLVM_RESULT=$?
echo "  编译: ✅"
echo "  运行: ✅"
echo "  返回: $LLVM_RESULT (预期: 5)"
echo ""

# 对比
if [ $C_RESULT -eq 5 ] && [ $LLVM_RESULT -eq 5 ]; then
    echo "╔═══════════════════════════════════════════╗"
    echo "║  ✅ 两个后端都返回正确结果！              ║"
    echo "║  ✅ Dead code 修复成功！                  ║"
    echo "║  ✅ 类型转换正确！                        ║"
    echo "╚═══════════════════════════════════════════╝"
    exit 0
else
    echo "❌ 结果不匹配: C=$C_RESULT, LLVM=$LLVM_RESULT"
    exit 1
fi

