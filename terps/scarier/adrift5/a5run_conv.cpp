/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- the conversation layer.
 *
 * clsUserSession's FindConversationNode / ExecuteConversation: driving a
 * Greet/Ask/Tell/Command/Farewell against a character, picking the best topic at
 * the current conversation node (Ask/Tell by keyword score, Command by command
 * pattern, Greet/Farewell by restrictions alone), emitting its reply and running
 * its actions, and ending the conversation when the partner walks away.  Split
 * out of a5run_action.cpp, which keeps the action dispatch that calls in here
 * (the Conversation action and the MoveCharacter branches).
 *
 * Entry points used from a5run_action.cpp -- exec_conversation,
 * clear_conv_if_partner_gone, conv_contains_word and the A5_CONV_* bits -- are
 * declared in a5run_internal.h.  Topic actions run back through run_action
 * there, so the two files are mutually recursive by design: the runner's
 * ExecuteConversation likewise calls ExecuteActions.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "a5parse.h"
#include "a5restr.h"
#include "a5run.h"
#include "a5run_internal.h"
#include "a5sb.h"
#include "a5text.h"

/* ContainsWord (clsUserSession.vb:3885): every space-separated word of `check`
   appears among the space-separated words of `sentence` (case-insensitive).

   FAITHFUL QUIRK: the runner splits with VB `Split(x, " ")`, which KEEPS empty tokens.
   So an empty `check` (an empty-keyword Ask/Tell topic, e.g. RtC's "ask about
   igor" Topic6 with <Keywords/>) splits to the single check-word "", which is
   found only when the sentence also has an empty token -- i.e. a non-empty
   subject like "freeze" never matches an empty keyword.  C++ `split_ws` drops
   empties, so an empty keyword used to match EVERYTHING (Topic6 stole the turn
   from the keyworded "freeze" Topic7).  Mirror VB Split (split on a single
   space, keep empties) exactly. */
int
conv_contains_word (const std::string &sentence, const std::string &check)
{
  auto vb_split = [] (const std::string &s) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;)
      {
        size_t sp = s.find (' ', start);
        if (sp == std::string::npos) { out.push_back (s.substr (start)); break; }
        out.push_back (s.substr (start, sp - start));
        start = sp + 1;
      }
    return out;
  };
  std::vector<std::string> words = vb_split (lower (sentence));
  for (auto &c : vb_split (check))
    {
      int found = 0;
      for (auto &w : words)
        if (w == c) { found = 1; break; }
      if (!found)
        return 0;
    }
  return 1;
}

static int
topic_has_children (const a5_character_t *ch, const char *key)
{
  int i;
  for (i = 0; i < ch->n_topics; i++)
    if (streq (ch->topics[i].parent_key, key))
      return 1;
  return 0;
}

/* Render a topic reply / message with the conversation character as the text
   context, trimmed and newline-terminated (one AddResponse). */
static void
emit_conv (char *rendered_owned, sb_t *out)
{
  if (msg_has_output (rendered_owned))
    { sb_pspace (out); sb_puts (out, rendered_owned); }
  free (rendered_owned);
}

/* Render a topic's <Conversation> with `ctx_key` as the text context and the
   intro flag set (the runner's conversation-reply emit), restoring both around
   the one AddResponse.  Shared by the intro / topic-reply / implicit-farewell
   paths of exec_conversation and clear_conv_if_partner_gone. */
static void
emit_topic_conv (a5_run_t *run, const char *ctx_key,
                 const a5_xml_node_t *conversation, sb_t *out)
{
  a5_state_t *st = run->st;
  const char *pc = st->ctx_char;
  int pia = st->intro_active;
  st->ctx_char = ctx_key;
  st->intro_active = 1;
  emit_conv (a5text_describe (st, conversation), out);
  st->intro_active = pia;
  st->ctx_char = pc;
}

/* On a topic whose trigger matched but whose restrictions failed, capture the
   restriction's fail-message text into *restr_text (last wins), so the caller can
   surface it as the "doesn't understand" default.  A no-op when restr_text is
   NULL (the callers that do not want the default). */
static void
capture_restr_text (a5_state_t *st, const a5_xml_node_t *restrictions,
                    std::string *restr_text)
{
  if (restr_text == NULL)
    return;
  const a5_xml_node_t *fm = a5restr_fail_message (st, restrictions);
  if (fm != NULL)
    { char *t2 = a5text_describe (st, fm); *restr_text = t2; free (t2); }
}

/*
 * FindConversationNode (clsUserSession): the best topic of `ch` matching the
 * conversation type + subject at the current node.  Ask/Tell topics match by
 * keyword (most matches, then highest %); Command topics by command pattern;
 * Greet/Farewell topics just by restrictions.  When a candidate matches its
 * trigger but fails its restrictions, its restriction fail-message is captured
 * into *restr_text (last wins), so the caller can surface it as the default.
 */
static const a5_topic_t *
find_conv_node (a5_run_t *run, const a5_character_t *ch, int conv_type,
                const char *subject, std::string *restr_text)
{
  a5_state_t *st = run->st;
  int t = conv_type;
  int b_farewell = 0, b_command = 0, b_tell = 0, b_ask = 0, b_intro = 0;
  double best_pct = 0.0;
  int best_matches = 0;
  const a5_topic_t *best = NULL;
  int i;

  if (t >= A5_CONV_FAREWELL) { b_farewell = 1; t -= A5_CONV_FAREWELL; }
  if (t >= A5_CONV_COMMAND)  { b_command  = 1; t -= A5_CONV_COMMAND; }
  if (t >= A5_CONV_TELL)     { b_tell     = 1; t -= A5_CONV_TELL; }
  if (t >= A5_CONV_ASK)      { b_ask      = 1; t -= A5_CONV_ASK; }
  if (t >= A5_CONV_GREET)    b_intro    = 1;   /* last bit: no further t use */

  for (i = 0; i < ch->n_topics; i++)
    {
      const a5_topic_t *tp = &ch->topics[i];
      if (!(streq (tp->parent_key, "") || streq (tp->parent_key, st->conv_node)))
        continue;
      if (b_intro && !tp->is_intro)       continue;
      if (b_farewell && !tp->is_farewell) continue;
      if (b_command != tp->is_command)    continue;
      if (b_ask && !tp->is_ask)           continue;
      if (b_tell && !tp->is_tell)         continue;

      if (b_ask || b_tell)
        {
          std::vector<std::string> kws;
          { std::string cur; const char *p = tp->keywords;
            for (; p && *p; p++)
              { if (*p == ',') { kws.push_back (cur); cur.clear (); }
                else cur += *p; }
            kws.push_back (cur); }
          int matched = 0, low_priority = 0;
          for (auto &kw : kws)
            {
              std::string k = lower (kw);
              size_t a = k.find_first_not_of (" \t");
              size_t z = k.find_last_not_of (" \t");
              k = (a == std::string::npos) ? "" : k.substr (a, z - a + 1);
              if (conv_contains_word (subject, k) || k == "*")
                {
                  if (a5restr_pass (st, tp->restrictions))
                    matched++;
                  else
                    capture_restr_text (st, tp->restrictions, restr_text);
                  if (k == "*") low_priority = 1;
                }
            }
          double pct = kws.empty () ? 0.0 : (double) matched / (double) kws.size ();
          if (low_priority && pct == 1.0) pct = 0.001;
          if (matched > best_matches
              || (matched == best_matches && pct > best_pct))
            { best = tp; best_pct = pct; best_matches = matched; }
        }
      else if (b_command)
        {
          a5_match_t m;
          if (a5parse_match_command (tp->keywords, subject, &m))
            {
              if (a5restr_pass (st, tp->restrictions))
                return tp;
              else
                capture_restr_text (st, tp->restrictions, restr_text);
            }
        }
      else /* greet / farewell: no keyword match, just restrictions */
        {
          if (a5restr_pass (st, tp->restrictions))
            return tp;
          else
            capture_restr_text (st, tp->restrictions, restr_text);
        }
    }
  return best;
}

/*
 * ExecuteConversation (clsUserSession): drive a Greet/Ask/Tell/Command/Farewell
 * against a character -- implicit/explicit intro on first contact, then the
 * matching topic's reply + actions, else a default "doesn't understand" message
 * (or a topic restriction's fail text).
 */
void
exec_conversation (a5_run_t *run, const char *char_key, int conv_type,
                   const char *subject, sb_t *out)
{
  a5_state_t *st = run->st;
  const a5_character_t *conv_ch;
  const a5_topic_t *topic = NULL;
  std::string restr_text;

  if (char_key == NULL || char_key[0] == '\0')
    return;

  /* Leaving a different conversation: implicit farewell, then reset. */
  if (st->conv_char[0] != '\0' && !streq (st->conv_char, char_key))
    {
      const a5_character_t *prev = a5model_character (st->adv, st->conv_char);
      if (prev != NULL)
        {
          const a5_topic_t *fw = find_conv_node (run, prev, conv_type, "", NULL);
          if (fw != NULL)
            emit_topic_conv (run, prev->key, fw->conversation, out);
        }
      a5state_set_conv_char (st, "");
      a5state_set_conv_node (st, "");
    }

  conv_ch = a5model_character (st->adv, char_key);
  if (conv_ch == NULL)
    return;

  /* Not yet in a conversation: try an explicit then an implicit intro. */
  if (st->conv_char[0] == '\0')
    {
      const a5_topic_t *intro =
        find_conv_node (run, conv_ch, conv_type | A5_CONV_GREET, subject, NULL);
      if (intro == NULL)
        intro = find_conv_node (run, conv_ch, A5_CONV_GREET, "", NULL);
      if (intro != NULL)
        {
          emit_topic_conv (run, conv_ch->key, intro->conversation, out);
          a5state_set_conv_node (st, intro->key);
          if (intro->is_ask || intro->is_tell || intro->is_command)
            {
              a5state_set_conv_char (st, char_key);
              if (intro->actions != NULL)
                {
                  /* The runner runs topic actions through the ActionArrayList
                     ExecuteActions overload, which passes task = Nothing to
                     ExecuteSingleAction -- so they carry NO owning task even
                     though a library Say/Ask task triggered the conversation
                     (and a topic's Score changes never land; see the Score
                     gate in the variable actions). */
                  int saved_sti = run->cur_score_ti;
                  run->cur_score_ti = -1;
                  for (const a5_xml_node_t *c = intro->actions->first_child;
                       c != NULL; c = c->next)
                    run_action (run, c->name, c->text, 0, out);
                  run->cur_score_ti = saved_sti;
                }
              return;
            }
        }
    }

  a5state_set_conv_char (st, char_key);

  if (conv_type == A5_CONV_COMMAND)
    topic = find_conv_node (run, conv_ch, conv_type | A5_CONV_FAREWELL,
                            subject, NULL);
  if (topic == NULL)
    topic = find_conv_node (run, conv_ch, conv_type, subject, &restr_text);
  else
    { a5state_set_conv_char (st, ""); a5state_set_conv_node (st, ""); }

  if (topic != NULL)
    {
      emit_topic_conv (run, conv_ch->key, topic->conversation, out);
      if (topic_has_children (conv_ch, topic->key))
        a5state_set_conv_node (st, topic->key);
      else if (!topic->stay_in_node)
        a5state_set_conv_node (st, "");
      if (topic->actions != NULL)
        {
          /* task = Nothing for topic actions, as above. */
          int saved_sti = run->cur_score_ti;
          run->cur_score_ti = -1;
          for (const a5_xml_node_t *c = topic->actions->first_child;
               c != NULL; c = c->next)
            run_action (run, c->name, c->text, 0, out);
          run->cur_score_ti = saved_sti;
        }
    }
  else
    {
      std::string msg;
      a5state_set_conv_node (st, "");
      if (!restr_text.empty ())
        msg = restr_text;
      else
        {
          std::string verb = (conv_type == A5_CONV_COMMAND)
            ? "ignores you." : "doesn't appear to understand you.";
          msg = std::string ("%CharacterName[") + conv_ch->key + "]% " + verb;
        }
      {
        int pia = st->intro_active; st->intro_active = 1;
        emit_conv (a5text_process (st, msg.c_str ()), out);
        st->intro_active = pia;
      }
    }
}

/* clsCharacter.Move conversation maintenance (clsCharacter.vb:535-545): after the
   Player moves, if we are in a conversation whose partner is no longer at the
   Player's (new) location, show an implicit Farewell topic and end the
   conversation.  Without this, conv_char persists across rooms so `say %text%`
   keeps matching `Say` (BeInConversationWith) instead of falling to `SayLazy`
   (BeAloneWith) -- e.g. Stone of Wisdom's `say yes` to Pamba never routes
   `SayToCharacter (yes|%AloneWithChar%)`, and the weapon-stall trade fails. */
void
clear_conv_if_partner_gone (a5_run_t *run, sb_t *out)
{
  a5_state_t *st = run->st;
  int pci;
  const char *ploc;
  if (st->conv_char == NULL || st->conv_char[0] == '\0')
    return;
  pci = a5state_character_index (st, st->conv_char);
  ploc = a5state_player_location (st);
  if (pci >= 0 && ploc != NULL && a5state_character_at_location (st, pci, ploc))
    return;                                   /* partner still here */
  /* partner gone -> implicit Farewell (if any), then end the conversation. */
  {
    const a5_character_t *prev = a5model_character (st->adv, st->conv_char);
    if (prev != NULL)
      {
        const a5_topic_t *fw =
          find_conv_node (run, prev, A5_CONV_FAREWELL, "", NULL);
        if (fw != NULL)
          emit_topic_conv (run, prev->key, fw->conversation, out);
      }
  }
  a5state_set_conv_char (st, "");
  a5state_set_conv_node (st, "");
}
