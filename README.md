# IzakayaCheater
东方夜雀食堂修改器

## 功能
- **金钱修改** — 运行时修改游戏内存中的金钱
- **存档修改** — 直接修改游戏存档文件
  - 添加邀请函（DLC2 怪诞料理邀请函 2014~2019）
  - DLC2 角色好感度满级
  - 触发博丽大祭（DLC3 好感度满级 + 任务完成 + 开关设置）

## 支持版本
4.1.1+

## 编译（仅 Windows）
### 1. 环境配置
- C/C++ 编译器（推荐 MinGW GCC）
- CMake >= 3.5.0
- wxWidgets >= 3.2（MSW 版本，用同一编译器编译）

### 2. 编译
```bash
mkdir build && cd build
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release
ninja
```

### 3. 运行
将 `DLLS/` 目录下的 `.dll` 文件复制到 exe 同目录，双击运行。
