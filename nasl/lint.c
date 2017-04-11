/* Nessus Attack Scripting Language "lint"
 *
 * Copyright (C) 2004 Michel Arboi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2,
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "nasl.h"
#include "nasl_tree.h"
#include "nasl_global_ctxt.h"
#include "nasl_func.h"
#include "nasl_var.h"
#include "nasl_lex_ctxt.h"
#include "exec.h"

#include "nasl_debug.h"
#include "nasl_init.h"

#ifndef NASL_DEBUG
#define NASL_DEBUG 0
#endif

#undef G_LOG_DOMAIN
/**
 * @brief GLib logging domain.
 */
#define G_LOG_DOMAIN "lib  nasl"

GSList* dont_check_list = NULL;

tree_cell *
nasl_lint_def (lex_ctxt * lexic, tree_cell * st, int lint_mode)
{
  int i;
  tree_cell *ret = FAKE_CELL;

  switch (st->type)
    {
    case NODE_FUN_DEF:

      if (lint_mode == 0)
        {
          if (decl_nasl_func (lexic, st, lint_mode) == NULL)
            ret = NULL;
          return ret;
        }

      /* x.str_val = function name, [0] = argdecl, [1] = block */
      decl_nasl_func (lexic, st, lint_mode);

    default:
      for (i = 0; i < 4; i++)
        if (st->link[i] != NULL && st->link[i] != FAKE_CELL)
          if ((ret = nasl_lint_def (lexic, st->link[i], lint_mode)) == NULL)
            return NULL;
      return ret;
    }
}


int stringcompare (char *list_data_a, char *list_data_b)
{
  return g_strcmp0 (list_data_a, list_data_b);
}


tree_cell *
nasl_lint_call (lex_ctxt * lexic, tree_cell * st, int *defined_flag)
{
  int i;
  tree_cell *ret = FAKE_CELL;
  nasl_func *pf;

  switch (st->type)
    {
    case CONST_DATA:
      if (st->x.str_val != NULL && *defined_flag == 1)
        {
          dont_check_list = g_slist_append (dont_check_list,
                                            (char*)(st->x.str_val));
          *defined_flag = 0;
          return FAKE_CELL;
        }
      return NULL;

    case NODE_FUN_CALL:
      if (st->x.str_val != NULL)
          if (g_slist_find_custom (dont_check_list, st->x.str_val,
                                   (GCompareFunc)stringcompare))
            return FAKE_CELL;

      pf = get_func_ref_by_name (lexic, st->x.str_val);
      if (pf == NULL)
        {
          nasl_perror (lexic, "Undefined function '%s'\n", st->x.str_val);
          return NULL;
        }
      if (g_strcmp0 (st->x.str_val, "defined_func") == 0)
        *defined_flag = 1;

    default:
      for (i = 0; i < 4; i++)
        if (st->link[i] != NULL && st->link[i] != FAKE_CELL)
          if ((ret = nasl_lint_call (lexic, st->link[i], defined_flag)) == NULL)
            return NULL;
      return ret;
    }
}


tree_cell *
nasl_lint (lex_ctxt * lexic, tree_cell * st)
{
  lex_ctxt * lexic_aux;
  tree_cell *ret = FAKE_CELL;
  int lint_mode = 1;
  int defined_flag = 0;

  lexic_aux = g_memdup (lexic, sizeof(struct struct_lex_ctxt));

  /* first loads all defined functions*/
  if ((ret = nasl_lint_def (lexic_aux, st, lint_mode)) == NULL)
    {
      g_free (lexic_aux);
      return ret;
    }

  /* check if a called function was defined */
  if ((ret = nasl_lint_call (lexic_aux, st, &defined_flag)) == NULL)
    {
      g_slist_free (dont_check_list);
      g_free (lexic_aux);
      return ret;
    }

  /* now check that each function was loaded just once */
  lint_mode = 0;
  nasl_lint_def (lexic, st, lint_mode);

  g_slist_free (dont_check_list);
  g_free (lexic_aux);
  return ret;
}
