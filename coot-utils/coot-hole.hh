/*
 * coot-utils/coot-hole.hh
 *
 * Copyright 2011 by University of York
 * Author: Paul Emsley
 *
 * This file is part of Coot
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copies of the GNU General Public License and
 * the GNU Lesser General Public License along with this program; if not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street,
 * Fifth Floor, Boston, MA, 02110-1301, USA.
 * See http://www.gnu.org/licenses/
 *
 */

#include <mmdb2/mmdb_manager.h>
#include "clipper/core/coords.h"
#include "clipper/core/xmap.h"
#include "clipper/core/nxmap.h"

#include "utils/coot-utils.hh"
#include "geometry/protein-geometry.hh"

namespace coot {

   class hole_surface_point_t {
   public:
      clipper::Coord_orth position;
      clipper::Coord_orth normal;
      colour_holder colour;
      hole_surface_point_t(const clipper::Coord_orth &p,
			   const clipper::Coord_orth &n,
			   const coot::colour_holder &c) {
	 position = p;
	 normal   = n;
	 colour   = c;
      }
   };

   class hole {
      mmdb::Manager *mol;
      int radius_handle; // set by assign_vdw_radii;
      clipper::Coord_orth from_pt;
      clipper::Coord_orth to_pt;
      clipper::Coord_orth v_hat;
      float colour_map_multiplier;
      float colour_map_offset;
      
      void make_atom_selection(int selhnd, const clipper::Coord_orth &pt,
			       double radius_prev) const;
      std::pair<clipper::Coord_orth, double>
      optimize_point(const clipper::Coord_orth &pt, int selhnd);
      double sphere_size(const clipper::Coord_orth &pt, int selhnd) const;
      void assign_vdw_radii(const coot::protein_geometry &geom);
      void debug_atom_radii() const;
      void write_probe_path(const std::vector<std::pair<clipper::Coord_orth, double> > &probe_path) const;
      void write_probe_path_using_spheres(const std::vector<coot::hole_surface_point_t> &surface_points,
					  const std::string &file_name) const;
      std::vector<hole_surface_point_t> 
      get_surface_points(const std::vector<std::pair<clipper::Coord_orth, double> > &probe_path) const;
      
      colour_holder probe_size_to_colour(double radius) const {
	 // fract is between 0->0.66, clamped
	 float max_radius = 5.0;
	 float min_radius = 0.5;
	 float blue_frac = 0.66; // max on colour wheel (don't want to go to purple).
	 float fract = blue_frac * ((radius - min_radius)/(max_radius - min_radius) * colour_map_multiplier +
				    colour_map_offset);
	 if (fract < 0.0)
	    fract = 0;
	 if (fract > blue_frac)
	    fract = blue_frac;

	 std::vector<float> hsv(3);
	 hsv[0] = fract;
	 hsv[1] = 0.74;
	 hsv[2] = 0.8;
	 colour_holder c = coot::hsv_to_colour(hsv);

	 // make blues more pastel so they don't disappear on the (projector) screen
	 c.red   += c.blue*0.3;
	 c.green += c.blue*0.3;
	 if (c.red > 1.0)
	    c.red = 1.0;
	 if (c.green > 1.0)
	    c.green = 1.0;
	 return c;
      }

      // for generating a map, return (min_x, min_y_min_z), (max_x,
      // max_y, max_z) for the points in the path
      // 
      std::pair<clipper::Coord_orth, clipper::Coord_orth>
      get_min_and_max(const std::vector<std::pair<clipper::Coord_orth, double> > &probe_path) const;

      void mask_around_coord(const clipper::Coord_orth &co, float atom_radius,
			     clipper::Xmap<float> *xmap) const;

   public:
      hole(mmdb::Manager *mol, 
	   const clipper::Coord_orth &from_pt,
	   const clipper::Coord_orth &to_pt,
	   const coot::protein_geometry &geom_in);

      // return the probe path (position and probe size) and the
      // surface point generated by the path.
      // 
      std::pair<std::vector<std::pair<clipper::Coord_orth, double> >, std::vector<hole_surface_point_t> >
      generate();
      void set_colour_shift(float colour_map_multiplier_in, float colour_map_offset_in) {
	 colour_map_multiplier = colour_map_multiplier_in;
	 colour_map_offset = colour_map_offset_in;
      }
      clipper::NXmap<float> carve_a_map(const std::vector<std::pair<clipper::Coord_orth, double> > &probe_path,
					const std::string &file_name) const;
      clipper::Xmap<float> carve_a_map(const std::vector<std::pair<clipper::Coord_orth, double> > &probe_path,
				       const clipper::Xmap<float> &xmap_ref,
				       const std::string &file_name) const;
      
   };
}
     
