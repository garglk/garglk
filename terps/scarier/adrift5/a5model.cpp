/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- in-memory object model.  See a5model.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "a5blorb.h"
#include "a5deobf.h"
#include "a5model.h"
#include "a5parse.h"   /* a5_correct_command (CorrectCommand) */
#include "a5util.h"

/* ------------------------------------------------------------------ helpers */

/* Parse an ADRIFT serialised boolean (FileIO.GetBool): only "TRUE"/"1"/"-1"/
   "VRAI" (case-insensitively) are true; everything else is false. */
int
a5xml_bool (const char *s)
{
  if (s == NULL)
    return 0;
  return strcasecmp (s, "True") == 0 || strcmp (s, "1") == 0
      || strcmp (s, "-1") == 0       || strcasecmp (s, "Vrai") == 0;
}

/* Every collector and per-type loader below has the same scaffold: count the
   direct children named `tag`, allocate that many records, then walk the
   children again and fill one record per match.  This factors the scaffold
   out so each caller supplies only the per-record fill. */
template <typename T, typename F>
static void
a5_load_each (const a5_xml_node_t *node, const char *tag, T **arr, int *out_n,
              F fill)
{
  const a5_xml_node_t *c;
  int i = 0;

  *out_n = (node != NULL) ? a5xml_count (node, tag) : 0;
  if (*out_n == 0)
    return;
  *arr = (T *) calloc ((size_t) *out_n, sizeof **arr);
  for (c = node->first_child; c != NULL; c = c->next)
    if (strcmp (c->name, tag) == 0)
      fill (&(*arr)[i++], c);
}

/* Collect the text of every direct child named `tag` into a new const char*[]. */
static const char **
a5_collect_text (const a5_xml_node_t *node, const char *tag, int *out_n)
{
  const char **arr = NULL;
  a5_load_each (node, tag, &arr, out_n,
                [] (const char **slot, const a5_xml_node_t *c)
                {
                  *slot = c->text;
                });
  return arr;
}

/* Collect an entity's <Property> children into an a5_prop_t array. */
static a5_prop_t *
a5_collect_props (const a5_xml_node_t *node, int *out_n)
{
  a5_prop_t *arr = NULL;
  a5_load_each (node, "Property", &arr, out_n,
                [] (a5_prop_t *p, const a5_xml_node_t *c)
                {
                  const a5_xml_node_t *value;
                  p->key = a5xml_child_text (c, "Key");
                  value = a5xml_child (c, "Value");
                  if (value != NULL)
                    {
                      if (value->n_children > 0)
                        p->value_node = value;   /* rich (Description) value */
                      else
                        p->value = value->text;  /* simple scalar */
                    }
                  /* else: flag property (both NULL) */
                });
  return arr;
}

const a5_prop_t *
a5_prop_find (const a5_prop_t *props, int n, const char *key)
{
  int i;
  for (i = 0; i < n; i++)
    if (props[i].key != NULL && strcmp (props[i].key, key) == 0)
      return &props[i];
  return NULL;
}

/* ----------------------------------------------------------- per-type loads */

static void
a5_load_objects (a5_adventure_t *a)
{
  a5_load_each (a->root, "Object", &a->objects, &a->n_objects,
                [] (a5_object_t *o, const a5_xml_node_t *c)
                {
                  o->node = c;
                  o->key = a5xml_child_text (c, "Key");
                  o->article = a5xml_child_text (c, "Article");
                  o->prefix = a5xml_child_text (c, "Prefix");
                  o->names = a5_collect_text (c, "Name", &o->n_names);
                  o->props = a5_collect_props (c, &o->n_props);
                });
}

static void
a5_load_locations (a5_adventure_t *a)
{
  a5_load_each (a->root, "Location", &a->locations, &a->n_locations,
                [] (a5_location_t *l, const a5_xml_node_t *c)
                {
                  l->node = c;
                  l->key = a5xml_child_text (c, "Key");
                  l->props = a5_collect_props (c, &l->n_props);
                });
}

static void a5_load_walks (a5_character_t *ch);   /* defined below the helpers */

/* Parse one of a character's <Topic> children (clsTopic / FileIO topic load).  The
   first direct <Description> child is the reply; <Restrictions>/<Actions> are
   the topic-level gating block and action list. */
static void
a5_parse_topic (a5_topic_t *t, const a5_xml_node_t *c)
{
  t->key = a5xml_child_text (c, "Key");
  t->parent_key = a5xml_child_text (c, "ParentKey");
  if (t->parent_key == NULL) t->parent_key = "";
  t->keywords = a5xml_child_text (c, "Keywords");
  if (t->keywords == NULL) t->keywords = "";
  /* Topic flags via FileIO.GetBool semantics (True/1/-1/Vrai), not a bare
     "1": RtC serialises <IsIntro>True</IsIntro> etc., so a "1"-only test
     left every topic flag false and the whole conversation system inert
     (say/ask/tell/command always fell to "ignores you" / "doesn't appear
     to understand you"). */
  t->is_intro     = a5xml_bool (a5xml_child_text (c, "IsIntro"));
  t->is_ask       = a5xml_bool (a5xml_child_text (c, "IsAsk"));
  t->is_tell      = a5xml_bool (a5xml_child_text (c, "IsTell"));
  t->is_command   = a5xml_bool (a5xml_child_text (c, "IsCommand"));
  t->is_farewell  = a5xml_bool (a5xml_child_text (c, "IsFarewell"));
  t->stay_in_node = a5xml_bool (a5xml_child_text (c, "StayInNode"));
  /* clsUserSession game-start init (vb:259): command-topic keywords go
     through CorrectCommand exactly like task commands, so an optional
     leading group absorbs its adjacent space (`{say} [hello]` ->
     `{say }[hello]`) and the bare form matches (October 31st's gardener
     explicit intro `say hello to ghost` -> subject "hello"). */
  if (t->is_command)
    t->keywords = a5_correct_command (t->keywords);
  t->conversation = a5xml_child (c, "Description");
  t->restrictions = a5xml_child (c, "Restrictions");
  t->actions = a5xml_child (c, "Actions");
}

static void
a5_load_characters (a5_adventure_t *a)
{
  a5_load_each (a->root, "Character", &a->characters, &a->n_characters,
                [] (a5_character_t *ch, const a5_xml_node_t *c)
                {
                  ch->node = c;
                  ch->key = a5xml_child_text (c, "Key");
                  ch->name = a5xml_child_text (c, "Name");
                  ch->article = a5xml_child_text (c, "Article");
                  ch->prefix = a5xml_child_text (c, "Prefix");
                  ch->type = a5xml_child_text (c, "Type");
                  ch->perspective = a5xml_child_text (c, "Perspective");
                  ch->descriptors = a5_collect_text (c, "Descriptor",
                                                     &ch->n_descriptors);
                  ch->props = a5_collect_props (c, &ch->n_props);
                  a5_load_each (c, "Topic", &ch->topics, &ch->n_topics,
                                a5_parse_topic);
                  a5_load_walks (ch);
                });
}

static void
a5_parse_task (a5_task_t *t, const a5_xml_node_t *c)
{
  const char *prio, *cont, *agg;
  int k;

  t->node = c;
  t->key = a5xml_child_text (c, "Key");
  prio = a5xml_child_text (c, "Priority");
  t->priority = (prio != NULL) ? strtol (prio, NULL, 10) : 0;
  t->type = a5xml_child_text (c, "Type");
  t->run_immediately = a5xml_bool (a5xml_child_text (c, "RunImmediately"));
  t->location_trigger = a5xml_child_text (c, "LocationTrigger");
  t->commands = a5_collect_text (c, "Command", &t->n_commands);
  /* Apply clsUserSession.CorrectCommand to every command (the runner does this once
     at game-start init).  The collected pointers alias the XML tree; replace
     each with an owned, bracket-corrected copy (freed in a5model_free). */
  for (k = 0; k < t->n_commands; k++)
    ((char **) t->commands)[k] = a5_correct_command (t->commands[k]);
  /* clsTask.Repeatable via FileIO.GetBool (FileIO.vb:1604) -- accepts
     True/1/-1/Vrai, not just the numeric "1".  RtC (and other Generator
     versions) serialise <Repeatable>True</Repeatable>; reading only "1"
     left every such task non-repeatable, so a general library task like
     ExamineObjects was marked Completed after its first use and then
     skipped (e.g. `x desk` failing once any object had been examined). */
  t->repeatable = a5xml_bool (a5xml_child_text (c, "Repeatable"));
  cont = a5xml_child_text (c, "Continue");
  t->continue_lower = (cont != NULL && strcmp (cont, "ContinueAlways") == 0);
  t->low_priority = a5xml_bool (a5xml_child_text (c, "LowPriority"));
  /* clsTask.AggregateOutput defaults True; FileIO only overrides it when
     an <Aggregate> element is present (FileIO.vb:1605). */
  agg = a5xml_child_text (c, "Aggregate");
  t->aggregate = (agg == NULL) ? 1 : a5xml_bool (agg);
  t->restrictions = a5xml_child (c, "Restrictions");
  t->actions = a5xml_child (c, "Actions");
  t->fail_override = a5xml_child (c, "FailOverride");

  /* Specific-override linkage. */
  t->general_key = a5xml_child_text (c, "GeneralTask");
  t->override_type = a5xml_child_text (c, "SpecificOverrideType");
  a5_load_each (c, "Specific", &t->specifics, &t->n_specifics,
                [] (a5_specific_t *sp, const a5_xml_node_t *s)
                {
                  sp->type = a5xml_child_text (s, "Type");
                  sp->keys = a5_collect_text (s, "Key", &sp->n_keys);
                });
}

static void
a5_load_tasks (a5_adventure_t *a)
{
  a5_load_each (a->root, "Task", &a->tasks, &a->n_tasks, a5_parse_task);
}

static void
a5_load_variables (a5_adventure_t *a)
{
  a5_load_each (a->root, "Variable", &a->variables, &a->n_variables,
                [] (a5_variable_t *v, const a5_xml_node_t *c)
                {
                  const char *al;
                  long len;
                  v->node = c;
                  v->key = a5xml_child_text (c, "Key");
                  v->name = a5xml_child_text (c, "Name");
                  v->type = a5xml_child_text (c, "Type");
                  v->initial = a5xml_child_text (c, "InitialValue");
                  al = a5xml_child_text (c, "ArrayLength");
                  len = (al != NULL) ? strtol (al, NULL, 10) : 0;
                  v->array_length = (len > 1) ? (int) len : 1;
                });
}

/* Parse "1" or "1 To 5" (a FromTo range) into [*from, *to].  The scan stops at
   the next digit OR a '-' so a negative upper bound ("0 To -5") is picked up.
   Known limitation: this means a dash used as a *separator* ("5 - 10") stops on
   the '-' and strtol("- 10") returns 0, so the upper bound reads as 0 instead of
   10.  The stock ADRIFT serialisation is always "N To M" (dashes only ever
   prefix a negative number), so no shipped game hits this; left as-is. */
static void
a5_parse_range (const char *s, long *from, long *to)
{
  char *end;
  if (s == NULL) { *from = *to = 0; return; }
  *from = strtol (s, &end, 10);
  while (*end != '\0' && (*end < '0' || *end > '9') && *end != '-') end++;
  *to = (*end != '\0') ? strtol (end, NULL, 10) : *from;
}

/* The last whitespace-delimited token of `s` (an enum name), or "". */
static const char *
a5_last_token (const char *s)
{
  const char *p, *last = s ? s : "";
  for (p = last; *p; p++)
    if (*p == ' ' || *p == '\t')
      last = p + 1;
  return last;
}

/* Copy the first space-delimited token of `s` into buf (NUL-terminated). */
static void
a5_first_token (const char *s, char *buf, int cap)
{
  int n = 0;
  while (s[n] != '\0' && s[n] != ' ' && n < cap - 1)
    {
      buf[n] = s[n];
      n++;
    }
  buf[n] = '\0';
}

static a5_se_when_t
a5_parse_se_when (const char *s)
{
  if (s != NULL && strcmp (s, "FromStartOfEvent") == 0) return A5_SE_FROM_START;
  if (s != NULL && strcmp (s, "BeforeEndOfEvent") == 0) return A5_SE_BEFORE_END;
  return A5_SE_FROM_LAST;
}

static a5_se_what_t
a5_parse_se_what (const char *s)
{
  if (s != NULL && strcmp (s, "SetLook") == 0)    return A5_SE_SETLOOK;
  if (s != NULL && strcmp (s, "ExecuteTask") == 0) return A5_SE_EXECTASK;
  if (s != NULL && strcmp (s, "UnsetTask") == 0)  return A5_SE_UNSETTASK;
  return A5_SE_DISPLAY;
}

static a5_ctrl_t
a5_parse_ctrl (const char *s)
{
  if (s != NULL && strcmp (s, "Stop") == 0)    return A5_CTRL_STOP;
  if (s != NULL && strcmp (s, "Suspend") == 0) return A5_CTRL_SUSPEND;
  if (s != NULL && strcmp (s, "Resume") == 0)  return A5_CTRL_RESUME;
  return A5_CTRL_START;
}

/* Parse one <Control> body ("Start Completion TaskKey") into an a5_eventctrl_t
   (EventOrWalkControl, shared by events and walks).  The task key aliases the
   final, NUL-terminated token of the body, so it stays valid as long as the
   DOM. */
static void
a5_parse_control (a5_eventctrl_t *cc, const a5_xml_node_t *c)
{
  const char *t = c->text ? c->text : "";
  const char *p = t, *toks[3]; int nt = 0;
  char buf[64];
  while (*p == ' ') p++;
  while (*p && nt < 3)
    {
      toks[nt++] = p;
      while (*p && *p != ' ') p++;
      while (*p == ' ') p++;
    }
  if (nt >= 1)
    {
      a5_first_token (toks[0], buf, sizeof buf);
      cc->control = a5_parse_ctrl (buf);
    }
  cc->on_completion = 1;
  if (nt >= 2)
    cc->on_completion = (strncmp (toks[1], "UnCompletion", 12) != 0);
  if (nt >= 3)
    cc->task_key = toks[2];
}

/* Split a subevent/subwalk <Action> into its two serialised forms: return the
   node itself when it carries a <Description> payload (DisplayMessage), else
   NULL after copying the leading verb of its "Verb Key" text ("ExecuteTask
   cl_Foo") into wbuf and pointing *key at the key -- the final, NUL-terminated
   token of the <Action> text, so it stays valid as long as the DOM.  *key is
   left untouched when the text carries no key. */
static const a5_xml_node_t *
a5_action_split (const a5_xml_node_t *act, char *wbuf, int wcap,
                 const char **key)
{
  const char *txt, *sp;
  wbuf[0] = '\0';
  if (a5xml_child (act, "Description") != NULL)
    return act;
  txt = act->text ? act->text : "";
  a5_first_token (txt, wbuf, wcap);
  sp = strchr (txt, ' ');
  if (sp != NULL)
    *key = sp + 1;
  return NULL;
}

static a5_sw_when_t
a5_parse_sw_when (const char *s)
{
  if (s != NULL && strcmp (s, "FromStartOfWalk") == 0) return A5_SW_FROM_START;
  if (s != NULL && strcmp (s, "BeforeEndOfWalk") == 0) return A5_SW_BEFORE_END;
  return A5_SW_FROM_LAST;
}

static a5_sw_what_t
a5_parse_sw_what (const char *s)
{
  if (s != NULL && strcmp (s, "ExecuteTask") == 0) return A5_SW_EXECTASK;
  if (s != NULL && strcmp (s, "UnsetTask") == 0)   return A5_SW_UNSETTASK;
  return A5_SW_DISPLAY;
}

/* One <Step>: "Player 1" or "cl_Foo 1 To 3" -> destination key, duration
   range.  The key is the first space-delimited token; parse the duration from
   the rest, then truncate the (in-situ DOM) text so location aliases just the
   key (the Step text is read nowhere else). */
static void
a5_parse_step (a5_walkstep_t *st, const a5_xml_node_t *c)
{
  char *t = (char *) (c->text ? c->text : "");
  char *sp = strchr (t, ' ');
  a5_parse_range (sp ? sp + 1 : NULL, &st->ft_from, &st->ft_to);
  if (sp != NULL) *sp = '\0';
  st->location = t;
}

/* One <Activity> of a walk (clsWalk SubWalk load). */
static void
a5_parse_subwalk (a5_subwalk_t *sw, const a5_xml_node_t *c)
{
  const char *when = a5xml_child_text (c, "When");
  a5_xml_node_t *act = a5xml_child (c, "Action");
  if (when != NULL && strncmp (when, "ComesAcross", 11) == 0)
    {
      /* "ComesAcross %Player%" -> come_key (the subject met). */
      const char *sp = strchr (when, ' ');
      sw->when = A5_SW_COMES_ACROSS;
      if (sp != NULL) sw->come_key = sp + 1;
    }
  else
    {
      a5_parse_range (when, &sw->ft_from, &sw->ft_to);
      sw->when = a5_parse_sw_when (a5_last_token (when));
    }
  if (act != NULL)
    {
      char wbuf[32];
      sw->description = a5_action_split (act, wbuf, sizeof wbuf, &sw->task_key);
      sw->what = (sw->description != NULL) ? A5_SW_DISPLAY
                                           : a5_parse_sw_what (wbuf);
    }
  sw->only_apply_at = a5xml_child_text (c, "OnlyApplyAt");   /* sKey3 */
}

/* Parse one <Walk> body (clsWalk: FileIO.vb:1799-1865). */
static void
a5_parse_walk (a5_walk_t *w, const char *char_key, const a5_xml_node_t *c)
{
  w->char_key = char_key;
  w->description = a5xml_child_text (c, "Description");
  /* GetBool: True/1/-1/Vrai */
  w->loops = a5xml_bool (a5xml_child_text (c, "Loops"));
  w->start_active = a5xml_bool (a5xml_child_text (c, "StartActive"));
  a5_load_each (c, "Step",     &w->steps,    &w->n_steps,    a5_parse_step);
  a5_load_each (c, "Control",  &w->controls, &w->n_controls, a5_parse_control);
  a5_load_each (c, "Activity", &w->subwalks, &w->n_subwalks, a5_parse_subwalk);
}

static void
a5_load_walks (a5_character_t *ch)
{
  a5_load_each (ch->node, "Walk", &ch->walks, &ch->n_walks,
                [ch] (a5_walk_t *w, const a5_xml_node_t *c)
                {
                  a5_parse_walk (w, ch->key, c);
                });
}

/* One <SubEvent> (clsEvent SubEvent load). */
static void
a5_parse_subevent (a5_subevent_t *se, const a5_xml_node_t *c)
{
  const char *when = a5xml_child_text (c, "When");
  const char *what_elem = a5xml_child_text (c, "What");
  const char *only = a5xml_child_text (c, "OnlyApplyAt");
  a5_xml_node_t *act = a5xml_child (c, "Action");
  a5_parse_range (when, &se->ft_from, &se->ft_to);
  se->when = a5_parse_se_when (a5_last_token (when));
  if (act != NULL)
    {
      char wbuf[32];
      se->description = a5_action_split (act, wbuf, sizeof wbuf, &se->key);
      se->what = (se->description != NULL) ? A5_SE_DISPLAY
                                           : a5_parse_se_what (wbuf);
    }
  /* The <What> element overrides the action-derived kind, and for a
     DisplayMessage/SetLook the location/group gate is <OnlyApplyAt>
     (FileIO.vb:1722-1723: se.eWhat / se.sKey). */
  if (what_elem != NULL)
    se->what = a5_parse_se_what (what_elem);   /* EnumParseSubEventWhat */
  if (only != NULL)
    se->key = only;
}

/* Decode the parsed-enum / range fields of one <Event> (clsEvent FileIO load). */
static void
a5_parse_event_body (a5_event_t *e, const a5_xml_node_t *c)
{
  const char *ws = a5xml_child_text (c, "WhenStart");

  /* WhenStartEnum: Immediately=1, BetweenXandYTurns=2, AfterATask=3.  The Adrift 5 runner
     loads this with [Enum].Parse, which also accepts the *numeric* serialisation
     (some games write "<WhenStart>0</WhenStart>" -- e.g. Axe of Kolt's "Xixon On
     Path" event).  A value of 0 (or any non-1/2/3) matches no case in the
     game-start init Select, so the event stays NotYetStarted and is armed only by
     its Start control -- it must NOT be treated as Immediately. */
  e->when_start = 1;                                  /* Immediately default */
  if (ws != NULL && strcmp (ws, "BetweenXandYTurns") == 0) e->when_start = 2;
  else if (ws != NULL && strcmp (ws, "AfterATask") == 0)   e->when_start = 3;
  else if (ws != NULL && (ws[0] == '-' || (ws[0] >= '0' && ws[0] <= '9')))
    e->when_start = atoi (ws);
  a5_parse_range (a5xml_child_text (c, "StartDelay"), &e->start_from, &e->start_to);
  a5_parse_range (a5xml_child_text (c, "Length"),     &e->length_from, &e->length_to);
  /* GetBool: True/1/-1/Vrai (JJ ships RepeatCountdown=-1) */
  e->repeating        = a5xml_bool (a5xml_child_text (c, "Repeating"));
  e->repeat_countdown = a5xml_bool (a5xml_child_text (c, "RepeatCountdown"));
  a5_load_each (c, "Control",  &e->controls,  &e->n_controls,  a5_parse_control);
  a5_load_each (c, "SubEvent", &e->subevents, &e->n_subevents, a5_parse_subevent);
}

static void
a5_load_events (a5_adventure_t *a)
{
  a5_load_each (a->root, "Event", &a->events, &a->n_events,
                [] (a5_event_t *e, const a5_xml_node_t *c)
                {
                  e->node = c;
                  e->key = a5xml_child_text (c, "Key");
                  e->type = a5xml_child_text (c, "Type");
                  a5_parse_event_body (e, c);
                });
}

static void
a5_load_groups (a5_adventure_t *a)
{
  a5_load_each (a->root, "Group", &a->groups, &a->n_groups,
                [] (a5_group_t *g, const a5_xml_node_t *c)
                {
                  g->node = c;
                  g->key = a5xml_child_text (c, "Key");
                  g->type = a5xml_child_text (c, "Type");
                  g->name = a5xml_child_text (c, "Name");
                  g->members = a5_collect_text (c, "Member", &g->n_members);
                  g->props = a5_collect_props (c, &g->n_props);
                });
}

/* Object-group property inheritance (clsItemWithProperties.htblProperties layers
   htblInheritedProperties from each property-group the item belongs to,
   clsItem.vb:272-345).  We fold each Objects-type group's <Property> list into
   its member objects' static props at load time, so every object-property
   accessor (obj_prop/obj_has_prop, a5restr HaveProperty, a5text) sees them with
   no call-site changes.  The Adrift 5 runner's precedence: an inherited group property
   overrides the object's own value when both are present, else it is appended;
   groups iterate in load order so the last property-group wins on value.

   Scope note: inheritance is resolved once, at load.  Runtime add/remove from a
   group does not re-trigger it (the Adrift 5 runner recomputes via ResetInherited);
   not exercised by the current corpus. */
static void
a5_apply_group_properties (a5_adventure_t *a)
{
  int gi, mi, pi;
  for (gi = 0; gi < a->n_groups; gi++)
    {
      a5_group_t *g = &a->groups[gi];
      if (g->n_props == 0 || g->type == NULL || strcmp (g->type, "Objects") != 0)
        continue;
      for (mi = 0; mi < g->n_members; mi++)
        {
          a5_object_t *o = (a5_object_t *) a5model_object (a, g->members[mi]);
          if (o == NULL)
            continue;
          for (pi = 0; pi < g->n_props; pi++)
            {
              a5_prop_t *ex = (a5_prop_t *)
                a5_prop_find (o->props, o->n_props, g->props[pi].key);
              if (ex != NULL)
                {
                  ex->value = g->props[pi].value;
                  ex->value_node = g->props[pi].value_node;
                }
              else
                {
                  a5_prop_t *grown = (a5_prop_t *)
                    realloc (o->props, (size_t) (o->n_props + 1) * sizeof *grown);
                  if (grown == NULL)
                    continue;
                  o->props = grown;
                  o->props[o->n_props] = g->props[pi];  /* shallow: shares doc strings */
                  o->n_props++;
                }
            }
        }
    }
}

static void
a5_load_propdefs (a5_adventure_t *a)
{
  a5_load_each (a->root, "Property", &a->propdefs, &a->n_propdefs,
                [] (a5_propdef_t *p, const a5_xml_node_t *c)
                {
                  p->node = c;
                  p->key = a5xml_child_text (c, "Key");
                  p->type = a5xml_child_text (c, "Type");
                  p->property_of = a5xml_child_text (c, "PropertyOf");
                  p->dependent_key = a5xml_child_text (c, "DependentKey");
                  p->append_to = a5xml_child_text (c, "AppendTo");
                });
}

/* The label -> integer mapping of a ValueList property definition: the
   propdef's <ValueList><Label>/<Value> pairs (FD clsProperty.ValueList, an
   OrdinalIgnoreCase dictionary).  Returns the <Value> text (a string in the
   same document) or NULL when the label is unknown. */
const char *
a5model_valuelist_value (const a5_propdef_t *pd, const char *label)
{
  const a5_xml_node_t *c;
  if (pd == NULL || pd->node == NULL || label == NULL)
    return NULL;
  for (c = pd->node->first_child; c != NULL; c = c->next)
    if (strcmp (c->name, "ValueList") == 0)
      {
        const char *l = a5xml_child_text (c, "Label");
        const char *v = a5xml_child_text (c, "Value");
        if (l != NULL && v != NULL && strcasecmp (l, label) == 0)
          return v;
      }
  return NULL;
}

/* FD's clsProperty stores a ValueList property's value as the label's mapped
   INTEGER (the Value getter returns IntData.ToString, clsProperty.vb:152; the
   setter maps a label through the ValueList dictionary, vb:180-186), so
   %obj%.Weight reads "81" for a "Very heavy" boulder and the library's
   MaxWeight/MaxBulk carry restrictions sum real numbers.  Scarier keeps prop
   values as raw doc strings; repoint a label-valued ValueList prop at the
   propdef's own <ValueList><Value> text (same document, stable lifetime).
   An already-numeric value stays as-is (the label lookup misses, matching
   FD's SafeInt passthrough). */
static void
a5_fix_valuelist_props (a5_adventure_t *a)
{
  auto fix = [&] (a5_prop_t *props, int n)
  {
    for (int i = 0; i < n; i++)
      {
        a5_prop_t *p = &props[i];
        const a5_propdef_t *pd;
        const char *v;
        if (p->value == NULL)
          continue;
        pd = a5model_propdef (a, p->key);
        if (pd == NULL || pd->type == NULL
            || strcmp (pd->type, "ValueList") != 0)
          continue;
        v = a5model_valuelist_value (pd, p->value);
        if (v != NULL)
          p->value = v;
      }
  };
  int i;
  for (i = 0; i < a->n_objects; i++)
    fix (a->objects[i].props, a->objects[i].n_props);
  for (i = 0; i < a->n_characters; i++)
    fix (a->characters[i].props, a->characters[i].n_props);
  for (i = 0; i < a->n_locations; i++)
    fix (a->locations[i].props, a->locations[i].n_props);
  for (i = 0; i < a->n_groups; i++)
    fix (a->groups[i].props, a->groups[i].n_props);
}

static void
a5_load_alrs (a5_adventure_t *a)
{
  a5_load_each (a->root, "TextOverride", &a->alrs, &a->n_alrs,
                [] (a5_alr_t *r, const a5_xml_node_t *c)
                {
                  r->node = c;
                  r->key = a5xml_child_text (c, "Key");
                  r->old_text = a5xml_child_text (c, "OldText");
                  r->new_text = a5xml_child (c, "NewText");
                });
}

static void
a5_load_synonyms (a5_adventure_t *a)
{
  a5_load_each (a->root, "Synonym", &a->synonyms, &a->n_synonyms,
                [] (a5_synonym_t *s, const a5_xml_node_t *c)
                {
                  s->node = c;
                  s->key = a5xml_child_text (c, "Key");
                  s->from = a5_collect_text (c, "From", &s->n_from);
                  s->to = a5xml_child_text (c, "To");
                });
}

static void
a5_load_udfs (a5_adventure_t *a)
{
  a5_load_each (a->root, "Function", &a->udfs, &a->n_udfs,
                [] (a5_udf_t *u, const a5_xml_node_t *c)
                {
                  u->node = c;
                  u->key = a5xml_child_text (c, "Key");
                  u->name = a5xml_child_text (c, "Name");
                });
}

static void
a5_load_hints (a5_adventure_t *a)
{
  a5_load_each (a->root, "Hint", &a->hints, &a->n_hints,
                [] (a5_hint_t *h, const a5_xml_node_t *c)
                {
                  h->node = c;
                  h->key = a5xml_child_text (c, "Key");
                  h->question = a5xml_child_text (c, "Question");
                  h->subtle = a5xml_child (c, "Subtle");
                  h->sledgehammer = a5xml_child (c, "Sledgehammer");
                  h->restrictions = a5xml_child (c, "Restrictions");
                });
}

static void
a5_load_filemappings (a5_adventure_t *a)
{
  a5_load_each (a5xml_child (a->root, "FileMappings"), "Mapping",
                &a->filemaps, &a->n_filemaps,
                [] (a5_filemap_t *m, const a5_xml_node_t *c)
                {
                  const char *num = a5xml_child_text (c, "Resource");
                  m->number = num != NULL ? atoi (num) : -1;
                  m->file = a5xml_child_text (c, "File");
                });
}

/* The basename of an original (usually Windows) media path. */
static const char *
a5_path_basename (const char *path)
{
  const char *base = path, *p;
  if (path == NULL)
    return NULL;
  for (p = path; *p != '\0'; p++)
    if (*p == '\\' || *p == '/')
      base = p + 1;
  return base;
}

/* ------------------------------------------------------------------- public */

int
a5model_resource_for_file (const a5_adventure_t *a, const char *src)
{
  const char *sbase;
  int i;
  if (a == NULL || src == NULL)
    return -1;
  sbase = a5_path_basename (src);
  for (i = 0; i < a->n_filemaps; i++)
    {
      const char *fbase = a5_path_basename (a->filemaps[i].file);
      if (fbase != NULL && strcasecmp (fbase, sbase) == 0)
        return a->filemaps[i].number;
    }
  return -1;
}

/* ------------------------------------------------------------- key index */

/* One hash per entity family, key -> array index.  Built once after load and
   consulted by every a5model_* / a5state_*_index lookup: the corpus replays
   otherwise spend the bulk of their time in linear strcmp scans (Starship
   Quest alone holds ~2000 tasks and every OO read walks them).

   Open-addressed, keyed by the model's own stable `key` pointers: profiling
   showed unordered_map<std::string,int>::find dominating the visibility hot
   path purely on the temporary std::string built for every const char * probe
   (its ctor's memmove/strlen were top-of-stack).  This table hashes the C
   string directly and compares with strcmp -- no per-lookup allocation or
   copy.  Slots hold the entry's hash for a cheap pre-compare. */
struct a5_key_table
{
  struct slot { const char *key; unsigned hash; int idx; };
  std::vector<slot> slots;      /* size is a power of two; key==NULL = empty */
  size_t mask = 0;

  static unsigned
  hash_key (const char *s)
  {
    /* FNV-1a. */
    unsigned h = 2166136261u;
    for (; *s; s++)
      {
        h ^= (unsigned char) *s;
        h *= 16777619u;
      }
    return h;
  }

  void
  build (size_t n)
  {
    size_t cap = 16;
    while (cap < n * 2)
      cap <<= 1;
    slots.assign (cap, slot{NULL, 0, -1});
    mask = cap - 1;
  }

  void
  insert_first (const char *key, int idx)
  {
    /* Keep the FIRST occurrence of a duplicate key, matching the linear
       scans' first-match semantics. */
    unsigned h = hash_key (key);
    size_t i = h & mask;
    while (slots[i].key != NULL)
      {
        if (slots[i].hash == h && strcmp (slots[i].key, key) == 0)
          return;
        i = (i + 1) & mask;
      }
    slots[i] = slot{key, h, idx};
  }

  int
  find (const char *key) const
  {
    unsigned h = hash_key (key);
    size_t i = h & mask;
    while (slots[i].key != NULL)
      {
        if (slots[i].hash == h && strcmp (slots[i].key, key) == 0)
          return slots[i].idx;
        i = (i + 1) & mask;
      }
    return -1;
  }
};

struct a5_key_index
{
  a5_key_table objects, locations, characters, tasks, variables;
};

template <typename T>
static void
index_family (a5_key_table &m, const T *arr, int n)
{
  int i;
  m.build ((size_t) (n > 0 ? n : 1));
  for (i = 0; i < n; i++)
    if (arr[i].key != NULL)
      m.insert_first (arr[i].key, i);
}

/* Look `key` up in one family: through its hash when the model carries one,
   else (hand-assembled unit-test fixtures) a first-match linear scan. */
template <typename T>
static int
family_index (const a5_key_table *m, const T *arr, int n, const char *key)
{
  int i;
  if (m != NULL)
    return m->find (key);
  for (i = 0; i < n; i++)
    if (arr[i].key != NULL && strcmp (arr[i].key, key) == 0)
      return i;
  return -1;
}

static void
a5model_build_key_index (a5_adventure_t *a)
{
  a5_key_index *ix = new a5_key_index;
  index_family (ix->objects,    a->objects,    a->n_objects);
  index_family (ix->locations,  a->locations,  a->n_locations);
  index_family (ix->characters, a->characters, a->n_characters);
  index_family (ix->tasks,      a->tasks,      a->n_tasks);
  index_family (ix->variables,  a->variables,  a->n_variables);
  a->key_index = ix;
}

int
a5model_key_index (const a5_adventure_t *a, int kind, const char *key)
{
  const a5_key_index *ix = (const a5_key_index *) a->key_index;
  if (key == NULL)
    return -1;
  switch (kind)
    {
    case 'O':
      return family_index (ix ? &ix->objects : NULL,
                           a->objects, a->n_objects, key);
    case 'L':
      return family_index (ix ? &ix->locations : NULL,
                           a->locations, a->n_locations, key);
    case 'C':
      return family_index (ix ? &ix->characters : NULL,
                           a->characters, a->n_characters, key);
    case 'T':
      return family_index (ix ? &ix->tasks : NULL,
                           a->tasks, a->n_tasks, key);
    case 'V':
      return family_index (ix ? &ix->variables : NULL,
                           a->variables, a->n_variables, key);
    }
  return -1;
}

a5_adventure_t *
a5model_from_doc (a5_xml_doc_t *doc)
{
  a5_adventure_t *a;
  a5_xml_node_t *root = a5xml_root (doc);

  if (root == NULL || strcmp (root->name, "Adventure") != 0)
    return NULL;

  a = (a5_adventure_t *) calloc (1, sizeof *a);
  if (a == NULL)
    return NULL;
  a->doc = doc;
  a->root = root;
  /* Not adventure data: the Runner layout rides alongside the game in its
     container, so a document on its own ships no answer (see a5model_load). */
  a->map_pane_open = -1;
  a->title = a5xml_child_text (root, "Title");
  a->author = a5xml_child_text (root, "Author");
  a->version = a5xml_child_text (root, "Version");
  a->introduction = a5xml_child (root, "Introduction");
  a->end_game_text = a5xml_child (root, "EndGameText");
  /* <ShowFirstLocation> gates the post-intro start-room display; absent =>
     true (clsAdventure.bShowFirstRoom default), "0" => false. */
  {
    const char *sfl = a5xml_child_text (root, "ShowFirstLocation");
    a->show_first_location = !(sfl != NULL && strcmp (sfl, "0") == 0);
  }
  /* <BackgroundColour> / <InputColour> / <OutputColour> / <LinkColour>: the
     palette the author chose for the Runner's output pane.  ADRIFT writes them
     as Windows OLE colour integers -- BLUE, GREEN, RED byte order -- rendered
     in decimal (ADRIFT-5-XML.md 2.6), and omits any element that still holds
     the Runner default, so a missing element means "the default", not "no
     colour".  Held here in the ordinary 0xRRGGBB order.
     The defaults are the ADRIFT 5 Runner's own (Global.vb DEFAULT_*COLOUR,
     stored there as ARGB): black background, red input, teal output, and a
     pale cyan for hyperlinks. */
  {
    static const struct { const char *name; uint32_t dflt; } A5_COLOURS[] = {
      {"BackgroundColour", 0x000000}, {"InputColour",  0xd22527},
      {"OutputColour",     0x19a58a}, {"LinkColour",   0x4bd7bc}
    };
    uint32_t *field[4];
    int i;

    field[0] = &a->bg_colour;    field[1] = &a->input_colour;
    field[2] = &a->output_colour; field[3] = &a->link_colour;
    for (i = 0; i < 4; i++)
      {
        const char *text = a5xml_child_text (root, A5_COLOURS[i].name);
        if (text != NULL && text[0] != '\0')
          {
            /* Read signed and mask: ADRIFT writes the value through VB's
               CInt, so a file that stored an ARGB rather than an OLE colour
               arrives negative, with the alpha byte to be dropped.  In an OLE
               integer the LOW byte is red and the high byte blue (pure red is
               0x0000FF, cyan 0xFFFF00), so swapping the outer two bytes gives
               0xRRGGBB. */
            uint32_t ole = (uint32_t) strtol (text, NULL, 10) & 0xffffffu;
            *field[i] = ((ole & 0x0000ffu) << 16)
                        | (ole & 0x00ff00u)
                        | ((ole & 0xff0000u) >> 16);
          }
        else
          *field[i] = A5_COLOURS[i].dflt;
      }
  }
  /* <ShowExits> gates the "Exits are .../An exit leads ..." listing appended
     to every location view; absent => true (clsAdventure.bShowExits default).
     ADRIFT serialises the boolean as "True"/"False" text (FileIO.GetBool:
     only TRUE/1/-1/VRAI are true, everything else false). */
  {
    const char *se = a5xml_child_text (root, "ShowExits");
    a->show_exits = (se == NULL) || a5xml_bool (se);
  }
  /* <WaitTurns>: how many turns a single "wait"/"z" advances (the SystemTasks
     wait loop); absent => clsAdventure._iWaitTurns default of 3. */
  {
    const char *wt = a5xml_child_text (root, "WaitTurns");
    a->wait_turns = (wt != NULL) ? (int) strtol (wt, NULL, 10) : 3;
    if (a->wait_turns < 0) a->wait_turns = 0;
  }
  /* <TaskExecution>: HighestPriorityTask vs HighestPriorityPassingTask.  Under
     the latter (the "v4 logic" mode) a task that matches the command but fails
     its restrictions with output does not claim the turn -- the scan keeps
     looking for a lower-priority passing task, and the recorded fail message is
     shown only if none is found.

     The <TaskExecution> element and the HighestPriorityTask default arrived in
     ADRIFT 5.0.22; games saved before that (and re-serialising without the
     element) predate the setting and ran with the v4-compatible
     HighestPriorityPassingTask behaviour in Campbell's Runner.  FrankenDrift
     hardcodes HighestPriorityTask for element-less files regardless of version,
     which makes genuinely pre-5.0.22 games (e.g. Return to Camelot, v5.000020,
     whose central "unlock chain" puzzle only fires as a lower-priority passing
     task after the stock library "cannot be unlocked" fallback) unwinnable.  So
     when the element is absent, fall back on the file version: below 5.000022 =>
     HighestPriorityPassingTask, at/after => HighestPriorityTask.  (This is the
     one point where Scarier deliberately diverges from FrankenDrift, which lacks
     the version gate -- see A5_WALKTHROUGH_FINDINGS.md.) */
  {
    const char *te = a5xml_child_text (root, "TaskExecution");
    if (te != NULL)
      a->hp_passing = (strcmp (te, "HighestPriorityPassingTask") == 0);
    else
      {
        double fv = (a->version != NULL) ? strtod (a->version, NULL) : 0.0;
        a->hp_passing = (fv > 0.0 && fv < 5.000022);
      }
  }
  /* <Direction*>: per-direction synonym specs for non-English games (the
     localization subsystem, FileIO.vb:1254-1265 overriding clsAdventure's
     English sDirectionsRE defaults).  Stored in DirectionsEnum order; an absent
     or empty field keeps the English default (a5parse_set_directions). */
  {
    static const char *const kFields[12] = {
      "DirectionNorth", "DirectionEast", "DirectionSouth", "DirectionWest",
      "DirectionUp", "DirectionDown", "DirectionIn", "DirectionOut",
      "DirectionNorthEast", "DirectionSouthEast", "DirectionSouthWest",
      "DirectionNorthWest" };
    int i;
    for (i = 0; i < 12; i++)
      {
        const char *v = a5xml_child_text (root, kFields[i]);
        a->dir_re[i] = (v != NULL && v[0] != '\0') ? v : NULL;
      }
  }

  a5_load_propdefs (a);
  a5_load_locations (a);
  a5_load_objects (a);
  a5_load_tasks (a);
  a5_load_events (a);
  a5_load_characters (a);
  a5_load_variables (a);
  a5_load_groups (a);
  a5_apply_group_properties (a);
  a5_fix_valuelist_props (a);
  a5_load_alrs (a);
  a5_load_udfs (a);
  a5_load_hints (a);
  a5_load_synonyms (a);
  a5_load_filemappings (a);
  a5model_build_key_index (a);
  return a;
}

/*
 * Extract the Babel IFID from a raw file buffer.  ADRIFT 5.0.20+ files carry a
 * plaintext <ifindex> metadata block (in a bare .taf, right after the version
 * header; in a blorb, an iFiction metadata chunk), which the obfuscation never
 * touches, so a literal case-insensitive scan for "<ifid>...</ifid>" over the
 * whole file finds it uniformly across .taf and .blorb layouts (Treaty of
 * Babel adrift.c get_story_file_IFID).  The deflated game payload is binary and
 * will not contain the ASCII tag by chance.  Returns an owned copy or NULL.
 */
static char *
a5_scan_tag (const uint8_t *buf, uint32_t len, const char *open)
{
  const uint32_t open_len = (uint32_t) strlen (open);
  uint32_t i;
  if (buf == NULL || len < open_len)
    return NULL;
  for (i = 0; i + open_len <= len; i++)
    {
      if (strncasecmp ((const char *) buf + i, open, open_len) != 0)
        continue;
      uint32_t s = i + open_len, e = s;
      while (e < len && buf[e] != '<')
        e++;
      if (e > s && e - s < 128)
        {
          char *out = (char *) malloc (e - s + 1);
          if (out != NULL)
            {
              memcpy (out, buf + s, e - s);
              out[e - s] = '\0';
            }
          return out;
        }
      return NULL;
    }
  return NULL;
}

static char *
a5_scan_ifid (const uint8_t *buf, uint32_t len)
{
  return a5_scan_tag (buf, len, "<ifid>");
}

/* The same <ifindex> block's <releases><attached><release><version> -- what
   %release% renders (BabelTreatyInfo.Stories(0).Releases.Attached.Release
   .Version, Global.vb:1766).  The compound open-tag scan keys on the
   <release> wrapper so the block's other <version>-bearing tags
   (<compilerversion>) cannot match. */
static char *
a5_scan_release (const uint8_t *buf, uint32_t len)
{
  return a5_scan_tag (buf, len, "<release><version>");
}

a5_adventure_t *
a5model_load (const char *path)
{
  FILE *fp;
  long size;
  uint8_t *file_buf, *payload;
  uint32_t file_len, payload_len, header, region_len, xml_len;
  uint8_t *xml;
  char *ifid;
  char *release;
  a5_blorb_chunk_t chunk;
  a5_xml_doc_t *doc;
  a5_adventure_t *adv;

  fp = fopen (path, "rb");
  if (fp == NULL)
    return NULL;
  fseek (fp, 0, SEEK_END);
  size = ftell (fp);
  fseek (fp, 0, SEEK_SET);
  if (size <= 0)
    {
      fclose (fp);
      return NULL;
    }
  file_buf = (uint8_t *) malloc ((size_t) size);
  if (file_buf == NULL || fread (file_buf, 1, (size_t) size, fp) != (size_t) size)
    {
      free (file_buf);
      fclose (fp);
      return NULL;
    }
  fclose (fp);
  file_len = (uint32_t) size;

  int from_blorb;
  if (a5blorb_find_exec (file_buf, file_len, &chunk))
    {
      payload = (uint8_t *) chunk.data;
      payload_len = chunk.size;
      from_blorb = 1;
    }
  else
    {
      payload = file_buf;
      payload_len = file_len;
      from_blorb = 0;
    }

  if (payload_len < 30)
    {
      free (file_buf);
      return NULL;
    }
  /* 5.0.20+ .taf/.blorb payloads insert a Babel <ifindex> metadata block
     right after the 4-byte hex size field at offset 12: a size of "0000"
     means an empty (absent) block, but a non-empty block is only detectable
     by the literal "<ifindex" tag at offset 16 (the Adrift 5 runner FileIO.vb:800,
     sSize = "0000" OrElse sCheck = "<ifindex"). Skip exactly that many bytes
     before the obfuscated/deflated game payload starts. */
  int obfuscated;
  {
    int is_zero_size = (payload[12] == '0' && payload[13] == '0'
                         && payload[14] == '0' && payload[15] == '0');
    int has_ifindex = (payload_len >= 24
                        && memcmp (payload + 16, "<ifindex", 8) == 0);
    if (is_zero_size || has_ifindex)
      {
        char size_hex[5];
        memcpy (size_hex, payload + 12, 4);
        size_hex[4] = '\0';
        header = 16 + (uint32_t) strtoul (size_hex, NULL, 16);
        obfuscated = 1;
      }
    else
      {
        /* Pre-5.0.20 layout: the deflate stream follows the 12-byte version
           header directly.  For a BARE .taf the runner treats this layout as NOT
           obfuscated (FileIO.vb:816 "Pre 5.0.20 ... bObfuscate = False") --
           it is also the layout of the game data embedded in ADRIFT 5
           .exe-wrapped games (the virtual human).  A BLORB Exec chunk takes a
           different the runner path (FileIO.vb:753): same 12-byte header offset, but
           obfuscation is decided by the Blorb metadata (bDeObfuscate), which
           is true for every ADRIFT-Developer-built blorb -- RtC.blorb is
           exactly this old-layout-but-obfuscated shape. */
        header = 12;
        obfuscated = from_blorb;
      }
  }
  if (header + 14 > payload_len)
    {
      free (file_buf);
      return NULL;
    }
  region_len = payload_len - header - 14;

  /* Read the Babel IFID from the still-plaintext buffer before deobfuscation
     scrambles the payload region (the <ifindex> block lies outside it, but scan
     first to be safe across layouts). */
  ifid = a5_scan_ifid (file_buf, file_len);
  release = a5_scan_release (file_buf, file_len);

  /* Likewise the author's Runner window layout, which lives in a chunk of its
     own beside the game (see a5blorb_find_layout): read it while the whole
     file is still to hand, since only the answer outlives file_buf. */
  int map_pane_open = -1;
  {
    a5_blorb_chunk_t layout;
    if (from_blorb && a5blorb_find_layout (file_buf, file_len, &layout))
      map_pane_open = a5blorb_layout_pane_open (layout.data, layout.size, "Map");
  }

  if (obfuscated)
    a5_deobfuscate (payload, header, region_len);
  xml = a5_inflate (payload + header, region_len, &xml_len);
  free (file_buf);
  if (xml == NULL)
    {
      free (ifid);
      free (release);
      return NULL;
    }

  {
    const char *dp = getenv ("A5_DUMP_XML");
    if (dp != NULL && dp[0] != '\0')
      {
        /* Convenience: A5_DUMP_XML is a FILE PATH.  As a shorthand, a bare
           "1" / "y" / "-" (i.e. someone treating it like a boolean flag) is
           mapped to a stable default path so the common "=1" mistake still
           produces a usable dump instead of a file literally named "1". */
        if ((dp[1] == '\0' && (dp[0] == '1' || dp[0] == 'y' || dp[0] == '-')))
          dp = "/tmp/a5dump.xml";
        FILE *df = fopen (dp, "wb");
        if (df != NULL)
          {
            fwrite (xml, 1, xml_len, df);
            fclose (df);
            fprintf (stderr, "[A5_DUMP_XML] wrote %lu bytes to %s\n",
                     (unsigned long) xml_len, dp);
          }
      }
  }

  doc = a5xml_parse ((char *) xml, xml_len);
  if (doc == NULL)
    {
      free (xml);
      free (ifid);
      free (release);
      return NULL;
    }

  adv = a5model_from_doc (doc);
  if (adv == NULL)
    {
      a5xml_free (doc);
      free (ifid);
      free (release);
    }
  else
    {
      adv->ifid = ifid;   /* ownership transfers; a5model_free releases them */
      adv->release = release;
      adv->map_pane_open = map_pane_open;
    }
  return adv;
}

/* ---------------------------------------- "Adventure Upgrade" (FileIO.vb) */

/*
 * Walk the whole document for <Restrictions> blocks carrying a
 * <BracketSequence>, calling fn on each such BracketSequence node.  The runner's
 * BuildRestrictions loads every restriction block (tasks, events, walks,
 * topics...) through the same code path, so the ask/correct pass must see them
 * all, not just the task blocks.
 */
static void
upgrade_walk (const a5_xml_node_t *n,
              void (*fn) (a5_xml_node_t *bs, void *ctx), void *ctx)
{
  const a5_xml_node_t *c;
  if (n == NULL)
    return;
  if (strcmp (n->name, "Restrictions") == 0)
    {
      a5_xml_node_t *bs = a5xml_child (n, "BracketSequence");
      if (bs != NULL && bs->text != NULL)
        fn (bs, ctx);
    }
  for (c = n->first_child; c != NULL; c = c->next)
    upgrade_walk (c, fn, ctx);
}

static void
upgrade_detect_cb (a5_xml_node_t *bs, void *ctx)
{
  if (strstr (bs->text, "#A#O#") != NULL)
    *(int *) ctx = 1;
}

int
a5model_upgrade_pending (const a5_adventure_t *a)
{
  a5_adventure_t *ma = (a5_adventure_t *) a;   /* memo fields only */
  if (a == NULL)
    return 0;
  if (!a->upgrade_scanned)
    {
      /* The runner asks at the first BuildRestrictions whose sequence contains
         "#A#O#", only for files saved before the 5.0.26 logic correction
         (FileIO.vb:634: dFileVersion < 5.000026). */
      double fv = (a->version != NULL) ? strtod (a->version, NULL) : 0.0;
      int wanted = 0;
      if (fv > 0.0 && fv < 5.000026)
        upgrade_walk (a->root, upgrade_detect_cb, &wanted);
      ma->upgrade_wanted = wanted;
      ma->upgrade_scanned = 1;
    }
  return a->upgrade_wanted && !a->upgrade_prompted;
}

const char *
a5model_upgrade_question (void)
{
  /* Glue.AskYesNoQuestion body (FileIO.vb:635); the title is "Adventure
     Upgrade".  FrankenDrift.Headless emits title + "\n" + body with no
     trailing break, so the first real output abuts "...AND restrictions.". */
  return
    "There was a logic correction in version 5.0.26 which means OR "
    "restrictions after AND restrictions were not evaluated.  Would "
    "you like to auto-correct these tasks?\n\n"
    "You may not wish to do so if you have already used brackets "
    "around any OR restrictions following AND restrictions.";
}

/*
 * the runner FileIO.vb:1157 CorrectBracketSequence: longest run first, rewrite every
 * "#A#O#O#..." into "#A(#O#O#...)".  VB String.Replace replaces ALL
 * occurrences per call and iCorrectedTasks counts the calls (one per While
 * pass), not the occurrences.  Returns a heap copy when anything changed,
 * else NULL.
 */
static char *
correct_bracket_sequence (const char *s, int *count)
{
  if (strstr (s, "#A#O#") == NULL)
    return NULL;
  std::string seq (s);
  for (int i = 10; i >= 0; i--)
    {
      std::string search = "#A#";
      for (int j = 0; j <= i; j++)
        search += "O#";
      while (seq.find (search) != std::string::npos)
        {
          std::string repl = "#A(#";
          for (int j = 0; j <= i; j++)
            repl += "O#";
          repl += ")";
          size_t pos = 0;
          while ((pos = seq.find (search, pos)) != std::string::npos)
            {
              seq.replace (pos, search.size (), repl);
              pos += repl.size ();
            }
          (*count)++;
        }
    }
  return strdup (seq.c_str ());
}

static void
upgrade_apply_cb (a5_xml_node_t *bs, void *ctx)
{
  a5_adventure_t *a = (a5_adventure_t *) ctx;
  char *fixed = correct_bracket_sequence (bs->text, &a->upgrade_count);
  if (fixed == NULL)
    return;
  /* The parse is in situ (node text aliases the file buffer) and the corrected
     sequence is longer, so repoint the node at an owned copy and remember it
     for a5model_free. */
  bs->text = fixed;
  a->upgrade_owned = (char **) realloc (a->upgrade_owned,
                                        (size_t) (a->n_upgrade_owned + 1)
                                        * sizeof *a->upgrade_owned);
  a->upgrade_owned[a->n_upgrade_owned++] = fixed;
}

int
a5model_upgrade_answer (a5_adventure_t *a, int yes)
{
  if (a == NULL || !a5model_upgrade_pending (a))
    return 0;
  a->upgrade_prompted = 1;
  if (yes)
    {
      a->upgrade_applied = 1;
      upgrade_walk (a->root, upgrade_apply_cb, a);
    }
  return a->upgrade_count;
}

/*
 * The "Adventure Upgrade" question defaults to NO (matching an unattended
 * the Adrift 5 runner: the pre-5.0.26 BracketSequence is read verbatim), so an
 * interactive host that suppresses the dialog can just leave the file
 * uncorrected.  A short hard-wired allow-list forces YES for the handful of
 * games whose intended (post-5.0.26) logic genuinely needs the correction to
 * play or score correctly -- keyed on the Babel IFID (unique per game; see
 * a5_scan_ifid), with a title+author fallback for any target that lacks one.
 */
int
a5model_upgrade_forced_yes (const a5_adventure_t *a)
{
  static const char *const ifids[] = {
    "ADRIFT-500-38B2-7938-479E-9893-D365BC92FFBF",  /* The House (Po. Prune), Ectocomp 2011 */
    "ADRIFT-500-ACE0-ED0B-46A9-8371-6310F781344A",  /* Head Case (Intro_compVB1) */
  };
  size_t i;
  if (a == NULL)
    return 0;
  if (a->ifid != NULL)
    for (i = 0; i < sizeof ifids / sizeof ifids[0]; i++)
      if (strcasecmp (a->ifid, ifids[i]) == 0)
        return 1;
  return 0;
}

const char *
a5model_load_error (const a5_adventure_t *a)
{
  if (a == NULL || a->n_locations == 0)
    return "This adventure has no locations.  Cannot continue.";
  return NULL;
}

void
a5model_free (a5_adventure_t *a)
{
  int i;
  if (a == NULL)
    return;
  delete (a5_key_index *) a->key_index;
  a->key_index = NULL;
  for (i = 0; i < a->n_objects; i++)
    {
      free ((void *) a->objects[i].names);
      free (a->objects[i].props);
    }
  for (i = 0; i < a->n_locations; i++)
    free (a->locations[i].props);
  for (i = 0; i < a->n_characters; i++)
    {
      int w;
      free ((void *) a->characters[i].descriptors);
      free (a->characters[i].props);
      for (w = 0; w < a->characters[i].n_topics; w++)
        if (a->characters[i].topics[w].is_command)
          free ((void *) a->characters[i].topics[w].keywords);  /* owned (a5_correct_command) */
      free (a->characters[i].topics);
      for (w = 0; w < a->characters[i].n_walks; w++)
        {
          free (a->characters[i].walks[w].steps);
          free (a->characters[i].walks[w].subwalks);
          free (a->characters[i].walks[w].controls);
        }
      free (a->characters[i].walks);
    }
  for (i = 0; i < a->n_tasks; i++)
    {
      int s;
      for (s = 0; s < a->tasks[i].n_commands; s++)
        free ((void *) a->tasks[i].commands[s]);  /* owned (a5_correct_command) */
      free ((void *) a->tasks[i].commands);
      for (s = 0; s < a->tasks[i].n_specifics; s++)
        free ((void *) a->tasks[i].specifics[s].keys);
      free (a->tasks[i].specifics);
    }
  for (i = 0; i < a->n_events; i++)
    {
      free (a->events[i].subevents);
      free (a->events[i].controls);
    }
  for (i = 0; i < a->n_groups; i++)
    {
      free ((void *) a->groups[i].members);
      free (a->groups[i].props);
    }
  free (a->objects);
  free (a->locations);
  free (a->characters);
  free (a->tasks);
  free (a->variables);
  free (a->events);
  free (a->groups);
  free (a->propdefs);
  free (a->alrs);
  free (a->udfs);
  free (a->hints);
  for (i = 0; i < a->n_synonyms; i++)
    free ((void *) a->synonyms[i].from);
  free (a->synonyms);
  free (a->filemaps);
  for (i = 0; i < a->n_upgrade_owned; i++)
    free (a->upgrade_owned[i]);  /* corrected BracketSequence copies */
  free (a->upgrade_owned);
  free ((void *) a->ifid);
  free ((void *) a->release);
  a5xml_free (a->doc);
  free (a);
}

/* ------------------------------------------------------------- key lookups */

const a5_object_t *
a5model_object (const a5_adventure_t *a, const char *key)
{
  int i = a5model_key_index (a, 'O', key);
  return i >= 0 ? &a->objects[i] : NULL;
}

const a5_location_t *
a5model_location (const a5_adventure_t *a, const char *key)
{
  int i = a5model_key_index (a, 'L', key);
  return i >= 0 ? &a->locations[i] : NULL;
}

const a5_character_t *
a5model_character (const a5_adventure_t *a, const char *key)
{
  int i = a5model_key_index (a, 'C', key);
  return i >= 0 ? &a->characters[i] : NULL;
}

const a5_task_t *
a5model_task (const a5_adventure_t *a, const char *key)
{
  int i = a5model_key_index (a, 'T', key);
  return i >= 0 ? &a->tasks[i] : NULL;
}

const a5_variable_t *
a5model_variable (const a5_adventure_t *a, const char *key)
{
  int i = a5model_key_index (a, 'V', key);
  return i >= 0 ? &a->variables[i] : NULL;
}

const a5_propdef_t *
a5model_propdef (const a5_adventure_t *a, const char *key)
{
  int i;
  if (key == NULL)
    return NULL;
  for (i = 0; i < a->n_propdefs; i++)
    if (a->propdefs[i].key != NULL && strcmp (a->propdefs[i].key, key) == 0)
      return &a->propdefs[i];
  return NULL;
}

/* ------------------------------------------------------------- exit lookups */

const a5_xml_node_t *
a5model_movement (const a5_location_t *loc, const a5_xml_node_t *after,
                  const char *dir)
{
  const a5_xml_node_t *c;

  if (loc == NULL || loc->node == NULL || dir == NULL)
    return NULL;
  c = (after != NULL) ? after->next : loc->node->first_child;
  for (; c != NULL; c = c->next)
    {
      if (strcmp (c->name, "Movement") != 0)
        continue;
      if (streq (a5xml_child_text (c, "Direction"), dir))
        return c;
    }
  return NULL;
}

const char *
a5model_exit_dest (const a5_adventure_t *a, const char *lockey, const char *dir)
{
  const a5_location_t *l = (lockey != NULL) ? a5model_location (a, lockey) : NULL;
  const a5_xml_node_t *m = a5model_movement (l, NULL, dir);
  const char *dest = (m != NULL) ? a5xml_child_text (m, "Destination") : NULL;

  return (dest != NULL && dest[0] != '\0') ? dest : NULL;
}
