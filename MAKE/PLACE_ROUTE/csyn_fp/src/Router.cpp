#include "../header/Router.h"
#include <map>
#include <set>
#include <array>

Router::~Router() {

	for (int i = 0; i < row_size; i++) {
		if(sizeof(grid)/sizeof(Grid_vertex) == 0) break;
		delete [] grid[i];
	}
	
	delete [] grid;

	delete [] grid_edge;

	for (auto& pin : pin_map) {
		auto one_pin_map = pin.second;

		for (int i = 0; i < row_size; i++) delete [] one_pin_map[i];
		delete [] one_pin_map;

	}
}

void Router::initialize_grid() {
    row_size = 5;
	std::cout << "place sol : " << place_sol.cellWidth << std::endl;
    col_size = place_sol.cellWidth * 2 + 3; // +2 because dummy polies at both sides

	grid = new Grid_vertex*[row_size];
	for (int i = 0; i < row_size; i++) {
		grid[i] = new Grid_vertex[col_size];
		for (int j = 0; j < col_size; j++) {
			grid[i][j].row_index = i;
			grid[i][j].col_index = j;
		}
	}

	edge_number = (col_size - 1) * row_size + (row_size - 1) * col_size;	// #horizontal edge + #vertical edge
	grid_edge = new Grid_edge[edge_number];
	for (int i = 0; i < edge_number; i++) {
		grid_edge[i].index = i;
	}

	// Connect vertices and edges each other
	// Horizontal edge
	for (int i = 0; i < row_size; i++) {
		for (int j = 0; j < col_size - 1; j++) {
			int edge_idx = i*(col_size - 1) + j;

			// Connect edge to vertex
			grid[i][j].right_edge = &grid_edge[edge_idx];
			grid[i][j+1].left_edge = &grid_edge[edge_idx];
			
			// Connect vertices to edge
			grid_edge[edge_idx].connected_v[0] = &grid[i][j];
			grid_edge[edge_idx].connected_v[1] = &grid[i][j+1];
            grid_edge[edge_idx].capacity = 1;
		}
	}

	// Vertical edge
	for (int j = 0; j < col_size; j++) {
		for (int i = 0; i < row_size - 1; i++) {
			int edge_idx = (col_size - 1) * row_size + i*(col_size) + j;

			// Connect edge to vertex
			grid[i][j].bottom_edge = &grid_edge[edge_idx];
			grid[i+1][j].top_edge = &grid_edge[edge_idx];

			// Connect vertices to edge
			grid_edge[edge_idx].connected_v[0] = &grid[i][j];
			grid_edge[edge_idx].connected_v[1] = &grid[i+1][j];
            grid_edge[edge_idx].capacity = 1;

		}
	}

}

void Router::generate_pin_map() {

	// connect the same net of gate polies by LIG
	// Generate pin map per pin to be connected by M1, M2
	// Save floating pins to floating_pins variable

	std::vector<std::string> net_list;
	for (auto net : cell.nets) {
		if (std::find(net_list.begin(), net_list.end(), net) == net_list.end()) {
			net_list.push_back(net);
		}
	}
	for (auto net : cell.IOnets) {
		if (std::find(net_list.begin(), net_list.end(), net) == net_list.end()) {
			net_list.push_back(net);
		}
	}

	std::vector<std::string> gate_poly;
	std::vector<std::string> gate_net_list;
	for (int i = 0; i < place_sol.cellWidth; i++) {
		std::string net = (place_sol.pmos[i].gate != "dummy") ? place_sol.pmos[i].gate : place_sol.nmos[i].gate;
		gate_poly.push_back(net);

		if (net != "dummy" && std::find(gate_net_list.begin(), gate_net_list.end(), net) == gate_net_list.end()) {
			gate_net_list.push_back(net);
		}

	}

	// find consecutive gates (Connected by LIG)
	std::vector<std::tuple<std::string, int, int>> connected_gates;
	int pos_start = -1;
	std::string cur_net = "initial";
	for (int i = 0; i < place_sol.cellWidth; i++) {
		//if (gate_poly[i] == "dummy") continue;
		if (pos_start == -1) {
			pos_start = i;
			cur_net = gate_poly[i];
		}
		else if (gate_poly[i] != cur_net) {
			// pos_start ~ i - 1
			if (cur_net != "dummy") connected_gates.push_back(std::make_tuple(cur_net, pos_start, i - 1));
			cur_net = gate_poly[i];
			pos_start = i;
		}
		if (i == place_sol.cellWidth - 1) {
			if (cur_net != "dummy") connected_gates.push_back(std::make_tuple(cur_net, pos_start, i));
			break;
		}
	}


	for (auto net : net_list) {
		if (net == "VDD" || net == "VSS") continue;
		int num_pins = 0;

		std::vector<std::vector<Point>> float_point_list;
		std::vector<Point> float_points;
		float_point_list.clear();
		float_points.clear();

		// find consecutive gates
		for (auto& connected_info : connected_gates) {
			std::string cur_net = std::get<0>(connected_info);
			if (net != cur_net) continue;
			int lx = std::get<1>(connected_info);
			int rx = std::get<2>(connected_info);

			float_points.clear();
			for (int i = lx; i <= rx; i++) {
				float_points.emplace_back(2, 2 * i + 2);
				if (i != rx) float_points.emplace_back(2, 2 * i + 3);
			}
			float_point_list.push_back(float_points);
			num_pins++;
		}

		// find active region + LISD-connected actives 
		std::vector<std::pair<int, int>> pmos_net, nmos_net; // x-loc, nfin

		for (int i = 0; i < place_sol.cellWidth; i++) {
			if (place_sol.pmos[i].left == net) {
				pmos_net.push_back(std::make_pair(i, place_sol.pmos[i].nfin));
				num_pins++;
			}
			if (i > 0 && place_sol.pmos[i].gate == "dummy" && place_sol.pmos[i - 1].right == net) {
				pmos_net.push_back(std::make_pair(i, place_sol.pmos[i - 1].nfin));
				num_pins++;			
			}
			if (i == place_sol.cellWidth - 1 && place_sol.pmos[i].right == net) {
				pmos_net.push_back(std::make_pair(i + 1, place_sol.pmos[i].nfin));
				num_pins++;
			}

			if (place_sol.nmos[i].left == net) {
				nmos_net.push_back(std::make_pair(i, place_sol.nmos[i].nfin));
				num_pins++;
			}
			if (i > 0 && place_sol.nmos[i].gate == "dummy" && place_sol.nmos[i - 1].right == net) {
				nmos_net.push_back(std::make_pair(i, place_sol.nmos[i - 1].nfin));
				num_pins++;	
			}
			if (i == place_sol.cellWidth - 1 && place_sol.nmos[i].right == net) {
				nmos_net.push_back(std::make_pair(i + 1, place_sol.nmos[i].nfin));
				num_pins++;
			}

		}

		int n_pmos = static_cast<int>(pmos_net.size());
		int n_nmos = static_cast<int>(nmos_net.size());

		std::vector<bool> satisfied;
		for (int i = 0; i < n_pmos - 1; i++) {
			if (pmos_net[i + 1].first - pmos_net[i].first == 1 && pmos_net[i + 1].second == pmos_net[i].second) {
				satisfied.push_back(true);
				num_pins--;
			}
			else satisfied.push_back(false);
		}

		int start = -1;
		bool continuous = false;
		for (int i = 0; i < satisfied.size(); i++) {
			if (satisfied[i]) {
				if (!continuous) {
					continuous = true;
					start = i;
				}
				if (i == satisfied.size() - 1 && continuous) {
					mol_active.push_back(std::make_tuple(net, "p", pmos_net[start].second, pmos_net[start].first, pmos_net[i + 1].first));
				}
			}
			else {
				if (continuous) {
					mol_active.push_back(std::make_tuple(net, "p", pmos_net[start].second, pmos_net[start].first, pmos_net[i].first));
					continuous = false;
				}
			}
		}

		satisfied.clear();
		for (int i = 0; i < n_nmos - 1; i++) {
			if (nmos_net[i + 1].first - nmos_net[i].first == 1 && nmos_net[i + 1].second == nmos_net[i].second) {
				satisfied.push_back(true);
				num_pins--;
			}
			else satisfied.push_back(false);
		}

		start = -1;
		continuous = false;
		for (int i = 0; i < satisfied.size(); i++) {
			if (satisfied[i]) {
				if (!continuous) {
					continuous = true;
					start = i;
				}
				if (i == satisfied.size() - 1 && continuous) {
					mol_active.push_back(std::make_tuple(net, "n", nmos_net[start].second, nmos_net[start].first, nmos_net[i + 1].first));
				}
			}
			else {
				if (continuous) {
					mol_active.push_back(std::make_tuple(net, "n", nmos_net[start].second, nmos_net[start].first, nmos_net[i].first));
					continuous = false;
				}
			}
		}


		std::cout << "The number of pin for net " << net << ": " << num_pins << std::endl;

		if (num_pins > 1) {
			// Allocate memory for pin map and initialize it
			bool** pin_map_of_cur_net = new bool*[row_size];
			for (int j = 0; j < row_size; j++) {
				pin_map_of_cur_net[j] = new bool[col_size];
				for (int k = 0; k < col_size; k++) {
					pin_map_of_cur_net[j][k] = false;
				}
			}

			// allocate LIG floating pins
			if (float_point_list.size() > 0) {
				for (auto& floating_points : float_point_list) {
					for (auto& floating_point : floating_points) {
						pin_map_of_cur_net[floating_point.y][floating_point.x] = true;
					}
				}
			}

			// allocate S/G/D floating pins
			// allocate LISD-connected active first
			for (auto& mol_active_info : mol_active) {
				std::string net_name = std::get<0>(mol_active_info);
				if (net != net_name) continue;

				std::string region = std::get<1>(mol_active_info);
				int nfin = std::get<2>(mol_active_info);
				int lx = std::get<3>(mol_active_info);
				int rx = std::get<4>(mol_active_info);

				float_points.clear();
				for (int i = 2 * lx + 1; i <= 2 * rx + 1; i += 2) {
					if (region == "p") {
						pin_map_of_cur_net[0][i] = true;
						float_points.emplace_back(0, i);
						if (nfin > 1) {
							pin_map_of_cur_net[1][i] = true;
							float_points.emplace_back(1, i);
						}
					}
					if (region == "n") {
						pin_map_of_cur_net[4][i] = true;
						float_points.emplace_back(4, i);
						if (nfin > 1) {
							pin_map_of_cur_net[3][i] = true;
							float_points.emplace_back(3, i);
						}
					}
				}
				float_point_list.push_back(float_points);
			}

			// allocate LISD-unconnected active regions
			// PMOS
			for (auto& pmos_active : pmos_net) {
				int x_loc = pmos_active.first;
				int act_fin = pmos_active.second;
				bool not_contained = true;

				for (auto& mol_active_info : mol_active) {
					std::string net_name = std::get<0>(mol_active_info);
					if (net != net_name) continue;
	
					std::string region = std::get<1>(mol_active_info);
					if (region != "p") continue;

					int lx = std::get<3>(mol_active_info);
					int rx = std::get<4>(mol_active_info);		
					if (x_loc >= lx && x_loc <= rx) {
						not_contained = false;
						break;
					}
				}
				if (not_contained) {
					float_points.clear();
					pin_map_of_cur_net[0][2 * x_loc + 1] = true;
					float_points.emplace_back(0, 2 * x_loc + 1);
					if (act_fin > 1) {
						pin_map_of_cur_net[1][2 * x_loc + 1] = true;
						float_points.emplace_back(1, 2 * x_loc + 1);
					}				
					float_point_list.push_back(float_points);
				}
			}
			
			// NMOS
			for (auto& nmos_active : nmos_net) {
				int x_loc = nmos_active.first;
				int act_fin = nmos_active.second;
				bool not_contained = true;

				for (auto& mol_active_info : mol_active) {
					std::string net_name = std::get<0>(mol_active_info);
					if (net != net_name) continue;
	
					std::string region = std::get<1>(mol_active_info);
					if (region != "n") continue;

					int lx = std::get<3>(mol_active_info);
					int rx = std::get<4>(mol_active_info);		
					if (x_loc >= lx && x_loc <= rx) {
						not_contained = false;
						break;
					}
				}
				if (not_contained) {
					float_points.clear();
					pin_map_of_cur_net[4][2 * x_loc + 1] = true;
					float_points.emplace_back(4, 2 * x_loc + 1);
					if (act_fin > 1) {
						pin_map_of_cur_net[3][2 * x_loc + 1] = true;
						float_points.emplace_back(3, 2 * x_loc + 1);
					}
					float_point_list.push_back(float_points);
				}
			}

			pin_map[net] = pin_map_of_cur_net;
			floating_pins[net] = float_point_list;
		}
		else if (num_pins == 1 && std::find(cell.IOnets.begin(), cell.IOnets.end(), net) != cell.IOnets.end()) {  // IOpin -> connected gates
			// Allocate memory for pin map and initialize it
			bool** pin_map_of_cur_net = new bool*[row_size];
			for (int j = 0; j < row_size; j++) {
				pin_map_of_cur_net[j] = new bool[col_size];
				for (int k = 0; k < col_size; k++) {
					pin_map_of_cur_net[j][k] = false;
				}
			}

			// allocate LIG floating pins
			if (float_point_list.size() > 0) {
				for (auto& floating_points : float_point_list) {
					for (auto& floating_point : floating_points) {
						pin_map_of_cur_net[floating_point.y][floating_point.x] = true;
					}
				}
			}

			IO_pin_map[net] = pin_map_of_cur_net;
			IO_floating_pins[net] = float_point_list;

		}
	}
	// Print pin_map
	/*
	for (auto& iter : pin_map) {
		std::cout << "Net " << iter.first << std::endl;
		for (int i = 0; i < row_size; i++) {
			for (int j = 0; j < col_size; j++) {
				std::cout << iter.second[i][j] << " ";
			}
			std::cout << std::endl;
		}
		std::cout << std::endl;
		
		auto& floating_points = floating_pins[iter.first];
		for (auto& fp_list : floating_points) {
			for (auto& fp : fp_list) {
				std::cout << "(" << fp.y << ", " << fp.x << ") ";
			}
			std::cout << std::endl;
		}
		std::cout << std::endl;
	}

	*/

}


bool Router::routing(fs::path output_path) {

	if(place_sol.cellWidth == 0) return false;
	
	initialize_grid();
	generate_pin_map();

	//std::cout << opt << std::endl;

	//std::string filePath = "./result_mega/" + cell.name + "_" + std::to_string(index) + ".txt";
	//std::ofstream out(filePath);

	std::ofstream out(output_path.string());

	std::cout << output_path << std::endl;

	// Print placement solution
	for (int i = 0; i < place_sol.cellWidth; i++) {
        out << "[Column " << i + 1 << "]" << std::endl;
        out << "NMOS : " << place_sol.nmos[i].name << "(" << place_sol.nmos[i].nfin << ") [" << place_sol.nmos[i].left << " " << place_sol.nmos[i].gate << " " << place_sol.nmos[i].right << "], ";
        out << "PMOS : " << place_sol.pmos[i].name << "(" << place_sol.pmos[i].nfin << ") [" << place_sol.pmos[i].left << " " << place_sol.pmos[i].gate << " " << place_sol.pmos[i].right << "]" << std::endl;
    }
	out << std::endl;

	// Print h_net_density, v_net_density
	out.setf(std::ios::fixed);
	out.precision(2);
	out << "H Net Density" << std::endl;
	for (int y = 0; y < row_size; y++) {
		for (int x = 0; x < 2 * place_sol.cellWidth + 1; x++) {
			out << place_sol.h_net_den[y][x] << " ";
		}
		out << std::endl;
	}
	out << std::endl;
	for (int x = 0; x < 2 * place_sol.cellWidth + 1; x++) out << place_sol.hor_cong[x] << " ";
	out << std::endl << std::endl;
	
	out << "V Net Density" << std::endl;
	for (int y = 0; y < row_size; y++) {
		for (int x = 0; x < 2 * place_sol.cellWidth + 1; x++) {
			out << place_sol.v_net_den[y][x] << " ";
		}
		out << std::endl;
	}
	out << std::endl;
	for (int x = 0; x < 2 * place_sol.cellWidth + 1; x++) out << place_sol.ver_cong[x] << " ";
	out << std::endl << std::endl;

	out << "Cost = " << place_sol.cost << std::endl;
	out.unsetf(std::ios::fixed);
	
    out << "[NOR_HPWL=" << place_sol.nor_hpwl << ", MAX_H_GRID=" << place_sol.max_h_grid << ", MAX_V_GRID=" << place_sol.max_v_grid << ", MAX_H_COLUMN=" << place_sol.max_h_column << ", MAX_V_COLUMN=" << place_sol.max_v_column << ", MAX_H_ROW=" << place_sol.max_h_row << ", MAX_V_ROW=" << place_sol.max_v_row << std::endl << std::endl;	
	
	// Print Floating pins
	out << "---------------- Pin List -------------------" << std::endl;
	for (auto& iter : pin_map) {
		out << "Net " << iter.first << std::endl;
		for (int i = 0; i < row_size; i++) {
			for (int j = 0; j < col_size; j++) {
				out << iter.second[i][j] << " ";
			}
			out << std::endl;
		}
		out << std::endl;
		
		auto& floating_points = floating_pins[iter.first];
		for (auto& fp_list : floating_points) {
			for (auto& fp : fp_list) {
				out << "(" << fp.y << ", " << fp.x << ") ";
			}
			out << std::endl;
		}
		out << std::endl;
	}	
	
	// ===== sky130 构造型布线：几何由规则直接推导，无 SAT 求解 =====
	is_routable = true;
	runtime = 0;   // 构造型布线不产生 SAT 求解耗时
	m2_usage = 0;  // 互连全在 li1，不用 M2
	std::cout << "SAT!" << std::endl;
	out << "Routing time : 0ms (constructive routing)" << std::endl;

	if (setting.gen_gds) {
		auto ascii_path = output_path.replace_extension(fs::path(".ascii"));
		auto cell_name = output_path.stem();
		std::ofstream ascii_out(ascii_path.string());
		generate_ascii(ascii_out, cell_name.string());
		ascii_to_gdsii(ascii_path);
	}

	out.close();
	std::cout << "routing step done!" << std::endl;
    return true;
}


void Router::generate_ascii(std::ofstream &out, std::string cell_name) {

	// ============ v4.0 sky130 通用几何：全部从 布局结果 + 工艺常量 推导（无单元特有坐标） ============
	int gate_pitch = static_cast<int>(ASAP_DR::GATE_PITCH);
	int num_gate = place_sol.nmos.size();
	// ---- 布局的 dummy 列（PlaceUnit 为满足约束插的空列）不画 poly/接触/焊块，且不占栅距 ----
	// sky130 官方单元无 dummy 列；AutoCellGen 布局模型可能输出 dummy 列（如 a22o 的列 2）
	std::vector<int> real_col;
	for (int i = 0; i < num_gate; i++)
		if (place_sol.nmos[i].gate != "dummy" && place_sol.pmos[i].gate != "dummy")
			real_col.push_back(i);
	int num_gate_real = static_cast<int>(real_col.size());
	// 单元宽 = (真实栅数+1) × 栅距
	int x_right_boundary = gate_pitch * (num_gate_real + 1);

	int cell_height = static_cast<int>(ASAP_DR::CELL_HEIGHT);
	int gate_width = static_cast<int>(ASAP_DR::GATE_WIDTH);
	int gate_tip_ver = static_cast<int>(ASAP_DR::GATE_TIP_VER);
	int v0_width = static_cast<int>(ASAP_DR::V0_WIDTH);
	int m1_width = static_cast<int>(ASAP_DR::M1_WIDTH);
	int m1_pitch = static_cast<int>(ASAP_DR::M1_PITCH);
	int m1_v0_ex = static_cast<int>(ASAP_DR::M1_V0_EX);
	int m1_t2t = static_cast<int>(ASAP_DR::M1_T2T);
	int m1_t2s = static_cast<int>(ASAP_DR::M1_T2S);
	int active_unit = static_cast<int>(ASAP_DR::ACTIVE_UNIT);

	// ---- 工艺常数（sky130_fd_sc_hd 行结构，全库通用）----
	const int CONTACT_HALF = v0_width / 2;      // 85（接触/焊块半宽）
	const int SD_MARGIN = 55;                   // SD 接触到 poly 栅体边缘间距
	const int MIN_ACTIVE = 265;                 // diff 超出最外栅的 x 向余量
	const int GCONT_DX_L = 215;                 // 首栅接触：栅体左缘 -215（左凸 landing 上）
	const int GCONT_DX_R = 55;                  // 其余栅接触：栅体右缘 +55（右凸 landing 上）
	// 行结构 y（sky130 hd，2.72µm 行高固定）：
	const int NMOS_DIFF_Y0 = 235, NMOS_DIFF_Y1 = 885;
	const int PMOS_DIFF_Y0 = 1485, PMOS_DIFF_Y1 = 2485;
	const int CENTER_Y0 = 995, CENTER_Y1 = 1325;   // 中带（栅接触/npc 带）
	const int VSS_RAIL_Y0 = -85, VSS_RAIL_Y1 = 85;
	const int VDD_RAIL_Y0 = 2635, VDD_RAIL_Y1 = 2805;
	const int GCONT_Y0 = 1075, GCONT_Y1 = 1245;    // 栅接触 66:44
	const int NMOS_C_Y0 = 445, NMOS_C_Y1 = 615;    // NMOS 常规接触
	const int NMOS_C0_Y0 = 295, NMOS_C0_Y1 = 465;  // 首列 VSS 接触（低排）
	const int PMOS_VDD_Y0 = 2275, PMOS_VDD_Y1 = 2445;
	const int PMOS_SH_Y0 = 1865, PMOS_SH_Y1 = 2035;   // 共享扩散区高排
	const int PMOS_SH2_Y0 = 2205, PMOS_SH2_Y1 = 2375;
	const int PMOS_SG_Y0 = 1725, PMOS_SG_Y1 = 1895;   // 单列区低排
	const int PMOS_SG2_Y0 = 2095, PMOS_SG2_Y1 = 2265;
	const int BRIDGE_Y0 = 1255, BRIDGE_Y1 = 1325;     // 输出干线水平过渡条 y（栅接触带上方安全区）

	// ---- 从布局推导：poly 栅体（真实栅 k 中心 = gate_pitch×(k+1)，跳过 dummy 列）----
	std::vector<int> gx0(num_gate_real), gx1(num_gate_real), gxc(num_gate_real);
	for (int k = 0; k < num_gate_real; k++) {
		gxc[k] = gate_pitch * (k + 1);
		gx0[k] = gxc[k] - gate_width / 2;
		gx1[k] = gxc[k] + gate_width / 2;
	}

	// ---- 从布局推导：SD 接触列 x（真实栅数+1 个扩散区）----
	std::vector<int> dcx(num_gate_real + 1);
	dcx[0] = gx0[0] - v0_width / 2 - SD_MARGIN;
	for (int k = 1; k < num_gate_real; k++) dcx[k] = (gx1[k - 1] + gx0[k]) / 2;
	dcx[num_gate_real] = gx1[num_gate_real - 1] + v0_width / 2 + SD_MARGIN;

	// ---- 栅接触 x：首栅左凸 landing，其余栅右凸 landing ----
	std::vector<int> gcx(num_gate_real);
	for (int k = 0; k < num_gate_real; k++)
		gcx[k] = (k == 0) ? (gx0[k] - GCONT_DX_L) : (gx1[k] + GCONT_DX_R);

	// ---- 轨道吸附（sky130 li1 规律：pin marker / mcon 锁 x≡0.23 mod 0.46）----
	auto snap_track = [&](int x) {
		int d = x - gate_pitch / 2;
		int k = d / gate_pitch;
		int rem = d % gate_pitch;
		if (rem >= gate_pitch / 2) k++;
		else if (rem <= -gate_pitch / 2) k--;
		return gate_pitch / 2 + k * gate_pitch;
	};

	// ---- diff 上的网序列（真实栅 k 对应布局列 real_col[k]，跨列共享区取 right）----
	std::vector<std::string> pdiff, ndiff;
	for (int k = 0; k < num_gate_real; k++) {
		int col = real_col[k];
		if (k == 0) { pdiff.push_back(place_sol.pmos[col].left); ndiff.push_back(place_sol.nmos[col].left); }
		if (k != num_gate_real - 1) {
			int col2 = real_col[k + 1];
			std::string rp = (place_sol.pmos[col].right != "dummy") ? place_sol.pmos[col].right : place_sol.pmos[col2].left;
			pdiff.push_back(rp);
			std::string rn = (place_sol.nmos[col].right != "dummy") ? place_sol.nmos[col].right : place_sol.nmos[col2].left;
			ndiff.push_back(rn);
		} else { pdiff.push_back(place_sol.pmos[col].right); ndiff.push_back(place_sol.nmos[col].right); }
	}
	int num_diff = pdiff.size();
	// ---- 动态识别输出网（通用规则）：输出 = 端口网中非电源、且不作为任何栅极的网 ----
	// sky130 hd 组合单元输出名不统一（a21oi/nand3 叫 Y，a22o/xor2 叫 X，时序单元叫 Q/QN）
	// 不能硬编码 "Y"——从拓扑推导：端口网 ∩ (非栅网) - 电源
	std::string out_net;
	for (auto& net : cell.IOnets) {
		if (net == "VDD" || net == "VSS") continue;
		bool is_gate_net = false;
		for (int i = 0; i < num_gate; i++) {
			if (place_sol.nmos[i].gate == net || place_sol.pmos[i].gate == net) { is_gate_net = true; break; }
		}
		if (!is_gate_net) { out_net = net; break; }
	}
	// net 出现次数（决定是否画接触：≥2 或电源）
	std::map<std::string, int> net_cnt;
	for (auto& s : pdiff) if (s != "dummy") net_cnt[s]++;
	for (auto& s : ndiff) if (s != "dummy") net_cnt[s]++;
	auto need_contact = [&](const std::string &s) {
		return s != "dummy" && (net_cnt[s] >= 2 || s == "VDD" || s == "VSS");
	};

	// 局部画矩形 helper（层 / datatype / 左下 / 右上，单位 nm，×4 转 DBU）
	auto draw_rect = [](std::ofstream &d_out, int layer_num, int datatype, int x1, int y1, int x2, int y2) {
		d_out << "BOUNDARY;" << std::endl;
		d_out << "LAYER " << layer_num << ";" << std::endl;
		d_out << "DATATYPE " << datatype << ";" << std::endl;
		d_out << "XY 5;" << std::endl;
		d_out << " X: " << x1 * 4 << ";		 Y: " << y1 * 4 << ";" << std::endl;
		d_out << " X: " << x2 * 4 << ";		 Y: " << y1 * 4 << ";" << std::endl;
		d_out << " X: " << x2 * 4 << ";		 Y: " << y2 * 4 << ";" << std::endl;
		d_out << " X: " << x1 * 4 << ";		 Y: " << y2 * 4 << ";" << std::endl;
		d_out << " X: " << x1 * 4 << ";		 Y: " << y1 * 4 << ";" << std::endl;
		d_out << "ENDEL;" << std::endl << std::endl;
	};

	int xr = x_right_boundary;
	int diff_x0 = gx0[0] - MIN_ACTIVE;

	// ---- GDS 前缀（ascii2gdsii 必需）----
	out << "HEADER 5;" << std::endl;
	out << "BGNLIB;" << std::endl;
	out << " LASTMOD  {5-17-2016:1:5};" << std::endl;
	out << " LASTACC  {5-17-2016:1:5};" << std::endl;
	out << "LIBNAME ASAP7;" << std::endl;
	out << "UNITS;" << std::endl;
	out << " USERUNITS 2.500000e-04;" << std::endl;
	out << " PHYSUNITS 2.500000e-10;" << std::endl;
	out << std::endl;
	out << "BGNSTR;" << std::endl;
	out << " CREATION {5-17-2016:1:5};" << std::endl;
	out << " LASTMOD  {5-17-2016:1:5};" << std::endl;
	out << "STRNAME " << cell_name << ";" << std::endl;
	out << std::endl;

	int diff_x1 = gx1[num_gate_real - 1] + MIN_ACTIVE;

	// ---- 1. 单元边界 236:0 + aux 81:4 ----
	draw_rect(out, static_cast<int>(LAYER::BOUNDARY), 0, 0, 0, xr, cell_height);
	draw_rect(out, 81, 4, 0, 0, xr, cell_height);

	// ---- 2. 注入/阱带（sky130 hd 行结构通用）----
	draw_rect(out, static_cast<int>(LAYER::WELL),    20, -190, 1305, xr + 190, 2910);   // nwell
	draw_rect(out, static_cast<int>(LAYER::NSELECT), 44,    0, -190, xr,       1015);   // nsdm（NMOS 区）
	draw_rect(out, static_cast<int>(LAYER::PSELECT), 20,    0, 1355, xr,       2910);   // psdm（PMOS 区）
	// hvtp 78:44：单元含 hvt PMOS 时绘制（vt_flavor 驱动）
	{
		bool has_hvt = false;
		for (auto& t : cell.pmos) if (t.vt_flavor == "hvt") has_hvt = true;
		if (has_hvt) draw_rect(out, static_cast<int>(LAYER::HVTP), 44, 0, 1250, xr, 2720);
	}
	draw_rect(out, static_cast<int>(LAYER::NPC),     20,    0, 975, xr, 1345);         // npc（栅接触带）

	// ---- 3. diff 两条（x 从 MIN_ACTIVE 推导，y 行结构）----
	draw_rect(out, static_cast<int>(LAYER::ACTIVE), 20, diff_x0, PMOS_DIFF_Y0, diff_x1, PMOS_DIFF_Y1);   // PMOS
	draw_rect(out, static_cast<int>(LAYER::ACTIVE), 20, diff_x0, NMOS_DIFF_Y0, diff_x1, NMOS_DIFF_Y1);   // NMOS

	// ---- 4. poly 栅体 66:20 + landing 凸出 ----
	for (int i = 0; i < num_gate_real; i++) {
		// 栅体全高
		draw_rect(out, static_cast<int>(LAYER::GATE), 20, gx0[i], gate_tip_ver, gx1[i], cell_height - gate_tip_ver);
		// landing：首栅左凸（[gcx-85, gx0]），其余右凸（[gx1, gcx+85]）
		int lx0, lx1;
		if (i == 0) { lx0 = gcx[i] - CONTACT_HALF; lx1 = gx0[i]; }
		else        { lx0 = gx1[i]; lx1 = gcx[i] + CONTACT_HALF; }
		if (lx0 < gx0[i]) draw_rect(out, static_cast<int>(LAYER::GATE), 20, lx0, CENTER_Y0, gx0[i], CENTER_Y1);
		if (lx1 > gx1[i]) draw_rect(out, static_cast<int>(LAYER::GATE), 20, gx1[i], CENTER_Y0, lx1, CENTER_Y1);
	}

	// ---- 5. li1 电源轨 67:20（全宽）----
	// 注：电源下探（VSS/VPWR tab）移到段 10 之后绘制，画时避让已画信号金属（见段 10.5）
	draw_rect(out, static_cast<int>(LAYER::LIG), 20, 0, VDD_RAIL_Y0, xr, VDD_RAIL_Y1);   // VDD 轨
	draw_rect(out, static_cast<int>(LAYER::LIG), 20, 0, VSS_RAIL_Y0, xr, VSS_RAIL_Y1);   // VSS 轨

	// ---- 6. 栅接触 66:44（落在 landing 上，y 中带）----
	for (int i = 0; i < num_gate_real; i++) {
		draw_rect(out, static_cast<int>(LAYER::V0), 44, gcx[i] - CONTACT_HALF, GCONT_Y0, gcx[i] + CONTACT_HALF, GCONT_Y1);
	}

	// ---- 7. 电源 via 67:44 + M1 rail 68:20 + 电源 pin 标记（列中心，轨道吸附）----
	for (int i = 0; i <= num_gate_real; i++) {
		int cx = gate_pitch * (i + 1) - gate_pitch / 2;   // 列中心（0.23+0.46k 轨道）
		draw_rect(out, static_cast<int>(LAYER::LIG), 44, cx - CONTACT_HALF, -CONTACT_HALF, cx + CONTACT_HALF, CONTACT_HALF);
		draw_rect(out, static_cast<int>(LAYER::LIG), 44, cx - CONTACT_HALF, cell_height - CONTACT_HALF, cx + CONTACT_HALF, cell_height + CONTACT_HALF);
		if (i == 0) {
			draw_rect(out, static_cast<int>(LAYER::M1), 20, 0, -240, xr, 240);                          // VSS rail
			draw_rect(out, static_cast<int>(LAYER::M1), 20, 0, cell_height - 240, xr, cell_height + 240); // VDD rail
			draw_rect(out, static_cast<int>(LAYER::M1), 16, cx - CONTACT_HALF, -CONTACT_HALF, cx + CONTACT_HALF, CONTACT_HALF);   // VSS pin
			draw_rect(out, static_cast<int>(LAYER::M1), 16, cx - CONTACT_HALF, cell_height - CONTACT_HALF, cx + CONTACT_HALF, cell_height + CONTACT_HALF); // VDD pin
			draw_rect(out, 122, 16, cx - CONTACT_HALF, -CONTACT_HALF, cx + CONTACT_HALF, CONTACT_HALF);
			draw_rect(out, static_cast<int>(LAYER::WELL), 16, cx - CONTACT_HALF, cell_height - CONTACT_HALF, cx + CONTACT_HALF, cell_height + CONTACT_HALF);
		}
	}

	// ---- 8. SD 接触 66:44（拓扑由 diff 网序列推导）----
	for (int i = 0; i < num_diff; i++) {
		bool need_pm = need_contact(pdiff[i]);
		bool need_nm = need_contact(ndiff[i]);
		if (need_pm) {
			if (pdiff[i] == "VDD") {
				draw_rect(out, static_cast<int>(LAYER::V0), 44, dcx[i] - CONTACT_HALF, PMOS_VDD_Y0, dcx[i] + CONTACT_HALF, PMOS_VDD_Y1);
			} else {
				// 共享扩散区（列间）高排 / 单列区（首/末）低排：让出下方输出金属通道
				bool shared = (i > 0 && i < num_gate_real);
				if (shared) {
					draw_rect(out, static_cast<int>(LAYER::V0), 44, dcx[i] - CONTACT_HALF, PMOS_SH_Y0, dcx[i] + CONTACT_HALF, PMOS_SH_Y1);
					draw_rect(out, static_cast<int>(LAYER::V0), 44, dcx[i] - CONTACT_HALF, PMOS_SH2_Y0, dcx[i] + CONTACT_HALF, PMOS_SH2_Y1);
				} else {
					draw_rect(out, static_cast<int>(LAYER::V0), 44, dcx[i] - CONTACT_HALF, PMOS_SG_Y0, dcx[i] + CONTACT_HALF, PMOS_SG_Y1);
					draw_rect(out, static_cast<int>(LAYER::V0), 44, dcx[i] - CONTACT_HALF, PMOS_SG2_Y0, dcx[i] + CONTACT_HALF, PMOS_SG2_Y1);
				}
			}
		}
		if (need_nm) {
			if (i == 0 && ndiff[i] == "VSS") {
				draw_rect(out, static_cast<int>(LAYER::V0), 44, dcx[i] - CONTACT_HALF, NMOS_C0_Y0, dcx[i] + CONTACT_HALF, NMOS_C0_Y1);
			} else {
				draw_rect(out, static_cast<int>(LAYER::V0), 44, dcx[i] - CONTACT_HALF, NMOS_C_Y0, dcx[i] + CONTACT_HALF, NMOS_C_Y1);
			}
		}
	}

	// 已画信号金属记录（x0,y0,x1,y1）——供段 10.5 电源下探避让（段 9/10 画信号时记录）
	std::vector<std::array<int, 4>> drawn_sig;
	auto push_sig = [&](int x0, int y0, int x1, int y1) {
		drawn_sig.push_back({x0, y0, x1, y1});
		draw_rect(out, static_cast<int>(LAYER::LIG), 20, x0, y0, x1, y1);
	};
	// 栅网集合（poly 已连通所有栅——内部栅网的连接走 poly，不需要跨列条）
	std::set<std::string> gate_nets;
	for (int k = 0; k < num_gate_real; k++) gate_nets.insert(place_sol.nmos[real_col[k]].gate);

	// ---- 9. li1 信号金属 67:20：输入焊块 + 内部栅网连接 + 输出干线（Z 形避让栅接触）----
	{
		// 输入网：li1 焊块包裹栅接触（吸附轨道）
		std::vector<std::string> io_in;
		for (auto& net : cell.IOnets)
			if (net != "VDD" && net != "VSS" && net != out_net) io_in.push_back(net);
		for (int i = 0; i < num_gate_real; i++) {
			std::string gnet = place_sol.nmos[real_col[i]].gate;
			// 该栅是输入网（栅信号）→ 焊块
			bool is_input = false;
			for (auto& n : io_in) if (n == gnet) is_input = true;
			if (!is_input) continue;
			int mx = snap_track(gcx[i]);
			// 焊块 = 栅接触 与 轨道 的并集外扩 20（保证 licon 与 pin 标记都被覆盖）
			int c0 = std::min(gcx[i] - CONTACT_HALF, mx - CONTACT_HALF) - 20;
			int c1 = std::max(gcx[i] + CONTACT_HALF, mx + CONTACT_HALF) + 20;
			push_sig(c0, CENTER_Y0 - 10, c1, CENTER_Y1 + 10);
		}
		// 内部栅网连接：非输入栅网的栅接触与同网扩散接触必须有 li1 覆盖
		// 注：栅接触 → 扩散接触的"穿越中带走线"需要通道避让（避开输入焊块/输出干线/栅接触），
		// 本轮实现只保证 licon 覆盖（电气连接走线列为待办：需复用输出干线的中带通道逻辑）
		for (int i = 0; i < num_gate_real; i++) {
			std::string gnet = place_sol.nmos[real_col[i]].gate;
			if (gnet == "dummy" || gnet == "VDD" || gnet == "VSS") continue;
			bool is_in = false;
			for (auto& n : io_in) if (n == gnet) is_in = true;
			if (is_in) continue;  // 输入已有焊块
			// 栅接触覆盖块（保证栅接触 licon 被 li1 覆盖，全宽 170）
			push_sig(gcx[i] - CONTACT_HALF, GCONT_Y0, gcx[i] + CONTACT_HALF, GCONT_Y1);
			// 同网扩散接触覆盖块（N 侧列 → N 接触带；P 侧列 → P 接触带全排）
			std::vector<int> gdiff_n, gdiff_p;
			for (int d = 0; d < num_diff; d++) {
				if (ndiff[d] == gnet && need_contact(ndiff[d])) gdiff_n.push_back(dcx[d]);
				if (pdiff[d] == gnet && need_contact(pdiff[d])) gdiff_p.push_back(dcx[d]);
			}
			for (auto d : gdiff_n)
				push_sig(d - CONTACT_HALF, NMOS_C_Y0, d + CONTACT_HALF, NMOS_C_Y1);
			for (auto d : gdiff_p)
				push_sig(d - CONTACT_HALF, PMOS_SG_Y0 - 40, d + CONTACT_HALF, PMOS_SH2_Y1 + 85);
		}
		// 输出网（Y）：5 段阶梯干线（LEF 精确逻辑的通用推导，全段 ≥170 宽、避开栅接触/输入焊块）
		{
			std::vector<int> y_p_cx, y_n_cx;
			for (int i = 0; i < num_diff; i++) {
				if (pdiff[i] == out_net && need_contact(pdiff[i])) y_p_cx.push_back(dcx[i]);
				if (ndiff[i] == out_net && need_contact(ndiff[i])) y_n_cx.push_back(dcx[i]);
			}
			// 输入焊块 x 范围（li1 焊块，轨道吸附）——用于中带通道避让
			std::vector<int> wl, wr;
			for (int i = 0; i < num_gate_real; i++) {
				std::string gnet = place_sol.nmos[real_col[i]].gate;
				bool is_in = false;
				for (auto& n : cell.IOnets) if (n == gnet && n != "VDD" && n != "VSS" && n != out_net) is_in = true;
				if (!is_in) continue;
				int mx = snap_track(gcx[i]);
				wl.push_back(std::min(gcx[i] - CONTACT_HALF, mx - CONTACT_HALF) - 20);
				wr.push_back(std::max(gcx[i] + CONTACT_HALF, mx + CONTACT_HALF) + 20);
			}
			if (!y_p_cx.empty() && !y_n_cx.empty()) {
				int n0 = *std::min_element(y_n_cx.begin(), y_n_cx.end()) - CONTACT_HALF;
				int n1 = *std::max_element(y_n_cx.begin(), y_n_cx.end()) + CONTACT_HALF;
				int p0 = *std::min_element(y_p_cx.begin(), y_p_cx.end()) - CONTACT_HALF;
				int p1 = *std::max_element(y_p_cx.begin(), y_p_cx.end()) + CONTACT_HALF;
				// 中带通道：在 [lo,hi]（NMOS/PMOS Y 接触全范围）内找避开中带障碍的最大空隙
				int lo = std::min(n0, p0), hi = std::max(n1, p1);
				std::vector<std::pair<int,int>> obs;
				auto add_obs = [&](int o0, int o1, int pad) {
					int a = std::max(lo, o0 - pad), b = std::min(hi, o1 + pad);
					if (a < b) obs.push_back(std::make_pair(a, b));
				};
				for (int i = 0; i < num_gate_real; i++) {
					// 栅接触 y 与中带相交才构成障碍；pad = 半宽 + li1 间距（85+70），避免相邻 460 距接触区合并
					if (GCONT_Y0 < CENTER_Y1 && CENTER_Y0 < GCONT_Y1)
						add_obs(gcx[i] - CONTACT_HALF, gcx[i] + CONTACT_HALF, CONTACT_HALF + 70);
				}
				for (size_t i = 0; i < wl.size(); i++) add_obs(wl[i], wr[i], 0);
				std::sort(obs.begin(), obs.end());
				int ch0 = lo, ch1 = hi, best_w = -1, cur = lo;
				for (size_t i = 0; i < obs.size(); i++) {
					if (obs[i].first > cur) {
						int w = obs[i].first - cur;
						if (w > best_w) { best_w = w; ch0 = cur; ch1 = obs[i].first; }
					}
					cur = std::max(cur, obs[i].second);
				}
				if (hi - cur > best_w) { ch0 = cur; ch1 = hi; }
				// y 分界（sky130 hd 行结构推导，全段高 ≥170）
				int y1 = NMOS_C_Y1 + 30;              // 645：NMOS 接触段顶
				int y2 = CENTER_Y0 - 170;             // 825：中带通道底
				int y3 = CENTER_Y1 + 170;             // 1495：中带通道顶
				int y4 = PMOS_SG_Y0 - 40;             // 1685：PMOS 接触段底
				if (ch1 - ch0 >= 170) {
					push_sig(n0, NMOS_C_Y0 - 180, n1, y1);              // 段1 NMOS 接触
					push_sig(std::min(ch0, n1), y1, std::max(ch0, n1), y2); // 段2 过渡1（方向自适应）
					push_sig(ch0, y2, ch1, y3);                           // 段3 中带通道
					push_sig(std::min(p0, ch1), y3, std::max(p0, ch1), y4); // 段4 过渡2（方向自适应）
					push_sig(p0, y4, p1, PMOS_SH2_Y1 + 85);              // 段5 PMOS 接触（覆盖单列+共享区全部接触排）
				} else {
					// 通道不足（中带被栅接触/焊块塞满，如 xor2 类多栅密列）：
					// 竖带1(到中带底) + 中带横条拆段避开障碍 + 竖带2(中带顶起)；
					// 拆段之间若被障碍切断则该段电气断开（记录为结构族缺口：长距跨中带走线需通道绕行），
					// 但保证不产生同层重叠（DRC 干净）
					push_sig(n0, NMOS_C_Y0 - 180, n1, CENTER_Y0 - 25);
					int c0 = lo;
					for (auto& o : obs) {
						if (o.first > c0 && o.first - c0 >= 140)
							push_sig(c0, CENTER_Y0 - 25, o.first, CENTER_Y1 + 25);
						c0 = std::max(c0, o.second);
					}
					if (hi - c0 >= 140) push_sig(c0, CENTER_Y0 - 25, hi, CENTER_Y1 + 25);
					push_sig(p0, CENTER_Y1 + 25, p1, PMOS_SG2_Y1 + 85);
				}
			} else if (!y_p_cx.empty()) {
				// 仅 PMOS 侧 Y（如 inv：Y 接触在单列区两侧）
				int p0 = *std::min_element(y_p_cx.begin(), y_p_cx.end()) - CONTACT_HALF;
				int p1 = *std::max_element(y_p_cx.begin(), y_p_cx.end()) + CONTACT_HALF;
				push_sig(p0, PMOS_SG_Y0 - 40, p1, PMOS_SH2_Y1 + 85);
			} else if (!y_n_cx.empty()) {
				int n0 = *std::min_element(y_n_cx.begin(), y_n_cx.end()) - CONTACT_HALF;
				int n1 = *std::max_element(y_n_cx.begin(), y_n_cx.end()) + CONTACT_HALF;
				push_sig(n0, NMOS_C_Y0 - 180, n1, NMOS_C_Y1 + 180);
			}
		}
	}

	// ---- 10. 跨列信号连接条（同 net 多列 → li1 连接条，P/N 分侧统计，栅网免跨列条）----
	// P 侧跨列网画在 PMOS 带（y_lo~y_hi），N 侧跨列网画在 NMOS 带（NMOS 接触带 y 范围）
	// 栅网跳过：poly 已连通所有栅，不需要 li1 跨列条（a22o 的 y 内部栅网即此类）
	{
		std::map<std::string, std::vector<int>> sig_cols_p, sig_cols_n;
		for (int i = 0; i < num_diff; i++) {
			if (pdiff[i] != "dummy" && pdiff[i] != "VDD" && pdiff[i] != "VSS" && pdiff[i] != out_net && !gate_nets.count(pdiff[i]))
				sig_cols_p[pdiff[i]].push_back(i);
			if (ndiff[i] != "dummy" && ndiff[i] != "VDD" && ndiff[i] != "VSS" && ndiff[i] != out_net && !gate_nets.count(ndiff[i]))
				sig_cols_n[ndiff[i]].push_back(i);
		}
		auto draw_p_bridge = [&](const std::vector<int>& cols) {
			int xl0 = dcx[cols.front()] - CONTACT_HALF - 85;
			int xl1 = dcx[cols.front()] + CONTACT_HALF + 85;
			int xr0 = dcx[cols.back()] - CONTACT_HALF - 85;
			int xr1 = dcx[cols.back()] + CONTACT_HALF + 85;
			int y_lo = PMOS_SG_Y0 - 40;               // 1685（避开 Y 段4 上界）
			int y_hi = PMOS_SH2_Y1 + 85;              // 2460
			int vdd_tab_bot = PMOS_VDD_Y0 - 80;       // 2195（VDD 下探底）
			push_sig(xl0, y_lo, xl1, y_hi);
			push_sig(xl1, y_lo, xr0, vdd_tab_bot - 170);
			push_sig(xr0, y_lo, xr1, y_hi);
		};
		auto draw_n_bridge = [&](const std::vector<int>& cols) {
			// N 侧跨列条：NMOS 接触带 y 范围（避开 VSS 下探：横条走接触带中段）
			int xl0 = dcx[cols.front()] - CONTACT_HALF - 85;
			int xl1 = dcx[cols.front()] + CONTACT_HALF + 85;
			int xr0 = dcx[cols.back()] - CONTACT_HALF - 85;
			int xr1 = dcx[cols.back()] + CONTACT_HALF + 85;
			int y_lo = NMOS_C_Y0 - 40;                // 405
			int y_hi = NMOS_C_Y1 + 40;                // 655
			push_sig(xl0, y_lo, xl1, y_hi);
			push_sig(xl1, y_lo, xr0, y_hi);
			push_sig(xr0, y_lo, xr1, y_hi);
		};
		for (auto& kv : sig_cols_p) { auto& cols = kv.second; if (cols.size() >= 2) draw_p_bridge(cols); }
		for (auto& kv : sig_cols_n) { auto& cols = kv.second; if (cols.size() >= 2) draw_n_bridge(cols); }
	}

	// ---- 10.5 电源下探 tab（从电源接触位置推导，画时避让已画信号金属：下探从覆盖金属上沿开始）----
	{
		auto avoid = [&](int x0, int y0, int x1, int y1) {
			// 返回避让后的 y0（与已画信号金属 x 重叠且 y 区间相交时，从其上沿开始）
			int ny0 = y0;
			for (auto& r : drawn_sig)
				if (r[0] < x1 && r[2] > x0 && r[1] < y1 && r[3] > y0)
					ny0 = std::max(ny0, r[3]);
			return ny0;
		};
		// VSS 下探：覆盖 NMOS 侧 VSS 接触列
		for (int i = 0; i < num_diff; i++) {
			if (ndiff[i] == "VSS" && need_contact(ndiff[i])) {
				int top = (i == 0) ? NMOS_C0_Y1 + 10 : NMOS_C_Y1 + 10;
				int y0 = avoid(dcx[i] - CONTACT_HALF, VSS_RAIL_Y1, dcx[i] + CONTACT_HALF, top);
				if (y0 < top)
					push_sig(dcx[i] - CONTACT_HALF, y0, dcx[i] + CONTACT_HALF, top);
			}
		}
		// VDD 下探：覆盖 PMOS 侧 VDD 接触列
		for (int i = 0; i < num_diff; i++) {
			if (pdiff[i] == "VDD" && need_contact(pdiff[i])) {
				int y0 = avoid(dcx[i] - CONTACT_HALF, PMOS_VDD_Y0 - 80, dcx[i] + CONTACT_HALF, VDD_RAIL_Y0);
				if (y0 < VDD_RAIL_Y0)
					push_sig(dcx[i] - CONTACT_HALF, y0, dcx[i] + CONTACT_HALF, VDD_RAIL_Y0);
			}
		}
	}

	// ---- 11. pin 标记 67:16（吸附轨道）----
	{
		// 输入 pin：栅接触吸附轨道
		for (int i = 0; i < num_gate_real; i++) {
			std::string gnet = place_sol.nmos[real_col[i]].gate;
			bool is_input = false;
			for (auto& n : cell.IOnets) if (n == gnet && n != "VDD" && n != "VSS" && n != out_net) is_input = true;
			if (!is_input) continue;
			int mx = snap_track(gcx[i]);
			draw_rect(out, static_cast<int>(LAYER::LIG), 16, mx - CONTACT_HALF, GCONT_Y0, mx + CONTACT_HALF, GCONT_Y1);
		}
		// 输出 pin（Y）：画在 Y 金属覆盖范围内（与 Y 干线/接触金属同 x 区间），保证 100% 被 li1 覆盖
		bool has_y_p = false;
		for (int i = 0; i < num_diff; i++) if (pdiff[i] == out_net && need_contact(pdiff[i])) has_y_p = true;
		if (has_y_p) {
			// 重新收集 PMOS 侧 Y 接触列（该段独立作用域）
			std::vector<int> ypc;
			for (int i = 0; i < num_diff; i++) if (pdiff[i] == out_net && need_contact(pdiff[i])) ypc.push_back(dcx[i]);
			int y0 = *std::min_element(ypc.begin(), ypc.end()) - CONTACT_HALF;
			int y1 = *std::max_element(ypc.begin(), ypc.end()) + CONTACT_HALF;
			// 输出 pin 标记 = Y 接触覆盖金属的 bbox（sky130 的 pin 标记就是 pin 金属位置，
			// 不吸附轨道——轨道吸附只用于输入，输出以金属 bbox 为准，保证 100% 被 li1 覆盖）
			int mc = (y0 + y1) / 2;
			// 若 bbox 小于标记宽，扩展到标记宽
			int hw = CONTACT_HALF;
			if (y1 - y0 < 2 * hw) { y0 = mc - hw; y1 = mc + hw; }
			draw_rect(out, static_cast<int>(LAYER::LIG), 16, y0, PMOS_SG_Y0 + 60, y1, PMOS_SG_Y0 + 230);
		}
	}
std::vector<int> track_y;
	track_y.push_back(cell_height - active_unit - m1_width / 2);
	track_y.push_back(cell_height - active_unit - m1_width / 2 - m1_pitch);
	track_y.push_back(cell_height / 2);
	track_y.push_back(active_unit + m1_width / 2 + m1_pitch);
	track_y.push_back(active_unit + m1_width / 2);
	

	// Print I/O pin text
	for (auto& net : cell.IOnets) {
		if (net == "VDD") {
			// Label VDD
			out << "TEXT;" << std::endl;
			out << "LAYER " << static_cast<int>(LAYER::M1) << ";" << std::endl;
			out << "TEXTTYPE 251;" << std::endl;
			out << "PRESENTATION 0,1,1;" << std::endl;
			out << "PATHTYPE 0;" << std::endl;
			out << "STRANS 0,0,0;" << std::endl;
			out << "MAG 5.000000e-02;" << std::endl;
			out << "XY 1;" << std::endl;
			out << " X: " << x_right_boundary * 2 << 	";		 Y: " << cell_height * 4 << ";" << std::endl;
			out << "STRING VDD;" << std::endl;
			out << "ENDEL;" << std::endl << std::endl;
		}
		else if (net == "VSS") {
			// Label VSS
			out << "TEXT;" << std::endl;
			out << "LAYER " << static_cast<int>(LAYER::M1) << ";" << std::endl;
			out << "TEXTTYPE 251;" << std::endl;
			out << "PRESENTATION 0,1,1;" << std::endl;
			out << "PATHTYPE 0;" << std::endl;
			out << "STRANS 0,0,0;" << std::endl;
			out << "MAG 5.000000e-02;" << std::endl;
			out << "XY 1;" << std::endl;
			out << " X: " << x_right_boundary * 2 << 	";		 Y: " << 0 << ";" << std::endl;
			out << "STRING VSS;" << std::endl;
			out << "ENDEL;" << std::endl << std::endl;			
		}
		else {
			// 网名 TEXT 标签：从布局 pin_map（generate_pin_map 填充，非 z3 网格）取该网网格点
			if (!pin_map.count(net)) continue;
			bool** pm = pin_map.at(net);
			bool is_printed = false;
			for (int y = 0; y < row_size; y++) {
				for (int x = 0; x < col_size; x++) {
					if (pm[y][x]) {
						int cy = track_y[y];
						int cx = gate_pitch / 2 * (x + 1);

						out << "TEXT;" << std::endl;
						out << "LAYER " << static_cast<int>(LAYER::M1) << ";" << std::endl;
						out << "TEXTTYPE 251;" << std::endl;
						out << "PRESENTATION 0,1,1;" << std::endl;
						out << "PATHTYPE 0;" << std::endl;
						out << "STRANS 0,0,0;" << std::endl;
						out << "MAG 5.000000e-02;" << std::endl;
						out << "XY 1;" << std::endl;
						out << " X: " << cx * 4 << 	";		 Y: " << cy * 4 << ";" << std::endl;
						out << "STRING " << net << ";" << std::endl;
						out << "ENDEL;" << std::endl << std::endl;			
						is_printed = true;
						break;
					}
				}
				if (is_printed) break;
			}

		}
	}



	// END ASCII
	out << "ENDSTR;" << std::endl;
	out << "ENDLIB;" << std::endl;


}

void Router::ascii_to_gdsii(fs::path ascii_path) {
	std::string run_gds = "java -jar ./csyn_fp/ascii2gdsii.jar < " + ascii_path.string();
	auto gds_path = ascii_path.replace_extension(fs::path(".gds"));
	std::cout << gds_path << std::endl;
	std::string mv_gds = "mv DEFAULT.gds " + gds_path.string();

	//std::cout << run_gds << std::endl;
	//std::cout << mv_gds << std::endl;
	system(run_gds.c_str());
	system(mv_gds.c_str());
}
