# 编码规范

## 文件

- 自有文档、代码注释和项目说明使用 UTF-8、LF、无 BOM。
- Windows PowerShell 脚本按 `.gitattributes` 保持 CRLF。
- 第三方源码、生成代码和 MAVLink 定义不做批量编码改写。
- 编码归一化必须单独提交，不能混入功能改动。

## 文本

中文用于面向维护者和操作员的说明；API、JSON 字段、MAVLink 枚举、文件名和通用
行业缩写保留稳定英文名称。终端显示乱码不等于文件损坏，不要把乱码复制回仓库。

## Windows 终端

PowerShell 7：

```powershell
$OutputEncoding = [System.Text.UTF8Encoding]::new()
[Console]::InputEncoding = [System.Text.UTF8Encoding]::new()
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()
```
PowerShell 5.1 可临时执行 `chcp 65001`。

## 检查

```bash
git diff --check
git ls-files -z | xargs -0 file | rg 'CRLF|with BOM'
```
