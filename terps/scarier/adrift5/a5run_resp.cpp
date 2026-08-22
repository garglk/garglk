/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- per-command response aggregation.
 *
 * clsUserSession collects every completion / restriction-fail message of one
 * top-level command into two ordered, message-keyed maps (htblResponsesPass /
 * htblResponsesFail) and Displays them once at the end of AttemptToExecuteTask:
 * identical messages dedup, an AggregateOutput task's raw template is the key so
 * two items through the same task merge into one entry whose %objects% list
 * grows, and a pass drops the matching fail.  This file is that layer -- the
 * adds, the reference snapshots a deferred message is re-rendered under, and the
 * final flush.  Split out of a5run_action.cpp, which keeps the task dispatch
 * that feeds it.
 *
 * The collector types (ref_snap / resp_entry / resp_map) and these entry points
 * are declared in a5run_internal.h; both files need them.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "a5model.h"
#include "a5restr.h"
#include "a5run.h"
#include "a5sexpr.h"
#include "a5run_internal.h"
#include "a5sb.h"
#include "a5text.h"
#include "a5xml.h"

/* ref_value slots are only ever read as C strings (the state is calloc'd, so
   every slot is NUL-terminated from birth), and a whole-array memcpy moves
   16 x A5_REFVAL_LEN = ~32 KB per call -- act_set_tasks pays a take/restore
   pair per SetTasks-Execute member, which profiling showed dominating the
   event path.  Copy each slot's string contents instead; the bytes past a
   slot's NUL are unobservable. */
static void
ref_slots_copy (char (*dst)[A5_REFVAL_LEN], const char (*src)[A5_REFVAL_LEN])
{
  int i;
  for (i = 0; i < 16; i++)
    memcpy (dst[i], src[i], strlen (src[i]) + 1);
}

void
ref_snap_take (a5_state_t *st, ref_snap *s)
{
  memcpy (s->ref_name, st->ref_name, sizeof s->ref_name);
  ref_slots_copy (s->ref_value, st->ref_value);
  s->n_refbind = st->n_refbind;
  s->ref_object1_plural = st->ref_object1_plural;
  s->ref_character1_plural = st->ref_character1_plural;
}
void
ref_snap_restore (a5_state_t *st, const ref_snap *s)
{
  memcpy (st->ref_name, s->ref_name, sizeof s->ref_name);
  ref_slots_copy (st->ref_value, s->ref_value);
  st->n_refbind = s->n_refbind;
  st->ref_object1_plural = s->ref_object1_plural;
  st->ref_character1_plural = s->ref_character1_plural;
}


/* The "did this produce a response" probe used by the override scan: with a map
   active it is the response-mutation count, else the byte length of out.  A count
   (not ents.size()) is required because an AggregateOutput task's second+ item
   MERGES into an existing entry (obj_keys grows, ents.size() does NOT): the runner treats
   such a merging AddResponse as output that claims the turn (clsUserSession
   vb:1295), so the override scan must break on it too -- otherwise it falls
   through to a lower-priority sibling (e.g. AoS `put all in bag`: the 2nd item's
   merge into PutObjectI let the scan reach cl_PutAllInBa, whose `EverythingHeldBy
   Player` action then dumped the food/money into the bag). */
size_t
sink_len (a5_run_t *run, sb_t *out)
{
  return run->resp != NULL ? run->resp->nmut : out->len;
}

/* Render a completion wrapper without retiring its DisplayOnce segments
   (the Adrift 5 runner's bTestingOutput=True): used for the pre/post-action comparison
   that decides whether a Before message is pinned or deferred. */
char *
render_comp_test (a5_state_t *st, const a5_xml_node_t *comp)
{
  return render_comp_test_ex (st, comp, NULL);
}

/* render_comp_test, additionally handing back the still-MARKED-UP render.
   The runner compares its pre- and post-action renders as the RAW strings its
   ReplaceExpressions produced -- tags and all -- so a difference that lives
   only inside a stripped tag still counts as "the actions changed the
   message".  Beginner's Cave's combat messages end in
   `<# "<" & rand(0,1000000) & ">" #>`, i.e. a pseudo-tag whose whole purpose is
   to make every render differ; comparing the PLAIN text made the two renders
   look identical, so Scarier took the runner's they-agree branch and drew a
   third time, sliding the whole combat RNG stream by one. */
char *
render_comp_test_ex (a5_state_t *st, const a5_xml_node_t *comp, char **marked)
{
  a5_mark_guard mg (st, 0);
  /* bTestingOutput and bDisplaying are independent: the runner sets BOTH for
     these probes (clsUserSession.vb:1177-1178 / 1199-1200), so the render skips
     DisplayOnce retirement but still runs clsCharacter.Name's bDisplaying
     block.  That matters beyond bookkeeping -- when the pre- and post-action
     probes differ, vb:1201 PINS the pre-action probe's text as the response, so
     what the player sees is a bDisplaying render. */
  a5_intro_guard ig (st, 1);
  return a5text_describe_ex (st, comp, NULL, NULL, marked);
}

/* Does this CompletionMessage bear a text-changing function -- an embedded
   <#...#> expression (OneOf/Either/...) or a RAND(..) inside a %function% or
   array reference?  Only such messages can draw RNG when rendered; run_task
   uses this to route a Before message through the runner-faithful
   render/actions/render interleave while plain static messages keep the flat
   fast path. */
int
comp_bears_function (const a5_xml_node_t *n)
{
  if (n == NULL)
    return 0;
  if (n->text != NULL
      && (strstr (n->text, "<#") != NULL
          || strstr (n->text, "RAND(") != NULL))
    return 1;
  for (const a5_xml_node_t *c = n->first_child; c != NULL; c = c->next)
    if (comp_bears_function (c))
      return 1;
  return 0;
}

/* Insert a freshly-built entry, honouring a reserved position (the slot a
   "Before" message claimed before its actions ran -- clsUserSession's
   iResponsePosition / OrderedHashTable.Insert, vb:1189/1311). */
void
resp_insert (resp_map *rm, resp_entry &e, int pos)
{
  if (pos >= 0 && (size_t) pos < rm->ents.size ())
    rm->ents.insert (rm->ents.begin () + pos, e);
  else
    rm->ents.push_back (e);
  rm->nmut++;                     /* a new response is output (see sink_len) */
}

/* Add a completion message (from task t) as a response.  An AggregateOutput
   task defers its function replacement: the comp node is the dedup key, and a
   second item through the same task merges into the existing entry (its
   %objects% list grows).  A non-aggregate task renders eagerly at the current
   (single-item) state and dedups on the rendered text. */
void
resp_add_comp (a5_run_t *run, const a5_task_t *t, const a5_xml_node_t *comp,
               bool is_pass, int pos)
{
  resp_map *rm = run->resp;
  a5_state_t *st = run->st;
  const std::string &item = rm->cur_item;
  /* The AggregateOutput deferral (store the comp, re-render at flush over the
     merged %objects% list) only earns its keep inside a *plural* iteration, where
     a later item merges into the same entry.  In a single-reference command
     (cur_item empty -- e.g. movement's `Beforeplay` "You move north." After
     message, and `ts_tasCheckTime`'s "Date: ...") render eagerly and dedup on the
     text: re-rendering at flush is fragile (the `move[//s]` verb conjugation reads
     transient per-turn context that has changed by the end of the command), and
     two identical "Date:" lines still collapse via the text dedup. */
  bool aggregate = t != NULL && t->aggregate && !item.empty ();

  /* The runner keys an AggregateOutput response by its RAW template -- AddResponse
     receives task.CompletionMessage.ToString UNexpanded (the OO chain and
     %functions% only resolve at Display).  The stock Look's response key is
     the literal "%Player%.Location.Description", so a movement task that both
     Executes Look AND carries that same completion (Murder Most Foul's custom
     PlayerMovement1) MERGES into the Look response: one room view, rendered
     once at Display -- which also means its DisplayOnce segments retire once
     and its ALR-borne %scor% reads that commit's state.  Mirror the merge:
     an aggregate single-reference completion whose raw template assembles to
     the same text as the pending is_look entry's (the Look task's raw
     CompletionMessage) is folded into it. */
  if (t != NULL && t->aggregate && item.empty ())
    for (auto &e : rm->ents)
      if (e.is_look && e.is_pass == is_pass)
        {
          const a5_task_t *look = a5model_task (st->adv, "Look");
          const a5_xml_node_t *lcomp =
              look != NULL ? a5xml_child (look->node, "CompletionMessage") : NULL;
          bool same = false;
          if (lcomp != NULL)
            {
              char *rawc, *rawl;
              {
                a5_mark_guard mg (st, 0);
                rawc = a5text_eval_description (st, comp);
                rawl = a5text_eval_description (st, lcomp);
              }
              same = rawc != NULL && rawl != NULL && streq (rawc, rawl);
              free (rawc); free (rawl);
            }
          if (same) { rm->nmut++; return; }   /* merge is output (sink_len) */
          break;
        }

  if (aggregate)
    {
      for (auto &e : rm->ents)
        if (e.aggregate && e.comp == comp && e.is_pass == is_pass)
          {
            if (!item.empty ()
                && std::find (e.obj_keys.begin (), e.obj_keys.end (), item)
                     == e.obj_keys.end ())
              { e.obj_keys.push_back (item); rm->nmut++; }  /* merge is output */
            return;
          }
      resp_entry e;
      e.is_pass = is_pass; e.aggregate = true; e.comp = comp;
      const char *o2 = a5state_lookup_ref (st, "ReferencedObject2");
      e.obj2 = o2 ? o2 : "";
      if (!item.empty ()) e.obj_keys.push_back (item);
      e.item = item;
      ref_snap_take (st, &e.snap); e.has_snap = true;
      resp_insert (rm, e, pos);
    }
  else
    {
      int raw_nonblank = 0, pre_alr_ink = 0;
      char *m;
      std::string marked;
      {
        a5_mark_guard mg (st, 1);
        /* An AggregateOutput task's message is the RAW template in the runner's
           response table and only expands inside Display, i.e. under
           bDisplaying; only the non-aggregate finalize (vb:1185/1204/1211) runs
           with the flag clear.  Scarier renders both eagerly here, so carry the
           distinction on the flag. */
        a5_intro_guard ig (st, t != NULL && t->aggregate);
        /* A single-reference AggregateOutput completion bearing a text function:
           the runner stores the RAW template and only ReplaceExpressions it at
           end-of-command Display -- AFTER e.g. the stock Look's two test renders
           of the destination room view.  Render the static skeleton now (the
           verb-conjugation context stays current), but push the `<#..#>` draw to
           the display_defers sink so the xoshiro stream stays aligned (Quest
           Giver's "You move North.. <#OneOf..#>" flavor must draw after the
           tavern-sign OneOf in the room description, not before). */
        void *prev_ed = st->expr_defer;
        if (t != NULL && t->aggregate && comp_bears_function (comp))
          st->expr_defer = run->display_defers;
        char *mk = NULL;
        m = a5text_describe_ex (st, comp, &pre_alr_ink, &raw_nonblank, &mk);
        marked = mk != NULL ? mk : "";
        free (mk);
        st->expr_defer = prev_ed;
      }
      /* The runner's AddResponse output test (bHasOutput) sees the message BEFORE the
         trailing-whitespace trim, so a whitespace-only "\n" completion counts
         as task output -- it stops an After-children scan (The Salvage's
         per-move station-known task suppressing the fuel child) even though
         nothing visible renders. */
      if (raw_nonblank)
        run->task_raw_output = 1;
      bool has = msg_has_output (m);
      std::string r = m ? m : ""; free (m);
      /* A message whose plain render is ONLY stripped-tag sentinels but which
         had visible text before the ALR pass -- WW2 Elevator Escape's
         "(standing up first)" -> ALR -> "<del>", leaving
         "<font..><del><font..>" -- passes the runner's AddResponse gate (bHasOutput on
         the stored PRE-ALR text) and is Displayed as a markup skeleton, so
         the runner's raw buffer ends in '>' and the NEXT message pSpace-joins with two
         spaces.  Keep such an entry: the mark bytes survive to the sb, where
         sb_pspace treats them as a non-newline tail; finish_turn strips them.
         A marks-only render with NO pre-ALR ink (Amazon's ts_tasSunset "<>"
         off-hours completion) fails the runner's gate and is dropped as before. */
      if (!has && !(pre_alr_ink && !r.empty ())) return;
      /* The runner keys htblResponses by the message as ReplaceFunctions left
         it -- markup and all -- and Display() strips the tags only on the way
         out, so two responses that differ ONLY inside a stripped tag are
         DISTINCT there.  Beginner's Cave exploits exactly that to get one line
         per blow: every combat message ends in
         `<# "<" & rand(0,1000000) & ">" #>`, a per-render pseudo-tag.  Keying
         the dedup on the plain text swallowed the second "A hit!" of a
         round. */
      for (auto &e : rm->ents)
        if (!e.aggregate && e.is_pass == is_pass && e.key == marked) return;
      resp_entry e;
      e.is_pass = is_pass; e.aggregate = false; e.comp = NULL;
      e.rendered = r; e.key = marked; e.item = item;
      resp_insert (rm, e, pos);
    }
}

/* Add an already-rendered string as a response (override-fail messages, room
   views, pinned Before messages).  Always non-aggregate; deduped on the text. */
void
resp_add_text (a5_run_t *run, const char *text, bool is_pass, int pos)
{
  resp_map *rm = run->resp;
  if (!msg_has_output (text)) return;
  std::string r = text;
  for (auto &e : rm->ents)
    if (!e.aggregate && e.is_pass == is_pass && e.key == r) return;
  resp_entry e;
  e.is_pass = is_pass; e.aggregate = false; e.comp = NULL;
  e.rendered = r; e.key = r; e.item = rm->cur_item;
  resp_insert (rm, e, pos);
}

/* Add a restriction-fail message (the restriction's <Message> node `fm`) as a
   fail response, mirroring the Adrift 5 runner's htblResponsesFail keyed by the raw
   message template (clsUserSession AddResponse, vb:1301-1307): a second item that
   fails the SAME restriction node merges its %objects% item into the existing
   entry instead of emitting a separate message, so at flush the template renders
   ONCE over the merged set.  AoS `put all in bag` re-fails the two nested coins on
   the general `Must BeHeldByCharacter %Player%`; The runner produces the single "You are
   not carrying the gold gonks and the silver ginks." (which the game's
   cl_YouAreNotC1 ALR then blanks), where Scarier used to show two singular fails
   that no ALR could match.  A single-item fail (the common case) keeps its
   eagerly-rendered singular text, so every non-aggregating fail stays byte-
   identical to the old resp_add_text path. */
void
resp_add_fail (a5_run_t *run, const a5_xml_node_t *fm)
{
  resp_map *rm = run->resp;
  a5_state_t *st = run->st;
  const std::string &item = rm->cur_item;

  /* Second+ item failing the SAME restriction node: merge its %objects% key
     (htblResponsesFail's ContainsKey branch, vb:1303-1307). */
  if (!item.empty ())
    for (auto &e : rm->ents)
      if (!e.is_pass && e.fail_comp == fm)
        {
          if (std::find (e.obj_keys.begin (), e.obj_keys.end (), item)
                == e.obj_keys.end ())
            { e.obj_keys.push_back (item); rm->nmut++; }   /* merge is output */
          return;
        }

  /* First occurrence: render eagerly at the failing item's state (identical to
     the old path) and text-dedup against the existing fails, so two DIFFERENT
     restrictions that render the same string still collapse.

     bDisplaying: unlike a completion message, a restriction failure has no
     "eager finalize" variant at all -- vb:1247 hands sRestrictionText to
     AddResponse verbatim, with no ReplaceFunctions/ReplaceExpressions on any
     path, so every fail message the runner shows is expanded inside Display.
     (Same at the merged re-render in resp_flush.) */
  a5_intro_guard ig (st, 1);
  char *f = a5text_describe (st, fm);
  if (!msg_has_output (f)) { free (f); return; }
  std::string r = f;
  free (f);
  for (auto &e : rm->ents)
    if (!e.aggregate && !e.is_pass && e.rendered == r) return;
  resp_entry e;
  e.is_pass = false; e.aggregate = false; e.comp = NULL; e.fail_comp = fm;
  e.rendered = r; e.key = r; e.item = item;
  if (!item.empty ()) e.obj_keys.push_back (item);
  ref_snap_take (st, &e.snap); e.has_snap = true;
  resp_insert (rm, e, -1);
}

/* Rebind %objects%/ReferencedObjects to KEYS (skipping unknown keys, capped at
   A5_MAX_ITEMS).  Shared by the three resp_flush re-render sites that render a
   merged completion or fail over a plural %objects% set. */
static void
rebind_objects (a5_state_t *st, const std::vector<std::string> &keys)
{
  std::string pipe;
  st->n_ref_items = 0;
  st->ref_items_type = 'o';
  for (auto &k : keys)
    {
      const a5_object_t *o = a5model_object (st->adv, k.c_str ());
      if (o == NULL) continue;
      if (st->n_ref_items < A5_MAX_ITEMS)
        st->ref_items[st->n_ref_items++] = o->key;
      if (!pipe.empty ()) pipe += "|";
      pipe += o->key;
    }
  bind_reference (st, "objects", pipe.c_str (), pipe.c_str ());
  a5state_bind_ref (st, "ReferencedObjects", pipe.c_str ());
}

/* Flush the map to `out`: pass responses first in insertion order (aggregate
   entries re-rendered over their merged %objects% set), then any fail response
   whose item was not covered by a pass (clsUserSession.vb:804-855). */
void
resp_flush (a5_run_t *run, resp_map *rm, sb_t *out)
{
  a5_state_t *st = run->st;
  std::set<std::string> passed;
  std::set<std::string> look_seen;
  for (auto &e : rm->ents)
    if (e.is_pass)
      {
        if (!e.item.empty ()) passed.insert (e.item);
        for (auto &k : e.obj_keys) passed.insert (k);
      }

  /* The runner sets NewReferences = refs only for the messages it actually Displays
     (htblResponses, which its AddResponse bHasOutput gate never adds an
     empty-output response to).  So the leftover reference the post-command event
     tasks iterate is the LAST *displayed* message's refs, NOT the last response
     entry's.  Track the last output-producing aggregate entry's refs here and
     re-apply them after the loop; an entry that renders to nothing (e.g. Book of
     Jax's `put all in bag` re-putting items already inside the bag -- their
     cl_PutObjInBa completions are empty) must not become the leftover set, or a
     per-turn TurnBased event (cl_TurnsTaken1) would tick once per silently-moved
     item instead of once. */
  std::vector<std::string> lo_keys;   /* last displayed aggregate's obj_keys   */
  std::string lo_obj2;
  int lo_have = 0;                    /* any entry produced output this flush   */
  int lo_multi = 0;                   /* last displayed entry was plural (>1)   */

  for (int phase = 0; phase < 2; phase++)
    for (auto &e : rm->ents)
      {
        if (e.is_pass != (phase == 0)) continue;
        /* The runner drops a fail whose every reference item also produced a pass
           (clsUserSession.vb:812-834).  A merged fail (obj_keys accumulated by
           resp_add_fail) is dropped only when ALL its items passed; a fail with no
           merged set falls back to the single-item check (the old resp_add_text
           path -- movement fails, SetTasks-Execute fails). */
        if (!e.is_pass)
          {
            if (!e.obj_keys.empty ())
              {
                bool all_passed = true;
                for (auto &k : e.obj_keys)
                  if (!passed.count (k)) { all_passed = false; break; }
                if (all_passed) continue;
              }
            else if (!e.item.empty () && passed.count (e.item))
              continue;
          }

        std::string text;
        if (e.is_look)
          {
            /* A deferred room view (Execute Look): rendered now, at the command's
               final state, mirroring the Adrift 5 runner's AggregateOutput Look whose
               function replacement is deferred to Display.  So a move whose After
               children relocated an NPC lists it at its new spot.  Real output:
               retire DisplayOnce segments (view_location_impl honours the
               ambient marking flag). */
            text = render_look_marked (run);
            /* The runner keys the stock Look's AggregateOutput on its response template
               (htblResponsesPass), so a command that Executes Look more than once
               -- e.g. Bug Hunt's cl_ZMovePlaye, which runs Execute Look after the
               player moves AND again after the marine squad follows -- shows a
               single room view, not one per Execute.  Mirror the non-resp path's
               pass_seen dedup: collapse identical deferred views. */
            if (!look_seen.insert (text).second)
              continue;
          }
        else if (e.aggregate)
          {
            /* Restore the references this message was queued with, so its
               deferred %object%/%character%/%direction% tokens resolve as they
               did when the task ran (the runner reassigns NewReferences at Display). */
            if (e.has_snap) ref_snap_restore (st, &e.snap);
            /* A genuine plural %objects% command merged several items into this
               entry: rebind the whole set (overriding the snapshot's singular
               object binding) so the completion renders its "a, b and c" list.
               A single-reference command has no merged items (obj_keys empty) and
               keeps the restored snapshot's singular binding untouched. */
            if (!e.obj_keys.empty ())
              {
                rebind_objects (st, e.obj_keys);
                if (!e.obj2.empty ())
                  bind_reference (st, "object2", e.obj2.c_str (), e.obj2.c_str ());
              }
            char *m;
            {
              a5_mark_guard mg (st, 1);
              a5_intro_guard ig (st, 1);   /* this IS the runner's Display */
              m = a5text_describe (st, e.comp);
            }
            if (m != NULL) text = m;
            free (m);
          }
        else if (e.fail_comp != NULL && e.obj_keys.size () > 1)
          {
            /* A merged fail: re-render the restriction template ONCE over the
               combined %objects% set, so %TheObjects[%objects%]% lists every
               failing item (the runner Display over the accumulated htblResponsesFail
               NewReferences).  The aggregate string is what a game ALR can match
               at the display boundary -- AoS "You are not carrying the gold gonks
               and the silver ginks." blanked by cl_YouAreNotC1.  A single-item
               fail (obj_keys.size()==1) keeps its eager render below, byte-exact. */
            if (e.has_snap) ref_snap_restore (st, &e.snap);
            rebind_objects (st, e.obj_keys);
            a5_intro_guard ig (st, 1);       /* fail text always expands in Display */
            char *m = a5text_describe (st, e.fail_comp);
            if (m != NULL) text = m;
            free (m);
          }
        else
          text = e.rendered;

        /* A `\004N\004` sentinel in an eagerly-rendered entry marks a deferred
           `<#..#>`/%var[rand]% draw (see resp_add_comp): the runner's Display
           renders responses IN ORDER, drawing each response's expressions at
           its slot -- so the movement completion's OneOf must draw here,
           BEFORE a later is_look entry's final room-view render, not at the
           post-drain display_defers flush.  Resolve the sentinels now (left
           to right, the runner's ReplaceExpressions scan) and blank the sink
           bodies so the later flush neither re-draws nor re-splices them. */
        if (run->display_defers != NULL
            && text.find ('\004') != std::string::npos)
          {
            std::string res;
            size_t p = 0;
            while (p < text.size ())
              {
                if (text[p] == '\004')
                  {
                    size_t q = text.find ('\004', p + 1);
                    if (q != std::string::npos)
                      {
                        int idx = atoi (text.substr (p + 1, q - p - 1).c_str ());
                        if (idx >= 0
                            && (size_t) idx < run->display_defers->size ())
                          {
                            std::string &body = (*run->display_defers)[idx];
                            char *val = (!body.empty () && body[0] == '\001')
                                ? a5text_process_noalr (st, body.c_str () + 1)
                                : a5_eval_sexpr (body.c_str ());
                            if (val != NULL) res += val;
                            free (val);
                            body = "\001";       /* consumed: no re-draw */
                          }
                        p = q + 1;
                        continue;
                      }
                  }
                res += text[p++];
              }
            text = res;
          }

        /* Marks-only entries (see resp_add_comp) are emitted too: the runner Displayed
           the markup skeleton, so it space-joins here and leaves the buffer
           mid-line for the next response's pSpace. */
        if (msg_has_output (text.c_str ()) || !text.empty ())
          {
            sb_pspace (out); sb_puts (out, text.c_str ());
            /* This message is Displayed, so it is the running NewReferences (the runner
               reassigns per Displayed message).  An aggregate carrying a merged
               %objects% set makes the leftover plural; any other displayed message
               (a single-item completion, a movement line, a room view) leaves it
               singular -- the event tasks then run once. */
            lo_have = 1;
            if (e.aggregate && e.obj_keys.size () > 1)
              { lo_keys = e.obj_keys; lo_obj2 = e.obj2; lo_multi = 1; }
            else
              { lo_keys.clear (); lo_obj2.clear (); lo_multi = 0; }
          }
      }

  /* Re-apply the last Displayed message's references as the runner's post-Display
     NewReferences (see the note above the loop).  Only a plural leftover matters
     to attempt_event_task's per-item iteration; a singular last message collapses
     n_ref_items so the event runs once. */
  if (lo_have)
    {
      if (lo_multi)
        {
          rebind_objects (st, lo_keys);
          if (!lo_obj2.empty ())
            bind_reference (st, "object2", lo_obj2.c_str (), lo_obj2.c_str ());
        }
      else if (st->n_ref_items > 1)
        st->n_ref_items = 1;
    }
}
