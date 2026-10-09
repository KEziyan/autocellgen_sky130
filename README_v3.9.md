# autocellgen_sky130 v3.9

基于 [The-OpenROAD-Project/AutoCellGen](https://github.com/The-OpenROAD-Project/AutoCellGen) 的二次开发版本，将标准单元版图智能生成流程适配到 **SkyWater sky130_fd_sc_hd（SKY130 平面工艺）**。

## 目标工艺

- **原项目**：ASAP7（预测性 FinFET，7.5T，工艺焊死）
- **本版本**：SKY130 平面 CMOS，高密库（hd），行高 2.72µm，CPP 0.46µm

## 核心成果（样本单元 `sky130_fd_sc_hd__a21oi_1`，Y = NOT((A1·A2) OR B1)）

- 生成的版图与官方 `sky130_fd_sc_hd__a21oi_1.gds` **高度一致**：
  - 17 个 (layer, datatype) 与官方层集合完全对应
  - 结构层（阱 / diff / 注入 / M1 / 边界等）逐层几何一致
  - 0 同层短路、接触全覆盖、via 与 pin 标记对齐
  - 仅 licon 及其连带 li1 存在可忽略的轻微差别

## 二次开发做了什么（简述）

| 类别 | 改动 |
|---|---|
| 工艺适配 | Router.h 工艺常量表 + sky130 GDS 层号映射；删除 dummy poly / gate-cut / 鳍绘制；diff 按平面工艺两条整带 |
| 布线层 | 内部互连从 ASAP7 M1/M2 网格路径改为 **li1 金属形状**（电源轨、接触覆盖、跨列连接条）；M1 仅做电源轨与引脚 |
| 网表适配 | sky130 模型白名单（nfet_01v8 / pfet_01v8_hvt 等）、w/l/m 解析、无鳍网表下 nfin 按宽度推导 |
| 宽度公式 | `width+2 → width`（sky130 单元无 dummy poly 列） |

## 运行

```bash
cd MAKE/PLACE_ROUTE
./1.run_csyn_fp ../../DATA/input/sky130_a21oi.sp   # 需在 csyn_fp/build 下先 make
```

输入：`DATA/input/sky130_a21oi.sp` + `DATA/input/placement_file.style`

## 说明

- 本版本以**单个样本单元 a21oi_1** 验证为主；部分几何为针对该单元的适配（详见对比报告中的"待确认项"）。
- 不做任何基于 v3.9 的进一步优化，对比报告完成后另行决策。
