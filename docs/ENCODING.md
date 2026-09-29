# 中文与编码规范

项目维护文本使用中文，自有文本文件使用 UTF-8。默认换行符为 LF；Windows 脚本按 `.gitattributes` 使用 CRLF。

## 规则

1. 文档、代码注释和项目说明默认使用中文。
2. API、JSON 字段、MAVLink 枚举、文件名和行业缩写保留稳定英文名称。
3. 新增自有文本文件使用 UTF-8 且不带 BOM。
4. 第三方源码、生成代码和上游协议定义不做批量编码改写。
5. 编码或 BOM 归一化必须独立成改动，不能混入功能修复。

## 当前遗留

仓库中的常规文本换行已经由 `.gitattributes` 约束；PowerShell 脚本保留 CRLF。部分历史 C++ 文件仍带 UTF-8 BOM，应在独立改动中归一化并运行完整后端测试。

Dart/Flutter 代码使用 UTF-8 和 LF，并统一通过 `dart format` 维护格式。

## Windows 终端

PowerShell 7 推荐使用 UTF-8 输入输出：

```powershell
$OutputEncoding = [System.Text.UTF8Encoding]::new()
[Console]::InputEncoding = [System.Text.UTF8Encoding]::new()
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()
```

Windows PowerShell 5.1 可临时执行 `chcp 65001`。终端显示异常不等于文件损坏，不要把乱码复制回仓库。

## 检查

```bash
git ls-files -z | xargs -0 file | rg 'CRLF|with BOM'
git diff --check
```

仓库级规则见 `.editorconfig` 和 `.gitattributes`。
