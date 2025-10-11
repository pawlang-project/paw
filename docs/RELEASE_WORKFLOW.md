# 🚀 PawLang 发布工作流程指南

## 📋 概述

PawLang 使用 GitHub Actions 自动化构建和发布 pre-built releases。这个工作流程支持 7 个平台的自动构建、打包和发布。

## 🎯 支持的 7 个平台

| 平台 | 架构 | 包名 | 兼容性 |
|------|------|------|--------|
| 🐧 Linux | x86_64 | `pawlang-linux-x86_64.tar.gz` | Ubuntu 22.04+, Debian 12+ |
| 🐧 Linux | x86 (32-bit) | `pawlang-linux-x86.tar.gz` | 32位 Linux 系统 |
| 🐧 Linux | ARM32 (armv7) | `pawlang-linux-armv7.tar.gz` | ARM32 设备 |
| 🍎 macOS | Intel (x86_64) | `pawlang-macos-x86_64.tar.gz` | macOS 10.15+ |
| 🍎 macOS | Apple Silicon (ARM64) | `pawlang-macos-arm64.tar.gz` | macOS 11+ |
| 🪟 Windows | x86_64 | `pawlang-windows-x86_64.zip` | Windows 10+ |
| 🪟 Windows | x86 (32-bit) | `pawlang-windows-x86.zip` | Windows 10+ (32位) |

### 📝 Linux 版本说明

Linux x86_64 包使用 **Ubuntu 22.04 LTS** 编译，确保最大兼容性：
- ✅ Ubuntu 22.04 LTS (2022-2027)
- ✅ Ubuntu 23.04, 23.10, 24.04 LTS
- ✅ Debian 12 (Bookworm) 及更新版本
- ✅ 其他基于 GLIBC 2.35+ 的发行版

## 🔄 工作流程触发方式

### 方式 1: 推送版本标签（自动触发）

```bash
# 创建新版本标签
git tag v0.1.9

# 推送标签到 GitHub
git push origin v0.1.9

# GitHub Actions 会自动:
# 1. 创建 GitHub Release
# 2. 在所有平台上构建
# 3. 上传所有分发包
```

### 方式 2: 手动触发

1. 访问 GitHub 仓库
2. 点击 **Actions** 标签
3. 选择 **Release** workflow
4. 点击 **Run workflow** 按钮
5. 选择分支并运行

## 📦 工作流程步骤详解

### 第 1 步: 创建 Release

```yaml
create-release:
  - 从标签获取版本号
  - 创建 GitHub Release
  - 生成发布说明
  - 输出 upload_url 供后续使用
```

### 第 2 步: 构建 Linux 平台

```yaml
build-linux:
  - 统一构建环境: Ubuntu 22.04 LTS
  - 矩阵构建: x86_64, x86, armv7
  - 安装 LLVM 19.1.7
  - 构建编译器 (原生或交叉编译)
  - 创建分发包
  - 上传到 Release
```

### 第 3 步: 构建 macOS 平台

```yaml
build-macos:
  - 矩阵构建: x86_64, ARM64
  - 安装 LLVM 19.1.7 (Homebrew)
  - 构建编译器
  - 打包 LLVM 库 (带 @rpath 修复)
  - 上传到 Release
```

### 第 4 步: 构建 Windows 平台

```yaml
build-windows:
  - 矩阵构建: x86_64 (LLVM), x86 (C-only)
  - 安装 LLVM 19.1.7 (x86_64)
  - 构建编译器
  - 打包所有 DLL
  - 上传到 Release
```

### 第 5 步: 完成发布

```yaml
finalize-release:
  - 等待所有构建完成
  - 输出成功摘要
  - 提供 Release 链接
```

## 🛠️ 完整发布流程示例

### 准备发布

```bash
# 1. 确保在正确的分支上
git checkout main  # 或 master

# 2. 更新版本相关文件
# - CHANGELOG.md
# - docs/RELEASE_NOTES_vX.X.X.md
# - README.md (如果需要)

# 3. 提交更改
git add .
git commit -m "chore: prepare for v0.1.9 release"

# 4. 推送到远程
git push origin main
```

### 创建和推送标签

```bash
# 1. 创建带注释的标签
git tag -a v0.1.9 -m "🎉 PawLang v0.1.9 - Feature Description"

# 2. 推送标签
git push origin v0.1.9

# 3. GitHub Actions 自动开始构建
```

### 监控构建过程

```bash
# 使用 GitHub CLI 监控
gh run list --workflow=release.yml

# 查看详细日志
gh run watch
```

### 验证发布

```bash
# 列出所有发布
gh release list

# 查看特定发布
gh release view v0.1.9

# 下载并测试发布包
gh release download v0.1.9 -p "pawlang-*.tar.gz"
```

## 📋 发布检查清单

在创建新发布之前，确保完成以下检查：

- [ ] **代码质量**
  - [ ] 所有测试通过
  - [ ] CI/CD 绿色通过
  - [ ] 无已知严重 bug
  - [ ] 代码审查完成

- [ ] **文档更新**
  - [ ] CHANGELOG.md 更新
  - [ ] 创建 RELEASE_NOTES_vX.X.X.md
  - [ ] README.md 版本号更新
  - [ ] 文档链接有效

- [ ] **版本管理**
  - [ ] 版本号遵循语义化版本 (SemVer)
  - [ ] 标签格式正确 (vX.X.X)
  - [ ] 分支状态干净

- [ ] **构建验证**
  - [ ] 本地构建成功
  - [ ] 交叉编译测试通过
  - [ ] 分发包测试完成

- [ ] **发布内容**
  - [ ] 发布说明完整
  - [ ] 包含破坏性更改说明
  - [ ] 迁移指南（如需要）
  - [ ] 已知问题列表

## 🔧 高级配置

### 自定义发布说明

编辑 `.github/workflows/release.yml` 中的 `body` 部分：

```yaml
body: |
  ## 🎉 PawLang ${{ steps.get_version.outputs.version }} Release
  
  ### ✨ New Features
  - Feature 1
  - Feature 2
  
  ### 🐛 Bug Fixes
  - Fix 1
  - Fix 2
  
  ### 📚 Documentation
  - [Full Changelog](https://github.com/${{ github.repository }}/blob/main/CHANGELOG.md)
```

### 添加更多平台

在矩阵中添加新平台：

```yaml
strategy:
  matrix:
    include:
      # 添加 FreeBSD
      - arch: x86_64
        target: x86_64-freebsd
        enable_llvm: true
```

### 条件发布

只在主分支上发布：

```yaml
on:
  push:
    tags:
      - 'v*.*.*'
    branches:
      - main
      - master
```

## 📊 发布统计

### 构建时间估计

| 阶段 | 估计时间 |
|------|---------|
| 创建 Release | ~30 秒 |
| Linux 构建 (3个) | ~12 分钟 |
| macOS 构建 (2个) | ~15 分钟 |
| Windows 构建 (2个) | ~12 分钟 |
| 完成总结 | ~10 秒 |
| **总计** | **~20-25 分钟** |

### 包大小估计

| 平台 | 大小 |
|------|------|
| Linux | ~40-60 MB |
| macOS | ~40-60 MB |
| Windows | ~50-80 MB |

## 🚨 故障排除

### 构建失败

```bash
# 查看构建日志
gh run view --log

# 常见问题:
# 1. LLVM 安装失败 -> 检查版本号
# 2. 交叉编译错误 -> 验证目标三元组
# 3. 权限错误 -> 检查 GITHUB_TOKEN
```

### 上传失败

```bash
# 检查 Release 是否创建
gh release view v0.1.9

# 手动上传缺失的包
gh release upload v0.1.9 pawlang-*.tar.gz
```

### 标签冲突

```bash
# 删除本地标签
git tag -d v0.1.9

# 删除远程标签
git push --delete origin v0.1.9

# 删除 Release
gh release delete v0.1.9

# 重新创建
git tag -a v0.1.9 -m "..."
git push origin v0.1.9
```

## 🔗 相关资源

- [GitHub Actions 文档](https://docs.github.com/en/actions)
- [Release Assets 文档](https://docs.github.com/en/repositories/releasing-projects-on-github)
- [Zig 交叉编译指南](https://ziglang.org/learn/overview/#cross-compiling-is-a-first-class-use-case)
- [LLVM 下载](https://github.com/llvm/llvm-project/releases)

## 💡 最佳实践

1. **定期发布**: 保持稳定的发布节奏
2. **语义化版本**: 遵循 SemVer 规范
3. **详细说明**: 提供完整的发布说明
4. **测试验证**: 发布前充分测试
5. **文档同步**: 保持文档与代码同步
6. **用户反馈**: 及时响应用户问题
7. **备份标签**: 重要版本创建备份

## 📝 发布模板

### Git 标签注释模板

```bash
git tag -a v0.1.9 -m "
🎉 PawLang v0.1.9

✨ Features:
- New feature 1
- New feature 2

🐛 Fixes:
- Bug fix 1
- Bug fix 2

📚 Documentation:
- Updated guides
- New examples

🔗 https://github.com/KinLeoapple/PawLang/releases/tag/v0.1.9
"
```

### Release 说明模板

参考 `docs/RELEASE_NOTES_v0.1.8.md` 作为模板。

## 🎊 总结

PawLang 的自动化发布工作流程提供了：

- ✅ **全自动构建**: 推送标签即可触发
- ✅ **多平台支持**: 7个平台同时构建
  - 🐧 Linux: x86_64 (Ubuntu 22.04 LTS), x86, armv7
  - 🍎 macOS: x86_64, ARM64
  - 🪟 Windows: x86_64, x86
- ✅ **最大兼容性**: Ubuntu 22.04 编译，向前兼容 24.04+
- ✅ **自包含分发**: 包含所有依赖
- ✅ **质量保证**: 构建和测试验证
- ✅ **快速发布**: ~20-25 分钟完成
- ✅ **易于维护**: 清晰的工作流程配置

现在您可以轻松发布 PawLang 的新版本了！🚀

---

**Built with ❤️ for the PawLang Community**

