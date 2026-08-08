/* vi: set ts=8:
 *
 * ADRIFT 5 support for Scarier -- minimal XML DOM parser.
 *
 * ADRIFT 5 games decompress to UTF-8 XML produced by .NET's XmlTextWriter:
 * no attributes (on elements), no CDATA, only the standard entities, and every
 * element holds *either* child elements *or* verbatim character data.  This is
 * a small in-situ DOM reader tuned for exactly that: it parses in place
 * (decoding entities and NUL-terminating within the buffer it is handed), so it
 * does almost no allocation beyond the node structs themselves.
 *
 * Self-contained at the container edge: plain malloc, no engine dependency.
 */

#ifndef SCARIER_A5XML_H
#define SCARIER_A5XML_H

#include <stddef.h>
#include <stdint.h>

typedef struct a5_xml_node_s a5_xml_node_t;
struct a5_xml_node_s {
  const char *name;             /* element tag (NUL-terminated)              */
  const char *text;             /* verbatim inner text ("" for branch nodes) */
  a5_xml_node_t *first_child;
  a5_xml_node_t *last_child;
  a5_xml_node_t *next;          /* next sibling                              */
  int n_children;
};

typedef struct a5_xml_doc_s a5_xml_doc_t;

/*
 * Parse XML.  Takes ownership of buf (length len) and modifies it in place; on
 * success the returned doc owns buf and frees it in a5xml_free().  On failure
 * returns NULL and the caller still owns buf.  buf need not be NUL-terminated
 * but may be (a5_inflate() supplies a NUL).
 */
extern a5_xml_doc_t *a5xml_parse (char *buf, uint32_t len);
extern void a5xml_free (a5_xml_doc_t *doc);

/* The document (root) element, e.g. <Adventure>. */
extern a5_xml_node_t *a5xml_root (a5_xml_doc_t *doc);

/* First direct child named `name`, or NULL. */
extern a5_xml_node_t *a5xml_child (const a5_xml_node_t *node, const char *name);

/* Text of the first direct child named `name`, or NULL if absent. */
extern const char *a5xml_child_text (const a5_xml_node_t *node, const char *name);

/* Count direct children named `name`. */
extern int a5xml_count (const a5_xml_node_t *node, const char *name);

/* Parse an ADRIFT serialised boolean (FileIO.GetBool): only "TRUE"/"1"/"-1"/
   "VRAI" (case-insensitively) are true; everything else (incl. NULL) is false. */
extern int a5xml_bool (const char *s);

#endif
