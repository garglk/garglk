/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- text engine (Phase 2 MVP).
 *
 * Turns the model's <Description> blocks into display text: it evaluates the
 * SingleDescription list (DisplayWhen + per-description restrictions), runs the
 * %function% / %variable% substitution and ALR ("Text Override") passes, applies
 * ADRIFT's sentence auto-capitalisation, and renders the resulting HTML-ish
 * markup (<br>, <c>, <b>, <i>, entities) down to plain text.
 *
 * Covers the variables, core functions and tags the static world needs; the
 * full perspective/pronoun/UDF engine is Phase 4.  Mirrors the Adrift 5 runner
 * SharedModule.Description.ToString, ReplaceALRs, ReplaceFunctions and the
 * GlkHtmlWin tag renderer.
 *
 * Every returned char* is heap-allocated; the caller frees it.
 */

#ifndef SCARIER_A5TEXT_H
#define SCARIER_A5TEXT_H

#include "a5state.h"
#include "a5xml.h"

/* Sentinel byte relayed by a5text_render_plain when it renders a <cls> screen
   clear.  The Adrift 5 runner's Display accumulates a whole turn's text into one
   sOutputText string and renders it once at end of turn, so a <cls> embedded
   anywhere in the turn wipes EVERYTHING accumulated before it -- not just the
   fragment it appears in.  The plain renderer can only wipe its own fragment,
   so it also leaves this marker; the per-turn flush (a5run finish_turn) drops
   everything up to and including the last marker.  \x01 (SOH) never occurs in
   game text. */
#define A5_CLS_MARK '\001'

/* Sentinel byte marking "the Adrift 5 runner's raw output buffer would end in a non-vbLf
   character here" -- appended by the message renderers when a message's raw
   (markup-bearing) text ends in a stripped tag / <br> / entity rather than a real
   newline, but the stripped plain text ends in '\n'.  The runner applies pSpace to the RAW
   buffer, so such a message leaves the buffer non-newline and the NEXT message
   space-joins with two spaces; Scarier strips markup per message, so its buffer
   would end in the '\n' that preceded the trailing tag and it would NOT join.
   sb_pspace treats this marker like any non-newline tail (adds the join spaces);
   finish_turn strips it before the text is shown.  \x02 (STX) never occurs in
   game text. */
#define A5_PS_MARK '\002'

/* Sentinel byte marking "a formatting tag was stripped here" in plain-rendered
   text.  The runner's ReplaceALRs runs at Display time over the still-MARKED-UP text,
   so a tag EMBEDDED INSIDE an ALR OldText span blocks the match (AoS names its
   Known guard "The guard<Halberd>", whose <Halberd> defeats the game's own
   "The guard comes marching from above" override; the real Runner shows the
   unreplaced text with the tag stripped).  Scarier's per-fragment ALR pass sees
   the markup too and blocks identically, but the display-boundary ALR pass
   (a5text_display_alr) re-runs over the accumulated PLAIN text, where the
   stripped tag would otherwise leave the two halves adjacent and let the match
   succeed.  The plain renderer therefore drops this marker where a tag vanished,
   so boundary matching is blocked exactly where the runner's is; finish_turn strips the
   markers after the boundary pass.  \x03 (ETX) never occurs in game text. */
#define A5_ALR_MARK '\003'

/* Sentinel byte standing for "a <del> that found nothing left to delete in its
   own message".  Like <cls>, <del> is a whole-turn operator: the runner's
   Display accumulates the turn into one buffer and a <del> deletes one
   character from ALL of it, so a <del> at the head of a message reaches back
   over the pSpace join and into the PREVIOUS message's tail.  Measured in the
   genuine 5.0.36 Runner with test/adrift5/probes/src/delrt.xml case 8: message
   "8[aZ" followed by an Execute-Task message "<del><del><del>]END8" prints
   "8[a]END8" -- the three deletes eat the two join spaces and then the Z.
   a5text_render_plain can only reach its own fragment, so when its backward
   walk comes up empty it leaves this marker instead and sb_resolve_del (called
   from finish_turn, after the <cls> pass) applies it to the accumulated turn
   buffer.  \x1F (US) never occurs in game text; \x04 is taken by the
   display-defer `\004<idx>\004` sentinel. */
#define A5_DEL_MARK '\037'

/* Sentinel pair wrapping a variable NAME whose expansion was deferred to the
   Display boundary.  The Adrift 5 runner applies ALRs only inside Display()
   (ReplaceALRs, Global.vb:519), whose leading ReplaceFunctions call expands any
   %variable% the ALR NewText carries with the END-OF-TURN value -- so a game
   ALR like Murder Most Foul's `<s1>` -> "Your score has increased ... to
   %scor%." reports the total AFTER the awarding task's IncVariable ran.
   Scarier applies ALRs eagerly at message render time; a bare %var% inside an
   ALR replacement is therefore emitted as \005name\005 and resolved by
   a5text_expand_var_defers (called from finish_turn, before the boundary ALR
   pass -- the runner's functions-before-ALRs Display order).  \x05 (ENQ) never occurs
   in game text. */
#define A5_VARDEF_MARK '\005'

/* Sentinels used only in INTERACTIVE mode (a5text_set_interactive), where the
   host front-end presents the turn text itself instead of receiving the
   the Adrift 5 runner-conformant pre-resolved transcript.  In this mode:

     - <cls> no longer wipes the accumulated text; the A5_CLS_MARK stays in the
       stream so the host can SHOW the pre-clear text (a title page like Anno
       1700's credits/cover) and then clear its window at the mark, exactly
       like the real Runner's output pane.
     - <waitkey> leaves A5_WAITKEY_MARK; the host pauses for a keypress there.
     - <img src="..."> leaves an A5_IMG_MARK-delimited resource number,
       \006<number>\006, at the tag's position (the number is the Blorb Pict
       resource the media sink resolved the src path to), so the host can draw
       the image inline where the author placed it.
     - <center>/<centre> and the matching close tags leave A5_CENTER_MARK and
       A5_ENDCENTER_MARK, so the host can show the span between them centered
       (a Glk host through a justification-hinted style).
     - <b> and </b> leave A5_BOLD_MARK and A5_ENDBOLD_MARK, so the host can
       show the span between them in bold (a Glk host through style_Subheader,
       or style_User2 when the span is also centered).
     - <i> and </i> leave A5_ITALIC_MARK and A5_ENDITALIC_MARK, so the host
       can show the span in italic (style_Emphasized, or style_Alert when
       combined with bold).  Alignment styles win over italic when nested.
     - <u> and </u> leave A5_UNDERLINE_MARK and A5_ENDUNDERLINE_MARK.  Until a
       Glk host can paint real underline (e.g. via CSS), the same styles as
       italic are used; the marks stay distinct so underline can be wired
       later without re-parsing tags.
     - <right> and </right> leave A5_RIGHT_MARK and A5_ENDRIGHT_MARK, so the
       host can show the span right-aligned (a Glk host through a
       RightFlush-hinted style_Note).
     - <window NAME> leaves A5_WINDOW_MARK<name>A5_WINDOW_MARK (the name span
       delimited like an image), and </window> leaves A5_ENDWINDOW_MARK, so the
       host can route the enclosed text to a named secondary window (a Glk host
       through a side text-buffer window).  A <cls> inside the span clears that
       window, not the main story window.
     - <audio ...> leaves an A5_SOUND_MARK-delimited media-list index,
       \024<index>\024 (the a5run_media_* slot the sink recorded the event
       in), so the host can start/stop the sound where the tag sits in the
       text -- BEFORE any later <waitkey> pause, the way the Runner's
       DisplayText acts on an audio tag the moment it reaches it (Pervert
       Action Crisis strikes its sting ahead of a keypress-paced cutscene).
     - <c> and <font colour="..."> leave an A5_COLOUR_MARK-delimited colour
       span, \027<value>\027, and their close tags leave A5_ENDCOLOUR_MARK, so
       the host can draw the enclosed text in the Adrift colour the author
       asked for (a Glk host through garglk_set_zcolors / CSS color, under
       "glk colour").
       <value> is the tag's colour token verbatim and lowercased -- a name
       ("red"), a hex triplet ("#ff0000") -- or empty for a <font> that names
       no colour, which inherits the enclosing one so that its </font> still
       pops symmetrically.  <c> writes the reserved token "input", since the
       colour it asks for is the adventure's InputColour rather than anything
       spelled out in the text; no Adrift colour name collides with it.
     - <u> / </u> leave A5_UNDERLINE_MARK / A5_ENDUNDERLINE_MARK.
     - <font face="..." size=...> (with or without colour=) leaves an
       A5_FONT_MARK-delimited payload \010face\177size\010 (face and/or size
       may be empty; \177 separates them) and </font> leaves A5_ENDFONT_MARK
       when a face/size was pushed, so the host can set CSS font-family /
       font-size.  Colour and face/size stack independently on the same
       <font> open.  The open mark is \x08 (BS), not A5_DEL_MARK (\x1F), and
       the close is \x0B (VT), not the display-defer `\004…\004` sentinel.
     - a <center>, <right>, <b>, <i>, <u>, colour, or font face/size span still
       open when a Display commit ends dies with that commit: the Runner
       renders each commit through its own Source2HTML parse, so an unclosed
       tag never bleeds into the next commit's text.  The turn assembler
       (sb_resolve_cls, a5sb.cpp) leaves A5_COMMIT_MARK at a boundary whose
       commit dangles a span, and the host resets its span state there --
       Death Shack's Introduction opens <center> and never closes it, yet the
       Runner shows the first room description (the next commit,
       clsUserSession.vb game-start) left-aligned.
     - <wait N> leaves an A5_WAIT_MARK-delimited delay, \026<seconds>\026
       (the tag's argument verbatim; fractions allowed), so the host can run
       a timed pause where the tag sits, the way the Runner's rendering
       pauses mid-Display -- Death Shack types its 80-point title one letter
       per <wait 3>.  A bare <wait> carries an empty span and pauses not at
       all, like the ADRIFT 4 path's unparsable-argument case.

   finish_turn keeps all of these in the returned turn text; a host that never
   enables interactive mode (the headless dump / ground-truth harness) sees no
   behaviour change.  \x06 (ACK), \x07 (BEL), \x08 (BS), \x0b (VT), \x0e (SO),
   \x0f (SI), \x10 (DLE),
   \x11 (DC1), \x12 (DC2), \x13 (DC3), \x14 (DC4), \x15 (NAK), \x16 (SYN),
   \x17 (ETB), \x18 (CAN), \x19 (EM), \x1a (SUB), \x1b (ESC), \x1c (FS),
   \x1d (GS), \x1e (RS), \x1f (US) and \x04 (EOT) never occur in game text. */
#define A5_IMG_MARK '\006'
#define A5_WAITKEY_MARK '\007'
#define A5_CENTER_MARK '\016'
#define A5_ENDCENTER_MARK '\017'
#define A5_BOLD_MARK '\020'
#define A5_ENDBOLD_MARK '\021'
#define A5_WINDOW_MARK '\022'
#define A5_ENDWINDOW_MARK '\023'
#define A5_SOUND_MARK '\024'
#define A5_COMMIT_MARK '\025'
#define A5_WAIT_MARK '\026'
#define A5_COLOUR_MARK '\027'
#define A5_ENDCOLOUR_MARK '\030'
#define A5_ITALIC_MARK '\031'
#define A5_ENDITALIC_MARK '\032'
#define A5_RIGHT_MARK '\033'
#define A5_ENDRIGHT_MARK '\034'
#define A5_UNDERLINE_MARK '\035'
#define A5_ENDUNDERLINE_MARK '\036'
#define A5_FONT_MARK '\010'
#define A5_ENDFONT_MARK '\013'
/* Separator inside an A5_FONT_MARK payload between face and size. */
#define A5_FONT_SEP '\177'


/* Interactive-presentation mode toggle (default off; see marks above). */
extern void a5text_set_interactive (int on);
extern int  a5text_interactive (void);

/* Expand any \005name\005 deferred-variable sentinels in `text` with the
   variable's CURRENT value (see A5_VARDEF_MARK).  Heap; never NULL. */
extern char *a5text_expand_var_defers (a5_state_t *st, const char *text);

/*
 * Evaluate a description wrapper node (e.g. a location's <LongDescription>, an
 * object's <Description>, the <Introduction>) into raw source text with markup
 * still embedded.  Returns "" (never NULL) if node is NULL/empty.
 */
extern char *a5text_eval_description (a5_state_t *st, const a5_xml_node_t *wrapper);
/* The effective short description of a location (base <ShortDescription> plus any
   inherited ShortLocationDescription group-property alternates -- darkness). */
extern char *a5text_location_short (a5_state_t *st, const char *lockey);
/* As a5text_location_short but fully rendered to plain text (the room NAME). */
extern char *a5text_location_short_plain (a5_state_t *st, const char *lockey);

/* Run the %function%/%variable% and ALR passes + auto-capitalisation on src. */
extern char *a5text_process (a5_state_t *st, const char *src);

/* Like a5text_process but WITHOUT the trailing auto-capitalisation -- for
   evaluating a value to an entity KEY (case-sensitive), where capitalising the
   first letter ("s_SkeletonKe" -> "S_SkeletonKe") would corrupt the key.  The runner
   applies auto-cap only at Display time, never when resolving a reference. */
extern char *a5text_process_nocap (a5_state_t *st, const char *src);

/* Like a5text_process_nocap but WITHOUT the ALR pass -- for evaluating ACTION
   values (Set/Inc/DecVariable amounts).  The runner evaluates those through
   EvaluateExpression only; ReplaceALRs is Display-time, so a game's display
   ALR (GFS "17000" -> "1.700.0") must not rewrite a stored number. */
extern char *a5text_process_noalr (a5_state_t *st, const char *src);

/* Would a lowercase letter at byte offset `pos` of `text` be capitalised by the
   runner's CapAfterFullStop?  For the deferred-draw flush, which splices its
   values in after a5text_process's cap pass has already run. */
extern int a5text_is_sentence_start (const char *text, size_t pos);

/* Evaluate a raw expression string (Global.EvaluateExpression): substitute its
   %references%/OO chains then reduce to a string value.  Heap; never NULL. */
extern char *a5text_eval_expression (a5_state_t *st, const char *expr);

/* Render markup (tags + entities) to plain text. */
extern char *a5text_render_plain (const char *src);

/* Compact an already-rendered plain string in place, dropping the presentation
   sentinels defined above -- the non-spanning ones outright, the spanning ones
   with their payload.  For text a host draws OUTSIDE the story window, where
   finish_turn's own strip pass never runs and a sentinel would be drawn as a
   glyph: Alyas of Starhollow tells its four River Lane rooms apart by naming
   them "River Lane<1>".."<4>", and the stripped <4> leaves the A5_ALR_MARK
   that Gargoyle shows as an ETX box at the end of the status line. */
extern void a5text_strip_pres_marks (char *s);

/*
 * Embedded-media side channel.  ADRIFT 5 embeds graphics/sound in description
 * text as <img src="..."> and <audio play|stop|pause src="..." channel=N> tags
 * (channel defaults to 1 when absent, as in the Runner's tag parse).
 * a5text_render_plain always drops these from the plain text (so the text output
 * stays byte-identical), but when a media sink is installed it also reports each
 * one, so the host (Glk driver) can show images / play sounds out of band.  src
 * is the original file path (resolve via a5model_resource_for_file); channel/loop
 * apply to sound.  A NULL callback (the default) disables reporting.
 */
enum { A5_MEDIA_IMAGE = 1, A5_MEDIA_SOUND = 2, A5_MEDIA_SOUND_STOP = 3,
       A5_MEDIA_SOUND_PAUSE = 4 };
/* For an image the callback returns the Blorb resource number the src path
   resolved to (or -1 when unknown); the renderer uses it in interactive mode
   to leave the positional \006<number>\006 mark (see A5_IMG_MARK).  For a
   sound it returns the sink's slot for the recorded event (a5run: the
   media-list index) or -1 to suppress the mark; in interactive mode the
   renderer leaves it as the positional \024<index>\024 mark (A5_SOUND_MARK). */
typedef int (*a5_media_cb) (void *ctx, int kind, const char *src,
                            int channel, int loop);
extern void a5text_set_media_sink (a5_media_cb cb, void *ctx);

/*
 * Scripted-input side channel for the %PopUpInput[prompt, default]% text
 * function (ADRIFT's clsFunction PopUpInput -> VB InputBox).  Interactively the
 * engine would pop a modal text box; headless, a host may instead feed the next
 * line of the command script so naming puzzles (e.g. Six Silver Bullets' "name
 * the agent") are scriptable and reproducible.  The callback is handed the
 * prompt and the author's default and returns a heap-allocated answer the
 * engine takes ownership of (frees), or NULL to fall back to the default.  With
 * no callback installed (the default) PopUpInput evaluates to its default,
 * matching the Adrift 5 runner's InputBox returning the default when unattended.  The
 * FrankenDrift.Headless frontend consumes exactly one script line per popup the
 * same way, so ground-truth transcripts stay byte-aligned.  PopUpInput ONLY:
 * %PopUpChoice% goes through MsgBox in the runner, not the scripted-input
 * channel, and must never be fed from this one -- that would desync the
 * transcript by a line.  It has a channel of its own; see below.
 */
typedef char *(*a5_popup_cb) (void *ctx, const char *prompt, const char *dflt);
extern void a5text_set_popup_cb (a5_popup_cb cb, void *ctx);

/*
 * Host callback for the %PopUpChoice[prompt, choice1, choice2]% text function
 * (clsFunction PopUpChoice -> VB MsgBox, Global.vb:2278): a Yes/No dialog whose
 * Yes yields choice1 and No choice2, in practice the gender question a game
 * asks before play (Beagle2's System <RunImmediately> Autorun, "Are you Male or
 * Female?").  The callback is handed the prompt and both choices and returns
 * non-zero for Yes, 0 for No, or negative to decline -- which leaves the token
 * unevaluated, as with no callback installed at all.
 *
 * Unevaluated is the default because it is what the ground truth does: MsgBox
 * throws off-Windows, so FrankenDrift lands in the ReplaceFunctions catch
 * (Global.vb:2483) and the %PopUpChoice[...]% text survives verbatim -- no
 * output, and no script line consumed.  Only an interactive host with somewhere
 * to ask (the Glk frontend, its story window standing in for the dialog) should
 * install one, reproducing what the runner does on Windows without disturbing
 * headless transcripts.
 */
typedef int (*a5_popup_choice_cb) (void *ctx, const char *prompt,
                                   const char *choice1, const char *choice2);
extern void a5text_set_popup_choice_cb (a5_popup_choice_cb cb, void *ctx);

/*
 * The Display() ALR boundary (clsUserSession.Display -> Global.ReplaceALRs):
 * apply the game's ALR ("Text Override") substitutions + auto-capitalisation to
 * an already-assembled, already-plain turn output, then render any markup an ALR
 * NewText introduced.  This is how a non-English game (e.g. Danish Halloween)
 * translates the engine's stock literals (NotUnderstood, "Also here is ",
 * "Time passes...") -- which never passed through a5text_process per fragment.
 * A no-op for a game without ALRs.  Heap; never NULL (caller frees).
 */
extern char *a5text_display_alr (a5_state_t *st, const char *plain);

/*
 * Convenience: eval_description -> process -> render_plain, i.e. the finished
 * plain text for a description wrapper.
 */
extern char *a5text_describe (a5_state_t *st, const a5_xml_node_t *wrapper,
                              int *raw_nonblank = 0);
extern char *a5text_describe_marked (a5_state_t *st, const a5_xml_node_t *wrapper);

/* a5text_describe + report whether the text had visible content BEFORE the ALR
   pass (the runner's bHasOutput runs pre-ALR; a game ALR mapping a phrase to nothing
   still leaves the response's paragraph slot in the runner's output).
   `raw_nonblank`, when non-NULL, receives whether the segment-evaluated RAW
   template held ANY character at all -- the runner's AddResponse output test
   (bHasOutput) sees that raw text, so even a whitespace-only "\n" completion
   counts as task output there (it stops an After-children scan) although the
   rendered plain text trims to nothing (The Salvage's per-move station-known
   task suppresses the fuel-consumption child that way). */
/* *marked (optional, caller-freed) additionally returns the still-MARKED-UP
   render -- what the runner's AddResponse stores and keys htblResponsesPass on,
   before Display() strips the tags.  Two responses whose only difference is
   inside a stripped tag are DISTINCT there but identical in the plain text
   (Beginner's Cave defeats the dedup on purpose with `<# "<" & rand(0,1000000)
   & ">" #>` so each "Dodged!" prints), so the dedup has to key on this. */
extern char *a5text_describe_ex (a5_state_t *st, const a5_xml_node_t *wrapper,
                                 int *pre_alr_ink, int *raw_nonblank = 0,
                                 char **marked = 0);

/* a5text_describe_ex on a PRE-FROZEN raw template from a5text_eval_description:
   function/OO/expression/ALR passes only, segment selection NOT re-evaluated
   (the runner re-renders its captured sMessage string around a Before task's actions;
   pre_alr_ink may be NULL). */
/* *marked (optional, caller-freed) additionally returns the still-MARKED-UP
   render -- the string the runner actually compares across the actions (its
   sMessage still carries the markup; Display() strips it only at output). */
extern char *a5text_process_frozen (a5_state_t *st, const char *raw,
                                    int *pre_alr_ink, char **marked = 0);

/* The full "LOOK" view of the player's current location (plain text). */
extern char *a5text_view_location (a5_state_t *st);

/* An object's display name with an article ("a"/"the"/none). */
typedef enum { A5_ART_NONE, A5_ART_INDEFINITE, A5_ART_DEFINITE } a5_article_t;
extern char *a5text_object_name (const a5_state_t *st, const a5_object_t *o,
                                 a5_article_t art);

/*
 * A character's displayed name honouring perspective: first/second person render
 * as the subjective pronoun ("I"/"you"), third person as the character's name.
 * Mirrors clsCharacter.Name(Subjective).  Heap-allocated.
 */
extern char *a5text_character_subjective (a5_state_t *st, const a5_character_t *c);

/*
 * `%character%.Name(args)` -- Global.vb:1286's character branch, whose article
 * defaults to Definite (the opposite of an object's) and whose args may select
 * a pronoun.  Heap-allocated.
 */
extern char *a5text_character_oo_name (a5_state_t *st, const a5_character_t *c,
                                       const char *args);

/*
 * A character's display name with only the first letter upper-cased
 * (Global.ToProper(.Name, bForceRestLower:=False)), used by a walk's
 * ShowEnterExit "<Name> enters/exits ..." message.  Heap-allocated.
 */
extern char *a5text_char_proper_name (a5_state_t *st, const char *charkey);

/*
 * A character's Known-aware display name (clsCharacter.Name's non-pronoun
 * branch): the proper name once Known/Selected, else the descriptor with an
 * article ("a young woman" indefinite / "the young woman" definite).  Used by
 * the NotUnderstood seen-character branch.  Heap-allocated.
 */
extern char *a5text_character_known_name (a5_state_t *st, const a5_character_t *c,
                                          int definite);

#endif
