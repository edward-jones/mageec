/*  GIMPLE Feature Extractor
    Copyright (C) 2013, 2014 Embecosm Limited and University of Bristol

    This file is part of MAGEEC.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */

/** @file feature-extract.cpp GIMPLE Feature Extractor. */
/* We need to undefine these as gcc-plugin.h redefines them */
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#ifndef BUILDING_GCC_MINOR
    #include "bversion.h"
    #define GCC_MINOR BUILDING_GCC_MINOR
#endif
#if (GCC_MINOR < 9)
  #include "gcc-plugin.h"
  #include "gimple-ssa.h"
  #include "ssa-iterators.h"
  #include "tree-core.h"
  #include "tree-ssa-operands.h"
  #include "tree-ssanames.h"
  #include "tree.h"
  #include "tree-pass.h"
  #include "gimple.h"
  #include "function.h"
  #include "basic-block.h"
  #include "toplev.h"
#else
  #include "gcc-plugin.h"
  #include "config.h"
  #include "system.h"
  #include "coretypes.h"
  #include "tm.h"
  #include "tree.h"
  #include "stringpool.h"
  #include "toplev.h"
  #include "basic-block.h"
  #include "pointer-set.h"
  #include "hash-table.h"
  #include "vec.h"
  #include "ggc.h"
  #include "basic-block.h"
  #include "tree-ssa-alias.h"
  #include "internal-fn.h"
  #include "gimple-fold.h"
  #include "tree-eh.h"
  #include "gimple-expr.h"
  #include "is-a.h"
  #include "gimple.h"
  #include "gimple-iterator.h"
  #include "tree.h"
  #include "tree-pass.h"
  #include "intl.h"
  #include "diagnostic.h"
  #include "context.h"
#endif

/* MAGEEC Headers */
#include "mageec-plugin.h"
#include "mageec/vectormath.h"
#include <iostream>
#include <vector>

using namespace mageec;

/**
 * Gate for whether to run the MAGEEC Feature Extractor.
 * Returns true to always run
 */
static bool mageec_featextract_gate(void)
{
  return true;
}

/**
 * MAGEEC Feature Extractor
 */
static unsigned mageec_featextract_exec(void)
{
  basic_block bb;
  gimple_stmt_iterator gsi;

  // Variables for holding feature information
  std::vector<unsigned> insn_counts;
  unsigned bb_count = 0;              // ft1
  unsigned bb_single_successor = 0;   // ft2
  unsigned bb_two_successors = 0;     // ft3
  unsigned bb_gt2_successors = 0;     // ft4
  unsigned bb_single_predecessor = 0; // ft5
  unsigned bb_two_predecessors = 0;   // ft6
  unsigned bb_gt2_predecessors = 0;   // ft7
  unsigned bb_1pred_1succ = 0;        // ft8
  unsigned bb_1pred_2succ = 0;        // ft9
  unsigned bb_2pred_1succ = 0;        //ft10
  unsigned bb_2pred_2succ = 0;        //ft11
  unsigned bb_gt2pred_gt2succ = 0;    //ft12
  unsigned insn_count_lt15 = 0;       //ft13
  unsigned insn_count_15_to_500 = 0;  //ft14
  unsigned insn_count_gt500 = 0;      //ft15
  unsigned method_assignments = 0;    //ft21
  unsigned average_phi_node_head = 0; //ft26
  unsigned average_phi_args = 0;      //ft27
  unsigned bb_phi_count_0 = 0;        //ft28
  unsigned bb_phi_count_0_to_3 = 0;   //ft29
  unsigned bb_phi_count_gt3 = 0;      //ft30
  unsigned bb_phi_args_gt5 = 0;       //ft31
  unsigned bb_phi_args_1_to_5 = 0;    //ft32
  unsigned method_switch_stmt = 0;    //ft33
  unsigned method_unary_ops = 0;      //ft34

  // Temporaries
  unsigned stmt_count = 0;
  unsigned phi_nodes = 0;
  unsigned phi_args = 0;
  bool in_phi_header = false;         //switch for ft26
  unsigned phi_header_nodes = 0;      //total for ft26
  unsigned total_phi_args = 0;        //total for ft27
  unsigned total_phi_nodes = 0;       //divisor for ft27

#if (GCC_MINOR < 9)
  FOR_EACH_BB (bb)
#else
  FOR_ALL_BB_FN (bb, cfun)
#endif
  {
    stmt_count = 0;
    phi_nodes = 0;
    phi_args = 0;
    in_phi_header = true;

    // For this block count instructions and types
    bb_count++;
    for (gsi=gsi_start_bb(bb); !gsi_end_p(gsi); gsi_next(&gsi))
    {
      gimple stmt = gsi_stmt (gsi);
      stmt_count++;
      // Assignment analysis
      if (is_gimple_assign (stmt))
      {
        method_assignments++;
        enum gimple_rhs_class grhs_class =
          get_gimple_rhs_class (gimple_expr_code (stmt));
        if (grhs_class == GIMPLE_UNARY_RHS)
          method_unary_ops++;
      }
      if (gimple_code (stmt) == GIMPLE_PHI)
      {
        phi_nodes++;
        total_phi_nodes++;
        if (in_phi_header)
          phi_header_nodes++;
        phi_args += gimple_phi_num_args (stmt);
        total_phi_args += gimple_phi_num_args (stmt);
      }
      else
        in_phi_header = false;
      if (gimple_code (stmt) == GIMPLE_SWITCH)
        method_switch_stmt++;
    }

    // Successor/predecessor information
    if (EDGE_COUNT(bb->succs) == 1)
      bb_single_successor++;
    else if (EDGE_COUNT(bb->succs) == 2)
      bb_two_successors++;
    else if (EDGE_COUNT(bb->succs) > 2)
      bb_gt2_successors++;
    if (EDGE_COUNT(bb->preds) == 1)
      bb_single_predecessor++;
    else if (EDGE_COUNT(bb->preds) == 2)
      bb_two_predecessors++;
    else if (EDGE_COUNT(bb->preds) > 2)
      bb_gt2_predecessors++;
    if ((EDGE_COUNT(bb->preds) == 1) && (EDGE_COUNT(bb->succs) == 1))
      bb_1pred_1succ++;
    if ((EDGE_COUNT(bb->preds) == 1) && (EDGE_COUNT(bb->succs) == 2))
      bb_1pred_2succ++;
    if ((EDGE_COUNT(bb->preds) == 2) && (EDGE_COUNT(bb->succs) == 1))
      bb_2pred_1succ++;
    if ((EDGE_COUNT(bb->preds) == 2) && (EDGE_COUNT(bb->succs) == 2))
      bb_2pred_2succ++;
    if ((EDGE_COUNT(bb->preds) > 2) && (EDGE_COUNT(bb->succs) > 2))
      bb_gt2pred_gt2succ++;

    // Store processed data about this block
    insn_counts.push_back(stmt_count);
    if (stmt_count < 15)
      insn_count_lt15++;
    else if (stmt_count > 500)
      insn_count_gt500++;
    else
      insn_count_15_to_500++;

    if (phi_nodes == 0)
      bb_phi_count_0++;
    if (phi_nodes <= 3)
      bb_phi_count_0_to_3++;
    else
      bb_phi_count_gt3++;
    if (phi_args > 5)
      bb_phi_args_gt5++;
    else if ((phi_args >= 1) && (phi_args <= 5))
      bb_phi_args_1_to_5++;
  }

  // Calculate averages once totals have been collected
  if (total_phi_nodes > 0)
    average_phi_args = total_phi_args / total_phi_nodes;
  average_phi_node_head = phi_header_nodes / bb_count;

  /* Build feature vector to pass to machine learner */
  std::vector<mageec::mageec_feature*> features;
  features.push_back(new basic_feature("1", "Basic Block Count", bb_count));
  features.push_back(new basic_feature("2", "BB with 1 successor", bb_single_successor));
  features.push_back(new basic_feature("3", "BB with 2 successor", bb_two_successors));
  features.push_back(new basic_feature("4", "BB with > 2 successor", bb_gt2_successors));
  features.push_back(new basic_feature("5", "BB with 1 predecessor", bb_single_predecessor));
  features.push_back(new basic_feature("6", "BB with 2 predecessor", bb_two_predecessors));
  features.push_back(new basic_feature("7", "BB with > 2 predecesso", bb_gt2_predecessors));
  features.push_back(new basic_feature("8", "BB with 1 pred 1 succ", bb_1pred_1succ));
  features.push_back(new basic_feature("9", "BB with 1 pred 2 succ", bb_1pred_2succ));
  features.push_back(new basic_feature("10", "BB with 2 pred 1 succ", bb_2pred_1succ));
  features.push_back(new basic_feature("11", "BB with 2 pred 2 succ", bb_2pred_2succ));
  features.push_back(new basic_feature("12", "BB with >2 pred >2 suc", bb_gt2pred_gt2succ));
  features.push_back(new basic_feature("13", "BB with insn < 15", insn_count_lt15));
  features.push_back(new basic_feature("14", "BB with insn [15, 500]", insn_count_15_to_500));
  features.push_back(new basic_feature("15", "BB with insn > 500", insn_count_gt500));
  features.push_back(new basic_feature("21", "Assignments in method", method_assignments));
  features.push_back(new basic_feature("24", "Total Statement in BB", vector_sum(insn_counts)));
  features.push_back(new basic_feature("25", "Avg Statement in BB", vector_sum(insn_counts)/bb_count));
  features.push_back(new basic_feature("26", "Avg phis at top of BB", average_phi_node_head));
  features.push_back(new basic_feature("27", "Average phi arg count", average_phi_args));
  features.push_back(new basic_feature("28", "BB with 0 phis", bb_phi_count_0));
  features.push_back(new basic_feature("29", "BB with [0, 3] phis", bb_phi_count_0_to_3));
  features.push_back(new basic_feature("30", "BB with > 3 phis", bb_phi_count_gt3));
  features.push_back(new basic_feature("31", "BB phis with > 5 args", bb_phi_args_gt5));
  features.push_back(new basic_feature("32", "BB phis with [1,5] arg", bb_phi_args_1_to_5));
  features.push_back(new basic_feature("33", "Switch stmts in method", method_switch_stmt));
  features.push_back(new basic_feature("34", "Unary ops in method", method_unary_ops));

  mageec_inst.take_features(current_function_name(), features);

  // Print feature vector, first as a list and then as JSON
  std::cerr << "Current Function: " << current_function_name() << std::endl;
  mageec_feature::dump_vector(features, std::cerr, false);
  mageec_feature::dump_vector(features, std::cerr, true);

  return 0;
}

/**
 * MAGEEC Feature Extractor Pass Definition
 */
#if (GCC_MINOR < 9)
static struct gimple_opt_pass mageec_featextract =
{
  {
    GIMPLE_PASS,
    "mageec-extractor",                   /* name */
    OPTGROUP_NONE,                        /* optinfo_flags */
    mageec_featextract_gate,              /* gate */
    mageec_featextract_exec,              /* execute */
    NULL,                                 /* sub */
    NULL,                                 /* next */
    0,                                    /* static_pass_number */
    TV_NONE,                              /* tv_id */
    PROP_ssa,                             /* properties_required */
    0,                                    /* properties_provided */
    0,                                    /* properties_destroyed */
    0,                                    /* todo_flags_start */
    0                                     /* todo_flags_finish */
  }
};
#else
namespace {

const pass_data pass_data_mageec_featextract = 
{
  GIMPLE_PASS, /* type */
  "mageec-extractor", /* name */
  OPTGROUP_NONE, /* optinfo_flags */
  true, /* has_gate */
  true, /* has_execute */
  TV_NONE, /* tv_id */
  PROP_ssa, /* properties_required */
  0, /* properties_provided */
  0, /* properties_destroyed */
  0, /* todo_flags_start */
  0, /* todo_flags_finish */
};

class mageec_featpass : public gimple_opt_pass
{
public:
  mageec_featpass(gcc::context *ctxt)
    : gimple_opt_pass(pass_data_mageec_featextract, ctxt)
  {}

  /* opt_pass methods: */
  bool gate () { return mageec_featextract_gate (); }
  unsigned int execute () { return mageec_featextract_exec (); }

}; // class mageec_featpass

}

static gimple_opt_pass *
make_mageec_pass (gcc::context *ctxt)
{
  return new mageec_featpass (ctxt);
}
#endif

/**
 * Registers the Feature Extractor in the pass list
 * Currently runs after the cfg pass
 */
void register_featextract (void)
{
  struct register_pass_info pass;

#if (GCC_MINOR < 9)
  pass.pass = &mageec_featextract.pass;
#else
  pass.pass = make_mageec_pass (g);
#endif
  pass.reference_pass_name = "ssa";
  pass.ref_pass_instance_number = 1;
  pass.pos_op = PASS_POS_INSERT_AFTER;

  register_callback (mageec_gcc_plugin_name, PLUGIN_PASS_MANAGER_SETUP, NULL,
                     &pass);
}
