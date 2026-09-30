# HydrologyModel

基于 **Barnes 填洼算法** 与 **哈希权重树** 的 DEM 洼地分析工具。  
项目使用 C++17 编写，依赖 GDAL 读写 GeoTIFF，主要用于：

- DEM 填洼与洼地识别
- 集水区划分
- 洼地父子关系树构建
- 洼地上下游查询
- 洼地出口高程与 ID 导出

---

## 功能特性

- **Barnes 填洼算法**：基于优先队列 + 普通队列的填洼流程，记录洼地 ID、集水区 ID、出口位置与父子关系。
- **哈希优先队列**：按 `spill` 值分桶，降低重复高程的排序开销。
- **哈希权重树**：将洼地组织为树结构，支持层级、DFS 区间、直接/全部上下游查询。
- **二进制序列化**：树结构保存为紧凑二进制文件，便于后续加载与查询。
- **交互式查询**：模块 2 可输入洼地 ID，查看出口坐标、高程、父子关系、上下游列表。
- **出口栅格导出**：模块 3 生成所有洼地出口的高程 DEM 与洼地 ID 栅格。

---

## 依赖环境

- CMake >= 3.10
- 支持 C++17 的编译器（g++ / clang++）
- GDAL（开发库）
- Linux 推荐，Windows 需自行调整路径与 GDAL 配置

### Ubuntu / Debian 安装依赖

```bash
sudo apt update
sudo apt install -y build-essential cmake git libgdal-dev
