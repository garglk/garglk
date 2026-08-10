/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- minimal XML DOM parser.  See a5xml.h.
 */

#include <stdlib.h>
#include <string.h>

#include "a5xml.h"

struct a5_xml_doc_s {
  char *buf;                    /* owned, modified in place                  */
  a5_xml_node_t *root;
};

static const char A5_EMPTY[] = "";

static int
is_name_char (int c)
{
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
      || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == ':' || c == '.';
}

/*
 * Decode XML entities in [s, e) in place, returning the new length.  Only the
 * five predefined entities and numeric character references are handled; an
 * unrecognised "&..." is copied through verbatim.  Output never grows.
 *
 * Line-ending normalisation (XML 1.0 sec. 2.11) is applied at the same time:
 * the two-character sequence CR-LF and a stand-alone CR are both translated to
 * a single LF before the value is handed to the application.  .NET's XmlReader
 * (which the Adrift 5 runner, the ground-truth engine, relies on) does this, so e.g. a
 * CR-LF-separated list field ("Alan looks smug.\r\nAlan looks pleased.\r\n...")
 * yields clean LF-delimited options rather than options each carrying a
 * trailing CR -- the latter defeats the pSpace newline check and leaks a stray
 * carriage return into the rendered transcript.
 */
static size_t
a5_decode_entities (char *s, char *e)
{
  char *out = s;
  char *p = s;

  while (p < e)
    {
      if (*p == '\r')
        {
          *out++ = '\n';
          p++;
          if (p < e && *p == '\n')   /* collapse CR-LF to a single LF */
            p++;
          continue;
        }
      if (*p != '&')
        {
          *out++ = *p++;
          continue;
        }

      /* Find the terminating ';' within a short window. */
      char *semi = p + 1;
      while (semi < e && *semi != ';' && (semi - p) < 12)
        semi++;
      if (semi >= e || *semi != ';')
        {
          *out++ = *p++;        /* stray '&' */
          continue;
        }

      size_t len = (size_t) (semi - (p + 1));
      if (len == 3 && memcmp (p + 1, "amp", 3) == 0)
        *out++ = '&';
      else if (len == 2 && memcmp (p + 1, "lt", 2) == 0)
        *out++ = '<';
      else if (len == 2 && memcmp (p + 1, "gt", 2) == 0)
        *out++ = '>';
      else if (len == 4 && memcmp (p + 1, "quot", 4) == 0)
        *out++ = '"';
      else if (len == 4 && memcmp (p + 1, "apos", 4) == 0)
        *out++ = '\'';
      else if (len >= 2 && p[1] == '#')
        {
          long cp;
          if (p[2] == 'x' || p[2] == 'X')
            cp = strtol (p + 3, NULL, 16);
          else
            cp = strtol (p + 2, NULL, 10);
          /* An out-of-range or malformed reference (negative, overflowed, or
             above U+10FFFF) becomes U+FFFD rather than a stray byte. */
          if (cp <= 0 || cp > 0x10ffff)
            cp = 0xfffd;
          /* Encode the code point as UTF-8 (output <= input length here: the
             shortest form of a 4-byte reference, "&#65536;", is 8 chars). */
          if (cp < 0x80)
            *out++ = (char) cp;
          else if (cp < 0x800)
            {
              *out++ = (char) (0xc0 | (cp >> 6));
              *out++ = (char) (0x80 | (cp & 0x3f));
            }
          else if (cp < 0x10000)
            {
              *out++ = (char) (0xe0 | (cp >> 12));
              *out++ = (char) (0x80 | ((cp >> 6) & 0x3f));
              *out++ = (char) (0x80 | (cp & 0x3f));
            }
          else
            {
              *out++ = (char) (0xf0 | (cp >> 18));
              *out++ = (char) (0x80 | ((cp >> 12) & 0x3f));
              *out++ = (char) (0x80 | ((cp >> 6) & 0x3f));
              *out++ = (char) (0x80 | (cp & 0x3f));
            }
        }
      else
        {
          /* Unknown entity: copy verbatim including & and ; */
          while (p <= semi)
            *out++ = *p++;
          continue;
        }
      p = semi + 1;
    }

  return (size_t) (out - s);
}

static a5_xml_node_t *
a5_node_new (void)
{
  a5_xml_node_t *n = (a5_xml_node_t *) calloc (1, sizeof *n);
  if (n != NULL)
    n->text = A5_EMPTY;
  return n;
}

static void
a5_node_free (a5_xml_node_t *n)
{
  a5_xml_node_t *c = n->first_child;
  while (c != NULL)
    {
      a5_xml_node_t *next = c->next;
      a5_node_free (c);
      c = next;
    }
  free (n);
}

/* Skip a processing instruction <? ... ?> or comment <!-- ... --> at *pp. */
static void
a5_skip_misc (char **pp, char *end)
{
  char *p = *pp;
  if (p + 1 < end && p[1] == '!' && p + 3 < end && p[2] == '-' && p[3] == '-')
    {
      p += 4;
      while (p + 2 < end && !(p[0] == '-' && p[1] == '-' && p[2] == '>'))
        p++;
      p = (p + 3 < end) ? p + 3 : end;
    }
  else
    {
      /* <? ... ?>  or  <! ... >  */
      while (p < end && *p != '>')
        p++;
      if (p < end)
        p++;
    }
  *pp = p;
}

/* Parse one element whose start tag '<' is at *pp.  Returns NULL on error. */
static a5_xml_node_t *
a5_parse_element (char **pp, char *end)
{
  char *p = *pp;
  a5_xml_node_t *node;
  char *name, *name_term;
  int self_close = 0;
  char *text_start = NULL, *text_lt = NULL;

  node = a5_node_new ();
  if (node == NULL)
    return NULL;

  p++;                          /* past '<' */
  name = p;
  while (p < end && is_name_char ((unsigned char) *p))
    p++;
  name_term = p;

  /* Skip any attributes (none in ADRIFT element tags, but be tolerant). */
  while (p < end && *p != '>')
    {
      if (*p == '/' && p + 1 < end && p[1] == '>')
        break;
      if (*p == '"' || *p == '\'')
        {
          char q = *p++;
          while (p < end && *p != q)
            p++;
          if (p < end)
            p++;
        }
      else
        p++;
    }
  if (p < end && *p == '/')
    {
      self_close = 1;
      p++;
    }
  if (p < end && *p == '>')
    p++;
  *name_term = '\0';
  node->name = name;

  if (self_close)
    {
      *pp = p;
      return node;
    }

  for (;;)
    {
      char *cd = p;
      while (p < end && *p != '<')
        p++;
      if (node->n_children == 0 && text_start == NULL)
        {
          text_start = cd;
          text_lt = p;
        }
      if (p >= end)
        break;                  /* malformed: unexpected EOF */

      if (p + 1 < end && p[1] == '/')
        {
          /* closing tag </name> */
          p += 2;
          while (p < end && *p != '>')
            p++;
          if (p < end)
            p++;
          break;
        }
      else if (p + 1 < end && (p[1] == '?' || p[1] == '!'))
        {
          a5_skip_misc (&p, end);
        }
      else
        {
          a5_xml_node_t *child = a5_parse_element (&p, end);
          if (child == NULL)
            {
              a5_node_free (node);
              return NULL;
            }
          if (node->last_child == NULL)
            node->first_child = node->last_child = child;
          else
            {
              node->last_child->next = child;
              node->last_child = child;
            }
          node->n_children++;
        }
    }

  if (node->n_children == 0 && text_start != NULL)
    {
      size_t len = a5_decode_entities (text_start, text_lt);
      text_start[len] = '\0';
      node->text = text_start;
    }

  *pp = p;
  return node;
}

a5_xml_doc_t *
a5xml_parse (char *buf, uint32_t len)
{
  a5_xml_doc_t *doc;
  char *p = buf;
  char *end = buf + len;
  a5_xml_node_t *root;

  if (buf == NULL || len == 0)
    return NULL;

  /* Skip a UTF-8 BOM. */
  if (len >= 3 && (unsigned char) p[0] == 0xef
      && (unsigned char) p[1] == 0xbb && (unsigned char) p[2] == 0xbf)
    p += 3;

  /* Skip prolog: whitespace, <?xml?>, comments, doctype. */
  for (;;)
    {
      while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
        p++;
      if (p < end && *p == '<' && p + 1 < end && (p[1] == '?' || p[1] == '!'))
        a5_skip_misc (&p, end);
      else
        break;
    }

  if (p >= end || *p != '<')
    return NULL;

  root = a5_parse_element (&p, end);
  if (root == NULL)
    return NULL;

  doc = (a5_xml_doc_t *) calloc (1, sizeof *doc);
  if (doc == NULL)
    {
      a5_node_free (root);
      return NULL;
    }
  doc->buf = buf;
  doc->root = root;
  return doc;
}

void
a5xml_free (a5_xml_doc_t *doc)
{
  if (doc == NULL)
    return;
  if (doc->root != NULL)
    a5_node_free (doc->root);
  free (doc->buf);
  free (doc);
}

a5_xml_node_t *
a5xml_root (a5_xml_doc_t *doc)
{
  return doc != NULL ? doc->root : NULL;
}

a5_xml_node_t *
a5xml_child (const a5_xml_node_t *node, const char *name)
{
  a5_xml_node_t *c;
  if (node == NULL)
    return NULL;
  for (c = node->first_child; c != NULL; c = c->next)
    if (strcmp (c->name, name) == 0)
      return c;
  return NULL;
}

const char *
a5xml_child_text (const a5_xml_node_t *node, const char *name)
{
  a5_xml_node_t *c = a5xml_child (node, name);
  return c != NULL ? c->text : NULL;
}

int
a5xml_count (const a5_xml_node_t *node, const char *name)
{
  a5_xml_node_t *c;
  int n = 0;
  if (node == NULL)
    return 0;
  for (c = node->first_child; c != NULL; c = c->next)
    if (strcmp (c->name, name) == 0)
      n++;
  return n;
}
