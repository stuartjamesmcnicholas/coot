/* coot-utils/coot-map-utils.hh
 * 
 * Copyright 2004, 2005, 2006 The University of York
 * Author: Paul Emsley
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.,  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef COOT_MAP_UTILS_HH
#define COOT_MAP_UTILS_HH

#include "clipper/core/coords.h"
#include "clipper/core/xmap.h"
#include "clipper/core/hkl_data.h"
#include "mmdb_manager.h"

namespace coot {

   namespace util { 

      clipper::RTop_orth make_rtop_orth_from(mat44 mat);
      
      float density_at_point(const clipper::Xmap<float> &map_in,
			     const clipper::Coord_orth &co);

      float density_at_map_point(const clipper::Xmap<float> &map_in,
				 const clipper::Coord_map &cg);

      class density_stats_info_t {
      public:
	 float n;
	 float sum_sq; // sum of squared elements
	 float sum;
	 float sum_weight;
	 density_stats_info_t() {
	    n = 0.0;
	    sum = 0.0;
	    sum_sq = 0.0;
	    sum_weight = 0.0;
	 }
	 void add(float v) {
	    n += 1.0;
	    sum += v;
	    sum_sq += v*v;
	    sum_weight += 1.0;
	 } 
	 void add(float v, float weight) {
	    n += weight;
	    sum += weight*v;
	    sum_sq += weight*v*v;
	    sum_weight += 1.0;
	 }
	 std::pair<float, float> mean_and_variance() const {
	    float mean = -1;
	    float var  = -1;
	    if (n > 0) {
	       mean = sum/sum_weight;
	       var = sum_sq/sum_weight - mean*mean;
	    }
	    return std::pair<float, float> (mean, var);
	 }
      };

      // return a variance of -1 on error.
      std::pair<float, float> mean_and_variance(const clipper::Xmap<float> &xmap);

      density_stats_info_t density_around_point(const clipper::Coord_orth &point,
						const clipper::Xmap<float> &xmap,
						float d);

      // This is a console/testing function.  Should not be used in a
      // real graphics program.  Use instead import_map_from() with a
      // precalculated map.
      //
      void map_fill_from_mtz(clipper::Xmap<float> *xmap,
			     std::string mtz_file_name,
			     std::string f_col,
			     std::string phi_col,
			     std::string weight_col,
			     short int use_weights,
			     short int is_diff_map);

      void map_fill_from_mtz(clipper::Xmap<float> *xmap,
			     std::string mtz_file_name,
			     std::string f_col,
			     std::string phi_col,
			     std::string weight_col,
			     short int use_weights,
			     short int is_diff_map,
			     float reso_limit_high,
			     short int use_reso_limit_high);

      // needed by above:
      void filter_by_resolution(clipper::HKL_data< clipper::datatypes::F_phi<float> > *fphidata,
				const float &reso_low,
				const float &reso_high);


      // return the max gridding of the map, e.g. 0.5 for a 1A map
      float max_gridding(const clipper::Xmap<float> &xmap); 


      // The sum of the density at the atom centres, optionally
      // weighted by atomic number.
      // 
      float map_score(PPCAtom atom_selection,
		      int n_selected_atoms,
		      const clipper::Xmap<float> &xmap,
		      short int with_atomic_weighting);
      

      float map_score_atom(CAtom *atom,
			   const clipper::Xmap<float> &xmap);
      
      clipper::Xmap<float> transform_map(const clipper::Xmap<float> &xmap_in,
					 const clipper::RTop_orth &rtop,
					 const clipper::Coord_orth &about_pt,
					 float box_size);

      clipper::Xmap<float> lapacian_transform(const clipper::Xmap<float> &xmap_in);

   }
}

#endif // COOT_MAP_UTILS_HH

