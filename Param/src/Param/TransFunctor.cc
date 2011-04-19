#include "TransFunctor.h"
#include "ChartCreator.h"
#include <hj_3rd/hjlib/math/blas_lapack.h>
#include <hj_3rd/zjucad/matrix/lapack.h>

#include <map>
#include <set>
#include <queue>
#include <iostream>
#include <limits>

using namespace zjucad::matrix;
using namespace std;
namespace PARAM
{
	TransFunctor::TransFunctor(boost::shared_ptr<ChartCreator> _p_chart_creator)
		: p_chart_creator(_p_chart_creator) {}
	TransFunctor::~TransFunctor(){}

	matrix<double> TransFunctor::GetTransMatrix(int from_chart_id, int to_chart_id, int vid) const
	{
		matrix<double> trans_mat(eye<double>(3));
		if(from_chart_id == to_chart_id ) return trans_mat;
		vector<int> trans_list;		
		if(!GetTranslistBetweenTwoCharts(from_chart_id, to_chart_id, trans_list)){
			std::cout<< "Cannot get the transition list!\n";
			return trans_mat;
		}
		assert(trans_list.size() >=2);

		for(size_t k=1; k<trans_list.size(); ++k){	
			matrix<double> temp_trans_mat = trans_mat;
			trans_mat =  GetTransMatrixOfAdjCharts(trans_list[k-1], trans_list[k], vid) * temp_trans_mat;
		}

		return trans_mat;
	}

	
	matrix<double> TransFunctor::GetTransMatrix(int from_vid, int from_chart_id, int to_vid, int to_chart_id) const
	{
		matrix<double> trans_mat(eye<double>(3));
		if(from_chart_id == to_chart_id ) return trans_mat;
		vector<int> trans_list;		
		map < pair<int, int>, vector<int> >::const_iterator iter = m_edge_trans_list.find(std::make_pair(from_vid, to_vid));
		if(iter != m_edge_trans_list.end()){
			trans_list = iter->second;
		}else{
			if(!GetTranslistBetweenTwoCharts(from_chart_id, to_chart_id, trans_list)){
				std::cout<< "Cannot get the transition list!\n";
				return trans_mat;
			}
		}
		assert(trans_list.size() >=2);		

		for(size_t k=1; k<trans_list.size(); ++k){	
			matrix<double> temp_trans_mat = trans_mat;
			trans_mat =  GetTransMatrixOfAdjCharts(trans_list[k-1], trans_list[k],from_vid) * temp_trans_mat;
		}

		return trans_mat;
	}

	bool TransFunctor::GetTranslistBetweenTwoCharts(int from_chart_id, int to_chart_id, vector<int>& trans_list) const
	{
		trans_list.clear();
		const vector<ParamPatch>& patch_array =p_chart_creator->GetPatchArray();

		set<int> visited_face_set;
		map<int, int> prev_chart_map;

		queue<int> q;
		q.push(to_chart_id);
		visited_face_set.insert(to_chart_id);
		prev_chart_map[to_chart_id] = -1;
        
		bool flag = false;
		while(!q.empty()){
			int cur_chart_id = q.front(); q.pop();
			if(cur_chart_id == from_chart_id) { flag = true; break;}
			const std::vector<int>& adj_charts = patch_array[cur_chart_id].m_nb_patch_index_array;
			for(size_t k=0; k<adj_charts.size(); ++k){
				int chart_id = adj_charts[k];
				if(visited_face_set.find(chart_id) == visited_face_set.end()){
					q.push(chart_id);
					visited_face_set.insert(chart_id);
					prev_chart_map[chart_id] = cur_chart_id;
				}
			}
		}

		if(!flag) return false;

		trans_list.push_back(from_chart_id);
		int prev_chart = prev_chart_map[from_chart_id];

		while(prev_chart != -1){
			trans_list.push_back(prev_chart);
			prev_chart = prev_chart_map[prev_chart];
		}

		return true;
	}

	matrix<double> TransFunctor::GetTransMatrixInOneChart(int chart_id, pair<int,int> old_x_axis, pair<int,int> new_x_axis) const
    {
        const vector<ParamPatch>& patch_array =p_chart_creator->GetPatchArray();
        const ParamPatch& param_patch = patch_array[chart_id];
		const vector<ParamCoord>& conner_pc_vec = param_patch.m_conner_pc_array;
        
		matrix<double> t_mat(eye<double>(3));
		matrix<double> r_mat(eye<double>(3));

		int old_origin = old_x_axis.first, new_origin = new_x_axis.first;
		if(old_origin != new_origin){
			double tx = conner_pc_vec[old_origin].s_coord - conner_pc_vec[new_origin].s_coord;
			double ty = conner_pc_vec[old_origin].t_coord - conner_pc_vec[new_origin].t_coord;
			t_mat(0, 2) = tx; t_mat(1, 2) = ty;
		}

		if(old_x_axis != new_x_axis){
			double x1, y1, x2, y2;
			x1 = conner_pc_vec[old_x_axis.second].s_coord - conner_pc_vec[old_x_axis.first].s_coord;
			y1 = conner_pc_vec[old_x_axis.second].t_coord - conner_pc_vec[old_x_axis.first].t_coord;
			x2 = conner_pc_vec[new_x_axis.second].s_coord - conner_pc_vec[new_x_axis.first].s_coord;
			y2 = conner_pc_vec[new_x_axis.second].t_coord - conner_pc_vec[new_x_axis.first].t_coord;

			double cross_v(x1*y2 - y1*x2), dot_v(x1*x2 + y1*y2);
			double len1(sqrt(x1*x1 + y1*y1)), len2(sqrt(x2*x2 + y2*y2));
            double asin_angle = asin(cross_v/(len1*len2));
			double acos_angle = acos(dot_v/(len1*len2));
            
			double r_angle;
			if(fabs(dot_v) < LARGE_ZERO_EPSILON){
				if(cross_v < 0) r_angle = PI*3/2;
				else r_angle = PI/2;
			}else if(fabs(cross_v) < LARGE_ZERO_EPSILON){
				if(dot_v < 0) r_angle = PI;
				else r_angle = 0;
			}else {             
   				if(cross_v < 0 && dot_v > 0) r_angle = asin_angle + 2*PI; /// 4 
				else if( cross_v < 0 && dot_v < 0) r_angle = PI - asin_angle; /// 3
				else if( cross_v > 0 && dot_v < 0) r_angle = acos_angle; /// 2
				else if( cross_v > 0 && dot_v > 0) r_angle = asin_angle; /// 1

				if(r_angle < 0) r_angle += 2*PI;
            }
			double cos_v = cos(r_angle), sin_v = sin(r_angle);
			r_mat(0, 0) = cos_v; r_mat(0, 1) = sin_v;
			r_mat(1, 0) = -sin_v; r_mat(1, 1) = cos_v;
		}

		return r_mat*t_mat;
	}

	matrix<double> TransFunctor::GetTransMatrixOfAdjCharts(int from_cid, int to_cid, int vid) const{
		if(p_chart_creator->IsAmbiguityChartPair(from_cid, to_cid)){
			return GetTransMatrixBetweenAmbiguityCharts(vid, from_cid, -1, to_cid);
		}
        
		const ParamPatch& fp = (p_chart_creator->GetPatchArray())[from_cid];
		const ParamPatch& tp = (p_chart_creator->GetPatchArray())[to_cid];
        
		/// find the common edge of these two charts
		const vector<int>& from_pe_vec = fp.m_edge_index_array;
		const vector<int>& to_pe_vec = tp.m_edge_index_array;
        
		vector<int> common_edges;
		for(size_t k=0; k<from_pe_vec.size(); ++k){
			if(find(to_pe_vec.begin(), to_pe_vec.end(), from_pe_vec[k]) != to_pe_vec.end()) common_edges.push_back(from_pe_vec[k]);
		}
		/// the common edges may be not only one, choose the minimum index edge
		int com_edge = *(min_element(common_edges.begin(), common_edges.end()));
		int com_edge_idx_1(-1), com_edge_idx_2(-1);
		for(size_t k=0; k<from_pe_vec.size(); ++k){
			if(from_pe_vec[k] == com_edge) { com_edge_idx_1 = (int)k; break; }
		}
		for(size_t k=0; k<to_pe_vec.size(); ++k){
			if(to_pe_vec[k] == com_edge) { com_edge_idx_2 = (int) k; break; }
		}
		assert(com_edge_idx_1 != -1 && com_edge_idx_2 != -1);
        
		/// find the from chart and to chart's origin and x-axis
		pair<int, int> old_x_axis_1, new_x_axis_1;
        size_t edge_num1 = fp.m_edge_index_array.size();
		old_x_axis_1 = make_pair(0, 1); 
		new_x_axis_1 = make_pair(com_edge_idx_1, (com_edge_idx_1+1)%edge_num1);

        size_t edge_num2 = tp.m_edge_index_array.size();
		pair<int, int> old_x_axis_2, new_x_axis_2;
		old_x_axis_2 = make_pair(0, 1);
		new_x_axis_2 = make_pair( (com_edge_idx_2+1)%edge_num2, com_edge_idx_2);        
		matrix<double> trans_mat_1 = GetTransMatrixInOneChart(from_cid, old_x_axis_1, new_x_axis_1);
		matrix<double> trans_mat_2 = GetTransMatrixInOneChart(to_cid, old_x_axis_2, new_x_axis_2);
        matrix<double> scale_mat = GetScaleMatrixBetweenTwoCharts(from_cid, to_cid, com_edge_idx_1, com_edge_idx_2);

        inv(trans_mat_2);
        matrix<double> temp_mat = scale_mat * trans_mat_1;

		return trans_mat_2 * temp_mat;
	}

    matrix<double> TransFunctor::GetScaleMatrixBetweenTwoCharts(int from_cid, int to_cid, int pe_index_1, int pe_index_2) const
    {
        const ParamPatch& fp = (p_chart_creator->GetPatchArray())[from_cid];
		const ParamPatch& tp = (p_chart_creator->GetPatchArray())[to_cid];
        assert(fp.m_edge_index_array[pe_index_1] == tp.m_edge_index_array[pe_index_2]);
        size_t corner_num1 = fp.m_conner_pc_array.size();
        ParamCoord pc11 = fp.m_conner_pc_array[pe_index_1];
        ParamCoord pc12 = fp.m_conner_pc_array[(pe_index_1+1) % corner_num1];
        double len1 = Coord2D(pc11.s_coord - pc12.s_coord, pc11.t_coord - pc12.t_coord).abs();
        size_t corner_num2 = tp.m_conner_pc_array.size();
        ParamCoord pc21 = tp.m_conner_pc_array[pe_index_2];
        ParamCoord pc22 = tp.m_conner_pc_array[(pe_index_2+1) % corner_num2];
        double len2 = Coord2D(pc21.s_coord - pc22.s_coord, pc21.t_coord - pc22.t_coord).abs();
        assert(fabs(len1) > 1e-8 && fabs(len2) > 1e-8);
        
        double scale = len1/len2;
        // if(from_cid == 2 && to_cid == 3 || from_cid == 3 && to_cid == 2){
        //     cout << "Scale : " << endl;
        //     cout << from_cid <<" " << to_cid << endl;
        //     cout << pe_index_1 <<" " << pe_index_2 << endl;
        //     cout << "(" << pc11.s_coord << ", " << pc11.t_coord <<")\t";
        //     cout << "(" << pc12.s_coord << ", " << pc12.t_coord <<")"<<endl;
        //     cout << "(" << pc21.s_coord << ", " << pc21.t_coord <<")\t";
        //     cout << "(" << pc22.s_coord << ", " << pc22.t_coord << endl;
        //     cout << "scale : " << scale << endl;   
        // }
                
        matrix<double> scale_mat( eye<double>(3));
        scale_mat(0, 0) = scale_mat(1, 1) = scale;
        return scale_mat;
    }

    
	matrix<double> TransFunctor::GetTransMatrixBetweenAmbiguityCharts(int from_vert, int from_chart_id, int to_vert, int to_chart_id) const
	{
		const vector<ParamPatch>& patch_array = p_chart_creator->GetPatchArray();
		const ParamPatch& from_patch = patch_array[from_chart_id];
		const ParamPatch& to_patch = patch_array[to_chart_id];

		const std::vector<PatchEdge>& patch_edge_array = p_chart_creator->GetPatchEdgeArray();

		/// find the common edge of these two charts
		const std::vector<int>& from_patch_edge_array = from_patch.m_edge_index_array;
		const std::vector<int>& to_patch_edge_array = to_patch.m_edge_index_array;

		std::vector<int> common_edges;
		for(size_t k=0; k<from_patch_edge_array.size(); ++k){
			if(find(to_patch_edge_array.begin(), to_patch_edge_array.end(), from_patch_edge_array[k]) 
				!= to_patch_edge_array.end()) common_edges.push_back(from_patch_edge_array[k]);
		}

		if(common_edges.size() == 0 ) {
			std::cerr << "Error: These two charts don't have common edges, so they are not ambiguity chart pair!" << std::endl;
			return zjucad::matrix::eye<double>(3);
		}else if(common_edges.size() == 1) {
			std::cerr << "Error: These two charts only have one common edge, so they are not ambiguity chart pair!" << std::endl;
			return GetTransMatrixOfAdjCharts(from_chart_id, to_chart_id, from_vert);
		}

		int com_edge = ChooseEdgeForAmbiguityChart(common_edges, from_vert, to_vert);
		
		int com_edge_idx_1(-1), com_edge_idx_2(-1);
		for(size_t k=0; k<from_patch_edge_array.size(); ++k){
			if(from_patch_edge_array[k] == com_edge) { com_edge_idx_1 = (int)k; break; }
		}
		for(size_t k=0; k<to_patch_edge_array.size(); ++k){
			if(to_patch_edge_array[k] == com_edge) { com_edge_idx_2 = (int) k; break; }
		}

		assert(com_edge_idx_1 != -1 && com_edge_idx_2 != -1);

		pair<int, int> old_x_axis_1, new_x_axis_1;
		/// find the from chart and to chart's origin and x-axis
		size_t edge_num = from_patch.m_edge_index_array.size();
		old_x_axis_1 = std::make_pair(0, 1); 
		new_x_axis_1 = std::make_pair(com_edge_idx_1, (com_edge_idx_1+1)%edge_num);

		std::pair<int, int> old_x_axis_2, new_x_axis_2;
		old_x_axis_2 = std::make_pair(0, 1);
		new_x_axis_2 = std::make_pair( (com_edge_idx_2+1)%edge_num, com_edge_idx_2);	

		zjucad::matrix::matrix<double> trans_mat_1 = GetTransMatrixInOneChart(from_chart_id, old_x_axis_1, new_x_axis_1);
		zjucad::matrix::matrix<double> trans_mat_2 = GetTransMatrixInOneChart(to_chart_id, old_x_axis_2, new_x_axis_2);
		inv(trans_mat_2);

		return trans_mat_2 * trans_mat_1;
	}

	void TransFunctor::TransParamCoordBetweenAmbiguityCharts(int from_vert, int from_chart_id, int to_chart_id, 
		const ParamCoord& from_coord, ParamCoord& to_coord) const
	{
		const std::vector<ParamPatch>& patch_array = p_chart_creator->GetPatchArray();
		const ParamPatch& from_patch = patch_array[from_chart_id];
		const ParamPatch& to_patch = patch_array[to_chart_id];

		const std::vector<PatchEdge>& patch_edge_array = p_chart_creator->GetPatchEdgeArray();

		/// find the common edge of these two charts
		const std::vector<int>& from_patch_edge_array = from_patch.m_edge_index_array;
		const std::vector<int>& to_patch_edge_array = to_patch.m_edge_index_array;

		std::vector<int> common_edges;
		for(size_t k=0; k<from_patch_edge_array.size(); ++k){
			if(find(to_patch_edge_array.begin(), to_patch_edge_array.end(), from_patch_edge_array[k]) 
				!= to_patch_edge_array.end()) common_edges.push_back(from_patch_edge_array[k]);
		}

		if(common_edges.size() == 0 ) {
			std::cerr << "Error: These two charts don't have common edges, so they are not ambiguity chart pair!" << std::endl;
			return ;
		}else if(common_edges.size() == 1) {
			std::cerr << "Error: These two charts only have one common edge, so they are not ambiguity chart pair!" << std::endl;
			return ;
		}

        int com_edge = ChooseEdgeForAmbiguityChart(common_edges, from_vert);

		int com_edge_idx_1(-1), com_edge_idx_2(-1);
		for(size_t k=0; k<from_patch_edge_array.size(); ++k){
			if(from_patch_edge_array[k] == com_edge) { com_edge_idx_1 = (int)k; break; }
		}
		for(size_t k=0; k<to_patch_edge_array.size(); ++k){
			if(to_patch_edge_array[k] == com_edge) { com_edge_idx_2 = (int) k; break; }
		}

		assert(com_edge_idx_1 != -1 && com_edge_idx_2 != -1);

		std::pair<int, int> old_x_axis_1, new_x_axis_1;
			/// find the from chart and to chart's origin and x-axis
			size_t edge_num = from_patch.m_edge_index_array.size();
		old_x_axis_1 = std::make_pair(0, 1); 
		new_x_axis_1 = std::make_pair(com_edge_idx_1, (com_edge_idx_1+1)%edge_num);

		std::pair<int, int> old_x_axis_2, new_x_axis_2;
		old_x_axis_2 = std::make_pair(0, 1);
		new_x_axis_2 = std::make_pair( (com_edge_idx_2+1)%edge_num, com_edge_idx_2);	

		zjucad::matrix::matrix<double> trans_mat_1 = GetTransMatrixInOneChart(from_chart_id,
			old_x_axis_1, new_x_axis_1);
		zjucad::matrix::matrix<double> trans_mat_2 = GetTransMatrixInOneChart(to_chart_id,
			old_x_axis_2, new_x_axis_2);
		inv(trans_mat_2);

		zjucad::matrix::matrix<double> trans_mat = trans_mat_2*trans_mat_1;

		to_coord.s_coord = trans_mat(0, 0)*from_coord.s_coord + 
			trans_mat(0, 1)*from_coord.t_coord + trans_mat(0, 2);
		to_coord.t_coord = trans_mat(1, 0)*from_coord.s_coord +
			trans_mat(1, 1)*from_coord.t_coord + trans_mat(1, 2);
	}

	int TransFunctor::ChooseEdgeForAmbiguityChart(const std::vector<int>& common_edges,
		int from_vert, int to_vert/* =-1 */) const		
	{
		const std::vector<PatchEdge>& patch_edge_array = p_chart_creator->GetPatchEdgeArray();
		boost::shared_ptr<MeshModel> p_mesh = p_chart_creator->GetMeshModel();
		
		double min_dist = std::numeric_limits<double>::infinity();
		int com_edge(-1);
		for(size_t k=0; k<common_edges.size(); ++k)
		{
			const std::vector<int>& mesh_path = patch_edge_array[common_edges[k]].m_mesh_path;
			double cur_dist = 0.0;
			int nearest_vert;
			cur_dist += GetNearestVertexOnPath(p_mesh, from_vert, mesh_path, nearest_vert);		
			if(to_vert!=-1) cur_dist += GetNearestVertexOnPath(p_mesh, to_vert, mesh_path, nearest_vert);
			if(cur_dist < min_dist) {
				min_dist = cur_dist;
				com_edge = common_edges[k];
			}
		}
		return com_edge;
	}
} 


