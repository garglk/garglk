#charset "us-ascii"

/*
** cquotes: a TADS 3 output filter for making single curly quotes
**
** To use, just add to your project. The PreinitObject at the end
** of this file automatically registers the curly quote output filter.
**
** You may use this module in your own game. You may distribute
** modified versions of this file, though I would prefer you contact
** me first at stephen@granades.com and see about having me add your
** changes to my source.
**
** Version: 0.2 (2 Feb 2004)
**            Added in fixes for patIsHTMLTag and patIsFormatTag from
**            Matt McGlone
**          0.1 (27 Aug 2002)
**            Original release
**
** Copyright (c) 2002, 2004 by Stephen Granade.  All Rights Reserved.
*/

#include <tadsgen.h>
#include <systype.h>

// A filter to change single quotes "'" to curly ones. Comes in two
// flavors:
//
// non-aggressive: will only change the single quotes that are part
//     of English contractions (like "won't") into &lsquo;
// aggressive: will change every single left quote it can find. Any
//     single quote that is preceeded by a letter or punctuation is
//     turned into &lsquo;.
//
// To choose between them, set cquoteOutputFilter.aggressive to
// true (for aggressive changing) or nil (for non-aggressive matching).
//
// No translation is done to single quotes which a) fall within HTML
// tags (i.e. <font face='courier'>), or b) fall within
// formatting tags (i.e. {It's obj})
cquoteOutputFilter: OutputFilter
    aggressive = nil

    // Patterns for our searches
    patIsHTMLTag = static new RexPattern('<langle><^rangle>+<squote><^rangle>*<rangle>')
    patIsFormatTag = static new RexPattern('{[^}]+<squote>[^}]*}')
    patAggressive = static new RexPattern('(<alphanum|punct>)<squote>')
    patIsCont1Tag = static new RexPattern('(<alpha>)<squote>(s|m|d|ve|re|ll)')
    patIsCont2Tag = static new RexPattern('(<alpha>)n<squote>t')
    patIsPossTag = static new RexPattern('(<alpha>)s<squote>')

    filterText(ostr, val) {
	local ret;

	// Look for an HTML tag. We only need to find the first one,
	// because we'll be recursing through the string
	ret = rexSearch(patIsHTMLTag, val);
	if (ret == nil) {
	    // Look for a formatting tag
	    ret = rexSearch(patIsFormatTag, val);
	}
	// If we got a match either from the HTML or the formatting
	// tag, ignore that match recursively; that is, run the output
	// filter on the text before and after the match. This is
	// assuming that the whole start wasn't prefixed by a backslash
	// (since e.g. "\<font face='courier>" isn't really an HTML tag)
	if (ret != nil && (ret[1] == 1 ||
			   val.substr(ret[1] - 1, 1) != '\\')) {
	    return filterText(ostr, val.substr(1, ret[1] - 1)) + ret[3] +
		filterText(ostr, val.substr(ret[1] + ret[2],
					    val.length() - (ret[1]+ret[2])
					    + 1));
	}

	// Do the appropriate replacements. First, aggressive
	if (aggressive) {
	    val = rexReplace(patAggressive, val, '%1&rsquo;',
			     ReplaceAll);
	}
	else {
	    // We recognize the contractions 's, 'm, 'd, 've, 're,
	    // 'll, and n't, as well as the plural possessive s'.
	    // (Possessive 's is handled by the contraction.) All
	    // must be preceeded by a letter.
	    val = rexReplace(patIsCont1Tag,
			     val, '%1&rsquo;%2', ReplaceAll);
	    val = rexReplace(patIsCont2Tag, val, '%1n&rsquo;t',
			     ReplaceAll);
	    val = rexReplace(patIsPossTag, val, '%1s&rsquo;',
			     ReplaceAll);
	}

	return val;
    }
;

// When we first start up, register us as an output filter
PreinitObject
    execute() {
	mainOutputStream.addOutputFilter(cquoteOutputFilter);
    }
;

