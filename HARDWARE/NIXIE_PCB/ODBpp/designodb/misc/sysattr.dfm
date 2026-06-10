UNITS=INCH

TEXT {
  NAME    = .area_name
  ENTITY  = FEATURE
  MIN_LEN = 0
  MAX_LEN = 500
  GROUP   = DFx Area
  PROMPT  = DFx Area Name
}

OPTION {
  NAME    = .comp_type
  ENTITY  = COMPONENT;PACKAGE
  OPTIONS = axial;bga;cbga;cob;dip;discrete;discrete402;discrete603;label;pga;pihconn;pihmisc;plcc;pqfp;printed;qfp;radial;sip;smtconn;smtmisc;socket;soic;soj;sop;sot;tab;tqfp;tsoic;tsop
  GROUP   = DFx Component
  PROMPT  = Type 1
}

BOOLEAN {
  NAME    = .center_fiducial
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = DFx Component
  PROMPT  = Center Fiducial Required
}

INTEGER {
  NAME    = .num_local_fiducials
  ENTITY  = COMPONENT;PACKAGE
  DEF     = 0
  MIN_VAL = 0
  MAX_VAL = 20
  GROUP   = DFx Component
  PROMPT  = Number Local Fiducials Required
}

FLOAT {
  NAME    = .local_fiducial_dist
  ENTITY  = COMPONENT;PACKAGE
  MIN_VAL = 0.0
  MAX_VAL = 100.0
  UNITS   = INCH_MM
  GROUP   = DFx Component
  PROMPT  = Search Distance for Local Fiducial
}

OPTION {
  NAME    = .otherside_keepout
  ENTITY  = COMPONENT;PACKAGE
  OPTIONS = full_area;pins_only;pads_only
  GROUP   = DFx Component
  PROMPT  = No Components on Other Side
}

BOOLEAN {
  NAME    = .comp_ign_spacing
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = DFx Component
  PROMPT  = Ignore During Spacing Analysis
}

BOOLEAN {
  NAME    = .no_tp_under
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = DFx Component
  PROMPT  = No Test Point Under
}

BOOLEAN {
  NAME    = .no_hole_under
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = DFx Component
  PROMPT  = No Hole Under
}

BOOLEAN {
  NAME    = .no_uncap_via_under
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = DFx Component
  PROMPT  = No Uncapped Vias Under
}

BOOLEAN {
  NAME    = .no_text_under
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = DFx Component
  PROMPT  = No Silkscreen Under
}

BOOLEAN {
  NAME    = .thvpad_required
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = DFx Component
  PROMPT  = Theiving Pad Required
}

BOOLEAN {
  NAME    = .label_clearance
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = DFx Component
  PROMPT  = Label Clearance
}

BOOLEAN {
  NAME    = .guard_comp
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = DFx Component
  PROMPT  = Component Guard
}

BOOLEAN {
  NAME    = .needs_guarding
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = DFx Component
  PROMPT  = Require Guards
}

BOOLEAN {
  NAME    = .ind_orient_req
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = DFx Component
  PROMPT  = Orientation Indication Required
}

BOOLEAN {
  NAME    = .no_fiducial_check
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = DFx Component
  PROMPT  = No Fiducial Check
}

BOOLEAN {
  NAME    = .is_shadowed
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = DFx Component
  PROMPT  = Check for Shadowing
}

OPTION {
  NAME    = .comp_mount_type
  ENTITY  = COMPONENT;PACKAGE
  OPTIONS = Other;SMT;THMT;PRESSFIT
  DEF     = Other
  GROUP   = Component
  PROMPT  = Mount Type
}

FLOAT {
  NAME    = .toep_spacing_req
  ENTITY  = COMPONENT;PACKAGE
  DEF     = 5
  MIN_VAL = 1
  MAX_VAL = 500
  UNITS   = MIL_MICRON
  GROUP   = DFx Component
  PROMPT  = Required Toeprint Spacing
}

FLOAT {
  NAME    = .spacing_req
  ENTITY  = FEATURE
  MIN_VAL = 0
  MAX_VAL = 100
  UNITS   = MIL_MICRON
  PROMPT  = Spacing Required
}

BOOLEAN {
  NAME    = .comp_ignore
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = Component
  PROMPT  = Ignore Graphically/Output
}

TEXT {
  NAME    = .layer_class
  ENTITY  = LAYER
  MIN_LEN = 0
  MAX_LEN = 1000
  PROMPT  = Layer Class
}

BOOLEAN {
  NAME    = .feature_ignore
  ENTITY  = FEATURE
  GROUP   = DFx Feature
  PROMPT  = Copper Feature Ignore
}

TEXT {
  NAME    = .drc_route_keepin_lyr
  ENTITY  = JOB
  MIN_LEN = 0
  MAX_LEN = 64
  PROMPT  = Route Keep In Layer
}

TEXT {
  NAME    = .drc_comp_keepin_lyr
  ENTITY  = JOB
  MIN_LEN = 0
  MAX_LEN = 64
  PROMPT  = Component Keep In Layer
}

TEXT {
  NAME    = .drc_tp_keepin_lyr
  ENTITY  = JOB
  MIN_LEN = 0
  MAX_LEN = 64
  PROMPT  = Test Point Keep In Layer
}

TEXT {
  NAME    = .drc_route_keepout_lyr
  ENTITY  = JOB
  MIN_LEN = 0
  MAX_LEN = 64
  PROMPT  = Route Keep Out Layer
}

TEXT {
  NAME    = .drc_comp_keepout_lyr
  ENTITY  = JOB
  MIN_LEN = 0
  MAX_LEN = 64
  PROMPT  = Component Keep Out Layer
}

TEXT {
  NAME    = .drc_pad_keepout_lyr
  ENTITY  = JOB
  MIN_LEN = 0
  MAX_LEN = 64
  PROMPT  = Pad Keep Out Layer
}

TEXT {
  NAME    = .drc_via_keepout_lyr
  ENTITY  = JOB
  MIN_LEN = 0
  MAX_LEN = 64
  PROMPT  = Via Keep Out Layer
}

TEXT {
  NAME    = .drc_trace_keepout_lyr
  ENTITY  = JOB
  MIN_LEN = 0
  MAX_LEN = 64
  PROMPT  = Trace Keep Out Layer
}

TEXT {
  NAME    = .drc_plane_keepout_lyr
  ENTITY  = JOB
  MIN_LEN = 0
  MAX_LEN = 64
  PROMPT  = Plane Keep Out Layer
}

TEXT {
  NAME    = .drc_comp_height_lyr
  ENTITY  = JOB
  MIN_LEN = 0
  MAX_LEN = 64
  PROMPT  = Comp. Height Restriction Layer
}

TEXT {
  NAME    = .drc_tp_keepout_lyr
  ENTITY  = JOB
  MIN_LEN = 0
  MAX_LEN = 64
  PROMPT  = Test Point Keep Out Layer
}

TEXT {
  NAME    = .fab_drc
  ENTITY  = STEP
  MIN_LEN = 0
  MAX_LEN = 20
  PROMPT  = Fab DRC
}

TEXT {
  NAME    = .hdi_drc
  ENTITY  = STEP
  MIN_LEN = 0
  MAX_LEN = 20
  PROMPT  = HDI DRC
}

TEXT {
  NAME    = .placement_layer
  ENTITY  = COMPONENT;PACKAGE
  MIN_LEN = 0
  MAX_LEN = 64
  PROMPT  = Placement Layer
}

BOOLEAN {
  NAME    = .toep_nochk_o_side
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = DFx Component
  PROMPT  = Toeprint Spacing on Placement Side
}

BOOLEAN {
  NAME    = .no_trace_under
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = DFx Component
  PROMPT  = No Trace Under
}

BOOLEAN {
  NAME    = .no_protrude_board
  ENTITY  = COMPONENT;PACKAGE
  GROUP   = Component
  PROMPT  = Pins Cannot Protrude
}

BOOLEAN {
  NAME    = .drc_bend_keepout
  ENTITY  = FEATURE
  GROUP   = DFx Area
  PROMPT  = FLEX Bend Keepout Area
}

BOOLEAN {
  NAME    = .drc_board
  ENTITY  = FEATURE
  GROUP   = DFx Area
  PROMPT  = Analysis Board Area
}

BOOLEAN {
  NAME    = .drc_mech
  ENTITY  = FEATURE
  GROUP   = OBSOLETE
  PROMPT  = Analysis Mechanical Area
}

TEXT {
  NAME    = .drc_ref_des
  ENTITY  = FEATURE
  MIN_LEN = 0
  MAX_LEN = 100
  GROUP   = DFx Area
  PROMPT  = Reference Designator
}

BOOLEAN {
  NAME    = .drc_etch_lyrs_all
  ENTITY  = FEATURE
  GROUP   = DFx Area
  PROMPT  = DFx Area All Layers
}

TEXT {
  NAME    = .drc_etch_lyrs
  ENTITY  = FEATURE
  MIN_LEN = 0
  MAX_LEN = 1000
  GROUP   = DFx Area
  PROMPT  = DFx Area Layers by Name
}

TEXT {
  NAME    = .drc_etch_lyrs_bit
  ENTITY  = FEATURE
  MIN_LEN = 0
  MAX_LEN = 64
  GROUP   = DFx Area
  PROMPT  = DFx Area Selected Layers by Bits
}

OPTION {
  NAME    = .drc_assembly_lyrs
  ENTITY  = FEATURE
  OPTIONS = Top;Bottom;Both
  GROUP   = DFx Area
  PROMPT  = Assigned Area to Component Side
}

BOOLEAN {
  NAME    = .drc_comp_keepin
  ENTITY  = FEATURE
  GROUP   = DFx Area
  PROMPT  = Component Keep In
}

BOOLEAN {
  NAME    = .drc_tp_keepin
  ENTITY  = FEATURE
  GROUP   = DFx Area
  PROMPT  = Test Point Keep In
}

BOOLEAN {
  NAME    = .drc_route_keepin 
  ENTITY  = FEATURE
  GROUP   = DFx Area
  PROMPT  = Rout Keep In
}

BOOLEAN {
  NAME    = .drc_comp_keepout
  ENTITY  = FEATURE
  GROUP   = DFx Area
  PROMPT  = Component Keep Out
}

BOOLEAN {
  NAME    = .drc_tp_keepout
  ENTITY  = FEATURE
  GROUP   = DFx Area
  PROMPT  = Test Point Keep Out
}

BOOLEAN {
  NAME    = .drc_trace_keepout
  ENTITY  = FEATURE
  GROUP   = DFx Area
  PROMPT  = Trace Keep Out
}

BOOLEAN {
  NAME    = .drc_pad_keepout
  ENTITY  = FEATURE
  GROUP   = DFx Area
  PROMPT  = Pad Keep Out
}

BOOLEAN {
  NAME    = .drc_plane_keepout
  ENTITY  = FEATURE
  GROUP   = DFx Area
  PROMPT  = Plane Keep Out
}

BOOLEAN {
  NAME    = .drc_via_keepout
  ENTITY  = FEATURE
  GROUP   = DFx Area
  PROMPT  = Via Keep Out
}

BOOLEAN {
  NAME    = .drc_route_keepout
  ENTITY  = FEATURE
  GROUP   = DFx Area
  PROMPT  = Rout Keep Out
}

BOOLEAN {
  NAME    = .drc_comp_height
  ENTITY  = FEATURE
  GROUP   = DFx Area
  PROMPT  = Component Height for Area
}

FLOAT {
  NAME    = .drc_max_height
  ENTITY  = FEATURE;COMPONENT
  MIN_VAL = 0.0
  MAX_VAL = 10.0
  UNITS   = INCH_MM
  GROUP   = DFx Area
  PROMPT  = Maximum Height for Component
}
FLOAT {
  NAME    = .drc_min_height
  ENTITY  = FEATURE;COMPONENT
  MIN_VAL = 0.0
  MAX_VAL = 10.0
  UNITS   = INCH_MM
  GROUP   = DFx Area
  PROMPT  = Minimum Height for Component
}

FLOAT {
  NAME    = .net_length_min
  ENTITY  = NET
  MIN_VAL = 0.0
  MAX_VAL = 100.0
  UNITS   = INCH_MM
  PROMPT  = Net Min. Length
}
FLOAT {
  NAME    = .net_length_max
  ENTITY  = NET
  MIN_VAL = 0.0
  MAX_VAL = 100.0
  UNITS   = INCH_MM
  PROMPT  = Net Max. Length
}

FLOAT {
  NAME    = .dpair_gap
  ENTITY  = NET
  MIN_VAL = 0.0
  MAX_VAL = 10.0
  UNITS   = INCH_MM
  PROMPT  = Differential Pair Spacing
}

TEXT {
  NAME    = .electrical_class
  ENTITY  = NET
  MIN_LEN = 0
  MAX_LEN = 64
  PROMPT  = Electrical Class
}

FLOAT {
  NAME    = .eclass_voltage_swing
  ENTITY  = NET
  MIN_VAL = 0.0
  MAX_VAL = 100.0
  PROMPT  = Voltage Swing
}

FLOAT {
  NAME    = .eclass_impedance
  ENTITY  = NET
  MIN_VAL = 0.0
  MAX_VAL = 1000.0
  UNIT_TYPE = RESISTANCE
  PROMPT  = Impedance
}

FLOAT {
  NAME    = .eclass_rise_time
  ENTITY  = NET
  MIN_VAL = 0.0
  MAX_VAL = 100.0
  PROMPT  = Rise Time
}

FLOAT {
  NAME    = .eclass_min_stub_length
  ENTITY  = NET
  MIN_VAL = 0.0
  MAX_VAL = 100.0
  UNITS   = INCH_MM
  PROMPT  = Min. Stub Length
}
FLOAT {
  NAME    = .eclass_max_stub_length
  ENTITY  = NET
  MIN_VAL = 0.0
  MAX_VAL = 100.0
  UNITS   = INCH_MM
  PROMPT  = Max. Stub Length
}

TEXT {
  NAME    = .eclass_individual_parallel_min_jog_list
  ENTITY  = NET
  MIN_LEN = 0
  MAX_LEN = 255
  PROMPT  = Individ. Parallel Min. Jog List
}

TEXT {
  NAME    = .eclass_individual_parallel_max_length_list
  ENTITY  = NET
  MIN_LEN = 0
  MAX_LEN = 255
  PROMPT  = Individ. Parallel Max. Len. List
}

TEXT {
  NAME    = .eclass_individual_parallel_dist_list
  ENTITY  = NET
  MIN_LEN = 0
  MAX_LEN = 255
  PROMPT  = Individ. Parallel Dist List
}

TEXT {
  NAME    = .eclass_accumulative_parallel_max_length_list
  ENTITY  = NET
  MIN_LEN = 0
  MAX_LEN = 255
  PROMPT  = Accum. Parallel Max. Len. List
}

TEXT {
  NAME    = .eclass_accumulative_parallel_dist_list
  ENTITY  = NET
  MIN_LEN = 0
  MAX_LEN = 255
  PROMPT  = Accum. Parallel Dist. List
}

INTEGER {
  NAME    = .eclass_max_via_count
  ENTITY  = NET
  MIN_VAL = 0
  MAX_VAL = 1000
  PROMPT  = Max. Via Count
}

INTEGER {
  NAME    = .testpoint_count
  ENTITY  = NET
  MIN_VAL = -1
  MAX_VAL = 10000
  PROMPT  = Expected Test Point Count
}

FLOAT {
  NAME    = .min_line_width
  ENTITY  = NET
  MIN_VAL = 0.0
  MAX_VAL = 100.0
  UNITS   = INCH_MM
  PROMPT  = Min. Line Width
}

FLOAT {
  NAME    = .ar_pad_drill_top_min
  ENTITY  = FEATURE
  MIN_VAL = -10.0
  MAX_VAL = 1000.0
  UNITS   = MIL_MICRON
  GROUP   = HDI
  PROMPT  = Min. Pad AR Drill Top
}
FLOAT {
  NAME    = .ar_pad_drill_top_max
  ENTITY  = FEATURE
  MIN_VAL = -10.0
  MAX_VAL = 1000.0
  UNITS   = MIL_MICRON
  GROUP   = HDI
  PROMPT  = Max. Pad AR Drill Top
}
FLOAT {
  NAME    = .ar_pad_drill_inner_min
  ENTITY  = FEATURE
  MIN_VAL = -10.0
  MAX_VAL = 1000.0
  UNITS   = MIL_MICRON
  GROUP   = HDI
  PROMPT  = Min. Pad AR Drill Inner
}
FLOAT {
  NAME    = .ar_pad_drill_inner_max
  ENTITY  = FEATURE
  MIN_VAL = -10.0
  MAX_VAL = 1000.0
  UNITS   = MIL_MICRON
  GROUP   = HDI
  PROMPT  = Max. Pad AR Drill Inner
}
FLOAT {
  NAME    = .ar_pad_drill_bottom_min
  ENTITY  = FEATURE
  MIN_VAL = -10.0
  MAX_VAL = 1000.0
  UNITS   = MIL_MICRON
  GROUP   = HDI
  PROMPT  = Min. Pad AR Drill Bottom
}
FLOAT {
  NAME    = .ar_pad_drill_bottom_max
  ENTITY  = FEATURE
  MIN_VAL = -10.0
  MAX_VAL = 1000.0
  UNITS   = MIL_MICRON
  GROUP   = HDI
  PROMPT  = Max. Pad AR Drill Bottom
}

FLOAT {
  NAME    = .ar_sm_pad_top_min
  ENTITY  = FEATURE
  MIN_VAL = -10.0
  MAX_VAL = 1000.0
  UNITS   = MIL_MICRON
  GROUP   = HDI
  PROMPT  = Min. SM AR Top
}
FLOAT {
  NAME    = .ar_sm_pad_top_max
  ENTITY  = FEATURE
  MIN_VAL = -10.0
  MAX_VAL = 1000.0
  UNITS   = MIL_MICRON
  GROUP   = HDI
  PROMPT  = Max. SM AR Top
}
FLOAT {
  NAME    = .ar_sm_pad_bottom_min
  ENTITY  = FEATURE
  MIN_VAL = -10.0
  MAX_VAL = 1000.0
  UNITS   = MIL_MICRON
  GROUP   = HDI
  PROMPT  = Min. SM AR Bottom
}
FLOAT {
  NAME    = .ar_sm_pad_bottom_max
  ENTITY  = FEATURE
  MIN_VAL = -10.0
  MAX_VAL = 1000.0
  UNITS   = MIL_MICRON
  GROUP   = HDI
  PROMPT  = Max. SM AR Bottom
}

FLOAT {
  NAME    = .ar_sm_drill_top_min
  ENTITY  = FEATURE
  MIN_VAL = -10.0
  MAX_VAL = 1000.0
  UNITS   = MIL_MICRON
  GROUP   = HDI
  PROMPT  = Min. SM AR Drill Top
}
FLOAT {
  NAME    = .ar_sm_drill_top_max
  ENTITY  = FEATURE
  MIN_VAL = -10.0
  MAX_VAL = 1000.0
  UNITS   = MIL_MICRON
  GROUP   = HDI
  PROMPT  = Max. SM AR Drill Top
}
FLOAT {
  NAME    = .ar_sm_drill_bottom_min
  ENTITY  = FEATURE
  MIN_VAL = -10.0
  MAX_VAL = 1000.0
  UNITS   = MIL_MICRON
  GROUP   = HDI
  PROMPT  = Min. SM AR Drill Bottom
}
FLOAT {
  NAME    = .ar_sm_drill_bottom_max
  ENTITY  = FEATURE
  MIN_VAL = -10.0
  MAX_VAL = 1000.0
  UNITS   = MIL_MICRON
  GROUP   = HDI
  PROMPT  = Max. SM AR Drill Bottom
}

TEXT {
  NAME    = .physical_type
  ENTITY  = NET
  MIN_LEN = 0
  MAX_LEN = 64
  GROUP   = SQA
  PROMPT  = Physical Type
}

OPTION {
  NAME    = .comp_type2
  ENTITY  = COMPONENT;PACKAGE
  OPTIONS = axial;axial-large;bga;cbga;cob;csp;dip;dip300;dip600;discrete;discrete201;discrete402;discrete603;electro-mech;flipchip;label;lcc;lqfp;pfconn;pga;pihconn-inline;pihconn-rt-angle;pihmisc;pih-polar;plcc;pqfp;printed;qfp;radial;radial-tall;sip;smtconn;smtelect-mech;smtmisc;smtmixedconn;smtpolar;socket;soic;soj;solderable-mech;sop-ssop;sot;tab;tqfp;tsoic;tsop;tsop-tssop;reserved47;reserved48;reserved49;reserved50;reserved51;reserved52;reserved53;reserved54;reserved55;reserved56;reserved57;reserved58;reserved59;reserved60;reserved61;reserved62;reserved63;reserved64;reserved65;reserved66;reserved67;reserved68;reserved69;reserved70;reserved71;reserved72;reserved73;reserved74;reserved75;reserved76;reserved77;reserved78;reserved79;reserved80;reserved81;reserved82;reserved83;reserved84;reserved85;reserved86;reserved87;reserved88;reserved89;reserved90;reserved91;reserved92;reserved93;reserved94;reserved95;reserved96;reserved97;reserved98;reserved99;reserved100
  GROUP   = DFx Component
  PROMPT  = Type 2
}

BOOLEAN {
  NAME    = .no_copper_shape_under
  ENTITY  = COMPONENT;PACKAGE
  DEF     = NO
  GROUP   = DFx Component
  PROMPT  = No Shape Under
}
OPTION {
  NAME    = .hdi_assembly_tech
  ENTITY  = LAYER
  OPTIONS = None;WireBond;FlipChip;Hybrid
  DEF     = None
  PROMPT  = HDI Assembly Technology
}
TEXT {
  NAME    = .lead_form
  ENTITY  = COMPONENT;PACKAGE
  MIN_LEN = 1
  MAX_LEN = 64
  GROUP   = DFx Component
  PROMPT  = Lead Form
  OPTIONS = gw-small;gw;j-bend;l-bend;c-bend-wrap;side-concave;corner-concave;end-cap;other;flat-insulated;qfn;lcc;capae;m-column;lga;ball;axial;radial;inline;wire;pin;pressfit;mixed;n/a
}
TEXT {
  NAME    = .package_type
  ENTITY  = COMPONENT;PACKAGE
  MIN_LEN = 1
  MAX_LEN = 64
  GROUP   = DFx Component
  PROMPT  = Package Type
  OPTIONS = melf;axial;radial;cyl-tall;cc;chip;connector;fp;ga;sip-dip;smt-misc;so;thru-misc
}
FLOAT {
  NAME    = .pin_length
  ENTITY  = COMPONENT;PACKAGE
  MIN_VAL = 0.0
  MAX_VAL = 10.0
  UNITS   = INCH_MM
  GROUP   = Component
  PROMPT  = Pin Length
}
INTEGER {
  NAME    = .src_orientation
  ENTITY  = COMPONENT
  DEF     = -1
  MIN_VAL = -1
  MAX_VAL = 3
  GROUP   = DFx Component
  PROMPT  = Source Orientation
}
OPTION {
  NAME    = .rf
  ENTITY  = FEATURE
  OPTIONS = original;fixed
  GROUP   = Feature
  PROMPT  = Radio Frequency Surface
}
TEXT {
  NAME    = .string_font
  ENTITY  = FEATURE
  MIN_LEN = 0
  MAX_LEN = 100
  PROMPT  = String font
}
OPTION {
  NAME    = .layer_side
  ENTITY  = LAYER
  OPTIONS = Top;Bottom;Inner;None
  DEF     = None
  PROMPT  = Layer Side
}

TEXT {
  NAME    = .shorted_pins
  ENTITY  = COMPONENT;PACKAGE
  MIN_LEN = 0
  MAX_LEN = 1000
  PROMPT  = Shorted Pins
}

