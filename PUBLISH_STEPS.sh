#!/bin/bash

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║        🚀 PawLang v0.2.1 发布脚本                           ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

echo "📋 发布准备状态："
echo "  ✅ 代码已提交 (1927e255)"
echo "  ✅ Tag已创建 (v0.2.1)"
echo "  ✅ 发布说明已准备 (RELEASE_v0.2.1.md)"
echo ""

echo "⏳ 开始推送..."
echo ""

# Step 1: 推送代码
echo "Step 1: 推送代码到 GitHub..."
git push origin 0.2.1
if [ $? -eq 0 ]; then
    echo "  ✅ 代码推送成功"
else
    echo "  ❌ 代码推送失败"
    echo "  请检查网络连接和 GitHub 认证"
    exit 1
fi
echo ""

# Step 2: 推送 tag
echo "Step 2: 推送 Tag..."
git push origin v0.2.1
if [ $? -eq 0 ]; then
    echo "  ✅ Tag 推送成功"
else
    echo "  ❌ Tag 推送失败"
    exit 1
fi
echo ""

echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║        ✅ 推送完成！                                         ║"
echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

echo "📝 下一步："
echo "1. 访问 GitHub Release 页面："
echo "   https://github.com/YOUR_USERNAME/paw/releases/new"
echo ""
echo "2. 填写 Release 信息："
echo "   • Tag: v0.2.1"
echo "   • Title: PawLang v0.2.1 - 完全泛型标准库"
echo "   • Description: 复制 RELEASE_v0.2.1.md 的内容"
echo ""
echo "3. 点击 'Publish release'"
echo ""
echo "🎉 恭喜！PawLang v0.2.1 即将发布！"
