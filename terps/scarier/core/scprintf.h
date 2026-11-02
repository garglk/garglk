/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
 * Copyright (C) 2026  Petter Sjölund
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * Prototypes for scprintf*.cpp: the print filter and its output buffer.
 */

#ifndef SCARIER_PRINTF_H
#define SCARIER_PRINTF_H

#include <stddef.h>
#include <string>
#include "scprops.h"
#include "scvars.h"

/* Print filtering opaque typedef and functions. */
typedef struct scr_filter_s *scr_filterref_t;
extern scr_filterref_t pf_create (void);
extern void pf_destroy (scr_filterref_t filter);
extern void pf_buffer_string (scr_filterref_t filter,
                              const scr_char *string);
extern void pf_buffer_character (scr_filterref_t filter,
                                 scr_char character);
extern void pf_buffer_integer (scr_filterref_t filter, scr_int value);
extern void pf_buffer_paragraph_line (scr_filterref_t filter,
                                      const scr_char *string);
extern void pf_buffer_paragraph_break (scr_filterref_t filter);
extern scr_bool pf_undo_auto_break (scr_filterref_t filter);
extern void pf_note_trailing_auto_break (scr_filterref_t filter);
extern void pf_buffer_answer_break (scr_filterref_t filter);
extern void pf_buffer_pspace (scr_filterref_t filter);
extern scr_bool pf_has_hidden_prefix (scr_filterref_t filter);
extern void pf_buffer_join_open (scr_filterref_t filter);
extern void pf_buffer_join_line (scr_filterref_t filter,
                                 const scr_char *string);
extern scr_bool pf_ends_with_double_space (scr_filterref_t filter);
extern void pf_buffer_join (scr_filterref_t filter,
                            const scr_char *string);
extern scr_bool pf_buffer_tentative_join (scr_filterref_t filter);
extern void pf_retract_tentative_join (scr_filterref_t filter);
extern void pf_buffer_hard_break (scr_filterref_t filter);
extern void pf_buffer_join_pending (scr_filterref_t filter);
extern void pf_clear_join_pending (scr_filterref_t filter);
extern void pf_prepend_string (scr_filterref_t filter,
                               const scr_char *string);
extern size_t pf_buffer_length (scr_filterref_t filter);
extern void pf_hoist_tail (scr_filterref_t filter, size_t from);
extern void pf_truncate (scr_filterref_t filter, size_t length);
extern std::string pf_buffer_tail (scr_filterref_t filter, size_t from);
extern std::string pf_cut_tail (scr_filterref_t filter, size_t from);
extern scr_int pf_printed_to (scr_filterref_t filter);
extern void pf_restore_printed_to (scr_filterref_t filter, scr_int note);
extern void pf_erase (scr_filterref_t filter, size_t from, size_t to);
extern void pf_new_sentence (scr_filterref_t filter);
extern void pf_mute (scr_filterref_t filter);
extern void pf_clear_mute (scr_filterref_t filter);
extern void pf_buffer_tag (scr_filterref_t filter, scr_int tag);
extern void pf_strip_tags (scr_char *string);
extern void pf_strip_tags_for_hints (scr_char *string);
extern scr_char *pf_filter (const scr_char *string,
                           scr_var_setref_t vars, scr_prop_setref_t bundle);
extern scr_bool pf_alr_mentions (scr_prop_setref_t bundle,
                                 const scr_char *text);
extern scr_bool pf_alr_rewrites_span (scr_prop_setref_t bundle,
                                      const scr_char *text,
                                      size_t start, size_t end);
extern scr_char *pf_filter_for_info (const scr_char *string,
                                    scr_var_setref_t vars);
extern void pf_flush (scr_filterref_t filter,
                      scr_var_setref_t vars, scr_prop_setref_t bundle);
extern void pf_print_so_far (scr_filterref_t filter,
                             scr_var_setref_t vars, scr_prop_setref_t bundle);
extern void pf_checkpoint (scr_filterref_t filter,
                           scr_var_setref_t vars, scr_prop_setref_t bundle);
/*
 * As pf_checkpoint(), but leaving the buffer marked as still needing to be
 * filtered, so that the flush filters it again; the extra pass a completing
 * version 4.0 task runs over the turn's output.
 */
extern void pf_refilter (scr_filterref_t filter,
                         scr_var_setref_t vars, scr_prop_setref_t bundle);
/*
 * The text pf_flush() has printed since the last call, taken and cleared;
 * and a way to buffer such already-filtered text back, for undo's replay.
 */
extern std::string pf_take_printed (scr_filterref_t filter);
extern const std::string &pf_get_printed (scr_filterref_t filter);
extern void pf_set_printed (scr_filterref_t filter, const std::string &text);
extern void pf_buffer_printed (scr_filterref_t filter,
                               scr_var_setref_t vars, scr_prop_setref_t bundle,
                               const std::string &text);
extern const scr_char *pf_get_buffer (scr_filterref_t filter);
extern scr_char *pf_transfer_buffer (scr_filterref_t filter);
/*
 * Hide the buffer from the paragraph-spacing helpers while version 4.0 task
 * actions run, the way pf_transfer_buffer() does for pre-4.0.
 */
extern void pf_buffer_reference (scr_filterref_t filter, const scr_char *text,
                                 scr_var_setref_t vars,
                                 scr_prop_setref_t bundle);
extern void pf_rebuffer_reference (scr_filterref_t filter,
                                   const scr_char *text);
extern std::string pf_leading_reference (scr_filterref_t filter);
extern size_t pf_hide_prefix (scr_filterref_t filter);
extern void pf_reveal_prefix (scr_filterref_t filter, size_t previous);
extern void pf_empty (scr_filterref_t filter);
extern scr_char *pf_escape (const scr_char *string);
extern scr_char *pf_filter_input (const scr_char *string,
                                 scr_prop_setref_t bundle,
                                 scr_bool take_to_get = TRUE,
                                 scr_bool runner_synonyms = TRUE,
                                 scr_bool builtins = TRUE);
extern void pf_debug_trace (scr_bool flag);
extern scr_bool pf_text_ends_with_break (const scr_char *text);

#endif
