#pragma once

#include "PlaceGrid.h"
#include "global.h"
#include "beol_data.h"
#include "RouteGrid.h"
#include "z3++.h"
#include "RoutingResult.h"

enum class routing_direction {BOTH, HOR, VER};
// === sky130_fd_sc_hd 版图常量（官方 GDS 实测 / tech LEF，单位 nm）===
// GATE_TIP_VER = poly 距上下轨边距（官方 poly y 105..2615）
// MIN_ACTIVE_HEIGHT = diff 超出最外栅的 x 向余量（≈265）
// LISD_WIDTH / V0_WIDTH = li1 最小线宽 170nm；LIG_CENTER 为栅接触条（官方 y 995..1325）
// 电源轨：VSS y[-85,815]，VDD y[2195,2805]（官方实测，轨高不对称）
enum class ASAP_DR { GATE_PITCH = 460, GATE_WIDTH = 150, CELL_HEIGHT = 2720, GATE_TIP_VER = 105, GCUT_HEIGHT = 0,
                     FIN_HEIGHT = 0, FIN_SPACING = 0, ACTIVE_UNIT = 10, MIN_ACTIVE_HEIGHT = 265, SDT_WIDTH = 0, LISD_WIDTH = 170,
                     LIG_PWR_HEIGHT = 900, LIG_CENTER_HEIGHT = 330, LIG_GATE = 10, V0_WIDTH = 170, M1_WIDTH = 140, M1_PITCH = 340, M1_V0_EX = 30, M1_T2T = 140, M1_T2S = 140};
// === sky130 GDS 层号映射（layer : datatype，见 sky130 层表实测）===
// WELL=64:20 nwell / NSELECT=93:44 nsdm / PSELECT=94:20 psdm / BOUNDARY=236:0 prBndry
// GATE=66:20 poly / ACTIVE=65:20 diff / LISD=LIG=LI1=67:20 li1 / V0=66:44 li1接触(栅+SD同层)
// M1=68:20 met1 / V1=69:44 via2(met1-met2) / M2=69:20 met2 / VIA_LI1_M1=68:44 via(li1-met1)
// HVTP=78:44 / LVTN=125:44 / NPC=95:20
enum class LAYER { WELL = 64, NSELECT = 93, PSELECT = 94, BOUNDARY = 236, P_SUB = 64, GATE = 66, GCUT = 0, FIN = 0, ACTIVE = 65,
                   SDT = 0, LISD = 67, LIG = 67, V0 = 66, M1 = 68, V1 = 69, M2 = 69, LI1 = 67, VIA_LI1_M1 = 68, VIA_M1_M2 = 69,
                   HVTP = 78, LVTN = 125, NPC = 95 };

class Router {
public:

    using Grid2D = std::vector<std::vector<int>>;
    using Grid3D = std::vector<std::vector<std::vector<int>>>;

    Router (Cell c, PlaceGrid s) : cell(c), place_sol(s) {
        grid = nullptr;
        grid_edge = nullptr;
        is_routable = false;
        m2_usage = 0;

        if (setting.m1_dir == "BOTH") M1 = routing_direction::BOTH;
        else if (setting.m1_dir == "HOR") M1 = routing_direction::HOR;
        else M1 = routing_direction::VER;

        if (setting.m2_dir == "BOTH") M2 = routing_direction::BOTH;
        else if (setting.m2_dir == "HOR") M2 = routing_direction::HOR;
        else M2 = routing_direction::VER;

    }

    ~Router();
    
    bool routing(fs::path output_path);

    void initialize_grid();
    void generate_pin_map();
    void add_detailed_routing_formulation (z3::context & c, z3::optimize & opt);

    z3::check_result solve_SMT (z3::optimize &opt);

    void gather_result (z3::context &c, z3::optimize &opt);
    void print_result (std::ofstream &out);

    void generate_ascii (std::ofstream &out, std::string cell_name);
    void ascii_to_gdsii (fs::path ascii_path);

    void DFS_Metal (std::shared_ptr<RoutingResult> route, std::vector<Point>& components, int y, int x, std::vector<std::vector<bool>>& explored, int metal_layer);


public:
    Cell cell;
    PlaceGrid place_sol;

    Grid_vertex** grid;	// 2D Vertex grid
	Grid_edge* grid_edge;	// Edge array

    std::unordered_map<std::string, bool**> pin_map, IO_pin_map;
    std::unordered_map<std::string, std::vector<std::vector<Point>>> floating_pins;
    std::unordered_map<std::string, std::vector<std::vector<Point>>> IO_floating_pins;
    
    std::unordered_map<std::string, std::shared_ptr<RoutingResult>> routing_result;

    std::vector<std::tuple<std::string, std::string, int, int, int>> mol_active;

    int row_size;
    int col_size;
    int num_layer;
    int edge_number;

    bool is_routable;
    double m2_usage;
    int64_t runtime;
	routing_direction M1, M2;
};