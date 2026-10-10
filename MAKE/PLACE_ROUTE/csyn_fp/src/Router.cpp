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


void Router::add_detailed_routing_formulation(z3::context & c, z3::optimize & opt) {

	// Variables for detailed routing
	// metal routing map (x, y, z, n)1
	z3::expr_vector metal_grid(c);

	z3::expr_vector hor_grid(c);
	z3::expr_vector ver_grid(c);
	z3::expr_vector via_grid(c);

	int track_num = 5;
	int num_net_to_connect = 0;
	int layer_num = 2;
	num_layer = layer_num;

	//routing_direction M1 = routing_direction::BOTH;
	//routing_direction M2 = routing_direction::HOR;
	//routing_direction M3 = routing_direction::HOR;

	std::vector<routing_direction> metal_dir;
	metal_dir.push_back(M1);
	metal_dir.push_back(M2);
	//metal_dir.push_back(M3);


	auto idx = [&](int x, int y, int z, int n_idx) {
		return n_idx * layer_num * track_num * col_size + z * track_num * col_size + y * col_size + x;
	};

	// generate grid (metal_grid)
	for (auto & iter : pin_map) {
		for (int z = 0; z < layer_num; z++) {
			for (int y = 0; y < track_num; y++) {
				for (int x = 0; x < col_size; x++) {
					std::string metal_name = iter.first + "_(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
					
					metal_grid.push_back(c.int_const(metal_name.c_str()));
					opt.add(metal_grid[idx(x, y, z, num_net_to_connect)] >= 0 && metal_grid[idx(x, y, z, num_net_to_connect)] <= 1);

				}
			}
		}
		num_net_to_connect++;
	}	
	// hor_grid
	auto h_idx = [&](int x, int y, int z, int n_idx) {
		return n_idx * layer_num * track_num * (col_size - 1) + z * track_num * (col_size - 1) + y * (col_size - 1) + x;
	};
	int n = 0;
	for (auto & iter : pin_map) {
		for (int z = 0; z < layer_num; z++) {
			for (int y = 0; y < track_num; y++) {
				for (int x = 0; x < col_size - 1; x++) {
					std::string hor_name = iter.first + "_h(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
					hor_grid.push_back(c.int_const(hor_name.c_str()));

					if (metal_dir[z] == routing_direction::HOR || metal_dir[z] == routing_direction::BOTH) {
						opt.add(hor_grid[h_idx(x, y, z, n)] >= 0 && hor_grid[h_idx(x, y, z, n)] <= 1);
						//opt.add(z3::implies(hor_grid[h_idx(x, y, z, n)] == 1, metal_grid[idx(x, y, z, n)] == 1 && metal_grid[idx(x + 1, y, z, n)] == 1));
					}
					else { // Vertical
						opt.add(hor_grid[h_idx(x, y, z, n)] == 0);
					}
				}
			}
		}		
		n++;
	}

	// ver_grid
	auto v_idx = [&](int x, int y, int z, int n_idx) {
		return n_idx * layer_num * (track_num - 1) * col_size + z * (track_num - 1) * col_size + y * col_size + x;
	};
	n = 0;
	for (auto & iter : pin_map) {
		for (int z = 0; z < layer_num; z++) {
			for (int y = 0; y < track_num - 1; y++) {
				for (int x = 0; x < col_size; x++) {
					std::string ver_name = iter.first + "_v(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
					ver_grid.push_back(c.int_const(ver_name.c_str()));

					if (metal_dir[z] == routing_direction::VER || metal_dir[z] == routing_direction::BOTH) {
						opt.add(ver_grid[v_idx(x, y, z, n)] >= 0 && ver_grid[v_idx(x, y, z, n)] <= 1);
						//opt.add(z3::implies(ver_grid[v_idx(x, y, z, n)] == 1, metal_grid[idx(x, y, z, n)] == 1 && metal_grid[idx(x, y + 1, z, n)] == 1));
					}
					else opt.add(ver_grid[v_idx(x, y, z, n)] == 0);
				}
			}
		}		
		n++;
	}

	// via_grid
	auto i_idx = [&](int x, int y, int z, int n_idx) {
		return n_idx * (layer_num - 1) * track_num * col_size + z * track_num * col_size + y * col_size + x;
	};
	n = 0;
	for (auto & iter : pin_map) {
		for (int z = 0; z < layer_num - 1; z++) {
			for (int y = 0; y < track_num; y++) {
				for (int x = 0; x < col_size; x++) {
					std::string via_name = iter.first + "_i(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
					via_grid.push_back(c.int_const(via_name.c_str()));

					opt.add(via_grid[i_idx(x, y, z, n)] >= 0 && via_grid[i_idx(x, y, z, n)] <= 1);
					//opt.add(z3::implies(via_grid[i_idx(x, y, z, n)] == 1, metal_grid[idx(x, y, z, n)] == 1 && metal_grid[idx(x, y, z + 1, n)] == 1));
				}
			}
		}		
		n++;
	}

	// Metal grid definition
	for (int n = 0; n < num_net_to_connect; n++) {
		for (int z = 0; z < layer_num; z++) {
			for (int y = 0; y < track_num; y++) {
				for (int x = 0; x < col_size; x++) {
					z3::expr adjacent = c.int_val(0);
					// HOR_LEFT (not VER layer)
					if (x > 0 && metal_dir[z] != routing_direction::VER) 
						adjacent = adjacent + hor_grid[h_idx(x - 1, y, z, n)];
					// HOR_RIGHT (not VER layer)
					if (x < col_size - 1 && metal_dir[z] != routing_direction::VER) 
						adjacent = adjacent + hor_grid[h_idx(x, y, z, n)];
					// VER_TOP (not HOR layer)
					if (y > 0 && metal_dir[z] != routing_direction::HOR) 
						adjacent = adjacent + ver_grid[v_idx(x, y - 1, z, n)];
					// VER_BOTTOM (not HOR layer)
					if (y < track_num - 1 && metal_dir[z] != routing_direction::HOR) 
						adjacent = adjacent + ver_grid[v_idx(x, y, z, n)];
					// VIA_UP OR VIA_DOWN
					adjacent = adjacent + via_grid[i_idx(x, y, 0, n)];

					opt.add(z3::ite(adjacent == 0, metal_grid[idx(x, y, z, n)] == 0, metal_grid[idx(x, y, z, n)] == 1));
				}
			}
		}
	}



	// 2. Vertex exclusiveness
	for (int z = 0; z < layer_num; z++) {
		for (int y = 0; y < track_num; y++) {
			for (int x = 0; x < col_size; x++) {
				z3::expr occupied = c.int_val(0);
				for (int n = 0; n < num_net_to_connect; n++) {
					occupied = occupied + metal_grid[idx(x, y, z, n)];
				}
				opt.add(occupied <= 1);
			}
		}
	}

	auto check_fp = [](std::vector<Point>& pin_list, Point pin) {
		int index = 0;
		for (auto& pin_point : pin_list) {
			if (pin_point == pin) break;
			index++;
		}
		return index;
	};


	// 3. Connectivity constraints
	int n_idx = 0;
	for (auto& iter : floating_pins) {
		auto pin_name = iter.first;
		auto& floating_pin_list = iter.second;

		if (floating_pin_list.size() == 2) {
			auto& source_pin = floating_pin_list[0];
			auto& dest_pin = floating_pin_list[1];

			int n_source_point = source_pin.size();
			int n_dest_point = dest_pin.size();

			z3::expr_vector source_edge(c);
			z3::expr_vector dest_edge(c);

			int s_i = 0;
			for (auto& s_pin : source_pin) {
				std::string point_name = pin_name + "_source_0_(" + std::to_string(s_pin.x) + ", " + std::to_string(s_pin.y) + ")";
				source_edge.push_back(c.int_const(point_name.c_str()));

				opt.add(source_edge[s_i] >= 0 && source_edge[s_i] <= 1);
				//opt.add(z3::implies(source_edge[s_i] == 1, metal_grid[idx(s_pin.x, s_pin.y, 0, n_idx)]));
				s_i++;
			}
			int d_i = 0;
			for (auto& d_pin : dest_pin) {
				std::string point_name = pin_name + "_dest_0_(" + std::to_string(d_pin.x) + ", " + std::to_string(d_pin.y) + ")";
				dest_edge.push_back(c.int_const(point_name.c_str()));

				opt.add(dest_edge[d_i] >= 0 && dest_edge[d_i] <= 1);
				//opt.add(z3::implies(dest_edge[d_i] == 1, metal_grid[idx(d_pin.x, d_pin.y, 0, n_idx)]));
				d_i++;				
			}
			/* CONSTRAIN same active net routing */
			
			int lx = 999, rx = -1;
			bool source_all_pmos = true, source_all_nmos = true;
			bool dest_all_pmos = true, dest_all_nmos = true;
			for (auto& source_point : source_pin) {
				if (source_point.x < lx) lx = source_point.x;
				if (source_point.x > rx) rx = source_point.x;
				if (!(source_point.y == 0 || source_point.y == 1)) source_all_pmos = false;
				if (!(source_point.y == 5 || source_point.y == 6)) source_all_nmos = false;
			}
			for (auto& dest_point : dest_pin) {
				if (dest_point.x < lx) lx = dest_point.x;
				if (dest_point.x > rx) rx = dest_point.x;
				if (!(dest_point.y == 0 || dest_point.y == 1)) dest_all_pmos = false;
				if (!(dest_point.y == 5 || dest_point.y == 6)) dest_all_nmos = false;
			}

			if (source_all_nmos && dest_all_nmos) { // both in NMOS
				assert(!source_all_pmos && !dest_all_pmos);
				// hor grid restriction
				for (int z = 0; z < layer_num; z++) {
					for (int y = 0; y < track_num; y++) {
						for (int x = 0; x < col_size - 1; x++) {
							if ((x >= lx && x <= rx - 1) && (y >= 5 && y <= 6)) continue;
							opt.add(hor_grid[h_idx(x, y, z, n_idx)] == 0);
						}
					}
				}
				// ver grid restriction
				for (int z = 0; z < layer_num; z++) {
					for (int y = 0; y < track_num - 1; y++) {
						for (int x = 0; x < col_size; x++) {
							if ((x >= lx && x <= rx) && (y == 5)) continue;
							opt.add(ver_grid[v_idx(x, y, z, n_idx)] == 0);
						}
					}
				}
				// via grid restriction
				for (int z = 0; z < layer_num - 1; z++) {
					for (int y = 0; y < track_num; y++) {
						for (int x = 0; x < col_size; x++) {
							if ((x >= lx && x <= rx) && (y >= 5 && y <= 6)) continue;
							opt.add(via_grid[i_idx(x, y, z, n_idx)] == 0);
						}
					}
				}
			}

			else if (source_all_pmos && dest_all_pmos) { // both in PMOS
				assert(!source_all_nmos && !dest_all_nmos);
				// hor grid restriction
				for (int z = 0; z < layer_num; z++) {
					for (int y = 0; y < track_num; y++) {
						for (int x = 0; x < col_size - 1; x++) {
							if ((x >= lx && x <= rx - 1) && (y >= 0 && y <= 1)) continue;
							opt.add(hor_grid[h_idx(x, y, z, n_idx)] == 0);
						}
					}
				}
				// ver grid restriction
				for (int z = 0; z < layer_num; z++) {
					for (int y = 0; y < track_num - 1; y++) {
						for (int x = 0; x < col_size; x++) {
							if ((x >= lx && x <= rx) && (y == 0)) continue;
							opt.add(ver_grid[v_idx(x, y, z, n_idx)] == 0);
						}
					}
				}
				// via grid restriction
				for (int z = 0; z < layer_num - 1; z++) {
					for (int y = 0; y < track_num; y++) {
						for (int x = 0; x < col_size; x++) {
							if ((x >= lx && x <= rx) && (y >= 0 && y <= 1)) continue;
							opt.add(via_grid[i_idx(x, y, z, n_idx)] == 0);
						}
					}
				}
			}
			// Constrain routing region into acceptance field
			else {
				// hor grid restriction
				for (int z = 0; z < layer_num; z++) {
					for (int y = 0; y < track_num; y++) {
						for (int x = 0; x < col_size - 1; x++) {
							if (x >= lx - setting.route_accept && x <= rx - 1 + setting.route_accept) continue;
							opt.add(hor_grid[h_idx(x, y, z, n_idx)] == 0);
						}
					}
				}					

				// ver grid restriction
				for (int z = 0; z < layer_num; z++) {
					for (int y = 0; y < track_num - 1; y++) {
						for (int x = 0; x < col_size; x++) {
							if (x >= lx - setting.route_accept && x <= rx + setting.route_accept) continue;
							opt.add(ver_grid[v_idx(x, y, z, n_idx)] == 0);
						}
					}
				}
				// via grid restriction
				for (int z = 0; z < layer_num - 1; z++) {
					for (int y = 0; y < track_num; y++) {
						for (int x = 0; x < col_size; x++) {
							if (x >= lx - setting.route_accept && x <= rx + setting.route_accept) continue;
							opt.add(via_grid[i_idx(x, y, z, n_idx)] == 0);
						}
					}
				}
			}			
			/* END OF RESTRICTION */



			// Grid nodes (flow == 0 or 2)
			for (int z = 0; z < layer_num; z++) {
				for (int y = 0; y < track_num; y++) {
					for (int x = 0; x < col_size; x++) {
						int index = idx(x, y, z, n_idx);
						z3::expr adjacent = c.int_val(0);
						// HOR_LEFT (not VER layer)
						if (x > 0 && metal_dir[z] != routing_direction::VER) 
							adjacent = adjacent + hor_grid[h_idx(x - 1, y, z, n_idx)];
						// HOR_RIGHT (not VER layer)
						if (x < col_size - 1 && metal_dir[z] != routing_direction::VER) 
							adjacent = adjacent + hor_grid[h_idx(x, y, z, n_idx)];
						// VER_TOP (not HOR layer)
						if (y > 0 && metal_dir[z] != routing_direction::HOR) 
							adjacent = adjacent + ver_grid[v_idx(x, y - 1, z, n_idx)];
						// VER_BOTTOM (not HOR layer)
						if (y < track_num - 1 && metal_dir[z] != routing_direction::HOR) 
							adjacent = adjacent + ver_grid[v_idx(x, y, z, n_idx)];
						// VIA_UP OR VIA_DOWN
						adjacent = adjacent + via_grid[i_idx(x, y, 0, n_idx)];

						if (z == 0 && check_fp(source_pin, Point(y, x)) < n_source_point) {
							int p_i = check_fp(source_pin, Point(y, x));
							adjacent = adjacent + source_edge[p_i];
						}
						else if (z == 0 && check_fp(dest_pin, Point(y, x)) < n_dest_point) {
							int p_i = check_fp(dest_pin, Point(y, x));
							adjacent = adjacent + dest_edge[p_i];
						}
						opt.add(adjacent == 0 || adjacent == 2);
					}
				}
			}

			// Supernodes (flow == 1)
			z3::expr adjacent = c.int_val(0);
			for (int i = 0; i < s_i; i++) {
				adjacent = adjacent + source_edge[i];
			}
			opt.add(adjacent == 1);

			adjacent = c.int_val(0);
			for (int i = 0; i < d_i; i++) {
				adjacent = adjacent + dest_edge[i];
			}
			opt.add(adjacent == 1);


		}
		else {
			// ************* TODO (Multi-pin) ********************

			auto& source_pin = floating_pin_list[0];
			int num_dest_pins = floating_pin_list.size() - 1;

			z3::expr_vector multi_hor_grid(c);
			z3::expr_vector multi_ver_grid(c);
			z3::expr_vector multi_via_grid(c);

			// flow horizontal grid
			auto m_h_idx = [&](int x, int y, int z, int d_idx) {
					return d_idx * layer_num * track_num * (col_size - 1) + z * track_num * (col_size - 1) + y * (col_size - 1) + x;
			};
			for (int d = 0; d < num_dest_pins; d++) {
				for (int z = 0; z < layer_num; z++) {
					for (int y = 0; y < track_num; y++) {
						for (int x = 0; x < col_size - 1; x++) {
							std::string hor_name = pin_name + "_" + std::to_string(d) + "_h(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
							multi_hor_grid.push_back(c.int_const(hor_name.c_str()));

							if (metal_dir[z] == routing_direction::HOR || metal_dir[z] == routing_direction::BOTH) {
								opt.add(multi_hor_grid[m_h_idx(x, y, z, d)] >= 0 && multi_hor_grid[m_h_idx(x, y, z, d)] <= 1);
								//opt.add(z3::implies(multi_hor_grid[m_h_idx(x, y, z, d)] == 1, hor_grid[h_idx(x, y, z, n_idx)] == 1));
							}
							else opt.add(multi_hor_grid[m_h_idx(x, y, z, d)] == 0);
						}
					}
				}
			}
			for (int z = 0; z < layer_num; z++) {
				for (int y = 0; y < track_num; y++) {
					for (int x = 0; x < col_size - 1; x++) {
						z3::expr sum = c.int_val(0);
						for (int d = 0; d < num_dest_pins; d++) {
							sum = sum + multi_hor_grid[m_h_idx(x, y, z, d)];
						}
						opt.add(z3::ite(sum == 0, hor_grid[h_idx(x, y, z, n_idx)] == 0, hor_grid[h_idx(x, y, z, n_idx)] == 1));
					}
				}
			}



			// flow vertical grid
			auto m_v_idx = [&](int x, int y, int z, int d_idx) {
				return d_idx * layer_num * (track_num - 1) * col_size + z * (track_num - 1) * col_size + y * col_size + x;
			};
			for (int d = 0; d < num_dest_pins; d++) {
				for (int z = 0; z < layer_num; z++) {
					for (int y = 0; y < track_num - 1; y++) {
						for (int x = 0; x < col_size; x++) {
							std::string ver_name = pin_name + "_" + std::to_string(d) + "_v(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
							multi_ver_grid.push_back(c.int_const(ver_name.c_str()));

							if (metal_dir[z] == routing_direction::VER || metal_dir[z] == routing_direction::BOTH) {
								opt.add(multi_ver_grid[m_v_idx(x, y, z, d)] >= 0 && multi_ver_grid[m_v_idx(x, y, z, d)] <= 1);							
								//opt.add(z3::implies(multi_ver_grid[m_v_idx(x, y, z, d)] == 1, ver_grid[v_idx(x, y, z, n_idx)] == 1));
							}
							else opt.add(multi_ver_grid[m_v_idx(x, y, z, d)] == 0);
						}
					}
				}
			}
			for (int z = 0; z < layer_num; z++) {
				for (int y = 0; y < track_num - 1; y++) {
					for (int x = 0; x < col_size; x++) {
						z3::expr sum = c.int_val(0);
						for (int d = 0; d < num_dest_pins; d++) {
							sum = sum + multi_ver_grid[m_v_idx(x, y, z, d)];
						}
						opt.add(z3::ite(sum == 0, ver_grid[v_idx(x, y, z, n_idx)] == 0, ver_grid[v_idx(x, y, z, n_idx)] == 1));
					}
				}
			}

			// flow via grid
			auto m_i_idx = [&](int x, int y, int z, int d_idx) {
				return d_idx * (layer_num - 1) * track_num * col_size + z * track_num * col_size + y * col_size + x;
			};
			for (int d = 0; d < num_dest_pins; d++) {
				for (int z = 0; z < layer_num - 1; z++) {
					for (int y = 0; y < track_num; y++) {
						for (int x = 0; x < col_size; x++) {
							std::string via_name = pin_name + "_" + std::to_string(d) + "_i(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
							multi_via_grid.push_back(c.int_const(via_name.c_str()));

							opt.add(multi_via_grid[m_i_idx(x, y, z, d)] >= 0 && multi_via_grid[m_i_idx(x, y, z, d)] <= 1);
							opt.add(z3::implies(multi_via_grid[m_i_idx(x, y, z, d)] == 1, via_grid[i_idx(x, y, z, n_idx)] == 1));
						}
					}
				}
			}
			for (int z = 0; z < layer_num - 1; z++) {
				for (int y = 0; y < track_num; y++) {
					for (int x = 0; x < col_size; x++) {
						z3::expr sum = c.int_val(0);
						for (int d = 0; d < num_dest_pins; d++) {
							sum = sum + multi_via_grid[m_i_idx(x, y, z, d)];
						}
						opt.add(z3::ite(sum == 0, via_grid[i_idx(x, y, z, n_idx)] == 0, via_grid[i_idx(x, y, z, n_idx)] == 1));
					}
				}
			}

			// Connectivity for each flow
			for (int d = 0; d < num_dest_pins; d++) {
				auto& dest_pin = floating_pin_list[d + 1];

				int n_source_point = source_pin.size();
				int n_dest_point = dest_pin.size();

				z3::expr_vector source_edge(c);
				z3::expr_vector dest_edge(c);

				int s_i = 0;
				for (auto& s_pin : source_pin) {
					std::string point_name = pin_name + "_source_" + std::to_string(d) + "_(" + std::to_string(s_pin.x) + ", " + std::to_string(s_pin.y) + ")";
					source_edge.push_back(c.int_const(point_name.c_str()));

					opt.add(source_edge[s_i] >= 0 && source_edge[s_i] <= 1);
					s_i++;
				}

				int d_i = 0;
				for (auto& d_pin : dest_pin) {
					std::string point_name = pin_name + "_dest_" + std::to_string(d) + "_(" + std::to_string(d_pin.x) + ", " + std::to_string(d_pin.y) + ")";
					dest_edge.push_back(c.int_const(point_name.c_str()));

					opt.add(dest_edge[d_i] >= 0 && dest_edge[d_i] <= 1);
					//opt.add(z3::implies(dest_edge[d_i] == 1, metal_grid[idx(d_pin.x, d_pin.y, 0, n_idx)]));
					d_i++;				
				}

				/* CONSTRAIN same active net routing */
				
				int lx = 999, rx = -1;
				bool source_all_pmos = true, source_all_nmos = true;
				bool dest_all_pmos = true, dest_all_nmos = true;
				for (auto& source_point : source_pin) {
					if (source_point.x < lx) lx = source_point.x;
					if (source_point.x > rx) rx = source_point.x;
					if (!(source_point.y == 0 || source_point.y == 1)) source_all_pmos = false;
					if (!(source_point.y == 5 || source_point.y == 6)) source_all_nmos = false;
				}
				for (auto& dest_point : dest_pin) {
					if (dest_point.x < lx) lx = dest_point.x;
					if (dest_point.x > rx) rx = dest_point.x;
					if (!(dest_point.y == 0 || dest_point.y == 1)) dest_all_pmos = false;
					if (!(dest_point.y == 5 || dest_point.y == 6)) dest_all_nmos = false;
				}

				if (source_all_nmos && dest_all_nmos) { // both in NMOS
					assert(!source_all_pmos && !dest_all_pmos);
					// hor grid restriction
					for (int z = 0; z < layer_num; z++) {
						for (int y = 0; y < track_num; y++) {
							for (int x = 0; x < col_size - 1; x++) {
								if ((x >= lx && x <= rx - 1) && (y >= 5 && y <= 6)) continue;
								opt.add(multi_hor_grid[m_h_idx(x, y, z, d)] == 0);
							}
						}
					}
					// ver grid restriction
					for (int z = 0; z < layer_num; z++) {
						for (int y = 0; y < track_num - 1; y++) {
							for (int x = 0; x < col_size; x++) {
								if ((x >= lx && x <= rx) && (y == 5)) continue;
								opt.add(multi_ver_grid[m_v_idx(x, y, z, d)] == 0);
							}
						}
					}
					// via grid restriction
					for (int z = 0; z < layer_num - 1; z++) {
						for (int y = 0; y < track_num; y++) {
							for (int x = 0; x < col_size; x++) {
								if ((x >= lx && x <= rx) && (y >= 5 && y <= 6)) continue;
								opt.add(multi_via_grid[m_i_idx(x, y, z, d)] == 0);
							}
						}
					}
				}

				else if (source_all_pmos && dest_all_pmos) { // both in PMOS
					assert(!source_all_nmos && !dest_all_nmos);
					int lx = 999, rx = -1;
					for (auto& source_point : source_pin) {
						if (source_point.x < lx) lx = source_point.x;
						if (source_point.x > rx) rx = source_point.x;
					}
					for (auto& dest_point : dest_pin) {
						if (dest_point.x < lx) lx = dest_point.x;
						if (dest_point.x > rx) rx = dest_point.x;
					}
					// hor grid restriction
					for (int z = 0; z < layer_num; z++) {
						for (int y = 0; y < track_num; y++) {
							for (int x = 0; x < col_size - 1; x++) {
								if ((x >= lx && x <= rx - 1) && (y >= 0 && y <= 1)) continue;
								opt.add(multi_hor_grid[m_h_idx(x, y, z, d)] == 0);
							}
						}
					}
					// ver grid restriction
					for (int z = 0; z < layer_num; z++) {
						for (int y = 0; y < track_num - 1; y++) {
							for (int x = 0; x < col_size; x++) {
								if ((x >= lx && x <= rx) && (y == 0)) continue;
								opt.add(multi_ver_grid[m_v_idx(x, y, z, d)] == 0);
							}
						}
					}
					// via grid restriction
					for (int z = 0; z < layer_num - 1; z++) {
						for (int y = 0; y < track_num; y++) {
							for (int x = 0; x < col_size; x++) {
								if ((x >= lx && x <= rx) && (y >= 0 && y <= 1)) continue;
								opt.add(multi_via_grid[m_i_idx(x, y, z, d)] == 0);
							}
						}
					}
				}
				// Constrain routing region into acceptance field
				else {
					// hor grid restriction
					for (int z = 0; z < layer_num; z++) {
						for (int y = 0; y < track_num; y++) {
							for (int x = 0; x < col_size - 1; x++) {
								if (x >= lx - setting.route_accept && x <= rx - 1 + setting.route_accept) continue;
								opt.add(multi_hor_grid[m_h_idx(x, y, z, d)] == 0);
							}
						}
					}					

					// ver grid restriction
					for (int z = 0; z < layer_num; z++) {
						for (int y = 0; y < track_num - 1; y++) {
							for (int x = 0; x < col_size; x++) {
								if (x >= lx - setting.route_accept && x <= rx + setting.route_accept) continue;
								opt.add(multi_ver_grid[m_v_idx(x, y, z, d)] == 0);
							}
						}
					}
					// via grid restriction
					for (int z = 0; z < layer_num - 1; z++) {
						for (int y = 0; y < track_num; y++) {
							for (int x = 0; x < col_size; x++) {
								if (x >= lx - setting.route_accept && x <= rx + setting.route_accept) continue;
								opt.add(multi_via_grid[m_i_idx(x, y, z, d)] == 0);
							}
						}
					}
				}
				
				/* END OF RESTRICTION */



				// Grid nodes (flow == 0 or 2)
				for (int z = 0; z < layer_num; z++) {
					for (int y = 0; y < track_num; y++) {
						for (int x = 0; x < col_size; x++) {
							z3::expr adjacent = c.int_val(0);
							// HOR_LEFT
							if (x > 0 && metal_dir[z] != routing_direction::VER) 
								adjacent = adjacent + multi_hor_grid[m_h_idx(x - 1, y, z, d)];
							// HOR_RIGHT
							if (x < col_size - 1 && metal_dir[z] != routing_direction::VER) 
								adjacent = adjacent + multi_hor_grid[m_h_idx(x, y, z, d)]; 
							// VER_TOP (only M1)
							if (y > 0 && metal_dir[z] != routing_direction::HOR) 
								adjacent = adjacent + multi_ver_grid[m_v_idx(x, y - 1, z, d)];
							// VER_BOT (only M1)
							if (y < track_num - 1 && metal_dir[z] != routing_direction::HOR) 
								adjacent = adjacent + multi_ver_grid[m_v_idx(x, y, z, d)];
							// VIA_UP or VIA_DOWN
							adjacent = adjacent + multi_via_grid[m_i_idx(x, y, 0, d)];

							if (z == 0 && check_fp(source_pin, Point(y, x)) < n_source_point) {
								int p_i = check_fp(source_pin, Point(y, x));
								adjacent = adjacent + source_edge[p_i];
							}
							else if (z == 0 && check_fp(dest_pin, Point(y, x)) < n_dest_point) {
								int p_i = check_fp(dest_pin, Point(y, x));
								adjacent = adjacent + dest_edge[p_i];
							}

							opt.add(adjacent == 0 || adjacent == 2);
						}
					}
				}

				// Supernodes (flow == 1)
				z3::expr adjacent = c.int_val(0);
				for (int i = 0; i < s_i; i++) {
					adjacent = adjacent + source_edge[i];
				}
				opt.add(adjacent == 1);

				adjacent = c.int_val(0);
				for (int i = 0; i < d_i; i++) {
					adjacent = adjacent + dest_edge[i];
				}
				opt.add(adjacent == 1);
			}

		}
		n_idx++;
	}

	// 4. Design rule constraints
	// 4-1. S2S for vertical metal (ver - ver)
	
	for (int z = 0; z < layer_num; z++) {
		if (metal_dir[z] == routing_direction::HOR) continue;
		for (int y = 0; y < track_num - 1; y++) {
			for (int x = 0; x < col_size - 1; x++) {
				z3::expr cur_ver_exist = c.bool_val(false);
				z3::expr next_ver_exist = c.bool_val(false);
				for (int n = 0; n < num_net_to_connect; n++) {
					cur_ver_exist = cur_ver_exist || ver_grid[v_idx(x, y, z, n)] == 1;
					next_ver_exist = next_ver_exist || ver_grid[v_idx(x + 1, y, z, n)] == 1;
				}
				z3::expr S2S_ver_violation = cur_ver_exist && next_ver_exist;
				opt.add(!S2S_ver_violation);
 
			}
		}
	}
	
	// 4-2. Vertical metal side-to-side (top - bottom)
	for (int z = 0; z < layer_num; z++) {
		if (metal_dir[z] == routing_direction::HOR) continue;
		for (int y = 0; y < track_num - 2; y++) {
			for (int x = 0; x < col_size - 1; x++) {
				z3::expr left_top_ver_exist = c.bool_val(false);
				z3::expr right_top_ver_exist = c.bool_val(false);
				z3::expr left_bot_ver_exist = c.bool_val(false);
				z3::expr right_bot_ver_exist = c.bool_val(false);
				z3::expr hor_exist = c.bool_val(false);
				for (int n = 0; n < num_net_to_connect; n++) {
					left_top_ver_exist = left_top_ver_exist || ver_grid[v_idx(x, y, z, n)] == 1;
					right_top_ver_exist = right_top_ver_exist || ver_grid[v_idx(x + 1, y, z, n)] == 1;
					left_bot_ver_exist = left_bot_ver_exist || ver_grid[v_idx(x, y + 1, z, n)] == 1;
					right_bot_ver_exist = right_bot_ver_exist || ver_grid[v_idx(x + 1, y + 1, z, n)] == 1;
					hor_exist = hor_exist || hor_grid[h_idx(x, y+1, z, n)] == 1;
				}
				z3::expr S2S_ver_violation_one = left_top_ver_exist && right_bot_ver_exist && !hor_exist;
				z3::expr S2S_ver_violation_two = left_bot_ver_exist && right_top_ver_exist && !hor_exist;
				z3::expr S2S_ver_violation = S2S_ver_violation_one || S2S_ver_violation_two;
				opt.add(!S2S_ver_violation);
 
			}
		}
	}


	// 4-3. Horizontal side-to-side for vias 
	for (int z = 0; z < layer_num - 1; z++) {
		for (int y = 0; y < track_num; y++) {
			for (int x = 0; x < col_size - 1; x++) {
				z3::expr cur_via_exist = c.bool_val(false);
				z3::expr next_via_exist = c.bool_val(false);
				for (int n = 0; n < num_net_to_connect; n++) {
					cur_via_exist = cur_via_exist || via_grid[i_idx(x, y, z, n)] == 1;
					next_via_exist = next_via_exist || via_grid[i_idx(x + 1 , y, z, n)] == 1;
				}
				z3::expr S2S_via_violation = cur_via_exist && next_via_exist;
				opt.add(!S2S_via_violation);
			}
		}
	}

	// 4-4 Corner-to-corner for vias in PMOS/NMOS region
	for (int z = 0; z < layer_num - 1; z++) {
		for (int y = 0; y < track_num; y += 3) {
			for (int x = 0; x < col_size - 1; x++) {
				z3::expr left_top_via = c.bool_val(false);
				z3::expr right_top_via = c.bool_val(false);
				z3::expr left_bot_via = c.bool_val(false);
				z3::expr right_bot_via = c.bool_val(false);
				for (int n = 0; n < num_net_to_connect; n++) {
					left_top_via = left_top_via || via_grid[i_idx(x, y, z, n)] == 1;
					right_top_via = right_top_via || via_grid[i_idx(x + 1, y, z, n)] == 1;
					left_bot_via = left_bot_via || via_grid[i_idx(x, y + 1, z, n)] == 1;
					right_bot_via = right_bot_via || via_grid[i_idx(x + 1, y + 1, z, n)] == 1;
				}

				z3::expr C2C_via_violation_1 = left_top_via && right_bot_via;
				z3::expr C2C_via_violation_2 = right_top_via && left_bot_via;
				z3::expr C2C_via_violation = C2C_via_violation_1 || C2C_via_violation_2;
				opt.add(!C2C_via_violation);
			}
		}
	}



	// 4-5. T2S for vertical metals (ver <- hor metal) (only for 2D layers  | - )
	
	for (int z = 0; z < layer_num; z++) {
		if (metal_dir[z] != routing_direction::BOTH) continue;
		for (int y = 0; y < track_num - 1; y++) {
			for (int x = 0; x < col_size; x++) {
				z3::expr cur_ver_exist = c.bool_val(false);
				z3::expr vio_hor_exist = c.bool_val(false);
				
				for (int n = 0; n < num_net_to_connect; n++) {
					cur_ver_exist = cur_ver_exist || ver_grid[v_idx(x, y, z, n)] == 1;

					if (x >= 2) {
						vio_hor_exist = vio_hor_exist || (hor_grid[h_idx(x - 2, y, z, n)] == 1 && hor_grid[h_idx(x - 1, y, z, n)] == 0);
						vio_hor_exist = vio_hor_exist || (hor_grid[h_idx(x - 2, y + 1, z, n)] == 1 && hor_grid[h_idx(x - 1, y + 1, z, n)] == 0);
					}
					if (x <= col_size - 3) {
						vio_hor_exist = vio_hor_exist || (hor_grid[h_idx(x + 1, y, z, n)] == 1 && hor_grid[h_idx(x, y, z, n)] == 0);
						vio_hor_exist = vio_hor_exist || (hor_grid[h_idx(x + 1, y + 1, z, n)] == 1 && hor_grid[h_idx(x, y + 1, z, n)] == 0);
					}

				}

				z3::expr T2S_violation = cur_ver_exist && vio_hor_exist;
				opt.add(!T2S_violation);
			}
		}
	}
	/*
	for (int z = 0; z < layer_num; z++) {
		if (metal_dir[z] != routing_direction::BOTH) continue;
		for (int y = 0; y < track_num - 1; y++) {
			for (int x = 0; x < col_size - 4; x++) {
				z3::expr top_left_left = c.bool_val(false);
				z3::expr top_left_right = c.bool_val(false);
				z3::expr top_right_left = c.bool_val(false);
				z3::expr top_right_right = c.bool_val(false);

				z3::expr bot_left_left = c.bool_val(false);
				z3::expr bot_left_right = c.bool_val(false);
				z3::expr bot_right_left = c.bool_val(false);
				z3::expr bot_right_right = c.bool_val(false);
				
				z3::expr mid_ver = c.bool_val(false);

				for (int n = 0; n < num_net_to_connect; n++) {
					top_left_left = top_left_left || (hor_grid[h_idx(x, y, z, n)] == 1);
					top_left_right = top_left_right || (hor_grid[h_idx(x + 1, y, z, n)] == 1);
					top_right_left = top_right_left || (hor_grid[h_idx(x + 2, y, z, n)] == 1);
					top_right_right = top_right_right || (hor_grid[h_idx(x + 3, y, z, n)] == 1);

					bot_left_left = bot_left_left || (hor_grid[h_idx(x, y + 1, z, n)] == 1);
					bot_left_right = bot_left_right || (hor_grid[h_idx(x + 1, y + 1, z, n)] == 1);
					bot_right_left = bot_right_left || (hor_grid[h_idx(x + 2, y + 1, z, n)] == 1);
					bot_right_right = bot_right_right || (hor_grid[h_idx(x + 3, y + 1, z, n)] == 1);

					mid_ver = mid_ver || (ver_grid[v_idx(x + 2, y, z, n)] == 1);
				}

				z3::expr T2S_violation_1 = mid_ver && top_left_left && !top_left_right;
				z3::expr T2S_violation_2 = mid_ver && top_right_right && !top_right_left;
				z3::expr T2S_violation_3 = mid_ver && bot_left_left && !bot_left_right;
				z3::expr T2S_violation_4 = mid_ver && bot_right_right && !bot_right_left;
				z3::expr T2S_violation = T2S_violation_1 || T2S_violation_2 || T2S_violation_3 || T2S_violation_4;
				opt.add(!T2S_violation);
			}
		}
	}*/


	// 4-6 T2S for M1 (hor <- ver metal)
	for (int z = 0; z < layer_num; z++) {
		if (metal_dir[z] != routing_direction::BOTH) continue;
		for (int y = 0; y < track_num - 1; y++) {
			for (int x = 0; x < col_size; x++) {
				z3::expr cur_ver_exist = c.bool_val(false);
				z3::expr top_ver_exist = c.bool_val(false);
				z3::expr top_hor_exist = c.bool_val(false);
				z3::expr bot_hor_exist = c.bool_val(false);
				z3::expr bot_ver_exist = c.bool_val(false);

				z3::expr up_hor_exist = c.bool_val(false);
				z3::expr down_hor_exist = c.bool_val(false);
				
				for (int n = 0; n < num_net_to_connect; n++) {
					cur_ver_exist = cur_ver_exist || ver_grid[v_idx(x, y, z, n)] == 1;

					if (y > 0) top_ver_exist = top_ver_exist || (ver_grid[v_idx(x, y - 1, z, n)] == 1);
					if (y < row_size - 1) bot_ver_exist = bot_ver_exist || (ver_grid[v_idx(x, y + 1, z, n)] == 1);

					if (y > 0) {
						if (x > 0) top_hor_exist = top_hor_exist || (hor_grid[h_idx(x - 1, y - 1, z, n)] == 1);
						if (x < col_size - 1) top_hor_exist = top_hor_exist || (hor_grid[h_idx(x, y - 1, z, n)] == 1);
					}
					if (y < row_size - 2) {
						if (x > 0) bot_hor_exist = bot_hor_exist || (hor_grid[h_idx(x - 1, y + 2, z, n)] == 1);
						if (x < col_size - 1) bot_hor_exist = bot_hor_exist || (hor_grid[h_idx(x, y + 2, z, n)] == 1);				
					}

					if (x > 0) up_hor_exist = up_hor_exist || (hor_grid[h_idx(x - 1, y, z, n)] == 1);
					if (x < col_size - 1) up_hor_exist = up_hor_exist || (hor_grid[h_idx(x, y, z, n)] == 1);

					if (y < row_size - 1) {
						if (x > 0) down_hor_exist = down_hor_exist || (hor_grid[h_idx(x - 1, y + 1, z, n)] == 1);
						if (x < col_size - 1) down_hor_exist = down_hor_exist || (hor_grid[h_idx(x, y + 1, z, n)] == 1);
					}

				}

				if (y != 2) {
					z3::expr T2S_violation_top = cur_ver_exist && !top_ver_exist && top_hor_exist && !up_hor_exist;
					opt.add(!T2S_violation_top);
				}
				if (y != 1) {
					z3::expr T2S_violation_bot = cur_ver_exist && !bot_ver_exist && bot_hor_exist && !down_hor_exist;
					opt.add(!T2S_violation_bot);
				}
			}
		}
	}


	// 4-7. M1 MAR Rule (avoid M1 island)
	for (int n = 0; n < num_net_to_connect; n++) {
		for (int y = 0; y < track_num; y++) {
			for (int x = 0; x < col_size; x++) {
				z3::expr cur_pt_metal_exist = (metal_grid[idx(x, y, 0, n)] == 1);

				z3::expr metal_exist = c.bool_val(false);
				if (x > 0) metal_exist = metal_exist || hor_grid[h_idx(x - 1, y, 0, n)] == 1;
				if (x < col_size - 1) metal_exist = metal_exist || hor_grid[h_idx(x, y, 0, n)] == 1;

				if (y > 0) metal_exist = metal_exist || ver_grid[v_idx(x, y - 1, 0, n)] == 1;
				if (y < track_num - 1) metal_exist = metal_exist || ver_grid[v_idx(x, y, 0, n)] == 1;

				z3::expr double_via_violation = cur_pt_metal_exist && !metal_exist;
				opt.add(!double_via_violation);

			}
		}
	}

	// 4-8. Horizontal End-to-End Spacing (-  -)
	for (int z = 0; z < layer_num; z++) {
		if (metal_dir[z] == routing_direction::VER) continue;
		for (int y = 0; y < track_num; y++) {
			for (int x = 0; x < col_size - 3; x++) {
				z3::expr cur_hor_exist = c.bool_val(false);
				z3::expr next_hor_exist = c.bool_val(false);
				z3::expr two_next_hor_exist = c.bool_val(false);
				/*
				z3::expr three_next_hor_exist = c.bool_val(false);

				z3::expr left_ver_exist = c.bool_val(false);
				z3::expr right_ver_exist = c.bool_val(false);
				*/
				for (int n = 0; n < num_net_to_connect; n++) {
					cur_hor_exist = cur_hor_exist || (hor_grid[h_idx(x, y, z, n)] == 1);
					next_hor_exist = next_hor_exist || (hor_grid[h_idx(x + 1, y, z, n)] == 1);
					two_next_hor_exist = two_next_hor_exist || (hor_grid[h_idx(x + 2, y, z, n)] == 1);
					
					/*
					if (y > 0) left_ver_exist = left_ver_exist || (ver_grid[v_idx(x + 1, y - 1, z, n)] == 1);
					if (y < track_num - 1) left_ver_exist = left_ver_exist || (ver_grid[v_idx(x + 1, y, z, n)] == 1);
					
					if (x < col_size - 4) {
						three_next_hor_exist = three_next_hor_exist || hor_grid[h_idx(x + 3, y, z, n)] == 1;
						if (y > 0) right_ver_exist = right_ver_exist || (ver_grid[v_idx(x + 3, y - 1, z, n)] == 1);
						if (y < track_num - 1) right_ver_exist = right_ver_exist || (ver_grid[v_idx(x + 3, y, z, n)] == 1);

					}
					*/
				}
				z3::expr E2E_violation_one = cur_hor_exist && !next_hor_exist && two_next_hor_exist;
				/*
				z3::expr E2E_violation_two = cur_hor_exist && !next_hor_exist && !two_next_hor_exist && three_next_hor_exist && !left_ver_exist && !right_ver_exist;
				z3::expr E2E_violation = E2E_violation_one || E2E_violation_two;
				opt.add(!E2E_violation);
				*/
				opt.add(!E2E_violation_one);
			}
		}
	}

	// 4-9. Avoid T2S with M1 Power Metal
	/*
	if (metal_dir[0] != routing_direction::HOR) {
		// VDD rail
		for (int x = 0; x < col_size; x++) {
			z3::expr ver_exist = c.bool_val(false);
			z3::expr hor_exist = c.bool_val(false);
			for (int n = 0; n < num_net_to_connect; n++) {
				ver_exist = ver_exist || (ver_grid[v_idx(x, 0, 0, n)] == 1);
				if (x - 1 >= 0) hor_exist = hor_exist || (hor_grid[h_idx(x - 1, 0, 0, n)] == 1);
				if (x<= col_size - 2) hor_exist = hor_exist || (hor_grid[h_idx(x, 0, 0, n)] == 1); 
			}
			z3::expr vdd_vio = ver_exist && !hor_exist;
			opt.add(!vdd_vio);
		}
		// GND rail
		for (int x = 0; x < col_size; x++) {
			z3::expr ver_exist = c.bool_val(false);
			z3::expr hor_exist = c.bool_val(false);
			for (int n = 0; n < num_net_to_connect; n++) {
				ver_exist = ver_exist || (ver_grid[v_idx(x, 3, 0, n)] == 1);
				if (x - 1 >= 0) hor_exist = hor_exist || (hor_grid[h_idx(x - 1, 4, 0, n)] == 1);
				if (x <= col_size - 2) hor_exist = hor_exist || (hor_grid[h_idx(x, 4, 0, n)] == 1); 
			}
			z3::expr vdd_vio = ver_exist && !hor_exist;
			opt.add(!vdd_vio);
		}
	}*/

	// 4-10. Metal horizontal Corner-to-corner in PMOS/NMOS Region
	/*
	for (int z = 0; z < layer_num; z++) {
		if (metal_dir[z] == routing_direction::VER) continue;
		for (int y = 0; y < track_num; y += 3) {
			for (int x = 0; x < col_size - 3; x++) {
				z3::expr top_left_hor = c.bool_val(false);
				z3::expr top_mid_hor = c.bool_val(false);
				z3::expr top_right_hor = c.bool_val(false);
				z3::expr bot_left_hor = c.bool_val(false);
				z3::expr bot_mid_hor = c.bool_val(false);
				z3::expr bot_right_hor = c.bool_val(false);
				z3::expr left_ver = c.bool_val(false);
				z3::expr right_ver = c.bool_val(false);
				for (int n = 0; n < num_net_to_connect; n++) {
					top_left_hor = top_left_hor || hor_grid[h_idx(x, y, z, n)] == 1;
					top_mid_hor = top_mid_hor || hor_grid[h_idx(x + 1, y, z, n)] == 1;
					top_right_hor = top_right_hor || hor_grid[h_idx(x + 2, y, z, n)] == 1;
					bot_left_hor = bot_left_hor || hor_grid[h_idx(x, y + 1, z, n)] == 1;
					bot_mid_hor = bot_mid_hor || hor_grid[h_idx(x + 1, y + 1, z, n)] == 1;
					bot_right_hor = bot_right_hor || hor_grid[h_idx(x + 2, y + 1, z, n)] == 1;
					left_ver = left_ver || ver_grid[v_idx(x + 1, y, z, n)] == 1;
					right_ver = right_ver || ver_grid[v_idx(x + 2, y, z, n)] == 1;
				
				}
				z3::expr C2C_hor_violation_1 = top_left_hor && bot_right_hor && !top_mid_hor && !bot_mid_hor && !left_ver && !right_ver;
				z3::expr C2C_hor_violation_2 = top_right_hor && bot_left_hor && !top_mid_hor && !bot_mid_hor && !left_ver && !right_ver;
				z3::expr C2C_hor_violation = C2C_hor_violation_1 || C2C_hor_violation_2;
				opt.add(!C2C_hor_violation);
			}
		}
	}
	*/

	// 4-11. M1 vertical tip to horizontal side spacing
	/*
	// Upside
	for (int z = 0; z < layer_num; z++) {
		if (metal_dir[z] != routing_direction::BOTH) continue;
		int y = 0;
		for (int x = 0; x < col_size - 2; x++) {
			z3::expr top_left_hor = c.bool_val(false);
			z3::expr top_right_hor = c.bool_val(false);
			z3::expr mid_left_hor = c.bool_val(false);
			z3::expr mid_right_hor = c.bool_val(false);
			z3::expr top_left_ver = c.bool_val(false);
			z3::expr top_mid_ver = c.bool_val(false);
			z3::expr top_right_ver = c.bool_val(false);
			z3::expr bot_left_ver = c.bool_val(false);
			z3::expr bot_right_ver = c.bool_val(false);
			for (int n = 0; n < num_net_to_connect; n++) {
				top_left_hor = top_left_hor || hor_grid[h_idx(x, y, z, n)] == 1;
				top_right_hor = top_right_hor || hor_grid[h_idx(x + 1, y, z, n)] == 1;
				mid_left_hor = mid_left_hor || hor_grid[h_idx(x, y + 1, z, n)] == 1;
				mid_right_hor = mid_right_hor || hor_grid[h_idx(x + 1, y + 1, z, n)] == 1;
				top_left_ver = top_left_ver || ver_grid[v_idx(x, y, z, n)] == 1;
				top_mid_ver = top_mid_ver || ver_grid[v_idx(x + 1, y, z, n)] == 1;
				top_right_ver = top_right_ver || ver_grid[v_idx(x + 2, y, z, n)] == 1;
				bot_left_ver = bot_left_ver || ver_grid[v_idx(x, y + 1, z, n)] == 1;
				bot_right_ver = bot_right_ver || ver_grid[v_idx(x + 2, y + 1, z, n)] == 1;
			}
			z3::expr c2c_vio_1 = bot_left_ver && top_right_hor && !top_left_hor && !mid_left_hor && !top_left_ver && !top_mid_ver;
			z3::expr c2c_vio_2 = bot_right_ver && top_left_hor && !top_right_hor && !top_mid_ver && !top_right_ver && !mid_right_hor;
			z3::expr c2c_vio = c2c_vio_1 || c2c_vio_2;
			opt.add(!c2c_vio);
		}
	}
	// Downside
	for (int z = 0; z < layer_num; z++) {
		if (metal_dir[z] != routing_direction::BOTH) continue;
		int y = 2;
		for (int x = 0; x < col_size - 2; x++) {
			z3::expr mid_left_hor = c.bool_val(false);
			z3::expr mid_right_hor = c.bool_val(false);
			z3::expr bot_left_hor = c.bool_val(false);
			z3::expr bot_right_hor = c.bool_val(false);
			z3::expr top_left_ver = c.bool_val(false);
			z3::expr top_right_ver = c.bool_val(false);
			z3::expr bot_left_ver = c.bool_val(false);
			z3::expr bot_mid_ver = c.bool_val(false);
			z3::expr bot_right_ver = c.bool_val(false);
			for (int n = 0; n < num_net_to_connect; n++) {
				mid_left_hor = mid_left_hor || hor_grid[h_idx(x, y + 1, z, n)] == 1;
				mid_right_hor = mid_right_hor || hor_grid[h_idx(x + 1, y + 1, z, n)] == 1;
				bot_left_hor = bot_left_hor || hor_grid[h_idx(x, y + 2, z, n)] == 1;
				bot_right_hor = bot_right_hor || hor_grid[h_idx(x + 1, y + 2, z, n)] == 1;
				top_left_ver = top_left_ver || ver_grid[v_idx(x, y, z, n)] == 1;
				top_right_ver = top_right_ver || ver_grid[v_idx(x + 2, y, z, n)] == 1;
				bot_left_ver = bot_left_ver || ver_grid[v_idx(x, y + 1, z, n)] == 1;
				bot_mid_ver = bot_mid_ver || ver_grid[v_idx(x + 1, y + 1, z, n)] == 1;
				bot_right_ver = bot_right_ver || ver_grid[v_idx(x + 2, y + 1, z, n)] == 1;
			}
			z3::expr c2c_vio_1 = top_left_ver && bot_right_hor && !mid_left_hor && !bot_left_hor && !bot_left_ver && !bot_mid_ver;
			z3::expr c2c_vio_2 = top_right_ver && bot_left_hor && !mid_right_hor && !bot_right_hor && !bot_mid_ver && !bot_right_ver;
			z3::expr c2c_vio = c2c_vio_1 || c2c_vio_2;
			opt.add(!c2c_vio);
		}
	}
	*/


	// 4-6. Avoid via turning
	/*
	for (int z = 0; z < layer_num; z++) {
		if (metal_dir[z] != routing_direction::BOTH) continue;
		for (int y = 0; y < track_num; y++) {
			for (int x = 0; x < col_size; x++) {
				z3::expr up_exist = c.bool_val(false);
				z3::expr down_exist = c.bool_val(false);
				z3::expr left_exist = c.bool_val(false);
				z3::expr right_exist = c.bool_val(false);
				z3::expr via_exist = c.bool_val(false);

				for (int n = 0; n < num_net_to_connect; n++) {
					if (x > 0) left_exist = left_exist || (hor_grid[h_idx(x - 1, y, z, n)] == 1);
					if (x < col_size - 1) right_exist = right_exist || (hor_grid[h_idx(x, y, z, n)] == 1);
					if (y > 0) up_exist = up_exist || (ver_grid[v_idx(x, y - 1, z, n)] == 1);
					if (y < track_num - 1) down_exist = down_exist || (ver_grid[v_idx(x, y, z, n)] == 1);
				
					via_exist = via_exist || (via_grid[i_idx(x, y, z, n)] == 1);
				}

				z3::expr ver_one = (up_exist && !down_exist) || (!up_exist && down_exist);
				z3::expr hor_one = (left_exist && !right_exist) || (!left_exist && right_exist);
 
				z3::expr via_turn = via_exist && ver_one && hor_one;
				opt.add(!via_turn);
			}
		}
	}*/


	// 5. I/O pin extraction
	
	for (auto& IO_pin : IO_floating_pins) {
		std::string pin_name = IO_pin.first;
		auto& fp_list = IO_pin.second;
		if (fp_list.size() != 1) {
			std::cout << "IO error!" << " Pin_name: " << pin_name << ", fp_list: " << fp_list.size() << std::endl;
			exit(0);
		}

		z3::expr_vector IO_candidates(c);
		int f_i = 0;
		for (auto fpoint : fp_list[0]) {
			std::string point_name = pin_name + "_point_(" + std::to_string(fpoint.x) + ", " + std::to_string(fpoint.y) + ")";
			IO_candidates.push_back(c.int_const(point_name.c_str()));
			opt.add(IO_candidates[f_i] >= 0 && IO_candidates[f_i] <= 1);
			f_i++;
		}

		// Extract only one point
		z3::expr IO_num = c.int_val(0);
		for (int i = 0; i < f_i; i++) {
			IO_num = IO_num + IO_candidates[i];
		}
		opt.add(IO_num == 1);
		
		// Do not overlap with routing metals
		// Spacing violation with routing metals

		f_i = 0;
		for (auto fpoint : fp_list[0]) {
			z3::expr left_top_ver_exist = c.bool_val(false);
			z3::expr left_bot_ver_exist = c.bool_val(false);
			z3::expr right_top_ver_exist = c.bool_val(false);
			z3::expr right_bot_ver_exist = c.bool_val(false);

			z3::expr hor_exist = c.bool_val(false);

			z3::expr is_metal = c.bool_val(false);
			for (int n = 0; n < num_net_to_connect; n++) {
				if (fpoint.x > 0) {
					if (fpoint.y > 0) left_top_ver_exist = left_top_ver_exist || ver_grid[v_idx(fpoint.x - 1, fpoint.y - 1, 0, n)] == 1;
					if (fpoint.y < track_num - 1) left_bot_ver_exist = left_bot_ver_exist || ver_grid[v_idx(fpoint.x - 1, fpoint.y, 0, n)] == 1;
				}
				if (fpoint.x < col_size - 1) {
					if (fpoint.y > 0) right_top_ver_exist = right_top_ver_exist || ver_grid[v_idx(fpoint.x + 1, fpoint.y - 1, 0, n)] == 1;
					if (fpoint.y < track_num - 1) right_bot_ver_exist = right_bot_ver_exist || ver_grid[v_idx(fpoint.x + 1, fpoint.y, 0, n)] == 1;
				}
				is_metal = is_metal || metal_grid[idx(fpoint.x, fpoint.y, 0, n)] == 1;

				if (fpoint.x > 1) hor_exist = hor_exist || (hor_grid[h_idx(fpoint.x - 2, fpoint.y, 0, n)] == 1);
				if (fpoint.x < col_size - 1) hor_exist = hor_exist || (hor_grid[h_idx(fpoint.x + 1, fpoint.y, 0, n)] == 1);

			}
			z3::expr ver_exist = left_top_ver_exist || left_bot_ver_exist || right_top_ver_exist || right_bot_ver_exist;
			
			opt.add(z3::implies(IO_candidates[f_i] == 1, !ver_exist && !is_metal && !hor_exist));
			
			f_i++;

		}
	}
	
	// M2 restriction
/*
	z3::expr sum_m2 = c.int_val(0);
	
	for (int n = 0; n < num_net_to_connect; n++) {
		for (int y = 0; y < track_num; y++) {
			for (int x = 0; x < col_size - 1; x++) {
				if (metal_dir[1] != routing_direction::VER) sum_m2 = sum_m2 + hor_grid[h_idx(x, y, 1, n)];
			}
		}
	}
	int num_resources = track_num * (col_size - 1);
	int limit = num_resources / 2;
	opt.add(sum_m2 <= limit);	
*/

	// Objective
	
	z3::expr sum_m1 = c.int_val(0);
	z3::expr sum_m2 = c.int_val(0);
	//z3::expr sum_v1 = c.int_val(0);

	
	// Hor metal (M1, M2)
	
	for (int n = 0; n < num_net_to_connect; n++) {
		for (int y = 0; y < track_num; y++) {
			for (int x = 0; x < col_size - 1; x++) {
				if (metal_dir[0] != routing_direction::VER) sum_m1 = sum_m1 + hor_grid[h_idx(x, y, 0, n)];
				if (metal_dir[1] != routing_direction::VER) sum_m2 = sum_m2 + hor_grid[h_idx(x, y, 1, n)];
			}
		}
	}

	
	// Ver metal (M1)
	
	for (int n = 0; n < num_net_to_connect; n++) {
		for (int y = 0; y < track_num - 1; y++) {
			for (int x = 0; x < col_size; x++) {
				if (metal_dir[0] != routing_direction::HOR) sum_m1 = sum_m1 + ver_grid[v_idx(x, y, 0, n)];
			}
		}
	}

	// Via (v1)
	/*
	for (int n = 0; n < num_net_to_connect; n++) {
		for (int y = 0; y < track_num; y++) {
			for (int x = 0; x < col_size; x++) {
				sum_v1 = sum_v1 + via_grid[i_idx(x, y, 0, n)];
			}
		} 
	}*/
	
		

	//z3::optimize::handle obj1 = opt.minimize(m2_track);
	if (setting.min_m2) z3::optimize::handle obj1 = opt.minimize(sum_m2);
	//z3::optimize::handle obj2 = opt.minimize(sum_v1);
	//z3::optimize::handle obj1 = opt.minimize(sum_m2);
	if (setting.min_m1) z3::optimize::handle obj2 = opt.minimize(sum_m1);
	
}


z3::check_result Router::solve_SMT (z3::optimize &opt) {
	std::cout << opt << std::endl;
	z3::check_result result = opt.check();
	std::cout << opt.statistics() << std::endl;
	std::cout << result << std::endl;
	if (result == z3::sat) {
		z3::model m = opt.get_model();
		std::cout << m << std::endl;
	}
	return result;
}

void Router::gather_result(z3::context &c, z3::optimize &opt) {
	z3::model m = opt.get_model();

	// Variables for detailed routing
	// metal routing map (x, y, z, n)
	z3::expr_vector metal_grid(c);

	z3::expr_vector hor_grid(c);
	z3::expr_vector ver_grid(c);
	z3::expr_vector via_grid(c);

	int track_num = row_size;
	int num_net_to_connect = 0;
	int layer_num = num_layer;


	auto idx = [&](int x, int y, int z, int n_idx) {
		return n_idx * layer_num * track_num * col_size + z * track_num * col_size + y * col_size + x;
	};

	for (auto & iter : pin_map) {
		for (int z = 0; z < layer_num; z++) {
			for (int y = 0; y < track_num; y++) {
				for (int x = 0; x < col_size; x++) {
					std::string metal_name = iter.first + "_(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
					
					metal_grid.push_back(c.int_const(metal_name.c_str()));
				}
			}
		}
		num_net_to_connect++;
	}	

	// hor_grid
	auto h_idx = [&](int x, int y, int z, int n_idx) {
		return n_idx * layer_num * track_num * (col_size - 1) + z * track_num * (col_size - 1) + y * (col_size - 1) + x;
	};
	int n = 0;
	for (auto & iter : pin_map) {
		for (int z = 0; z < layer_num; z++) {
			for (int y = 0; y < track_num; y++) {
				for (int x = 0; x < col_size - 1; x++) {
					std::string hor_name = iter.first + "_h(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
					hor_grid.push_back(c.int_const(hor_name.c_str()));
				}
			}
		}		
		n++;
	}

	// ver_grid
	auto v_idx = [&](int x, int y, int z, int n_idx) {
		return n_idx * layer_num * (track_num - 1) * col_size + z * (track_num - 1) * col_size + y * col_size + x;
	};
	n = 0;
	for (auto & iter : pin_map) {
		for (int z = 0; z < layer_num; z++) {
			for (int y = 0; y < track_num - 1; y++) {
				for (int x = 0; x < col_size; x++) {
					std::string ver_name = iter.first + "_v(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
					ver_grid.push_back(c.int_const(ver_name.c_str()));
				}
			}
		}		
		n++;
	}

	// via_grid
	auto i_idx = [&](int x, int y, int z, int n_idx) {
		return n_idx * (layer_num - 1) * track_num * col_size + z * track_num * col_size + y * col_size + x;
	};
	n = 0;
	for (auto & iter : pin_map) {
		for (int z = 0; z < layer_num - 1; z++) {
			for (int y = 0; y < track_num; y++) {
				for (int x = 0; x < col_size; x++) {
					std::string via_name = iter.first + "_i(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")";
					via_grid.push_back(c.int_const(via_name.c_str()));

				}
			}
		}		
		n++;
	}
	routing_result["overall"] = std::make_shared<RoutingResult>(num_layer, row_size, col_size, false);
		
	int n_idx = 0;
	for (auto & iter : pin_map) {
		std::string net_name = iter.first;

		routing_result[net_name] = std::make_shared<RoutingResult>(num_layer, row_size, col_size, false);

		// Parse V0 result
		auto& fp_list = floating_pins[iter.first];
		int n_pin = fp_list.size();

		for (int d = 0; d < n_pin - 1; d++) {
			auto& source_pin = fp_list[0];
			auto& dest_pin = fp_list[d + 1];

			for (auto source_point : source_pin) {
				std::string source_name = iter.first + "_source_" + std::to_string(d) + "_(" + std::to_string(source_point.x) + ", " + std::to_string(source_point.y) + ")";
				//std::cout << source_name << std::endl;
				z3::expr source_point_exp = c.int_const(source_name.c_str());
				
				if (m.eval(source_point_exp).get_numeral_int() == 1) {
					routing_result[net_name]->via_grid[0][source_point.y][source_point.x] = 1;
					routing_result["overall"]->via_grid[0][source_point.y][source_point.x] = 1;
				}
			}

			for (auto dest_point : dest_pin) {
				std::string dest_name = iter.first + "_dest_" + std::to_string(d) + "_(" + std::to_string(dest_point.x) + ", " + std::to_string(dest_point.y) + ")";
				//std::cout << dest_name << std::endl;
				z3::expr dest_pin_exp = c.int_const(dest_name.c_str());

				if (m.eval(dest_pin_exp).get_numeral_int() == 1) {
					routing_result[net_name]->via_grid[0][dest_point.y][dest_point.x] = 1;
					routing_result["overall"]->via_grid[0][dest_point.y][dest_point.x] = 1;
				}
			}
		}


		// M1_Vertex
		for (int y = 0; y < track_num; y++) {
			for (int x = 0; x < col_size; x++) {
				int index = idx(x, y, 0, n_idx);
				if (m.eval(metal_grid[index]).get_numeral_int() == 1) {
					routing_result[net_name]->metal_grid[0][y][x] = 1;
					routing_result["overall"]->metal_grid[0][y][x] = 1;
				}

				if (x < col_size - 1) {
					if (m.eval(hor_grid[h_idx(x, y, 0, n_idx)]).get_numeral_int() == 1) {
						routing_result[net_name]->hor_grid[0][y][x] = 1;
						routing_result["overall"]->hor_grid[0][y][x] = 1;
					} 
				}
			}
			if (y == track_num - 1) continue;
			for (int x = 0; x < col_size; x++) {
				if (m.eval(ver_grid[v_idx(x, y, 0, n_idx)]).get_numeral_int() == 1) {
					routing_result[net_name]->ver_grid[0][y][x] = 1;
					routing_result["overall"]->ver_grid[0][y][x] = 1;					
				}
			}
		}

		// V1
		for (int y = 0; y < track_num; y++) {
			for (int x = 0; x < col_size; x++) {
				int index = i_idx(x, y, 0, n_idx);
				if (m.eval(via_grid[index]).get_numeral_int() == 1) {
					routing_result[net_name]->via_grid[1][y][x] = 1;
					routing_result["overall"]->via_grid[1][y][x] = 1;
				}
			}
		}


		// M2
		for (int y = 0; y < track_num; y++) {
			for (int x = 0; x < col_size; x++) {
				int index = idx(x, y, 1, n_idx);
				if (m.eval(metal_grid[index]).get_numeral_int() == 1) {
					routing_result[net_name]->metal_grid[1][y][x] = 1;
					routing_result["overall"]->metal_grid[1][y][x] = 1;
				} 
				if (x < col_size - 1) {
					if (m.eval(hor_grid[h_idx(x, y, 1, n_idx)]).get_numeral_int() == 1) {
						routing_result[net_name]->hor_grid[1][y][x] = 1;
						routing_result["overall"]->hor_grid[1][y][x] = 1;
					}
				}
			}			
			if (y == track_num - 1) continue;
			for (int x = 0; x < col_size; x++) {
				if (m.eval(ver_grid[v_idx(x, y, 1, n_idx)]).get_numeral_int() == 1) {
					routing_result[net_name]->ver_grid[1][y][x] = 1;
					routing_result["overall"]->ver_grid[1][y][x] = 1;					
				}
			}
		}

		if (layer_num > 2) {
			// V2
			for (int y = 0; y < track_num; y++) {
				for (int x = 0; x < col_size; x++) {
					int index = i_idx(x, y, 1, n_idx);
					if (m.eval(via_grid[index]).get_numeral_int() == 1) {
						routing_result[net_name]->via_grid[2][y][x] = 1;
						routing_result["overall"]->via_grid[2][y][x] = 1;
					} 
				}
			}


			// M3
			for (int y = 0; y < track_num; y++) {
				for (int x = 0; x < col_size; x++) {
					int index = idx(x, y, 2, n_idx);
					if (m.eval(metal_grid[index]).get_numeral_int() == 1) {
						routing_result[net_name]->metal_grid[2][y][x] = 1;
						routing_result["overall"]->metal_grid[2][y][x] = 1;
					}
					if (x < col_size - 1) {
						if (m.eval(hor_grid[h_idx(x, y, 2, n_idx)]).get_numeral_int() == 1) {
							routing_result[net_name]->hor_grid[2][y][x] = 1;
							routing_result["overall"]->hor_grid[2][y][x] = 1;
						}
					}
				}			
				if (y == track_num - 1) continue;
				for (int x = 0; x < col_size; x++) {
					if (m.eval(ver_grid[v_idx(x, y, 2, n_idx)]).get_numeral_int() == 1) {
						routing_result[net_name]->ver_grid[2][y][x] = 1;
						routing_result["overall"]->ver_grid[2][y][x] = 1;					
					} 
				} 
			}
		}

		n_idx++;
	}

	// Print IO_pins
	for (auto& IO_pin : IO_floating_pins) {
		std::string pin_name = IO_pin.first;

		routing_result[pin_name] = std::make_shared<RoutingResult>(num_layer, row_size, col_size, true);

		auto& fp_list = IO_pin.second;
		if (fp_list.size() != 1) {
			std::cout << "IO error!" << std::endl;
			exit(0);
		}
		
		for (auto source_point : fp_list[0]) {
			std::string source_name = pin_name + "_point_(" + std::to_string(source_point.x) + ", " + std::to_string(source_point.y) + ")";
			//std::cout << source_name << std::endl;
			z3::expr source_point_exp = c.int_const(source_name.c_str());
			
			if (m.eval(source_point_exp).get_numeral_int() == 1) {
				routing_result["overall"]->metal_grid[0][source_point.y][source_point.x] = 1;
				routing_result["overall"]->via_grid[0][source_point.y][source_point.x] = 1;
				routing_result[pin_name]->metal_grid[0][source_point.y][source_point.x] = 1;
				routing_result[pin_name]->via_grid[0][source_point.y][source_point.x] = 1;
			

			} 
		}
	}
}

void Router::print_result(std::ofstream &out) {

	// Routing result for individual pins
	out << "-------- Routing Results -----------------" << std::endl;
	for (auto& pin_route : routing_result) {
		std::string name = pin_route.first;
		auto pin_route_result = pin_route.second;

		if (name == "overall") continue;
		
		out << "========== " << name << " ===========" << std::endl;

		// V0
		out << "V0" << std::endl;
		for (int y = 0; y < row_size; y++) {
			for (int x = 0; x < col_size; x++) {
				out << pin_route_result->via_grid[0][y][x] << " ";

			}
			out << std::endl << std::endl;
		}
		out << std::endl;		

		// M1
		out << "M1" << std::endl;
		for (int y = 0; y < row_size; y++) {
			for (int x = 0; x < col_size; x++) {
				out << pin_route_result->metal_grid[0][y][x];

				if (x < col_size - 1) {
					if (pin_route_result->hor_grid[0][y][x] == 1) out << "-";
					else out << " ";
				}
			}
			out << std::endl;
			if (y == row_size - 1) continue;
			for (int x = 0; x < col_size; x++) {
				if (pin_route_result->ver_grid[0][y][x] == 1) out << "| ";
				else out << "  ";
			}
			out << std::endl;
		}
		out << std::endl;		

		// V1
		out << "V1" << std::endl;
		for (int y = 0; y < row_size; y++) {
			for (int x = 0; x < col_size; x++) {
				out << pin_route_result->via_grid[1][y][x] << " ";
			}
			out << std::endl << std::endl;
		}
		out << std::endl;		

		// M2
		out << "M2" << std::endl;
		for (int y = 0; y < row_size; y++) {
			for (int x = 0; x < col_size; x++) {
				out << pin_route_result->metal_grid[1][y][x];

				if (x < col_size - 1) {
					if (pin_route_result->hor_grid[1][y][x] == 1) out << "-";
					else out << " ";
				}
			}
			out << std::endl;
			if (y == row_size - 1) continue;
			for (int x = 0; x < col_size; x++) {
				if (pin_route_result->ver_grid[1][y][x] == 1) out << "| ";
				else out << "  ";
			}
			out << std::endl;
		}
		out << std::endl;		
	}

	// Overall
	out << "--------------- Overall -----------------" << std::endl;
	auto overall_result = routing_result["overall"];
	// V0
	out << "V0" << std::endl;
	for (int y = 0; y < row_size; y++) {
		for (int x = 0; x < col_size; x++) {
			out << overall_result->via_grid[0][y][x] << " ";
		}
		out << std::endl << std::endl;
	}
	out << std::endl;		

	// M1
	out << "M1" << std::endl;
	for (int y = 0; y < row_size; y++) {
		for (int x = 0; x < col_size; x++) {
			out << overall_result->metal_grid[0][y][x];
			if (x < col_size - 1) {
				if (overall_result->hor_grid[0][y][x] == 1) out << "-";
				else out << " ";
			}
		}
		out << std::endl;
		if (y == row_size - 1) continue;
		for (int x = 0; x < col_size; x++) {
			if (overall_result->ver_grid[0][y][x] == 1) out << "| ";
			else out << "  ";
		}
		out << std::endl;
	}
	out << std::endl;		

	// V1
	out << "V1" << std::endl;
	for (int y = 0; y < row_size; y++) {
		for (int x = 0; x < col_size; x++) {
			out << overall_result->via_grid[1][y][x] << " ";
		}
		out << std::endl << std::endl;
	}
	out << std::endl;		
	
	int m2_hor_resource = (col_size - 1) * row_size;
	int m2_hor_usage = 0;

	// M2
	out << "M2" << std::endl;
	for (int y = 0; y < row_size; y++) {
		for (int x = 0; x < col_size; x++) {
			out << overall_result->metal_grid[1][y][x];

			if (x < col_size - 1) {
				if (overall_result->hor_grid[1][y][x] == 1) {
					m2_hor_usage++;
					out << "-";
				} 
				else out << " ";
			}
		}
		out << std::endl;
		if (y == row_size - 1) continue;
		for (int x = 0; x < col_size; x++) {
			if (overall_result->ver_grid[1][y][x] == 1) out << "| ";
			else out << "  ";
		}
		out << std::endl;
	}
	out << std::endl;		

	m2_usage = static_cast<double>(m2_hor_usage) / m2_hor_resource;


}

bool Router::routing(fs::path output_path) {

	if(place_sol.cellWidth == 0) return false;
	
	initialize_grid();
	generate_pin_map();

	z3::set_param("timeout", 60 * 60000);
	//z3::set_param("parallel.enable", true);
	//z3::set_param("parallel.threads.max", 4);
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
	
	z3::context c;
	z3::optimize opt(c);

	add_detailed_routing_formulation(c, opt);

	auto start = std::chrono::high_resolution_clock::now();
	auto result = opt.check();
	auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
	runtime = duration.count();
	//out << "Routing time : " << duration.count() << "ms" << std::endl;
	
	if (result == z3::sat) {
		gather_result (c, opt);
		print_result (out);
		int var_num = 0;
		int clause_num = 0;
		auto st = opt.statistics();
		for (int j = 0; j < st.size(); j++) {
			std::string k = st.key(j);
			if (k == "sat mk var") var_num = st.uint_value(j);
			if (k == "sat mk clause 2ary") clause_num += st.uint_value(j);
			if (k == "sat mk clause 3ary") clause_num += st.uint_value(j);
			if (k == "sat mk clause nary") clause_num += st.uint_value(j);
		}
		out << "var_num : " << var_num << ", clause_num : " << clause_num << std::endl;
		out << "Routing time : " << duration.count() << "ms" << std::endl;

		is_routable = true;

		std::cout << "SAT!" << std::endl;
		
		if (setting.gen_gds) {
			auto ascii_path = output_path.replace_extension(fs::path(".ascii"));
			auto cell_name = output_path.stem();
			std::ofstream ascii_out(ascii_path.string());
			generate_ascii(ascii_out, cell_name.string());
			ascii_to_gdsii(ascii_path);
		}

	}

	else { 
		std::cout << "UNSAT!" << std::endl;

		out << "Unsat!" << std::endl;
		out << "Routing time : " << duration.count() << "ms" << std::endl;

		is_routable = false;
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
	

	// Print nets
	for (auto& it : routing_result) {
		std::string net_name = it.first;
		auto route = it.second;

		if (net_name == "overall") continue;

		// not I/O pin
		else {
			// sky130 互连模型：li1 金属 = 电源轨 + LEF pin 区 + 接触覆盖（金属填充）。
			// 网格路径画法是 ASAP7 长互连模型，sky130 单元内互连是短金属片，无需路径形状；
			// z3 仍求解并验证连通（SAT），仅不再绘制路径。
			continue;
		}

			// draw M2
			// Find metal component through DFS
			std::vector<std::vector<bool>> explored;
			explored.resize(row_size);
			for (int i = 0; i < row_size; i++) {
				explored[i].resize(col_size);
				for (int j = 0; j < col_size; j++) explored[i][j] = false;
			}

			for (int y = 0; y < row_size; y++) {
				for (int x = 0; x < col_size; x++) {
					if (route->metal_grid[1][y][x] == 1 && !explored[y][x]) {
						
						std::vector<Point> component_points;

						// Find connected components
						DFS_Metal(route, component_points, y, x, explored, 1);
						
						// Find boundary points
						std::vector<std::tuple<Point, bool, bool, bool, bool>> boundary_points;
						for (auto& point : component_points) {
							// hor, ver both exist or only one direction

							bool left =  (point.x > 0 && route->hor_grid[1][point.y][point.x - 1] == 1) ? true : false; 
							bool right = (point.x < col_size - 1 && route->hor_grid[1][point.y][point.x] == 1) ? true : false;
							bool up = (point.y > 0 && route->ver_grid[1][point.y - 1][point.x] == 1) ? true : false;
							bool down = (point.y < row_size - 1 && route->ver_grid[1][point.y][point.x] == 1) ? true : false;

							int cy = track_y[point.y];
							int cx = gate_pitch / 2 * (point.x + 1);

							assert(left || right || up || down);

							int hor_dir = 0, ver_dir = 0;
							if (left) hor_dir++;
							if (right) hor_dir++;
							if (up) ver_dir++;
							if (down) ver_dir++;

							if (!((hor_dir > 0 && ver_dir > 0) || hor_dir + ver_dir == 1)) continue;

							// End point	
							if (left && !right && !up && !down) {
								boundary_points.push_back(std::make_tuple(Point(cy - m1_width / 2, cx + m1_width / 2 + m1_v0_ex), true, false, true, false));
								boundary_points.push_back(std::make_tuple(Point(cy + m1_width / 2, cx + m1_width / 2 + m1_v0_ex), true, false, false, true));
							}
							else if (!left && right && !up  && !down) {
								boundary_points.push_back(std::make_tuple(Point(cy - m1_width / 2, cx - m1_width / 2 - m1_v0_ex), false, true, true, false));
								boundary_points.push_back(std::make_tuple(Point(cy + m1_width / 2, cx - m1_width / 2 - m1_v0_ex), false, true, false, true));
							}
							else if (!left && !right && up && !down) {
								boundary_points.push_back(std::make_tuple(Point(cy - m1_width / 2 - m1_v0_ex, cx - m1_width / 2), false, true, true, false));
								boundary_points.push_back(std::make_tuple(Point(cy - m1_width / 2 - m1_v0_ex, cx + m1_width / 2), true, false, true, false));
							}
							else if (!left && !right && !up && down) {
								boundary_points.push_back(std::make_tuple(Point(cy + m1_width / 2 + m1_v0_ex, cx - m1_width / 2), false, true, false, true));
								boundary_points.push_back(std::make_tuple(Point(cy + m1_width / 2 + m1_v0_ex, cx + m1_width / 2), true, false, false, true));
							}						
						
							// two-direction points
							else if (left && !right && up && !down) {
								boundary_points.push_back(std::make_tuple(Point(cy + m1_width / 2, cx - m1_width / 2), true, false, true, false));
								boundary_points.push_back(std::make_tuple(Point(cy - m1_width / 2, cx + m1_width / 2), true, false, true, false));
							}
							else if (!left && right && up && !down) {
								boundary_points.push_back(std::make_tuple(Point(cy + m1_width / 2, cx + m1_width / 2), false, true, true, false));
								boundary_points.push_back(std::make_tuple(Point(cy - m1_width / 2, cx - m1_width / 2), false, true, true, false));
							}
							else if (left && !right && !up && down) {
								boundary_points.push_back(std::make_tuple(Point(cy + m1_width / 2, cx + m1_width / 2), true, false, false, true));
								boundary_points.push_back(std::make_tuple(Point(cy - m1_width / 2, cx - m1_width / 2), true, false, false, true));
							}
							else if (!left && right && !up && down) {
								boundary_points.push_back(std::make_tuple(Point(cy + m1_width / 2, cx - m1_width / 2), false, true, false, true));
								boundary_points.push_back(std::make_tuple(Point(cy - m1_width / 2, cx + m1_width / 2), false, true, false, true));
							}

							// three-direction points
							else if (left && right && up && !down) {
								boundary_points.push_back(std::make_tuple(Point(cy + m1_width / 2, cx - m1_width / 2), true, false, true, false));
								boundary_points.push_back(std::make_tuple(Point(cy + m1_width / 2, cx + m1_width / 2), false, true, true, false));
							}
							else if (!left && right && up && down) {
								boundary_points.push_back(std::make_tuple(Point(cy - m1_width / 2, cx + m1_width / 2), false, true, false, true));
								boundary_points.push_back(std::make_tuple(Point(cy + m1_width / 2, cx + m1_width / 2), false, true, true, false));
							}
							else if (left && right && !up && down) {
								boundary_points.push_back(std::make_tuple(Point(cy - m1_width / 2, cx - m1_width / 2), true, false, false, true));
								boundary_points.push_back(std::make_tuple(Point(cy - m1_width / 2, cx + m1_width / 2), false, true, false, true));
							}
							else if (left && !right && up && down) {
								boundary_points.push_back(std::make_tuple(Point(cy - m1_width / 2, cx - m1_width / 2), true, false, false, true));
								boundary_points.push_back(std::make_tuple(Point(cy + m1_width / 2, cx - m1_width / 2), true, false, true, false));
							}

							// four-direction points
							else if (left && right && up && down) {
								boundary_points.push_back(std::make_tuple(Point(cy - m1_width / 2, cx - m1_width / 2), true, false, false, true));
								boundary_points.push_back(std::make_tuple(Point(cy + m1_width / 2, cx - m1_width / 2), true, false, true, false));
								boundary_points.push_back(std::make_tuple(Point(cy - m1_width / 2, cx + m1_width / 2), false, true, false, true));
								boundary_points.push_back(std::make_tuple(Point(cy + m1_width / 2, cx + m1_width / 2), false, true, true, false));
			
							}
						}

						// Circulate
						std::vector<Point> cir_boundary;
						cir_boundary.push_back(std::get<0>(boundary_points[0]));
						
						// Left, Right, Up, Down
						int to_direction = (std::get<1>(boundary_points[0])) ? 0 : ((std::get<2>(boundary_points[0])) ? 1 : 2);

						Point previous_point = std::get<0>(boundary_points[0]);

						int n_boundary_points = boundary_points.size();
						std::vector<bool> visited(n_boundary_points, false);
						
						for (int i = 0; i < n_boundary_points; i++) {
							// Find next point index

							int next_pt_idx = -1;
							int dist = 99999999;
							for (int j = 0; j < n_boundary_points; j++) {
								auto& next_cand_point = std::get<0>(boundary_points[j]);

								if (visited[j]) continue;
								if (to_direction == 0 && previous_point.x > next_cand_point.x && previous_point.y == next_cand_point.y) {
									if (dist > std::abs(previous_point.x - next_cand_point.x)) {
										dist = std::abs(previous_point.x - next_cand_point.x);
										next_pt_idx = j;
									}									
								}
								if (to_direction == 1 && previous_point.x < next_cand_point.x && previous_point.y == next_cand_point.y) {
									if (dist > std::abs(previous_point.x - next_cand_point.x)) {
										dist = std::abs(previous_point.x - next_cand_point.x);
										next_pt_idx = j;
									}									
								}
								if (to_direction == 2 && previous_point.y < next_cand_point.y && previous_point.x == next_cand_point.x) {
									if (dist > std::abs(previous_point.y - next_cand_point.y)) {
										dist = std::abs(previous_point.y - next_cand_point.y);
										next_pt_idx = j;
									}													
								}
								if (to_direction == 3 && previous_point.y > next_cand_point.y && previous_point.x == next_cand_point.x) {
									if (dist > std::abs(previous_point.y - next_cand_point.y)) {
										dist = std::abs(previous_point.y - next_cand_point.y);
										next_pt_idx = j;
									}													
								}
							}

							if (next_pt_idx == -1) {
								std::cout << "Cannot find next points" << std::endl;
								exit(0);
							}
							cir_boundary.push_back(std::get<0>(boundary_points[next_pt_idx]));
							visited[next_pt_idx] = true;
						
							previous_point = std::get<0>(boundary_points[next_pt_idx]);
							if (to_direction == 0 || to_direction == 1) {
								if (std::get<3>(boundary_points[next_pt_idx])) to_direction = 2;
								else if (std::get<4>(boundary_points[next_pt_idx])) to_direction = 3;
								else {
									std::cout << "Direction Error!" << std::endl;
									exit(0);
								}
							}
							else if (to_direction == 2 || to_direction == 3) {
								if (std::get<1>(boundary_points[next_pt_idx])) to_direction = 0;
								else if (std::get<2>(boundary_points[next_pt_idx])) to_direction = 1;
								else {
									std::cout << "Direction Error!" << std::endl;
									exit(0);
								}
							}
							else {
								std::cout << "Direction Out of bound!" << std::endl;
								exit(0);
							}
						
						}

						// Draw M2

						out << "BOUNDARY;" << std::endl;
						out << "LAYER " << static_cast<int>(LAYER::M2) << ";" << std::endl;
						out << "DATATYPE 20;" << std::endl;
						out << "XY "<< static_cast<int>(cir_boundary.size()) << " ;" << std::endl;
						for (int i = 0; i < cir_boundary.size(); i++) {
							out << " X: " << cir_boundary[i].x * 4 << ";		 Y: " << cir_boundary[i].y * 4 << ";" << std::endl;
						}
						out << "ENDEL;" << std::endl << std::endl;
					}
				}
			}
		}

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
			auto result = routing_result[net];
			bool is_printed = false;
			for (int y = 0; y < row_size; y++) {
				for (int x = 0; x < col_size; x++) {
					if (result->metal_grid[0][y][x] == 1) {
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

void Router::DFS_Metal (std::shared_ptr<RoutingResult> route, std::vector<Point>& components, int y, int x, std::vector<std::vector<bool>>& explored, int metal_layer) {
	
	explored[y][x] = true;
	components.emplace_back(y, x);

	// Left?
	if (x > 0 && route->hor_grid[metal_layer][y][x - 1] == 1 && !explored[y][x - 1]) {
		DFS_Metal(route, components, y, x - 1, explored, metal_layer);
	} 

	// Right?
	if (x < col_size - 1 && route->hor_grid[metal_layer][y][x] == 1 && !explored[y][x + 1]) {
		DFS_Metal(route, components, y, x + 1, explored, metal_layer);
	}

	// Up?
	if (y > 0 && route->ver_grid[metal_layer][y - 1][x] == 1 && !explored[y - 1][x]) {
		DFS_Metal(route, components, y - 1, x, explored, metal_layer);
	}

	// Down?
	if (y < row_size - 1 && route->ver_grid[metal_layer][y][x] == 1 && !explored[y + 1][x]) {
		DFS_Metal(route, components, y + 1, x, explored, metal_layer);
	}

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
