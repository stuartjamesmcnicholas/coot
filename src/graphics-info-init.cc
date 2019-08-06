
#ifdef USE_PYTHON
#include <Python.h>
#endif

#include "coords/cos-sin.h"
#include "graphics-info.h"

 #include <glm/glm.hpp>
 #include <glm/gtc/matrix_transform.hpp>
 #include <glm/gtc/type_ptr.hpp>

void
graphics_info_t::init() {

#ifdef WINDOWS_MINGW
   prefer_python = 1;
#endif
   // The cosine->sine lookup table, used in picking.
   //
   // The data in it are static, so we can get to them anywhere
   // now that we have run this
   cos_sin cos_sin_table(1000);


   // ----------------------------- font test -----------------
   FT_Library ft;
   if (FT_Init_FreeType(&ft))
   std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;

   FT_Face face;
   if (FT_New_Face(ft, "fonts/Vera.ttf", 0, &face))
   std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
   FT_Set_Pixel_Sizes(face, 0, 48);

   if (FT_Load_Char(face, 'X', FT_LOAD_RENDER))
   std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;

   glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Disable byte-alignment restriction
   // only using one byte.

   for (GLubyte ic = 0; ic < 128; ic++) {
      // Load character glyph
      if (FT_Load_Char(face, ic, FT_LOAD_RENDER)) {
         std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
         continue;
      }
      // Generate texture
      GLuint texture;
      glGenTextures(1, &texture);
      glBindTexture(GL_TEXTURE_2D, texture);
      glTexImage2D( GL_TEXTURE_2D,
         0,
         GL_RED,
         face->glyph->bitmap.width,
         face->glyph->bitmap.rows,
         0,
         GL_RED,
         GL_UNSIGNED_BYTE,
         face->glyph->bitmap.buffer);
         // Set texture options
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

      // Now store the character
      FT_character character = {
         texture,
         glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
         glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
         face->glyph->advance.x};
         ft_characters[ic] = character; // ic type change
   }

   FT_Done_Face(face);
   FT_Done_FreeType(ft);

   // ----------------------------- done font test -----------------



      // transform = Transform(glm::vec3(0.0, 0.0, 0.0),
      //                       glm::vec3(0.0, 0.0, 0.0),
      //                       glm::vec3(1.0, 1.0, 1.0));

      float aspect = 1.2;
      // camera = benny::Camera(glm::vec3(0.0f,0.0f,-3.5f), 70, aspect, 0.1f, 500.0f);

      which_eye = FRONT_EYE;

      find_ligand_ligand_mols_ = new std::vector<std::pair<int, bool> >;
      geom_p = new coot::protein_geometry;
      geom_p->set_verbose(true);

      cif_dictionary_read_number = geom_p->init_standard();
      geom_p->add_planar_peptide_restraint();

      geom_p->init_ccp4srs("srsdata"); // overridden by COOT_CCP4SRS_DIR and CCP4_LIB

      // rotamer probabilitiles
      // guess we shall rather use COOT_DATA_DIR and only as fallback PKGDATADIR?!
      // maybe only for windows!?
      //
      // 20090920-PE, no, not only windows.  If they set
      // COOT_DATA_DIR, let's use that instead of PKGDATADIR (useful
      // for Justin Lecher and Gentoo who test before installing (and
      // they need a way to specify the data dir (before installing
      // it's not in PKGDATADIR)).
      //
      std::string tables_dir = PKGDATADIR;

      char *data_dir = getenv("COOT_DATA_DIR");
      if (data_dir) {
	tables_dir = data_dir;
      }

      tables_dir += "/rama-data";
      rot_prob_tables.set_tables_dir(tables_dir);

      moving_atoms_asc = new atom_selection_container_t;
      moving_atoms_asc->mol = NULL;
      moving_atoms_asc->atom_selection = NULL;
      moving_atoms_asc->n_selected_atoms = 0;

      standard_residues_asc.read_success = 0;
      standard_residues_asc.n_selected_atoms = 0;
      read_standard_residues(); // updates read_success

      symmetry_colour_merge_weight = 0.5; // 0.0 -> 1.0

      symmetry_colour = std::vector<double> (4, 0.5);
      symmetry_colour[0] = 0.1;
      symmetry_colour[1] = 0.2;
      symmetry_colour[2] = 0.8;

      // use_graphics_interface_flag = 1;  don't (re)set this here,
      // it is set as a static and possibly modified by immediate
      // handling of command line data in main.cc

      // moving_atoms_asc gets filled in copy_mol_and_regularize, not
      // here.

      // db_main = NULL;

      // command line scripts:
      command_line_scripts = new std::vector<std::string>;

      // LSQ matching info
      lsq_matchers = new std::vector<coot::lsq_range_match_info_t>;

      // LSQ Plane
      lsq_plane_atom_positions = new std::vector<clipper::Coord_orth>;

      directory_for_fileselection = "";
      directory_for_filechooser = "";

      baton_next_ca_options = new std::vector<coot::scored_skel_coord>;
      baton_previous_ca_positions = new std::vector<clipper::Coord_orth>;

      // rotamer distortion graph scale
      rotamer_distortion_scale = 0.3;

      // cif dictionary
      cif_dictionary_filename_vec = new std::vector<std::string>;

/*       // ramachandran plots: */
/*       dynarama_is_displayed = new GtkWidget *[n_molecules_max]; */
/*       for (int i=0; i<n_molecules_max; i++) */
/* 	 dynarama_is_displayed[i] = NULL; // belt and braces */

/*       // sequence_view */
/*       sequence_view_is_displayed = new GtkWidget * [n_molecules_max]; */
/*       for (int i=0; i<n_molecules_max; i++) */
/* 	 sequence_view_is_displayed[i] = NULL; */

      // residue edits
      residue_info_edits = new std::vector<coot::select_atom_info>;

      // display distances
      distance_object_vec = new std::vector<coot::simple_distance_object_t>;

      // pointer distances
      pointer_distances_object_vec = new std::vector<std::pair<clipper::Coord_orth, clipper::Coord_orth> >;

      // ligand blobs:
      ligand_big_blobs = new std::vector<clipper::Coord_orth>;

      // rot_trans adjustments:
      for (int i=0; i<6; i++)
	 previous_rot_trans_adjustment[i] = -10000;

      // merging molecules
      merge_molecules_merging_molecules = new std::vector<int>;

      // generic display objects
      generic_objects_p = new std::vector<coot::generic_display_object_t>;
      generic_objects_dialog = NULL;

      // generic text:
      generic_texts_p = new std::vector<coot::generic_text_object_t>;

      // views
      views = new std::vector<coot::view_info_t>;

      // glob extensions:
      coordinates_glob_extensions = new std::vector<std::string>;
      data_glob_extensions = new std::vector<std::string>;
      map_glob_extensions = new std::vector<std::string>;
      dictionary_glob_extensions  = new std::vector<std::string>;

      coordinates_glob_extensions->push_back(".pdb");
      coordinates_glob_extensions->push_back(".pdb.gz");
      coordinates_glob_extensions->push_back(".brk");
      coordinates_glob_extensions->push_back(".brk.gz");
      coordinates_glob_extensions->push_back(".ent");
      coordinates_glob_extensions->push_back(".ent.gz");
      coordinates_glob_extensions->push_back(".ent.Z");
      coordinates_glob_extensions->push_back(".cif");
      coordinates_glob_extensions->push_back(".mmcif");
      coordinates_glob_extensions->push_back(".mmCIF");
      coordinates_glob_extensions->push_back(".cif.gz");
      coordinates_glob_extensions->push_back(".mmcif.gz");
      coordinates_glob_extensions->push_back(".mmCIF.gz");
      coordinates_glob_extensions->push_back(".res");  // SHELX
      coordinates_glob_extensions->push_back(".ins");  // SHELX
      coordinates_glob_extensions->push_back(".pda");  // SHELX

      data_glob_extensions->push_back(".mtz");
      data_glob_extensions->push_back(".hkl");
      data_glob_extensions->push_back(".data");
      data_glob_extensions->push_back(".phs");
      data_glob_extensions->push_back(".pha");
      data_glob_extensions->push_back(".cif");
      data_glob_extensions->push_back(".fcf"); // SHELXL
      data_glob_extensions->push_back(".mmcif");
      data_glob_extensions->push_back(".mmCIF");
      data_glob_extensions->push_back(".cif.gz");
      data_glob_extensions->push_back(".mmcif.gz");
      data_glob_extensions->push_back(".mmCIF.gz");

      map_glob_extensions->push_back(".map");
      map_glob_extensions->push_back(".mrc");
      map_glob_extensions->push_back(".ext");
      map_glob_extensions->push_back(".msk");
      map_glob_extensions->push_back(".ccp4");
      map_glob_extensions->push_back(".cns");

      dictionary_glob_extensions->push_back(".cif");
      dictionary_glob_extensions->push_back(".mmcif");
      dictionary_glob_extensions->push_back(".mmCIF");
      dictionary_glob_extensions->push_back(".cif.gz");
      dictionary_glob_extensions->push_back(".mmcif.gz");
      dictionary_glob_extensions->push_back(".mmCIF.gz");
      dictionary_glob_extensions->push_back(".lib");

      /* things for preferences */
      //preferences_internal = new std::vector<coot::preference_info_t>;
      preferences_general_tabs = new std::vector<std::string>;
      preferences_bond_tabs = new std::vector<std::string>;
      preferences_geometry_tabs = new std::vector<std::string>;
      preferences_colour_tabs = new std::vector<std::string>;
      preferences_map_tabs = new std::vector<std::string>;
      preferences_other_tabs = new std::vector<std::string>;

      preferences_general_tabs->push_back("preferences_file_selection");
      preferences_general_tabs->push_back("preferences_dock_accept_dialog");
      preferences_general_tabs->push_back("preferences_hid");
      preferences_general_tabs->push_back("preferences_recentre_pdb");
      preferences_general_tabs->push_back("preferences_model_toolbar_style");
      preferences_general_tabs->push_back("preferences_smooth_scroll");
      preferences_general_tabs->push_back("preferences_main_toolbar_style");

      preferences_bond_tabs->push_back("preferences_bond_parameters");
      preferences_bond_tabs->push_back("preferences_bond_colours");

      preferences_map_tabs->push_back("preferences_map_parameters");
      preferences_map_tabs->push_back("preferences_map_colours");
      preferences_map_tabs->push_back("preferences_map_drag");

      preferences_geometry_tabs->push_back("preferences_cis_peptides");

      preferences_colour_tabs->push_back("preferences_background_colour");
      preferences_colour_tabs->push_back("preferences_bond_colours");
      preferences_colour_tabs->push_back("preferences_map_colours");

      preferences_other_tabs->push_back("preferences_console");
      preferences_other_tabs->push_back("preferences_tips");
      preferences_other_tabs->push_back("preferences_speed");
      preferences_other_tabs->push_back("preferences_antialias");
      preferences_other_tabs->push_back("preferences_font");
      preferences_other_tabs->push_back("preferences_pink_pointer");
      // for toolbar icons in preferences
      model_toolbar_icons = new std::vector<coot::preferences_icon_info_t>;
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(0, "refine-1.svg",
								   "Real Space Refine",
								   "model_toolbar_refine_togglebutton",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(1, "regularize-1.svg",
								   "Regularize",
								   "model_toolbar_regularize_togglebutton",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(2, "anchor.svg",
								   "Fixed Atoms...",
								   "model_toolbar_fixed_atoms_button",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(3, "rigid-body.svg",
								   "Rigid Body Fit Zone",
								   "model_toolbar_rigid_body_fit_togglebutton",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(4, "rtz.svg",
								   "Rotate/Translate Zone",
								   "model_toolbar_rot_trans_toolbutton",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(5, "auto-fit-rotamer.svg",
								   "Auto Fit Rotamer",
								   "model_toolbar_auto_fit_rotamer_togglebutton",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(6, "rotamers.svg",
								   "Rotamers",
								   "model_toolbar_rotamers_togglebutton",
								   1,1 ));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(7, "edit-chi.svg",
								   "Edit Chi Angles",
								   "model_toolbar_edit_chi_angles_togglebutton",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(8, "torsion-general.svg",
								   "Torsion General",
								   "model_toolbar_torsion_general_toggletoolbutton",
								   1, 0));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(9, "flip-peptide.svg",
								   "Flip Peptide",
								   "model_toolbar_flip_peptide_togglebutton",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(10, "side-chain-180.svg",
								   "Side Chain 180 Degree Flip",
								   "model_toolbar_sidechain_180_togglebutton",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(11, "edit-backbone.svg",
								   "Edit Backbone Torsions",
								   "model_toolbar_edit_backbone_torsions_toggletoolbutton",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(12, "",
								   "---------------------",
								   "model_toolbar_hsep_toolitem",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(13, "mutate-auto-fit.svg",
								   "Mutate and Auto-Fit...",
								   "model_toolbar_mutate_and_autofit_togglebutton",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(14, "mutate.svg",
								   "Simple Mutate...",
								   "model_toolbar_simple_mutate_togglebutton",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(15, "add-peptide-1.svg",
								   "Add Terminal Residue...",
								   "model_toolbar_add_terminal_residue_togglebutton",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(16, "add-alt-conf.svg",
								   "Add Alt Conf...",
								   "model_toolbar_add_alt_conf_toolbutton",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(17, "atom-at-pointer.svg",
								   "Place Atom at Pointer",
								   "model_toolbar_add_atom_button",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(18, "gtk-clear",
								   "Clear Pending Picks",
								   "model_toolbar_clear_pending_picks_button",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(19, "gtk-delete",
								   "Delete...",
								   "model_toolbar_delete_button",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(20, "gtk-undo",
								   "Undo",
								   "model_toolbar_undo_button",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(21, "gtk-redo",
								   "Redo",
								   "model_toolbar_redo_button",
								   1, 1));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(22, "",
								   "---------------------",
								   "model_toolbar_hsep_toolitem2",
								   1, 0));
      model_toolbar_icons->push_back(coot::preferences_icon_info_t(23, "azerbaijan.svg",
								   "Run Refmac...",
								   "model_toolbar_refmac_button",
								   1, 0));
      // for main icons in preferences
      main_toolbar_icons = new std::vector<coot::preferences_icon_info_t>;
      main_toolbar_icons->push_back(coot::preferences_icon_info_t(0, "gtk-open",
								  "Open Coords...",
								  "coords_toolbutton",
								  1, 1));
      main_toolbar_icons->push_back(coot::preferences_icon_info_t(1, "gtk-zoom-fit",
								  "Reset View",
								  "reset_view_toolbutton",
								  1, 1));
      main_toolbar_icons->push_back(coot::preferences_icon_info_t(2, "display-manager.png",
								  "Display Manager",
								  "display_manager_toolbutton",
								  1, 1));
      main_toolbar_icons->push_back(coot::preferences_icon_info_t(3, "go-to-atom.svg",
								  "Go To Atom...",
								  "go_to_atom_toolbutton",
								  1, 1));
      main_toolbar_icons->push_back(coot::preferences_icon_info_t(4, "go-to-ligand.svg",
								  "Go To Ligand",
								  "go_to_ligand_toolbutton",
								  1, 1));

      do_expose_swap_buffers_flag = 1;
#ifdef WII_INTERFACE_WIIUSE
      wiimotes = NULL;
#endif

      refmac_dialog_mtz_file_label = NULL;
      /* set no of refmac cycles */
      preset_number_refmac_cycles = new std::vector<int>;
      preset_number_refmac_cycles->push_back(0);
      preset_number_refmac_cycles->push_back(1);
      preset_number_refmac_cycles->push_back(2);
      preset_number_refmac_cycles->push_back(3);
      preset_number_refmac_cycles->push_back(5);
      preset_number_refmac_cycles->push_back(7);
      preset_number_refmac_cycles->push_back(10);
      preset_number_refmac_cycles->push_back(15);
      preset_number_refmac_cycles->push_back(20);
      preset_number_refmac_cycles->push_back(50);

   }
