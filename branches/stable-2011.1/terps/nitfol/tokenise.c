/*  Nitfol - z-machine interpreter using Glk for output.
    Copyright (C) 1999  Evin Robertson

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

    The author can be reached at nitfol@deja.com
*/
#include "nitfol.h"

static int tokenise(zword dictionarytable, const char *text, int length,
		    zword *parse_dest, int maxwords,
		    zword sepratortable, int numseparators,
		    BOOL write_unrecognized);

static void addparsed(zword *parse_dest, int word_num, int length, int off)
{
  if(zversion <= 4)
    off+=1;
  else
    off+=2;

  LOWORDwrite(*parse_dest, word_num);
  *parse_dest+=ZWORD_SIZE;
  LOBYTEwrite(*parse_dest, length);
  *parse_dest+=1;
  LOBYTEwrite(*parse_dest, off);
  *parse_dest+=1;
}

static int dictentry_len;

static int cmpdictentry(const void *a, const void *b)
{
  return n_memcmp(a, b, dictentry_len);
}


static zword find_word(zword dictionarytable, const char *word, int length)
{
  zbyte zsciibuffer[12];
  int entry_length, word_length;
  int num_entries;
  void *p;

  entry_length = LOBYTE(dictionarytable);
  dictionarytable++;
  num_entries = LOWORD(dictionarytable);
  dictionarytable+=ZWORD_SIZE;

  if(zversion <= 3)
    word_length = 4;
  else
    word_length = 6;

  encodezscii(zsciibuffer, word_length, word_length, word, length);

  dictentry_len = word_length;
  
  if(is_neg(num_entries)) {  /* Unordered dictionary */
    num_entries = neg(num_entries);
    p = n_lfind(zsciibuffer, z_memory + dictionarytable, &num_entries,
		entry_length, cmpdictentry);
  } else {                   /* Ordered dictionary */
    p = n_bsearch(zsciibuffer, z_memory + dictionarytable, num_entries,
		  entry_length, cmpdictentry);
  }
  if(p)
    return ((zbyte *) p) - z_memory;

  return 0;
}


#ifdef SMART_TOKENISER

struct Typocorrection {
  struct Typocorrection *next;
  struct Typocorrection *prev;
  char original[13];
  char changedto[13];
};

struct Typocorrection *recent_corrections;  /* Inform requests two parses of
					       each input; remember what
					       corrections we've made so
					       we don't print twice */
void forget_corrections(void)
{
  LEdestroy(recent_corrections);
}

static zword smart_tokeniser(zword dictionarytable,
			     const char *text, unsigned length, BOOL is_begin)
{
  zword word_num = 0;
  unsigned tlength = (length < 12) ? length : 12;
  char tbuffer[13];

  /* Letter replacements are tried in this order - */
  const char fixmeletters[] = "abcdefghijklmnopqrstuvwxyz";
  /* char fixmeletters[] = "etaonrishdlfcmugpywbvkxjqz"; */

  
  word_num = find_word(dictionarytable, text, length);  

  /* Some game files don't contain abbreviations for common commands */
  if(!word_num && do_expand && length == 1 && is_begin) {
    const char * const abbrevs[26] = {
      "a",              "b",           "close",          "down",
      "east",           "f",           "again",          "h",
      "inventory",      "j",           "attack",         "look",
      "m",              "north",       "oops",           "open",
      "quit",           "drop",        "south",          "take",
      "up",             "v",           "west",           "examine",
      "yes",            "wait"
    };
    if('a' <= text[0] && text[0] <= 'z') {
      strcpy(tbuffer, abbrevs[text[0] - 'a']);
      tlength = strlen(tbuffer);
      word_num = find_word(dictionarytable, tbuffer, tlength);
    }
  }

  /* Check for various typing errors */

  /* Don't attempt typo correction in very short words */
  if(do_spell_correct && length >= 3) {

    if(!word_num) {  /* Check for transposes */
      /* To fix, try all possible transposes */
      unsigned position;
      for(position = 1; position < tlength; position++) {
	unsigned s;
	for(s = 0; s < tlength; s++)
	  tbuffer[s] = text[s];

	tbuffer[position - 1] = text[position];
	tbuffer[position]     = text[position - 1];

	word_num = find_word(dictionarytable, tbuffer, tlength);
	if(word_num)
	  break;
      }
    }

    if(!word_num) {  /* Check for deletions */
      /* To fix, try all possible insertions */
      unsigned position;
      for(position = 0; position <= tlength; position++) {
	unsigned s;
	for(s = 0; s < position; s++)    /* letters before the insertion */
	  tbuffer[s] = text[s];

	for(s = position; s < tlength; s++)       /* after the insertion */
	  tbuffer[s + 1] = text[s];

	/* try each letter */
	for(s = 0; s < sizeof(fixmeletters); s++) {
	  tbuffer[position] = fixmeletters[s];
	  word_num = find_word(dictionarytable, tbuffer, tlength + 1);
	  if(word_num)
	    break;
	}

	if(word_num) {
	  tlength++;
	  break;
	}
      }
    }

    if(!word_num) {  /* Check for insertions */
      /* To fix, try all possible deletions */
      unsigned position;
      for(position = 0; position < tlength; position++) {
	unsigned s;
	for(s = 0; s < position; s++)    /* letters before the deletion */
	  tbuffer[s] = text[s];

	for(s = position + 1; s < tlength; s++)   /* after the deletion */
	  tbuffer[s - 1] = text[s];

	word_num = find_word(dictionarytable, tbuffer, tlength - 1);

	if(word_num) {
	  tlength--;
	  break;
	}
      }
    }

    if(!word_num) {  /* Check for substitutions */
      /* To fix, try all possible substitutions */
      unsigned position;
      for(position = 0; position < tlength; position++) {
      unsigned s;
      for(s = 0; s < tlength; s++)
	tbuffer[s] = text[s];

      /* try each letter */
      for(s = 0; s < sizeof(fixmeletters); s++) {
	tbuffer[position] = fixmeletters[s];
	word_num = find_word(dictionarytable, tbuffer, tlength);
	if(word_num)
	  break;
      }

      if(word_num)
	break;
      }
    }
  }

  /* Report any corrections made */
  if(word_num) {
    struct Typocorrection *p;
    char original[13], changedto[13];
    n_strncpy(original, text, 13);
    n_strncpy(changedto, tbuffer, 13);
    if(length < 13)
      original[length] = 0;
    if(tlength < 13)
      changedto[tlength] = 0;

    LEsearch(recent_corrections, p, ((n_strncmp(p->original, original, 13) == 0) &&
				     (n_strncmp(p->changedto, changedto, 13) == 0)));

    /* Only print a correction if it hasn't yet been reported this turn */
    if(!p) {
      struct Typocorrection newcorrection;
      n_strncpy(newcorrection.original, original, 13);
      n_strncpy(newcorrection.changedto, changedto, 13);
      LEadd(recent_corrections, newcorrection);

      set_glk_stream_current();

      if(allow_output) {
	glk_put_char('[');
	w_glk_put_buffer(text, length);
	w_glk_put_string(" -> ");
	w_glk_put_buffer(tbuffer, tlength);
	glk_put_char(']');
	glk_put_char(10);
      }
    }
  }

  return word_num;
}

#endif

static void handle_word(zword dictionarytable, const char *text,
			zword word_start, int length,
			zword *parse_dest,
			BOOL write_unrecognized, int *parsed_words)
{
  
  zword word_num;

  word_num = find_word(dictionarytable, text + word_start, length);

#ifdef SMART_TOKENISER
  if(!word_num)
    word_num = smart_tokeniser(dictionarytable, text + word_start, length,
			       *parsed_words == 0);
#endif

  if(!word_num && !write_unrecognized)
    *parse_dest += ZWORD_SIZE + 2;
  else
    addparsed(parse_dest, word_num, length, word_start);

  (*parsed_words)++;
}


static int tokenise(zword dictionarytable, const char *text, int length,
		    zword *parse_dest, int maxwords,
		    zword separatortable, int numseparators,
		    BOOL write_unrecognized)
{
  int i;
  int parsed_words = 0;
  int word_start = 0;
  for(i = 0; i <= length && parsed_words < maxwords; i++) {
    BOOL do_tokenise = FALSE;
    BOOL do_add_separator = FALSE;
    if((i == length) || text[i] == ' ') {  /* A space or at the end */
      do_tokenise = TRUE;
    } else {
      int j;
      for(j = 0; j < numseparators; j++) {
	if(text[i] == (char) LOBYTE(separatortable + j)) {
	  do_tokenise = TRUE;
	  do_add_separator = TRUE;
	  break;
	}
      }
    }

    if(do_tokenise) {
      int wordlength = i - word_start;
      if(wordlength > 0) {
	handle_word(dictionarytable, text, word_start, wordlength,
		    parse_dest, write_unrecognized, &parsed_words);
      }
      word_start = i + 1;
    }
    if(do_add_separator && parsed_words < maxwords) {
      handle_word(dictionarytable, text, i, 1,
		  parse_dest, write_unrecognized, &parsed_words);

    }
  }
  return parsed_words;
}


void z_tokenise(const char *text, int length, zword parse_dest,
		zword dictionarytable, BOOL write_unrecognized)
{
  zword separatortable;
  zword numparsedloc;
  int numseparators;
  int maxwords, parsed_words;

  if(parse_dest > dynamic_size || parse_dest < 64) {
    n_show_error(E_OUTPUT, "parse table in invalid location", parse_dest);
    return;
  }
  
  numseparators = LOBYTE(dictionarytable);
  separatortable = dictionarytable + 1;
  dictionarytable += numseparators + 1;

  maxwords = LOBYTE(parse_dest);
  numparsedloc = parse_dest + 1;
  parse_dest+=2;

  if(maxwords == 0)
    n_show_warn(E_OUTPUT, "small parse size", maxwords);

  parsed_words = tokenise(dictionarytable, text, length,
			  &parse_dest, maxwords, separatortable, numseparators,
			  write_unrecognized);
  
  LOBYTEwrite(numparsedloc, parsed_words);
}


void op_tokenise(void)
{
  if(numoperands < 3 || operand[2] == 0)
    operand[2] = z_dictionary;
  if(numoperands < 4)
    operand[3] = 0;
  z_tokenise((char *) z_memory + operand[0] + 2, LOBYTE(operand[0] + 1),
	     operand[1], operand[2], operand[3]==0);
}

