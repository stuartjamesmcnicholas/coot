/* src/c-interface-ligands.cc
 *
 * Copyright 2002, 2003, 2004, 2005, 2006, 2007 The University of York
 * Author: Paul Emsley
 * Copyright 2008, 2009 The University of Oxford
 * Copyright 2013, 2014 by Medical Research Council
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#if defined (USE_PYTHON)
#include "Python.h"  // before system includes to stop "POSIX_C_SOURCE" redefined problems
#endif

#ifdef USE_SQLITE3
#include <sqlite3.h>
#endif // USE_SQLITE

#include "compat/coot-sysdep.h"


#include <stdlib.h>
#include <iostream>
#include <stdexcept>

#if defined _MSC_VER
#include <windows.h>
#endif

#include "lbg/lbg.hh" // it matters where this is - rdkit issues...

#include "globjects.h" //includes gtk/gtk.h

#include <vector>
#include <string>

#include <mmdb2/mmdb_manager.h>
#include "coords/mmdb-extras.h"
#include "coords/mmdb.h"
#include "coords/mmdb-crystal.h"

#include "graphics-info.h"
#include "c-interface.h"
#include "c-interface-gtk-widgets.h"
#include "cc-interface.hh"
#include "cc-interface-scripting.hh"
#include "coot-utils/coot-coord-utils.hh"
#include "coot-utils/peak-search.hh"

#include "ligand/wligand.hh"

#include "c-interface-ligands.hh"
#include "c-interface-ligands-swig.hh"
#include "c-interface-ligand-search.hh"
#include "widget-headers.hh"

#include "widget-from-builder.hh"

#include "guile-fixups.h"

void clear_out_container(GtkWidget *vbox);  // in c-interface.cc


/* in here we check if libcheck is available (if scripting is available) */
GtkWidget *wrapped_create_get_monomer_dialog() {

   // GtkWidget *w = create_libcheck_monomer_dialog();
   GtkWidget *w = widget_from_builder("get_monomer_dialog");
   return w;
}

/*  ----------------------------------------------------------------------- */
/*                  ligand fitting stuff                                    */
/*  ----------------------------------------------------------------------- */

// We need to look up the vboxes and add items, like we did in code in
// gtk-manual.c
//
// This is called by callback.c's find ligand button clicked callback.
//
// Return the state.  If there is not ligands, protein or map, then
// return 0.  This is checked by the calling function (in callbacks.c)
// and the dialog is not displayed if 0 is returned.
//
int fill_ligands_dialog(GtkWidget *find_ligand_dialog) {

   int ifound_map, ifound_coords, ifound_ligand;
   short int diff_maps_only_flag = 0;
   graphics_info_t g;

   GtkWidget *find_ligand_ligands_vbox = widget_from_builder("find_ligand_ligands_vbox");
   GtkWidget *find_ligand_protein_vbox = widget_from_builder("find_ligand_protein_vbox");
   GtkWidget *find_ligand_map_vbox     = widget_from_builder("find_ligand_map_vbox");

   clear_out_container(find_ligand_map_vbox);
   clear_out_container(find_ligand_protein_vbox);
   clear_out_container(find_ligand_ligands_vbox);

   ifound_map = fill_ligands_dialog_map_bits(find_ligand_dialog, diff_maps_only_flag);
   if (ifound_map == 0) {
      std::cout << "WARNING:: you must have a map to search for ligands!"
		<< std::endl;
      std::string s("WARNING:: you must have a map to\n search for ligands!");
      GtkWidget *w = wrapped_nothing_bad_dialog(s);
      gtk_widget_show(w);
   }
   ifound_coords = fill_ligands_dialog_protein_bits(find_ligand_dialog);
   if (ifound_coords == 0) {
      std::cout << "Error: you must have a protein to mask the map!"
		<< std::endl;
      std::string s("WARNING:: you must have a protein\n to mask the map");
      GtkWidget *w = wrapped_nothing_bad_dialog(s);
      gtk_widget_show(w);
   }
   ifound_ligand = fill_ligands_dialog_ligands_bits(find_ligand_dialog);
   if (ifound_ligand == 0) {
      std::cout << "Error: you must have at least one  ligand to search for!"
		<< std::endl;
      std::string s("WARNING:: you must have at least one\n          ligand to search for!\n");
      s += "         Ligands have less than ";
      s += coot::util::int_to_string(graphics_info_t::find_ligand_ligand_atom_limit);
      s += " atoms\n";
      GtkWidget *w = wrapped_nothing_bad_dialog(s);
      gtk_widget_show(w);
   }

   // The mask waters toggle buttons:

   GtkWidget *togglebutton = widget_from_builder("find_ligand_mask_waters_yes_radiobutton");
   if (g.find_ligand_mask_waters_flag)
      gtk_check_button_set_active(GTK_CHECK_BUTTON(togglebutton), TRUE);
   else
      gtk_check_button_set_active(GTK_CHECK_BUTTON(togglebutton), FALSE);


   // The Search/Here toggle buttons:
   //
   GtkWidget *search_here_check_button;
   search_here_check_button = widget_from_builder("find_ligands_search_here_radiobutton");
   if (search_here_check_button) {
      if (graphics_info_t::find_ligand_here_cluster_flag)
	 gtk_check_button_set_active(GTK_CHECK_BUTTON(search_here_check_button), TRUE);
   } else {
      std::cout << "ERROR no search here check button" << std::endl;
   }

   fill_ligands_sigma_level_entry();

   // multi-solution check button
   //
   GtkWidget *multi_solution_check_button = widget_from_builder("find_ligand_multi_solution_checkbutton");
   if (multi_solution_check_button) {

      if (g.find_ligand_multiple_solutions_per_cluster_flag) {

	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(multi_solution_check_button), TRUE);

	 // g.find_ligand_score_by_correl_frac_limit
	 // g.find_ligand_score_correl_frac_interesting_limit
	 //
	 GtkWidget *entry_1 = widget_from_builder("find_ligand_multi_solution_entry_1");
	 GtkWidget *entry_2 = widget_from_builder("find_ligand_multi_solution_entry_2");
	 if (entry_1) {
	    gtk_editable_set_text(GTK_EDITABLE(entry_1),
                                  coot::util::float_to_string(g.find_ligand_score_by_correl_frac_limit).c_str());
	 }
	 if (entry_2) {
	    gtk_editable_set_text(GTK_EDITABLE(entry_2),
                                  coot::util::float_to_string(g.find_ligand_score_correl_frac_interesting_limit).c_str());
	 }
      }
   }

   // expert options
   fill_ligands_expert_options();
   // shall we see the expert option frame?

   // 20140907 Yes. We always see the ligand expert frame now.
   //          never hide it.
   // if (graphics_info_t::ligand_expert_flag == 0) {
   if (false) {
      GtkWidget *frame = widget_from_builder("ligand_expert_frame");
      gtk_widget_hide(frame);
   }

   return ifound_ligand * ifound_map * ifound_coords;

   // 050924 New "Expert Options" entries:

}

// 110204: fill new sigma level entry
//
void fill_ligands_sigma_level_entry() {

   GtkWidget *entry = widget_from_builder("find_ligand_sigma_level_entry");
   graphics_info_t g;
   gtk_editable_set_text(GTK_EDITABLE(entry), g.float_to_string(g.ligand_cluster_sigma_level).c_str());

}

void fill_ligands_expert_options() {

   GtkWidget *entry = widget_from_builder("ligand_n_samples_entry");
   graphics_info_t g;
   gtk_editable_set_text(GTK_EDITABLE(entry), g.int_to_string(g.ligand_wiggly_ligand_n_samples).c_str());

   entry = widget_from_builder("ligand_n_top_ligands_entry");
   gtk_editable_set_text(GTK_EDITABLE(entry), g.int_to_string(g.find_ligand_n_top_ligands).c_str());

}


int fill_ligands_dialog_map_bits(GtkWidget *find_ligand_dialog,
				 short int diff_maps_only_flag) {

   return fill_ligands_dialog_map_bits_by_dialog_name(find_ligand_dialog,
						      "find_ligand_map",
						      diff_maps_only_flag);

}

// dialog_name is typically find_ligand_map
//
int fill_ligands_dialog_map_bits_by_dialog_name(GtkWidget *find_ligand_dialog,
						const char *dialog_name,
						short int diff_maps_only_flag) {

   // this funny passing of the dialog name is used beause I want to use the function
   // for difference map peaks as well as ligands

   int ifound = 0;
   graphics_info_t g;
   // Add map elements:
   GSList *find_ligand_map_group = NULL;
   //

   // e.g. can be "generate_diff_map_peaks_map", + _"vbox"
   std::string vbox_name = dialog_name;
   vbox_name += "_vbox";

   GtkWidget *find_ligand_map_vbox = widget_from_builder(vbox_name.c_str());

   clear_out_container(find_ligand_map_vbox);

   std::cout << "DEBUG:: in fill_ligands_dialog_map_bits_by_dialog_name() found find_ligand_map_vbox "
             << find_ligand_map_vbox << std::endl;

   if (find_ligand_map_vbox == NULL) {
      std::cout << "ERROR:: disaster! find_ligand map vbox not found " << std::endl;
   } else {
      for (int imol=0; imol<g.n_molecules(); imol++) {
	 if (g.molecules[imol].has_xmap()) {

	    if ((!diff_maps_only_flag) ||
		g.molecules[imol].is_difference_map_p()) {

	       ifound++; // there was a map
	       std::string map_str(dialog_name);
	       map_str += "_radiobutton_";
	       map_str += g.int_to_string(imol);
	       std::string map_button_label = g.int_to_string(imol);
	       map_button_label += " ";
	       map_button_label += g.molecules[imol].name_;

#if (GTK_MAJOR_VERSION >= 4)
               // 20220602-PE FIXME radio buttons
               std::cout << "in fill_ligands_dialog_map_bits_by_dialog_name() FIXME new radiobuttons - needs group" << std::endl;
	       GtkWidget *find_ligand_map_radiobutton_imol = gtk_check_button_new_with_label(map_button_label.c_str());
#else
	       GtkWidget *find_ligand_map_radiobutton_imol = gtk_radio_button_new_with_label(find_ligand_map_group, map_button_label.c_str());
	       find_ligand_map_group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(find_ligand_map_radiobutton_imol));
               g_object_set_data(G_OBJECT(find_ligand_map_radiobutton_imol), "imol", GINT_TO_POINTER(imol));
	       g_signal_connect(G_OBJECT(find_ligand_map_radiobutton_imol), "toggled",
				G_CALLBACK(on_find_ligand_map_radiobutton_imol_toggled),
				GINT_TO_POINTER(imol));
	       gtk_widget_show(find_ligand_map_radiobutton_imol);
#endif


#if (GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION == 94) || (GTK_MAJOR_VERSION == 4)
 	       gtk_box_append(GTK_BOX(find_ligand_map_vbox), find_ligand_map_radiobutton_imol);
#else
	       gtk_box_pack_start (GTK_BOX (find_ligand_map_vbox),
				   find_ligand_map_radiobutton_imol, FALSE, FALSE, 0);
#endif
	    }
	 }
      }
   }
   return ifound;
}

void
on_find_ligand_map_radiobutton_imol_toggled(GtkToggleButton *togglebutton,
					    gpointer         user_data) {

   int imol = GPOINTER_TO_INT(user_data);
   if (gtk_toggle_button_get_active(togglebutton)) {
      std::cout << "imol " << imol << " active "<< std::endl;
      GtkWidget *w = widget_from_builder("find_ligand_sigma_level_entry");
      if (w) {
	 if (map_is_difference_map(imol)) {
	    gtk_editable_set_text(GTK_EDITABLE(w), "3.0");
	 } else {
	    gtk_editable_set_text(GTK_EDITABLE(w), "1.0");
	 }
      }
   }
}


int fill_ligands_dialog_protein_bits(GtkWidget *find_ligand_dialog) {

   return fill_ligands_dialog_protein_bits_by_dialog_name(find_ligand_dialog,
							  "find_ligand_protein");

}


int fill_ligands_dialog_protein_bits_by_dialog_name(GtkWidget *find_ligand_dialog,
						    const char *dialog_name) {


   int ifound = 0;
   graphics_info_t g;
   // Add protein elements:
   GSList *find_ligand_protein_group = NULL;
   //
   std::string vbox_name(dialog_name);
   vbox_name += "_vbox";

   GtkWidget *find_ligand_protein_vbox = widget_from_builder(vbox_name.c_str()); // 20220309-PE is this the right name?
   if (find_ligand_protein_vbox == NULL) {
      std::cout << "disaster! find_ligand protein vbox not found " << std::endl;
   } else {
      clear_out_container(find_ligand_protein_vbox);
      for (int imol=0; imol<g.n_molecules(); imol++) {
	 if (g.molecules[imol].atom_sel.n_selected_atoms) {
	    //
	    ifound = 1; // there was a protein
	    std::string protein_str(dialog_name);
	    protein_str += "_radiobutton_";
	    protein_str += g.int_to_string(imol);
	    std::string protein_button_label = g.int_to_string(imol);
	    protein_button_label += " ";
	    protein_button_label += g.molecules[imol].name_;
#if (GTK_MAJOR_VERSION == 4)
            // 20220602-PE FIXME radio buttons
            std::cout << "in fill_ligands_dialog_protein_bits_by_dialog_name() FIXME radiobuttons " << std::endl;
	    GtkWidget *find_ligand_protein_radiobutton_imol =
	       gtk_check_button_new_with_label(protein_button_label.c_str());
	    gtk_box_append(GTK_BOX (find_ligand_protein_vbox), find_ligand_protein_radiobutton_imol);
#else
	    GtkWidget *find_ligand_protein_radiobutton_imol =
	       gtk_radio_button_new_with_label (find_ligand_protein_group, protein_button_label.c_str());
	    find_ligand_protein_group =
	       gtk_radio_button_get_group(GTK_RADIO_BUTTON(find_ligand_protein_radiobutton_imol));
	    // gtk_widget_ref (find_ligand_protein_radiobutton_imol);
	    //                g_object_set_data_full (G_OBJECT (find_ligand_dialog),
            //                protein_str.c_str(),
            //                find_ligand_protein_radiobutton_imol,
            //                (GDestroyNotify) NULL);
            g_object_set_data(G_OBJECT(find_ligand_protein_radiobutton_imol), "imol", GINT_TO_POINTER(imol));
	    gtk_widget_show (find_ligand_protein_radiobutton_imol);
	    gtk_box_pack_start (GTK_BOX (find_ligand_protein_vbox),
				find_ligand_protein_radiobutton_imol, FALSE, FALSE, 0);
#endif
	 }
      }
   }
   return ifound;
}

int fill_vbox_with_coords_options_by_dialog_name(GtkWidget *find_ligand_dialog,
						 const char *dialog_name,
						 short int have_ncs_flag) {


   int ifound = 0;
   graphics_info_t g;
   // Add protein elements:
   GSList *find_ligand_protein_group = NULL;
   //
   std::string vbox_name(dialog_name);
   vbox_name += "_vbox";

   GtkWidget *find_ligand_protein_vbox = widget_from_builder(vbox_name.c_str());

   if (find_ligand_protein_vbox == NULL) {
      std::cout << "disaster! fill_vbox_with_coords_options_by_dialog_name coords"
		<< " vbox not found " << std::endl;
   } else {
      for (int imol=0; imol<g.n_molecules(); imol++) {
	 if (g.molecules[imol].has_model()) {

	    if (!have_ncs_flag || g.molecules[imol].has_ncs_p()) {
	       //
	       ifound = 1; // there was a protein
	       std::string protein_str(dialog_name);
	       protein_str += "_radiobutton_";
	       protein_str += g.int_to_string(imol);
	       std::string protein_button_label = g.int_to_string(imol);
	       protein_button_label += " ";
	       protein_button_label += g.molecules[imol].name_;

#if (GTK_MAJOR_VERSION >= 4)
               // 20220528-PE FIXME radio buttons and box packing
#else
	       GtkWidget *find_ligand_protein_radiobutton_imol =
		  gtk_radio_button_new_with_label (find_ligand_protein_group,
						   protein_button_label.c_str());
	       find_ligand_protein_group =
		  gtk_radio_button_get_group (GTK_RADIO_BUTTON
					  (find_ligand_protein_radiobutton_imol));
	       // gtk_widget_ref (find_ligand_protein_radiobutton_imol);
	       g_object_set_data_full (G_OBJECT (find_ligand_dialog),
					 protein_str.c_str(),
					 find_ligand_protein_radiobutton_imol,
					 (GDestroyNotify) NULL);
	       gtk_widget_show (find_ligand_protein_radiobutton_imol);
	       gtk_box_pack_start (GTK_BOX (find_ligand_protein_vbox),
				   find_ligand_protein_radiobutton_imol, FALSE, FALSE, 0);
#endif
	    }
	 }
      }
   }
   return ifound;
}


int fill_ligands_dialog_ligands_bits(GtkWidget *find_ligand_dialog) {

   int ifound = 0;
   graphics_info_t g;
   // Add ligand elements
   //
   GtkWidget *hbox;

   GtkWidget *find_ligand_ligands_vbox = widget_from_builder("find_ligand_ligands_vbox");
   if (find_ligand_ligands_vbox == NULL) {
      std::cout << "disaster! find_ligand protein vbox not found " << std::endl;
   } else {
      for (int imol=0; imol<g.n_molecules(); imol++) {
	 if ((g.molecules[imol].atom_sel.n_selected_atoms < graphics_info_t::find_ligand_ligand_atom_limit) &&
	     g.molecules[imol].has_model()) {
	    ifound = 1; // there was a ligand

	    // create an hbox:
	    // hbox = gtk_hbox_new (FALSE, 0);
	    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	    std::string ligands_str("find_ligand_ligand_checkbutton_");
	    ligands_str += g.int_to_string(imol);
	    std::string ligands_button_label = g.int_to_string(imol);
	    ligands_button_label += " ";
	    ligands_button_label += g.molecules[imol].name_;

	    // flexible/conformer-generation on/off check button
	    //
	    std::string wligands_str("find_ligand_wligand_checkbutton_");
	    wligands_str += g.int_to_string(imol);
	    GtkWidget *find_ligand_wligands_checkbutton_imol = gtk_check_button_new_with_label("Flexible?");

	    // gtk_widget_ref (find_ligand_wligands_checkbutton_imol);
	    // g_object_set_data_full (G_OBJECT (find_ligand_dialog),
            // wligands_str.c_str(),
            // find_ligand_wligands_checkbutton_imol,
            // (GDestroyNotify) NULL);
            // g_object_set_data(G_OBJECT(find_ligand_wligands_checkbutton_imol), "wiggly", 0);

	    // ligand molecule on/off check button
	    //
	    GtkWidget *find_ligand_ligands_checkbutton_imol = gtk_check_button_new_with_label (ligands_button_label.c_str());

	    // gtk_widget_ref (find_ligand_ligands_checkbutton_imol);
	    // g_object_set_data_full(G_OBJECT (find_ligand_dialog),
            // ligands_str.c_str(),
            // find_ligand_ligands_checkbutton_imol,
            // (GDestroyNotify) NULL);

            g_object_set_data(G_OBJECT(find_ligand_ligands_checkbutton_imol), "imol", GINT_TO_POINTER(imol));

	    gtk_widget_show (find_ligand_wligands_checkbutton_imol);
	    gtk_widget_show (find_ligand_ligands_checkbutton_imol);

#if (GTK_MAJOR_VERSION == 4)
	    gtk_box_append(GTK_BOX(hbox), find_ligand_wligands_checkbutton_imol);
	    gtk_box_append(GTK_BOX(hbox), find_ligand_ligands_checkbutton_imol);
	    // pack the hbox into the ligands vbox
	    gtk_box_append(GTK_BOX(find_ligand_ligands_vbox), hbox);

#else
	    gtk_box_pack_start (GTK_BOX(hbox), find_ligand_wligands_checkbutton_imol, FALSE, FALSE, 0);
	    gtk_box_pack_start (GTK_BOX(hbox), find_ligand_ligands_checkbutton_imol, FALSE, FALSE, 0);
	    // pack the hbox into the ligands vbox
	    gtk_box_pack_start (GTK_BOX(find_ligand_ligands_vbox), hbox, FALSE, FALSE, 0);
#endif
	    gtk_widget_show(hbox);

	 }
      }
   }
   return ifound;
}


// this can't be caputured for a gtk lambda function (capturing is not allowed)
//
// for items in the ligands vbox (which are hboxes) fill data.
//
std::pair<int, bool>
execute_get_mols_ligand_search_get_item_pair(GtkWidget *ligand_item_hbox) {

   std::pair<int, bool> ligand_mol_info(-1, false);

#if (GTK_MAJOR_VERSION >= 4)

   // 20220602-PE FIXME container children
   std::cout << "in execute_get_mols_ligand_search_get_item_pair() FIXME container children" << std::endl;

#else
   
   if (GTK_IS_CONTAINER(ligand_item_hbox)) {

      GList *dlist = gtk_container_get_children(GTK_CONTAINER(ligand_item_hbox));
      GList *free_list = dlist;

      int count = 0;
      while (dlist) {
         count += 1;
         dlist = dlist->next;
      }

      count = 0;
      dlist = gtk_container_get_children(GTK_CONTAINER(ligand_item_hbox));
      while (dlist) {
         GtkWidget *list_item = GTK_WIDGET(dlist->data);

         if (count == 0) {
            // the wiggly checkbutton
            if (GTK_IS_TOGGLE_BUTTON(list_item)) {
               if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(list_item))) {
                  ligand_mol_info.second = true;
               }
            }
         }
         if (count == 1) {
            // the ligand molecule checkbutton
            if (GTK_IS_TOGGLE_BUTTON(list_item)) {
               if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(list_item))) {
                  int imol = -1;
                  int i = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(list_item), "imol"));
                  imol = i;
                  ligand_mol_info.first = imol;
               }
            }
         }

         count += 1;
         dlist = dlist->next;
      }
      g_list_free(free_list);
      
   } else {
      std::cout << "ERROR:: in execute_get_mols_ligand_search_get_item_pair() hbox_item is NOT a container " << ligand_item_hbox << std::endl;
   }

#endif

   return ligand_mol_info;
}


int execute_get_mols_ligand_search(GtkWidget *button) {

   // 20220313-PE new version - in this version, instead
   // of iterating over molecules, I will iterate over items
   // in the vbox, find the one that is active and act on that.

   int n_ligands = 0;
   std::vector<int> chief_ligand_many_atoms; // caches imols with lots
					     // of atoms.
   std::vector<std::pair<int, bool> > wiggly_ligand_info;

   // extract the sigma level and stick it in
   // graphics_info_t::ligand_cluster_sigma_level
   //
   set_ligand_cluster_sigma_level_from_widget(button);
   set_ligand_expert_options_from_widget(button); // ligand_n_top_sites
						  // and wiggly ligand
						  // n (conformer)
						  // samples
   GtkWidget *map_vbox     = widget_from_builder("find_ligand_map_vbox");
   GtkWidget *protein_vbox = widget_from_builder("find_ligand_protein_vbox");
   GtkWidget *ligands_vbox = widget_from_builder("find_ligand_ligands_vbox");

   // void *data is actually a pointer to imol_map (also works for protein molecule too)
   //
   auto get_mol_for_find_ligands = [] (GtkWidget *item, void *data) {
                                      if (GTK_IS_TOGGLE_BUTTON(item)) {
                                         if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(item))) {
                                            int *imol_ptr = static_cast<int *>(data);
                                            int imol = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(item), "imol"));
                                            *imol_ptr = imol;
                                         }
                                      }
                                   };

   auto get_ligands_for_find_ligands = [] (GtkWidget *ligand_item_hbox, void *data) {
                                          // the items in the ligands vbox is a hbox with
                                          // a button for the ligand and another button for the wiggly flag (first)
                                                if (GTK_IS_BOX(ligand_item_hbox)) {
                                                std::cout << "item_cont is a box " << ligand_item_hbox << std::endl;
                                                std::pair<int, bool> item_pair = execute_get_mols_ligand_search_get_item_pair(ligand_item_hbox);
                                                if (item_pair.first != -1) {
                                                   std::vector<std::pair<int, bool> > *ligs_ptr = static_cast<std::vector<std::pair<int, bool> > *>(data);
                                                   ligs_ptr->push_back(item_pair); // phew!
                                                }
                                             }
                                         };

   int imol_map = -1;
   int imol_protein = -1;

#if (GTK_MAJOR_VERSION >= 4)

   std::cout << "in execute_get_mols_ligand_search() FIXME container foreach " << std::endl;
#else
   if (GTK_IS_CONTAINER(map_vbox)) {
      void *imol_ptr = &imol_map;
      gtk_container_foreach(GTK_CONTAINER(map_vbox), get_mol_for_find_ligands, imol_ptr);
   }
   if (GTK_IS_CONTAINER(protein_vbox)) {
      void *imol_ptr = &imol_protein;
      gtk_container_foreach(GTK_CONTAINER(protein_vbox), get_mol_for_find_ligands, imol_ptr);
   }
   if (GTK_IS_CONTAINER(ligands_vbox)) {
      void *data = static_cast<void *>(&wiggly_ligand_info);
      gtk_container_foreach(GTK_CONTAINER(ligands_vbox), get_ligands_for_find_ligands, data);
      n_ligands = wiggly_ligand_info.size();
      if (false)
         for (unsigned int i=0; i<wiggly_ligand_info.size(); i++)
            std::cout << "    " << wiggly_ligand_info[i].first << " wiggly: " << wiggly_ligand_info[i].second << std::endl;
   }
#endif

   graphics_info_t g;

   // multi-solution check button
   //
   GtkWidget *multi_solution_check_button = widget_from_builder("find_ligand_multi_solution_checkbutton");
   if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(multi_solution_check_button))) {
      g.find_ligand_multiple_solutions_per_cluster_flag = true;
   }

   GtkWidget *entry_1 = widget_from_builder("find_ligand_multi_solution_entry_1");
   GtkWidget *entry_2 = widget_from_builder("find_ligand_multi_solution_entry_2");
   if (entry_1) {
      const gchar *e1t = gtk_editable_get_text(GTK_EDITABLE(entry_1));
      if (e1t) {
	 try {
	    float f1 = coot::util::string_to_float(e1t);
	    g.find_ligand_score_by_correl_frac_limit = f1;
	 }
	 catch (const std::exception &e) {
	    std::cout << "WARNING:: failed to convert to number: " << e.what() << std::endl;
	 }
      }
   }
   if (entry_2) {
      const gchar *e2t = gtk_editable_get_text(GTK_EDITABLE(entry_2));
      if (e2t) {
	 try {
	    float f2 = coot::util::string_to_float(e2t);
	    g.find_ligand_score_correl_frac_interesting_limit = f2;
	 }
	 catch (const std::exception &e) {
	    std::cout << "WARNING:: failed to convert to number: " << e.what() << std::endl;
	 }
      }
   }

   if (is_valid_model_molecule(imol_protein) && is_valid_map_molecule(imol_map) && !wiggly_ligand_info.empty()) {

      // So store the clicked info in a graphics_info_t static:
      // because we need this info when we click the OK button of the
      // create_find_ligand_many_atoms_dialog() widget.  We don't want
      // to mess with set_user_data for many data.
      //
      g.set_find_ligands_mols(imol_map, imol_protein, wiggly_ligand_info);

      if (chief_ligand_many_atoms.size() == 0 ) {

         // OK, let's go

	 execute_ligand_search();

      } else {

	 // we need to delete this widget when OK and cancel of the
	 // many atoms widget is pressed, so let's attach it as user
	 // data.
	 // GtkWidget *widget = lookup_widget(button, "find_ligand_dialog");
	 GtkWidget *widget = widget_from_builder("find_ligand_dialog");
         do_find_ligand_many_atoms_in_ligands(widget);

      }
   } else {
	 std::cout << "Something wrong in the selection of map/molecules"
		   << std::endl;
   }
   return n_ligands;

}


/* get which map to search, protein mask and ligands from button and
   then do it*/
// Recall that the map and the protein are radio buttons (i.e. we want
// exactly one of each).  Hoewever, the ligands are check buttons, we
// can have as many of those on that we like.
//
// We should add a check to the install_ligand that checks the number
// of atoms in the ligand and gives a warning... hmmm... that invovles
// another popup-sigh...  Oh well, plan for it, even if you don't
// implement it now - i.e. invoke a post-check function that creates
// the coot::ligand object.
//
int execute_get_mols_ligand_search_old(GtkWidget *button) {

   graphics_info_t g;
   GtkWidget *ligand_button;
   std::vector<int> chief_ligand_many_atoms; // caches imols with lots
					     // of atoms.
   std::vector<std::pair<int, bool> > wiggly_ligand_info;
   int n_ligands = 0;

   // extract the sigma level and stick it in
   // graphics_info_t::ligand_cluster_sigma_level
   //
   set_ligand_cluster_sigma_level_from_widget(button);
   set_ligand_expert_options_from_widget(button); // ligand_n_top_sites
						  // and wiggly ligand
						  // n (conformer)
						  // samples

   // flags to say if we have found things:
   short int found_active_button_for_map = 0;
   short int found_active_button_for_protein = 0;
   short int found_active_button_for_ligands = 0;
   // This is where we store the mols:
   int find_ligand_map_mol = -1; // gets set?
   int find_ligand_protein_mol = -1; // gets set.
   // std::vector<int> find_ligand_ligand_mols;  old.

   // Find the first active map radiobutton
   found_active_button_for_map = 0;
   for (int imol=0; imol<g.n_molecules(); imol++) {
      if (g.molecules[imol].has_xmap()) {
	 std::string map_str = "find_ligand_map_radiobutton_";
	 map_str += g.int_to_string(imol);
         // ligand_button = lookup_widget(button, map_str.c_str());
         ligand_button = 0; // 20220309-PE FIXME set tne name in the caller
	 if (ligand_button) {
	    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ligand_button))) {
	       find_ligand_map_mol = imol;
	       found_active_button_for_map = 1;
	       break;
	    }
	 } else {
	    std::cout << map_str << " widget not found in "
		      << "execute_get_mols_ligand_search" << std::endl;
	 }
      }
   }

   // Find the first active protein radiobutton
   found_active_button_for_protein = 0;
   for (int imol=0; imol<g.n_molecules(); imol++) {
      if (g.molecules[imol].atom_sel.n_selected_atoms > 0) {
	 std::string protein_str = "find_ligand_protein_radiobutton_";
	 protein_str += g.int_to_string(imol);
	 // ligand_button = lookup_widget(button, protein_str.c_str());
	 ligand_button = 0;  // 20220309-PE FIXME set tne name in the caller
	 if (ligand_button) {
	    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ligand_button))) {
	       find_ligand_protein_mol = imol;
	       found_active_button_for_protein = 1;
	       break;
	    }
	 } else {
	    std::cout << protein_str << " widget not found in "
		      << "execute_get_mols_ligand_search" << std::endl;
	 }
      }
   }

   // Now, do we mask waters for the protein mask?

   GtkWidget *togglebutton = widget_from_builder("find_ligand_mask_waters_yes_radiobutton");
   if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togglebutton)))
      graphics_info_t::find_ligand_mask_waters_flag = 1;
   else
      graphics_info_t::find_ligand_mask_waters_flag = 0;


   // The Search/Here toggle buttons:

   GtkWidget *search_here_toggle_button = widget_from_builder("find_ligands_search_here_radiobutton");
   if (search_here_toggle_button) {
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(search_here_toggle_button))) {
	 std::cout << " Activating SEARCH HERE in ligand fitting" << std::endl;
	 graphics_info_t::find_ligand_here_cluster_flag = 1;
      } else {
	 graphics_info_t::find_ligand_here_cluster_flag = 0;
      }
   }


   // For each imol in molecules, if we have selected coordinates,
   // construct a string begining "find_ligand_ligands_imol_" then the
   // molecule number

   found_active_button_for_ligands = 0;
   for (int imol=0; imol<g.n_molecules(); imol++) {
      if (g.molecules[imol].has_model() &&
	  g.molecules[imol].atom_sel.n_selected_atoms < graphics_info_t::find_ligand_ligand_atom_limit) {
	 std::string ligand_str = "find_ligand_ligand_checkbutton_";
	 ligand_str += g.int_to_string(imol);
	 // ligand_button = lookup_widget(button, ligand_str.c_str());
	 ligand_button = 0;

	 std::string wiggly_str = "find_ligand_wligand_checkbutton_";
	 wiggly_str += g.int_to_string(imol);
	 // GtkWidget *wiggly_button = lookup_widget(button, wiggly_str.c_str());
	 GtkWidget *wiggly_button = 0;

	 if (ligand_button && wiggly_button) {
	    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ligand_button))) {

	       bool wiggly_state = 0;
	       if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wiggly_button)))
		  wiggly_state = 1;
	       wiggly_ligand_info.push_back(std::pair<int, bool> (imol, wiggly_state));
	       found_active_button_for_ligands = 1;
	       int n_atoms = g.molecules[imol].atom_sel.n_selected_atoms;
	       if (n_atoms > 200) {
		  std::cout << "WARNING:: molecule " << imol
			    << " has unexpectedly many atoms ("
			    << n_atoms << ")" << std::endl;
		  chief_ligand_many_atoms.push_back(imol);
	       }
	    }
	 } else {
	    std::cout << ligand_str << " widget not found in "
		      << "execute_get_mols_ligand_search" << std::endl;
	 }
      }
   }

   // multi-solution check button
   //
   GtkWidget *multi_solution_check_button = widget_from_builder("find_ligand_multi_solution_checkbutton");
   if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(multi_solution_check_button))) {
      g.find_ligand_multiple_solutions_per_cluster_flag = true;
   }

   GtkWidget *entry_1 = widget_from_builder("find_ligand_multi_solution_entry_1");
   GtkWidget *entry_2 = widget_from_builder("find_ligand_multi_solution_entry_2");
   if (entry_1) {
      const gchar *e1t = gtk_editable_get_text(GTK_EDITABLE(entry_1));
      if (e1t) {
	 try {
	    float f1 = coot::util::string_to_float(e1t);
	    g.find_ligand_score_by_correl_frac_limit = f1;
	 }
	 catch (const std::exception &e) {
	    std::cout << "WARNING:: failed to convert to number: " << e.what() << std::endl;
	 }
      }
   }
   if (entry_2) {
      const gchar *e2t = gtk_editable_get_text(GTK_EDITABLE(entry_2));
      if (e2t) {
	 try {
	    float f2 = coot::util::string_to_float(e2t);
	    g.find_ligand_score_correl_frac_interesting_limit = f2;
	 }
	 catch (const std::exception &e) {
	    std::cout << "WARNING:: failed to convert to number: " << e.what() << std::endl;
	 }
      }
   }

   if ( found_active_button_for_map &&
	found_active_button_for_protein &&
	found_active_button_for_ligands) {

      // So store the clicked info in a graphics_info_t static:
      // because we need this info when we click the OK button of the
      // create_find_ligand_many_atoms_dialog() widget.  We don't want
      // to mess with set_user_data for many data.
      //
      g.set_find_ligands_mols(find_ligand_map_mol,
			      find_ligand_protein_mol,
			      wiggly_ligand_info);

      if (chief_ligand_many_atoms.size() == 0 ) {
#if defined USE_GUILE && !defined WINDOWS_MINGW
	 execute_ligand_search();
#else
#ifdef USE_PYTHON
	 execute_ligand_search_py();
#endif // USE_PYTHON
#endif // USE_GUILE
      } else {
	 // we need to delete this widget when OK and cancel of the
	 // many atoms widget is pressed, so let's attach it as user
	 // data.
	 // GtkWidget *widget = lookup_widget(button, "find_ligand_dialog");
	 GtkWidget *widget = widget_from_builder("find_ligand_dialog");
         do_find_ligand_many_atoms_in_ligands(widget);
      }
   } else {
	 std::cout << "Something wrong in the selection of map/molecules"
		   << std::endl;
   }

   return n_ligands;
}

// q_ligands is questionable ligands (i.e. very large)
//
void do_find_ligand_many_atoms_in_ligands(GtkWidget *find_ligand_dialog) {

   // GtkWidget *widget = create_find_ligand_many_atoms_dialog();
   GtkWidget *widget = widget_from_builder("find_ligand_many_atoms_dialog");
   g_object_set_data(G_OBJECT(widget), "find_ligand_dialog", find_ligand_dialog);
   gtk_widget_show(widget);
}

void set_ligand_expert_options_from_widget(GtkWidget *button) {

   // GtkWidget *entry = lookup_widget(button, "ligand_n_samples_entry");
   GtkWidget *entry = widget_from_builder("ligand_n_samples_entry");
   const gchar *text = gtk_editable_get_text(GTK_EDITABLE(entry));
   if (text) {
      int isample = atoi(text);
      if ((isample > 0) && (isample < 1000000))
	 graphics_info_t::ligand_wiggly_ligand_n_samples = isample;
   }
   // entry = lookup_widget(button, "ligand_n_top_ligands_entry");
   entry = widget_from_builder("ligand_n_top_ligands_entry");
   text = gtk_editable_get_text(GTK_EDITABLE(entry));
   if (text) {
      int itop = atoi(text);
      if ((itop > 0) && (itop < 1000000))
	 graphics_info_t::find_ligand_n_top_ligands = itop;
   }
}

void set_ligand_dialog_number_of_sites_sensitivity(GtkWidget *toggle_button) {

   // GtkWidget *hbox = lookup_widget(toggle_button, "find_ligands_dialog_number_of_sites_hbox");
   GtkWidget *hbox = widget_from_builder("find_ligands_dialog_number_of_sites_hbox");
   if (hbox) {
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button))) {
	 gtk_widget_set_sensitive(hbox, FALSE);
      } else {
	 gtk_widget_set_sensitive(hbox, TRUE);
      }
   }
}

void set_ligand_dialog_real_space_refine_sites_checkbutton_state(GtkWidget *toggle_button) {

   if (toggle_button) {
      graphics_info_t g;
      if (g.find_ligand_do_real_space_refine_state())
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_button), TRUE);
      else
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_button), FALSE);
   }
}

void set_find_ligand_do_real_space_refinement(short int state) {
   graphics_info_t g;
   g.set_find_ligand_do_real_space_refine_state(state);

}



/*  extract the sigma level and stick it in */
/*  graphics_info_t::ligand_cluster_sigma_level */
void set_ligand_cluster_sigma_level_from_widget(GtkWidget *button) {

   // GtkWidget *entry = lookup_widget(button, "find_ligand_sigma_level_entry");
   GtkWidget *entry = widget_from_builder("find_ligand_sigma_level_entry");
   short int setit = 0;

   if (entry) {
      const gchar *text = gtk_editable_get_text(GTK_EDITABLE(entry));
      if (text) {
	 float f = atof(text);
	 if (f > 0.0 && f < 1000.0) {
	    graphics_info_t::ligand_cluster_sigma_level = f;
	    setit = 1;
	 }
      }
   }
   if (setit == 0)
      std::cout << "INFO:: ignoring bogus attempt to set "
		<< "the ligand search sigma level"
		<< std::endl;
}



// The name has beend changed because the function we want at the
// scripting layer (start_ligand_builder_gui()) should not need
// arguments.
//
void
start_ligand_builder_gui_internal(GMenuItem     *menuitem,
				  gpointer       user_data) {

      start_ligand_builder_gui();
}

void
start_ligand_builder_gui() {

   if (graphics_info_t::use_graphics_interface_flag) {

#ifdef HAVE_GOOCANVAS
      lig_build::molfile_molecule_t mm;
      mmdb::Manager *mol = NULL;
      std::string molecule_file_name = "coot-lidia.mol"; // non-null file name passed to lbg, used
      // in save function
      std::string view_name;
      std::pair<bool, coot::residue_spec_t> dummy_pair(0, coot::residue_spec_t());
      bool use_graphics_interface_flag = 1;
      bool stand_alone_flag = 0;
      int imol_dummy = -1;

      int (*get_url_func_pointer) (const char *s1, const char *s2) = NULL;
#ifdef USE_LIBCURL
      get_url_func_pointer= coot_get_url;
#endif

      lbg(mm, dummy_pair, mol, view_name, molecule_file_name, imol_dummy,
	  graphics_info_t::Geom_p(),
	  use_graphics_interface_flag, stand_alone_flag,
	  get_url_func_pointer,
	  prodrg_import_function,
	  sbase_import_function,
	  get_drug_mdl_via_wikipedia_and_drugbank
	  );
#else
      std::cout << "No goocanvas" << std::endl;
#endif // HAVE_GOOCANVAS

   }
}


// for "better than the median", percentile_limit should be 0.5 (of course).
#ifdef USE_GUILE
void gui_ligand_metrics_scm(SCM ligand_spec, SCM ligand_metrics, double percentile_limit) {

   if (scm_is_true(scm_list_p(ligand_metrics))) {
      SCM lm_len_scm = scm_length(ligand_metrics);
      int lm_len     = scm_to_int(lm_len_scm);

      if (lm_len == 3) {  // currently 3
	 double d = scm_to_double(scm_list_ref(ligand_metrics, scm_from_int(0)));
	 double m = scm_to_double(scm_list_ref(ligand_metrics, scm_from_int(1)));
	 int n_bumps = scm_to_int(scm_list_ref(ligand_metrics, scm_from_int(2)));
	 // coot::probe_clash_score_t cs =
	 //    probe_clash_score_from_scm(scm_list_ref(ligand_metrics, scm_from_int(2)));
	 coot::probe_clash_score_t pcs(n_bumps, -1, -1, -1, -1);
	 coot::ligand_report_absolute_t lr(d, m, pcs);

	 coot::residue_spec_t spec = residue_spec_from_scm(ligand_spec);

	 coot::ligand_check_dialog(spec, lr, percentile_limit);

      }
   }
}
#endif // USE_GUILE

// for "better than the median", percentile_limit should be 0.5 (of course).
#ifdef USE_PYTHON
void gui_ligand_metrics_py(PyObject *ligand_spec, PyObject *ligand_metrics, double percentile_limit) {

   if (PyList_Check(ligand_metrics)) {
      Py_ssize_t lm_len = PyList_Size(ligand_metrics);

      if (lm_len == 3) {  // currently 3
         double d = PyFloat_AsDouble(PyList_GetItem(ligand_metrics, 0));
         double m = PyFloat_AsDouble(PyList_GetItem(ligand_metrics, 1));
         int n_bumps = PyLong_AsLong(PyList_GetItem(ligand_metrics, 2));
         // coot::probe_clash_score_t cs = probe_clash_score_from_scm(scm_list_ref(ligand_metrics, scm_from_int(2)));
         coot::probe_clash_score_t cs(n_bumps, -1, -1, -1, -1);
         coot::ligand_report_absolute_t lr(d, m, cs);

	 coot::residue_spec_t spec = residue_spec_from_py(ligand_spec);
	 coot::ligand_check_dialog(spec, lr, percentile_limit);
      }
   }

}
#endif // USE_PYTHON

int
coot::ligands_db_sql_callback(void *param, int argc, char **argv, char **azColName) {

   // move all this into
   //
   // ligand_report_percentiles_t::db_percentiles(argc, argv, azColName)

   try {

      coot::ligand_report_percentiles_t *lrp =
	 static_cast<ligand_report_percentiles_t *> (param);

      for(int i=0; i<argc; i++) {
	 // printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	 std::string col_name(azColName[i]);
	 if (col_name == "density_correlation") {
	    std::string v(argv[i]);
	    double d = coot::util::string_to_float(v);
	    lrp->density_correlation_vec.push_back(d);
	 }
	 if (col_name == "mogul_z_worst") {
	    std::string v(argv[i]);
	    double d = coot::util::string_to_float(v);
	    lrp->mogul_z_worst_vec.push_back(d);
	 }
	 if (col_name == "contact_score_clash") {
	    std::string v(argv[i]);
	    int b = coot::util::string_to_int(v);
	    lrp->bad_contacts_vec.push_back(b);
	 }
      }
   }
   catch (const std::runtime_error &rte) {
      std::cout << "problem converting a number" << std::endl;
   }
   // printf("\n");
   return 0;
}


#include <gsl/gsl_sf_erf.h>

coot::ligand_report_percentiles_t
coot::ligand_report_absolute_t::make_percentiles() const {

   coot::ligand_report_percentiles_t lrp(-1, -1, -1);

#ifdef USE_SQLITE3

   std::string pkg_data_dir = coot::package_data_dir();
   std::string ligands_db_file_name = "ligands.db";
   std::string d = coot::util::append_dir_file(pkg_data_dir, "data");
   std::string f = coot::util::append_dir_file(d, ligands_db_file_name);

   // hack, for testing ligands.db in the working directory.
   // f = ligands_db_file_name;

   if (! coot::file_exists(f)) {
      std::cout << "WARNING:: No such file: " << f << std::endl;
   } else {
      sqlite3 *db;
      char *zErrMsg = 0;
  
      int rc = sqlite3_open(f.c_str(), &db);
      if (rc) {
	 fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
	 sqlite3_close(db);
      }

      // Ugly. Unsafe? (because "this" is a const pointer, and we cast
      // away the constness (we can't even use static_cast because
      // that won't let us cast away constness).
      //
      void *this_ptr = (void *) (&lrp);

      rc = sqlite3_exec(db, "SELECT * FROM ligands ;",
			ligands_db_sql_callback,
			this_ptr, &zErrMsg);
      if (rc!=SQLITE_OK) {
	 fprintf(stderr, "SQL error: %s\n", zErrMsg);
	 sqlite3_free(zErrMsg);
      }
      sqlite3_close(db);

      std::cout << "INFO:: density_correlation_vec.size() "
		<< lrp.density_correlation_vec.size() << std::endl;
      // std::cout << "INFO:: mogul_z_worst_vec.size() "
      //   	<< lrp.mogul_z_worst_vec.size() << std::endl;
      std::cout << "INFO:: bad_contacts_vec.size() "
		<< lrp.bad_contacts_vec.size() << std::endl;

      std::sort(lrp.density_correlation_vec.begin(), lrp.density_correlation_vec.end());
      // std::sort(lrp.mogul_z_worst_vec.begin(),       lrp.mogul_z_worst_vec.end());
      std::sort(lrp.bad_contacts_vec.begin(),        lrp.bad_contacts_vec.end());


      std::vector<double>::iterator it_d, it_m;
      std::vector<int>::iterator it_b;

      // high is good
      it_d = std::lower_bound(lrp.density_correlation_vec.begin(),
			      lrp.density_correlation_vec.end(),
			      density_correlation);

      // // low is good, need to reverse for percentiles
      // it_m = std::lower_bound(lrp.mogul_z_worst_vec.begin(),
      //    		      lrp.mogul_z_worst_vec.end(),
      //   		      mogul_z_score);

      // low is good, need to reverse for percentiles
      // it_gd = std::lower_bound(lrp.mogul_z_worst_vec.begin(),
      // lrp.mogul_z_worst_vec.end(),
      // dictionary_geometry_distortion_max);

      // low is good, need to reverse for percentiles
       it_b = std::lower_bound(lrp.bad_contacts_vec.begin(),
                               lrp.bad_contacts_vec.end(),
                               pcs.n_bad_overlaps);

      std::cout << "DEBUG:: ############### make_percentiles() density:      lower_bound at position of "
                << density_correlation << " is "
		<< (it_d - lrp.density_correlation_vec.begin())
		<< " of " << lrp.density_correlation_vec.size()
		<< '\n';
			      
      // std::cout << "mogul:        lower_bound at position of " << mogul_z_score
      //   	<< " is "
      //   	<< (it_m - lrp.mogul_z_worst_vec.begin())
      //   	<< " of " << lrp.mogul_z_worst_vec.size()
      //   	<< '\n';

      std::cout << "DEBUG:: ############### make_percentiles() bad-contacts: lower_bound at position of "
                << pcs.n_bad_overlaps << " is "
		<< (it_b - lrp.bad_contacts_vec.begin())
		<< " of " << lrp.bad_contacts_vec.size()
		<< '\n';

      double frac_d =
	 double(it_d - lrp.density_correlation_vec.begin())/
	 double(lrp.density_correlation_vec.size());
      // double frac_m = 1 - 
      //    double(it_m - lrp.mogul_z_worst_vec.begin())/
      //    double(lrp.mogul_z_worst_vec.size());
      double frac_dg = 1.0 - double(it_m - lrp.mogul_z_worst_vec.begin())/ double(lrp.mogul_z_worst_vec.size());
      double frac_b  = 1.0 - double(it_b - lrp.bad_contacts_vec.begin())/ double(lrp.bad_contacts_vec.size());

      lrp.density_correlation_percentile = frac_d;
      // lrp.mogul_percentile = frac_m;
      lrp.dictionary_geometry_percentile = frac_dg;
      lrp.probe_clash_percentile = frac_b;

      ///////////////////////////////////////////////////////////////////////////////////////////
      ///////////////////////////////////////////////////////////////////////////////////////////
      // Now let's create a substituary percentile for the geometry
      ///////////////////////////////////////////////////////////////////////////////////////////
      ///////////////////////////////////////////////////////////////////////////////////////////
      //
      double v = (dictionary_geometry_distortion_max - 9.0)/3.0;
      double e = gsl_sf_erf_scm(v);
      double ff = (e + 1.0)/2.0;
      double g = 1.0 - ff;
      lrp.dictionary_geometry_percentile = g;

      std::cout << "debug:: v " << v << " e " << e << " f " << f << " g " << g << std::endl;

      std::cout << "INFO:: returning lrp: density_correlation_percentile "
		<< lrp.density_correlation_percentile << std::endl;
      std::cout << "INFO:: returning lrp: dictionary_geometry_percentile "
		<< lrp.dictionary_geometry_percentile << std::endl;
      std::cout << "INFO:: returning lrp:         probe_clash_percentile "
		<< lrp.probe_clash_percentile << std::endl;

   }
#endif // USE_SQLITE
   return lrp;
}


void coot::ligand_check_dialog(coot::residue_spec_t spec,
			       const coot::ligand_report_absolute_t &lr, double percentile_limit) {

   // convert from absolute metrics to percentiles and then make a gui.

   // make_percentiles creates fractions (i.e. between 0 and 1)!
   coot::ligand_report_percentiles_t lrp = lr.make_percentiles();

   std::cout << "INPUT percentiles " << lr.dictionary_geometry_distortion_max << std::endl;
   std::cout << "INPUT percentiles " << lr.density_correlation << std::endl;
   std::cout << "INPUT percentiles " << lr.pcs.n_bad_overlaps << std::endl;

   std::cout << "OUTPUT percentiles " << lrp.dictionary_geometry_percentile << std::endl;
   std::cout << "OUTPUT percentiles " << lrp.density_correlation_percentile << std::endl;
   std::cout << "OUTPUT percentiles " << lrp.probe_clash_percentile << std::endl;

   ligand_check_percentiles_dialog(spec, lrp, percentile_limit);
}



void
coot::ligand_check_percentiles_dialog(coot::residue_spec_t spec,
				      const coot::ligand_report_percentiles_t &lr,
				      double percentile_limit) {

   if (graphics_info_t::use_graphics_interface_flag) {
      // GtkWidget *w = create_ligand_check_dialog();
      GtkWidget *w = widget_from_builder("ligand_check_dialog");

      GtkWidget *mogul_tick_w  = widget_from_builder("image_tick_mogul");
      GtkWidget *mogul_cross_w = widget_from_builder("image_cross_mogul");
      GtkWidget *mogul_incom_w = widget_from_builder("image_incomplete_mogul");

      GtkWidget *density_tick_w  = widget_from_builder("image_tick_density");
      GtkWidget *density_cross_w = widget_from_builder("image_cross_density");
      GtkWidget *density_incom_w = widget_from_builder("image_incomplete_density");

      GtkWidget *bumps_tick_w  = widget_from_builder("image_tick_bumps");
      GtkWidget *bumps_cross_w = widget_from_builder("image_cross_bumps");
      GtkWidget *bumps_incom_w = widget_from_builder("image_incomplete_bumps");

      GtkWidget *spec_label = widget_from_builder("ligand_check_ligand_spec_label");

      // GtkWidget *db_label   = lookup_widget(w, "ligand_check_db_label");

      std::cout << "percentile_limit                  " << percentile_limit << std::endl;
      // std::cout << "lr.mogul_percentile               " << lr.mogul_percentile << std::endl;
      std::cout << "lr.density_correlation_percentile " << lr.density_correlation_percentile << std::endl;
      std::cout << "lr.probe_clash_percentile         " << lr.probe_clash_percentile << std::endl;

      std::string l = "Residue: " + spec.chain_id + " " + util::int_to_string(spec.res_no);
      gtk_label_set_text(GTK_LABEL(spec_label), l.c_str());

#if 0
      if (lr.mogul_percentile < percentile_limit) {
	 // bad ligand
	 if (lr.mogul_percentile < 0) {
	    // test failed
	    gtk_widget_hide(mogul_tick_w);
	    gtk_widget_hide(mogul_cross_w);
	 } else {
	    // ligand failed test
	    gtk_widget_hide(mogul_tick_w);
	    gtk_widget_hide(mogul_incom_w);
	 }
      } else {
	 // happy ligand
	 gtk_widget_hide(mogul_cross_w);
	 gtk_widget_hide(mogul_incom_w);
      } 
#endif

      // Modern, dictionary-based
      
      if (lr.dictionary_geometry_percentile < percentile_limit) {
         // bad ligand
         if (lr.dictionary_geometry_percentile < 0) {
            // test failed
            gtk_widget_hide(mogul_tick_w);
            gtk_widget_hide(mogul_cross_w);
         } else {
            // ligand failed percentile test
            gtk_widget_hide(mogul_tick_w);
            gtk_widget_hide(mogul_incom_w);
         }
      } else {
         // happy ligand
         gtk_widget_hide(mogul_incom_w);
         gtk_widget_hide(mogul_cross_w);
      }

      
      if (lr.density_correlation_percentile < percentile_limit) {
	 // bad ligand
	 if (lr.density_correlation_percentile < -1) {
	    // the test failed/was incomplete
	    gtk_widget_hide(density_tick_w);
	    gtk_widget_hide(density_cross_w);
	 } else {
	    // the ligand failed the percentile test
	    gtk_widget_hide(density_tick_w);
	    gtk_widget_hide(density_incom_w);
	 }
      } else {
	 // happy
	 gtk_widget_hide(density_cross_w);
	 gtk_widget_hide(density_incom_w);
      }

      if (lr.probe_clash_percentile < percentile_limit) {
	 // bad bumps
	 if (lr.probe_clash_percentile < 0) {
	    gtk_widget_hide(bumps_tick_w);
	    gtk_widget_hide(bumps_cross_w);
	 } else {
	    gtk_widget_hide(bumps_tick_w);
	    gtk_widget_hide(bumps_incom_w);
	 }
      } else {
	 // happy bumps
	 gtk_widget_hide(bumps_cross_w);
	 gtk_widget_hide(bumps_incom_w);
      }


      gtk_widget_show(w);
   }

}

#include "c-interface-ligands-widgets.hh"

ligand_wiggly_ligand_data_t
setup_ligands_progress_bar() {

   GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
   GtkWidget *progress_bar = gtk_progress_bar_new();
   GtkWidget *window = gtk_window_new();
   GtkWidget *label = gtk_label_new("  Installing Ligand Conformers  ");

   gtk_window_set_title(GTK_WINDOW (window), "Fitting Ligands");
#if (GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION == 94) || (GTK_MAJOR_VERSION == 4)
   // 20220602-PE FIXME border width
#else
   gtk_container_set_border_width(GTK_CONTAINER (window), 0);
   gtk_container_set_border_width(GTK_CONTAINER (vbox), 10);
#endif

   gtk_window_set_child(GTK_WINDOW(window), vbox);

   graphics_info_t g;
   GtkWidget *main_window = g.get_main_window();
   // was: lookup_widget(graphics_info_t::glarea, "window1");
   gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(main_window));

   gtk_widget_show(vbox);
   gtk_widget_show(progress_bar);
   gtk_widget_show(label);
   gtk_widget_show(window);
#if (GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION == 94) || (GTK_MAJOR_VERSION == 4)
   gtk_box_append(GTK_BOX(vbox), progress_bar);
   gtk_box_append(GTK_BOX(vbox), label);
#else
   gtk_box_pack_start(GTK_BOX(vbox), progress_bar, FALSE, FALSE, 5);
   gtk_box_pack_start(GTK_BOX(vbox), label,        FALSE, FALSE, 5);
#endif

   // std::pair<GtkWidget *, GtkWidget *> p(progress_bar, window);

   ligand_wiggly_ligand_data_t r;
   r.progress_bar = progress_bar;
   r.progress_bar_window = window;
   r.progress_bar_label = label;

   return r;

}

void setup_ligands_progress_bar_idle(coot::wligand *wlig,
				     int imol_ligand,
				     ligand_wiggly_ligand_data_t ld) {

   ligand_wiggly_ligand_data_t *ldb = new ligand_wiggly_ligand_data_t(ld);

   // this GtkFunction returns a gboolean and takes a gpointer

   // 20170925 do we need this cast I doubt it.
   // gint idle = gtk_idle_add((GtkFunction) install_simple_wiggly_ligand_idle_fn, ldb);
   gint idle = g_idle_add(install_simple_wiggly_ligand_idle_fn, ldb);

   graphics_info_t g;
   g.ligand_wiggly_ligand_count = 0;

}

// return 1 for keep going (next time)
//
gboolean install_simple_wiggly_ligand_idle_fn(gpointer data) {

   gboolean status = 1;

   graphics_info_t g;
   ligand_wiggly_ligand_data_t *ldp = static_cast<ligand_wiggly_ligand_data_t *>(data);

   if (false)
      std::cout << "INFO:: running install_simple_wiggly_ligand_idle_fn() with g.ligand_wiggly_ligand_count "
	        << g.ligand_wiggly_ligand_count << " " << g.ligand_wiggly_ligand_n_samples
	        << " g.ligand_wiggly_ligand_n_samples and finish " << ldp->finish << std::endl;

   if (g.ligand_wiggly_ligand_count >= g.ligand_wiggly_ligand_n_samples) {

      if (ldp->finish) {
	 status = 0;
	 execute_ligand_search_internal(ldp->wlig);
	 gtk_widget_hide(ldp->progress_bar_window);
      } else {
	 // continue one more round
	 gtk_label_set_text(GTK_LABEL(ldp->progress_bar_label), "Searching density clusters");
	 gdouble frac = 0;
	 gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR (ldp->progress_bar), frac);
	 ldp->finish = true; // set for next time round
      }

   } else {
      coot::minimol::molecule mmol(g.molecules[ldp->imol_ligand].atom_sel.mol);

      try {
	 ldp->wlig->install_simple_wiggly_ligand(g.Geom_p(), mmol, ldp->imol_ligand,
						 g.ligand_wiggly_ligand_count, true);
      }
      catch (const std::exception &e) {
	 // this happens when there is no dictionary.
	 std::cout << "WARNING::" << e.what() << std::endl;
      }
      gdouble frac = double(g.ligand_wiggly_ligand_count)/double(g.ligand_wiggly_ligand_n_samples);
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR (ldp->progress_bar), frac);

   }
   g.ligand_wiggly_ligand_count++;

   return status;
}

#include <gsl/gsl_sf_erf.h>

double gsl_sf_erf_scm(double v) {
   return gsl_sf_erf(v);
}



coot::geometry_distortion_info_container_t
get_ligand_distortion_info(int imol, coot::residue_spec_t &rs) {

   // based on residues_distortions_py

   coot::geometry_distortion_info_container_t r;
   graphics_info_t g;

   if (is_valid_model_molecule(imol)) {
      mmdb::Residue *residue_p = graphics_info_t::molecules[imol].get_residue(rs);
      if (residue_p) {
         mmdb::Manager *mol = graphics_info_t::molecules[imol].atom_sel.mol;
         std::vector<std::pair<bool,mmdb::Residue *> > refining_residues;  // not fixed.
         refining_residues.push_back(std::make_pair(false, residue_p));
         const coot::protein_geometry &geom = *g.Geom_p();
         bool do_residue_internal_torsions = false;
         bool do_trans_peptide_restraints = false;
         bool do_rama_restraints = false;
         float rama_plot_restraint_weight = 1.0;
         coot::pseudo_restraint_bond_type pseudo_bonds_type = coot::NO_PSEUDO_BONDS;
         std::vector<coot::atom_spec_t> fixed_atom_specs;
         std::vector<mmdb::Link> links;
         coot::restraint_usage_Flags flags = coot::TYPICAL_RESTRAINTS;
         clipper::Xmap<float> xmap_dummy;
         coot::restraints_container_t restraints(refining_residues, links, geom, mol, fixed_atom_specs, &xmap_dummy);
         unsigned int n_threads = coot::get_max_number_of_threads();
         restraints.thread_pool(&g.static_thread_pool, n_threads);
         restraints.make_restraints(imol, geom, flags,
                                    do_residue_internal_torsions,
                                    do_trans_peptide_restraints,
                                    rama_plot_restraint_weight,
                                    do_rama_restraints,
                                    false, false, false,
                                    pseudo_bonds_type);
         bool keep_distortion_for_hydrogen_atom_restraints = false;
         r = restraints.geometric_distortions(keep_distortion_for_hydrogen_atom_restraints);
      }
   }
   return r;
}



#ifdef USE_GUILE
//! \brief return the summary info for ligand distortion
SCM get_ligand_distortion_summary_info_scm(int imol, SCM residue_spec) {

   SCM r = SCM_BOOL_F;

   if (is_valid_model_molecule(imol)) {
      coot::residue_spec_t rs = residue_spec_from_scm(residue_spec);
      coot::geometry_distortion_info_container_t gdic = get_ligand_distortion_info(imol, rs);
      unsigned int n_items =  gdic.geometry_distortion.size();
      if (n_items > 0) {
         double max_distortion = 0.0;
         coot::simple_restraint restraint_for_max_distortion;
         for (const auto &item : gdic.geometry_distortion) {
            if (item.restraint.restraint_type == coot::NON_BONDED_CONTACT_RESTRAINT)
               continue;
            // filter out Bonds and angles for Hydrogen atoms
            if (item.distortion_score > max_distortion) {
               max_distortion = item.distortion_score;
               restraint_for_max_distortion = item.restraint;
               std::cout << "selected: " << item.restraint << " with distortion " << max_distortion << std::endl;
            }
         }

         SCM max_distortion_score_scm = scm_from_double(max_distortion);
         // does restraint_to_scm belong in graphics_info_t?
         graphics_info_t g;
         r = scm_list_2(max_distortion_score_scm, g.restraint_to_scm(restraint_for_max_distortion));
      }
   }
   return r;
   
}
#endif

#ifdef USE_PYTHON
//! \brief return the summary info for ligand distortion
PyObject *get_ligand_distortion_summary_info_py(int imol, PyObject *residue_spec) {

   PyObject *r = Py_False;
  
   if (is_valid_model_molecule(imol)) {
      coot::residue_spec_t rs = residue_spec_from_py(residue_spec);
      coot::geometry_distortion_info_container_t gdic = get_ligand_distortion_info(imol, rs);
      unsigned int n_items =  gdic.geometry_distortion.size();
      if (n_items > 0) {
         double max_distortion = 0.0;
         coot::simple_restraint restraint_for_max_distortion;
         for (const auto &item : gdic.geometry_distortion) {
            if (item.restraint.restraint_type == coot::NON_BONDED_CONTACT_RESTRAINT)
               continue;
            if (item.distortion_score > max_distortion) {
               max_distortion = item.distortion_score;
               restraint_for_max_distortion = item.restraint;
            }
         }
         graphics_info_t g;
         r = PyList_New(2);
         PyList_SetItem(r, 0, PyFloat_FromDouble(max_distortion));
         PyList_SetItem(r, 1, g.restraint_to_py(restraint_for_max_distortion));
      }
   }
   if (PyBool_Check(r)) {
      Py_INCREF(r);
   }
   return r;
}
#endif

