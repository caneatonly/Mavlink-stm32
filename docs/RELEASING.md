# 提交、合并与发布

## 日常工作树流程

1. 从master建立功能分支及独立工作树，在该目录开发、编译和实机验证。
2. 在功能工作树检查差异，只提交源码、依赖、测试输入及文档，使用中文commit信息。
3. 在干净的master工作树合并功能分支；原test工作树的未提交改动可以保留。
4. 在master的新检出目录重建并测试，确认不依赖其他工作树中的未跟踪文件。
5. 选择版本号，为确认的master提交创建附注标签，再保存发布固件、源码和SHA-256校验。

工作树是独立目录，分支是版本历史。合并提交更新master分支，不会自动把原来test目录中的文件切换为master内容。

## 本项目的发布建议

建议首个标签v0.1.0，名称“原生云台控制台架版”。发布范围以CHANGELOG和HARDWARE_NATIVE为准：实机控制链路已由用户确认跑通，精确指向和机动飞行验证仍单独记录。

- Git保存源码、构建配置、依赖、测试输入和说明。
- 发布附件保存Release HEX、BIN、ELF、同一commit的源码ZIP、manifest.json及SHA256SUMS.txt。
- manifest记录完整commit、构建工具版本、硬件、串口配置、两轴符号及验证范围。
- HEX/BIN不加入源码提交；测试抓包telem2_mount_rx.hex是明确放行的文本测试输入。
- 已发布版本保持不变，修复后创建新版本号；不要移动旧标签指向另一份代码。

## 打标签与远程发布（需明确执行）

下面命令在master工作树执行，先确认HEAD就是准备发布的提交：

```powershell
git status --short
git log -1 --oneline
$releaseCommit = git rev-parse HEAD
git tag -a v0.1.0 $releaseCommit -m "发布v0.1.0：原生云台控制台架版"
```

本仓库当前未配置远程地址。准备远程发布时，先在选定平台创建仓库并配置origin，然后推送：

```powershell
git push origin master
git push origin v0.1.0
```

在GitHub/Gitee等平台创建对应Release，上传发布包，并在说明中记录已验证功能和未完成验证项。推送或上传前检查仓库的可见性及源文件内容；本流程不会自动公开代码。

参考：[Git工作树](https://git-scm.com/docs/git-worktree)、[Git附注标签](https://git-scm.com/docs/git-tag)、[语义化版本](https://semver.org/lang/zh-CN/)。
