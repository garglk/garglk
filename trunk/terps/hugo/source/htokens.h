/*
	HTOKENS.H

	contains token definitions
	for the Hugo Compiler and Engine

	Copyright (c) 1995-2006 by Kent Tessman

	The enum constants of type TOKEN_T reflect the token names
	given in the token array.  Token names followed by a # are
	for system use.
*/


#define TOKENS 0x7B	/* i.e., highest numbered token */

#define HASH_KEY 1023	/* arbitrary */


#if !defined (TOKEN_SET_DEFINED)
#define TOKEN_SET_DEFINED

enum TOKEN_T
{
	/* 0x00 - 0x0f */
	NULL_T, OPEN_BRACKET_T, CLOSE_BRACKET_T, DECIMAL_T,
	COLON_T, EQUALS_T, MINUS_T, PLUS_T,
	ASTERISK_T, FORWARD_SLASH_T, PIPE_T, SEMICOLON_T,
	OPEN_BRACE_T, CLOSE_BRACE_T, OPEN_SQUARE_T, CLOSE_SQUARE_T,

	/* 0x10 - 0x1f */
	POUND_T, TILDE_T, GREATER_EQUAL_T, LESS_EQUAL_T,
	NOT_EQUAL_T, AMPERSAND_T, GREATER_T, LESS_T,
	IF_T, COMMA_T, ELSE_T, ELSEIF_T,
	WHILE_T, DO_T, SELECT_T, CASE_T,
	
	/* 0x20 - 0x2f */
	FOR_T, RETURN_T, BREAK_T, AND_T,
	OR_T, JUMP_T, RUN_T, IS_T,
	NOT_T, TRUE_T, FALSE_T, LOCAL_T,
	VERB_T, XVERB_T, HELD_T, MULTI_T,
	
	/* 0x30 - 0x3f */
	MULTIHELD_T, NEWLINE_T, ANYTHING_T, PRINT_T,
	NUMBER_T, CAPITAL_T, TEXT_T, GRAPHICS_T, 
	COLOR_T, REMOVE_T, MOVE_T, TO_T,
	PARENT_T, SIBLING_T, CHILD_T, YOUNGEST_T,

	/* 0x40 - 0x4f */
	ELDEST_T, YOUNGER_T, ELDER_T, PROP_T,
	ATTR_T, VAR_T, DICTENTRY_T, TEXTDATA_T,
	ROUTINE_T, DEBUGDATA_T, OBJECTNUM_T, VALUE_T,
	EOL_T, SYSTEM_T, NOTHELD_T, MULTINOTHELD_T,

	/* 0x50 - 0x5f */
	WINDOW_T, RANDOM_T, WORD_T, LOCATE_T,
	PARSE_T, CHILDREN_T, IN_T, PAUSE_T,
	RUNEVENTS_T, ARRAYDATA_T, CALL_T, STRINGDATA_T,
	SAVE_T, RESTORE_T, QUIT_T, INPUT_T,

	/* 0x60 - 0x6f */
	SERIAL_T, CLS_T, SCRIPTON_T, SCRIPTOFF_T,
	RESTART_T, HEX_T, OBJECT_T, XOBJECT_T,
	STRING_T, ARRAY_T, PRINTCHAR_T, UNDO_T,
	DICT_T, RECORDON_T, RECORDOFF_T, WRITEFILE_T,
	
	/* 0x70 - */
	READFILE_T, WRITEVAL_T, READVAL_T, PLAYBACK_T,
	COLOUR_T, PICTURE_T, LABEL_T, SOUND_T,
	MUSIC_T, REPEAT_T, ADDCONTEXT_T, VIDEO_T
};

#endif  /* if !defined (TOKEN_SET_DEFINED) */


#if defined (INIT_PASS)

char *token[] =
{
	/* 0x00 - 0x0f */
	"",  "(", ")", ".", ":", "=", "-", "+",
	"*", "/", "|", ";", "{", "}", "[", "]",
	
	/* 0x10 - 0x1f */
	"#", "~", ">=", "<=", "~=", "&", ">", "<",
	"if", ",", "else", "elseif", "while", "do", "select", "case",

	/* 0x20 - 0x2f */	
	"for", "return", "break",  "and", "or", "jump", "run", "is",
	"not", "true", "false", "local", "verb", "xverb", "held", "multi",
	
	/* 0x30 - 0x3f */
	"multiheld", "newline", "anything", "print",
	"number", "capital", "text", "graphics",
	"color", "remove", "move", "to",
	"parent", "sibling", "child", "youngest",
	
	/* 0x40 - 0x4f */
	"eldest", "younger", "elder", "prop#",
	"attr#", "var#", "dictentry#", "textdata#",
	"routine#","debugdata#","objectnum#", "value#",
	"eol#", "system", "notheld", "multinotheld",

	/* 0x50 - 0x5f */
	"window", "random", "word", "locate",
	"parse$", "children", "in", "pause", 
	"runevents", "arraydata#", "call", "stringdata#",
	"save", "restore", "quit", "input",
	
	/* 0x60 - 0x6f */
	"serial$", "cls", "scripton", "scriptoff",
	"restart", "hex", "object", "xobject",
	"string", "array", "printchar", "undo",
	"dict", "recordon", "recordoff", "writefile",
	
	/* 0x70 - */
	"readfile", "writeval", "readval", "playback",
	"colour", "picture", "label#", "sound",
	"music", "repeat", "addcontext", "video"
};

	int token_hash[TOKENS+1];

#else
	extern char *token[];
	extern int token_hash[];

#endif	/* defined (INIT_PASS) */

