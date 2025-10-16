#!/usr/bin/env python3
"""自动拆分codegen.cpp为6个文件"""

import re

# 读取codegen.cpp
with open('src/codegen/codegen.cpp', 'r') as f:
    lines = f.readlines()

# 函数分类（函数名 -> 文件）
FUNCTION_MAP = {
    # Match functions
    'generateMatchExpr': 'match',
    'generateIsExpr': 'match',
    'matchPattern': 'match',
    
    # Type functions
    'convertType': 'type',
    'convertPrimitiveType': 'type',
    'getOrCreateStructType': 'type',
    'getEnumType': 'type',
    'createOptionalType': 'type',
    'ensureOptionalEnumDef': 'type',
    'resolveGenericType': 'type',
    'mangleGenericName': 'type',
    'resolveGenericStructName': 'type',
    'isGenericFunction': 'type',
    'instantiateGenericFunction': 'type',
    'instantiateGenericStruct': 'type',
    'instantiateGenericEnum': 'type',
    'instantiateGenericStructMethods': 'type',
    'convertTypeToCurrentContext': 'type',
    
    # Struct functions
    'generateFunctionStmt': 'struct',
    'generateExternStmt': 'struct',
    'generateStructStmt': 'struct',
    'generateEnumStmt': 'struct',
    'generateImplStmt': 'struct',
    'generateStructLiteralExpr': 'struct',
    'generateEnumVariantExpr': 'struct',
    'generateMemberAccessExpr': 'struct',
    'generateArrayLiteralExpr': 'struct',
    'importTypeFromModule': 'struct',
    
    # Statement functions
    'generateStmt': 'stmt',
    'generateLetStmt': 'stmt',
    'generateReturnStmt': 'stmt',
    'generateIfStmt': 'stmt',
    'generateLoopStmt': 'stmt',
    'generateBreakStmt': 'stmt',
    'generateContinueStmt': 'stmt',
    'generateBlockStmt': 'stmt',
    'generateExprStmt': 'stmt',
    
    # Expression functions
    'generateExpr': 'expr',
    'generateBinaryExpr': 'expr',
    'generateUnaryExpr': 'expr',
    'generateCallExpr': 'expr',
    'generateArgumentValue': 'expr',
    'generateBuiltinCall': 'expr',
    'generateAssignExpr': 'expr',
    'generateIndexExpr': 'expr',
    'generateIdentifierExpr': 'expr',
    'generateIfExpr': 'expr',
    'generateCastExpr': 'expr',
    'generateTryExpr': 'expr',
    'generateOkExpr': 'expr',
    'generateErrExpr': 'expr',
}

# 查找函数边界
def find_functions():
    functions = {}
    i = 0
    while i < len(lines):
        line = lines[i]
        # 匹配函数定义
        match = re.match(r'^(llvm::\w+\*|void|bool|std::string) CodeGenerator::(\w+)\(', line)
        if match:
            func_name = match.group(2)
            if func_name in FUNCTION_MAP:
                start = i
                # 查找函数结束（下一个函数或文件结束）
                j = i + 1
                brace_count = 0
                in_function = False
                while j < len(lines):
                    if '{' in lines[j]:
                        brace_count += lines[j].count('{')
                        in_function = True
                    if '}' in lines[j]:
                        brace_count -= lines[j].count('}')
                    if in_function and brace_count == 0:
                        end = j + 1
                        break
                    j += 1
                else:
                    end = len(lines)
                
                functions[func_name] = (start, end, FUNCTION_MAP[func_name])
                print(f"Found {func_name}: lines {start+1}-{end} ({end-start} lines) -> {FUNCTION_MAP[func_name]}")
                i = end
                continue
        i += 1
    
    return functions

print("=== 扫描函数 ===")
functions = find_functions()
print(f"\n找到 {len(functions)} 个函数")

# 按文件分组
file_functions = {'match': [], 'type': [], 'struct': [], 'stmt': [], 'expr': []}
for func_name, (start, end, target_file) in functions.items():
    file_functions[target_file].append((func_name, start, end))

# 排序（按行号）
for key in file_functions:
    file_functions[key].sort(key=lambda x: x[1])

# 生成新文件
headers = {
    'match': '// 模式匹配代码生成\n',
    'type': '// 类型转换和泛型实例化\n',
    'struct': '// Struct和Enum代码生成\n',
    'stmt': '// 语句代码生成\n',
    'expr': '// 表达式代码生成\n',
}

for file_key, funcs in file_functions.items():
    if not funcs:
        continue
    
    filename = f'src/codegen/codegen_{file_key}.cpp'
    print(f"\n生成 {filename} ({len(funcs)} 个函数)...")
    
    with open(filename, 'w') as f:
        f.write(headers[file_key])
        f.write('#include "codegen.h"\n')
        f.write('#include <iostream>\n\n')
        f.write('namespace pawc {\n\n')
        
        for func_name, start, end in funcs:
            f.write(''.join(lines[start:end]))
            f.write('\n')
        
        f.write('} // namespace pawc\n')
    
    print(f"  ✓ {filename}")

# 生成精简的codegen.cpp
print("\n生成精简的codegen.cpp...")
with open('src/codegen/codegen_new.cpp', 'w') as f:
    # 写入头部（到第一个函数之前）
    i = 0
    while i < len(lines):
        if re.match(r'^(llvm::\w+\*|void|bool|std::string) CodeGenerator::', lines[i]):
            break
        f.write(lines[i])
        i += 1
    
    # 只保留核心函数
    keep_functions = ['printIR', 'saveIR', 'compileToObject', 'generate']
    for func_name in keep_functions:
        if func_name in functions:
            start, end, _ = functions[func_name]
            f.write(''.join(lines[start:end]))
            f.write('\n')
    
    # 写入结束
    f.write('} // namespace pawc\n')

print("  ✓ codegen_new.cpp")

print("\n=== 拆分完成 ===")
print(f"生成5个新文件 + 1个精简文件")

