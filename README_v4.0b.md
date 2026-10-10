# AutoCellGen Sky130 二次开发 —— v4.0b 去硬编码实验版

> 本分支基于 [The-OpenROAD-Project/AutoCellGen](https://github.com/The-OpenROAD-Project/AutoCellGen) 的 SKY130 二次开发实验。
> 与 `main` 分支（v3.9 归档版）的关系：v3.9 是"与官方 GDS 几乎一致"的基准版；v4.0b 是**去硬编码实验版**。

## 核心目标

消除 v3.9 中针对 `a21oi_1` 的三处官方几何直抄（`sky130_polys[]` / `LefPin[]` / `PinMark[]`）与 7 处硬编码坐标，
改为**从网表 + 布局结果 + 工艺常量推导**的通用规则。

## 已实现

- **构造型布线（规则驱动绘制）**：li1 金属形状由确定性规则生成（电源轨 + pin 金属 + 接触覆盖 + 跨列连接条），
  不再消费 z3 网格路径解（z3 仍作为连通性 SAT 验证器运行）。
- **布局输出驱动几何**：栅列线、接触列、diff 带、pin 标记全部从布局列序推导。
- **动态输出网识别**：输出干线不硬编码 `Y`（a22o 输出为 `X` 时同样正确）。
- **dummy 列跳过**：布局输出的 dummy 列不画 poly/接触/焊块、不占栅距（a22o 单元宽 3.22→2.76µm 对齐官方）。

## 验证状态

| 单元 | 宽 (µm) | pin 100% 覆盖 | li1 重叠 | licon 全覆盖 | 状态 |
|---|---|---|---|---|---|
| a21oi_1 | 1.84 | ✅ | 0 | ✅ | ✅ 通过 |
| nand2_1 | 1.38 | ✅ | 0 | ✅ | ✅ 通过 |
| inv_1 | 0.92 | ✅ | 0 | ✅ | ✅ 通过 |
| nand3_1 | 1.84 | ✅ | ⚠ 1 处 | ✅ | 待修（并联带撞下探） |
| a22o_1 | 2.76 | ✅ | ⚠ 1 处 | ⚠ 2 个 | 待修（P/N 撞车 + 内部栅网） |

## 已识别待修复结构族缺口（v4.0b 报告）

1. **跨列条 P/N 侧不分**：同一列区间 P 侧网与 N 侧网混入同一跨列统计 → 修复方向：P/N 分侧统计 + 栅网（poly 已连通）免跨列条。
2. **并联 D 带横条撞电源下探**：全并联 PMOS（D 带 / S 带）输出干线画全宽横条 → 修复方向：电源下探避让已覆盖金属。
3. **内部栅网连接缺失**：内部栅网栅接触无 li1 覆盖 → 修复方向：栅接触 → 同网扩散接触连接规则。

## 运行

```bash
cd MAKE/PLACE_ROUTE/csyn_fp
cmake . && make -j$(nproc)
./placement -i ../../DATA/input/sky130_a21oi.sp -d ../../DATA/input/placement_file.style -o <输出目录>
```

输入：`.sp` 网表（sky130 约定：`.SUBCKT` + 晶体管行 D G S B，模型名 `nfet_01v8`/`pfet_01v8`，电源 `VDD`/`VSS`）+ `placement_file.style`。
输出：placement 文本 → route 文本（z3 SAT 验证）→ ascii → GDS。
