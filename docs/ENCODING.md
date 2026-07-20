# 中文与编码规范

## 结论

项目维护语言是中文，自有文本文件目标状态是 **UTF-8 + LF**。当前源码包从 Windows 拷贝而来，仍有大量 CRLF 换行和部分 UTF-8 BOM，这是已知仓库卫生问题，不应和功能改动混在一起处理。

如果 Windows PowerShell 或终端把中文显示成异常字符，根因通常不是文件损坏，而是终端使用 GBK/代码页 936 去显示 UTF-8 文本。文件本身仍然可能是正确 UTF-8。

## 强制规则

1. 文档、代码注释、提交说明、GitHub 页面默认使用中文。
2. 自有文本文件必须保存为 UTF-8。
3. 新增或大幅改写的自有文本文件使用 LF 换行。
4. 不要把 PowerShell 默认输出看到的乱码复制回源码或文档。
5. 协议名、API 名、JSON 字段、MAVLink 枚举、文件名和行业通用缩写可以保留英文。
6. 第三方源码、生成代码和上游协议定义不做中文化改写。

## 当前遗留状态

已观察到：

- 多个 `.cc`、`.h`、`.json` 文件使用 CRLF。
- 部分 UTF-8 文件带 BOM。
- 少量文件混合 CRLF 和 LF。
- `.gitattributes` 已声明文本文件应归一化为 LF/UTF-8，但当前目录不是 git 仓库，无法通过 git 自动规范化工作区。

处理建议：

1. 先恢复完整 git 仓库。
2. 单独创建一次“编码和换行归一化”改动。
3. 不在同一次改动中混入功能修复。
4. 归一化后运行完整后端构建和默认测试。

## Windows 终端建议

读取中文文件时优先使用显式 UTF-8：

```powershell
Get-Content -Raw -Encoding utf8 README.md
```

PowerShell 7 推荐设置：

```powershell
$OutputEncoding = [System.Text.UTF8Encoding]::new()
[Console]::InputEncoding = [System.Text.UTF8Encoding]::new()
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()
```

Windows PowerShell 5.1 可临时切换代码页：

```powershell
chcp 65001
```

如果终端仍显示乱码，以 UTF-8 字节验证结果为准，不要直接判断文件损坏。

## macOS / Linux 检查命令

检查 CRLF/BOM：

```bash
find . -path './build' -prune -o -path './build_check' -prune -o \
  -type f \( -name '*.cc' -o -name '*.h' -o -name '*.md' -o -name '*.json' \) \
  -print0 | xargs -0 file | rg 'CRLF|with BOM'
```

检查 JSON 是否可解析：

```bash
python3 - <<'PY'
from pathlib import Path
import json
for p in [
    Path('core/src/transport/USBBoardInfo.json'),
    Path('core/src/sensor/SensorDeviceInfo.json'),
    Path('core/src/config/ParameterDescriptors.json'),
    Path('core/src/config/ParameterTranslations.zh-CN.json'),
]:
    with p.open('r', encoding='utf-8-sig') as f:
        json.load(f)
    print('json ok', p)
PY
```

## 仓库配置

- `.editorconfig` 规定新文件默认 UTF-8 和 LF 换行。
- `.gitattributes` 规定文本文件换行归一化，并把二进制文件标记为 binary。

新增脚本或文档时必须遵守这两个文件。
