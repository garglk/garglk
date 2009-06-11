#charset "us-ascii"

/*
 *   Grammar displayer.  This demonstrates how to use
 *   GrammarProd.getGrammarInfo() by displaying the information on a given
 *   production object in human-readable format.
 *   
 *   You can add this into any adv3-based game (make sure you have the
 *   basic library's reflection module - reflect.t - included in the
 *   build).  We'll add a new verb to the game - "gramprod <name>" - that
 *   lets you display a grammar production's internal information
 *   interactively.
 */

#include <gramprod.h>
#include <reflect.h>
#include "adv3.h"
#include "en_us.h"


/*
 *   display the grammar tree 
 */
DefineLiteralAction(GramProd) 
    execAction()
    {
        /* look up the production by name */
        local prod = reflectionServices.symtab_[getLiteral()];

        /* check to see if we found it */
        if (prod != nil && prod.ofKind(GrammarProd))
        {
            /* found it - display the given production */
            displayProd(prod);
        }
        else
        {
            /* didn't find it */
            "Sorry, there's no such production as <q><<getLiteral>></q>. ";
        }
    }

    /*
     *   Display the given production in human-readable format.  This
     *   contains the same information as a 'grammar' statement, but adds
     *   some annotations to indicate the types of the symbols displayed.  
     */
    displayProd(prod)
    {
        local lastSubName;
        
        /* show the name of the overall production */
        "<<reflectionServices.valToSymbol(prod)>>:\n";

        /* get the grammar info - this is a list of GrammarAltInfo objects */
        local alts = prod.getGrammarInfo();

        /* we don't have a last-sub-name yet */
        lastSubName = nil;

        /* display each alternative */
        foreach (local alt in alts)
        {
            /* 
             *   parse the sub-name out of the match object - the name will
             *   be in the format prod(subname), so pull out the part in
             *   the parentheses 
             */
            rexMatch('.+<lparen>(.+)<rparen>',
                     reflectionServices.valToSymbol(alt.gramMatchObj));
            local subName = rexGroup(1)[3];

            /* 
             *   if this matches the previous rule's sub-name, just show it
             *   as another alternative for the same named rule; otherwise,
             *   show the new named rule 
             */
            if (subName == lastSubName)
                "\t\t| ";
            else
                "\t(<<subName>>):\n\t\t";

            /* remember the sub-name for next time */
            lastSubName = subName;

            /* check for badness */
            if (alt.gramBadness != 0)
                "[badness <<alt.gramBadness>>] ";

            /* show the token list */
            foreach (local tok in alt.gramTokens)
            {
                local nm = reflectionServices.valToSymbol(tok.gramTokenInfo);
                
                /* show the token information */
                switch (tok.gramTokenType)
                {
                case GramTokTypeProd:
                    "(prod:<<nm>>)";
                    break;

                case GramTokTypeSpeech:
                    "(pos:<<nm>>)";
                    break;

                case GramTokTypeNSpeech:
                    "(pos:";
                    for (local i = 1 ; i <= tok.gramTokenInfo.length() ; ++i)
                    {
                        if (i != 1)
                            ",";
                        say(reflectionServices.valToSymbol(
                            tok.gramTokenInfo[i]));
                    }
                    ")";
                    break;

                case GramTokTypeLiteral:
                    "'<<tok.gramTokenInfo>>'";
                    break;

                case GramTokTypeTokEnum:
                    "(tok:<<nm>>)";
                    break;

                case GramTokTypeStar:
                    "*";
                    break;

                default:
                    "(unknown)";
                    break;
                }

                /* add the target property, if present */
                if (tok.gramTargetProp != nil)
                    "-&gt;<<
                      reflectionServices.valToSymbol(tok.gramTargetProp)>>";

                /* add a space before the next one */
                " ";
            }

            /* finish the line */
            "\n";
        }
    }

    /*
     *   Display the given production as a 'grammar' statement or
     *   statements.  This will generate 'grammar' syntax that could be
     *   used in source code.  Note that this won't necessarily generate
     *   the identical syntax as the original 'grammar' statement that was
     *   used to create the GrammarProd object; there are many ways to
     *   write the same production, and the compiler doesn't keep
     *   information on superficial variations in syntax.  The syntax we
     *   generate will have the identical meaning to the original, though.
     *   Note also that we don't (and can't) display the source code of any
     *   methods defined with the match object, nor do we show any
     *   additional properties of the object; we only show the grammar
     *   definitions.  
     */
    displayProdGrammar(prod)
    {
        local lastMatchObj = nil;
        
        /* get the grammar info - this is a list of GrammarAltInfo objects */
        local alts = prod.getGrammarInfo();

        /* display each alternative */
        for (local ai = 1 ; ai <= alts.length() ; ++ai)
        {
            local alt = alts[ai];
            
            /* 
             *   if this matches the previous rule's full name, just show
             *   it as another alternative for the same named rule;
             *   otherwise, show the new named rule 
             */
            if (lastMatchObj == alt.gramMatchObj)
                "\t| ";
            else
                "grammar <<reflectionServices.valToSymbol(alt.gramMatchObj)
                  >>:\n\t";
            
            /* check for badness */
            if (alt.gramBadness != 0)
                "[badness <<alt.gramBadness>>] ";

            /* show the token list */
            foreach (local tok in alt.gramTokens)
            {
                local nm = reflectionServices.valToSymbol(tok.gramTokenInfo);
                
                /* show the token information */
                switch (tok.gramTokenType)
                {
                case GramTokTypeProd:
                case GramTokTypeSpeech:
                case GramTokTypeTokEnum:
                    say(nm);
                    break;

                case GramTokTypeNSpeech:
                    "&lt;";
                    for (local i = 1 ; i <= tok.gramTokenInfo.length() ; ++i)
                    {
                        if (i != 1)
                            " ";
                        say(reflectionServices.valToSymbol(
                            tok.gramTokenInfo[i]));
                    }
                    "&gt;";
                    break;

                case GramTokTypeLiteral:
                    "'<<tok.gramTokenInfo>>'";
                    break;

                case GramTokTypeStar:
                    "*";
                    break;

                default:
                    "(???)";
                    break;
                }

                /* add the target property, if present */
                if (tok.gramTargetProp != nil)
                    "-&gt;<<
                      reflectionServices.valToSymbol(tok.gramTargetProp)>>";

                /* add a space before the next one */
                " ";
            }

            /* finish the line */
            "\n";

            /* 
             *   if this is the last one with this match object, add the
             *   superclass specifier 
             */
            if (ai == alts.length()
                || alt.gramMatchObj != alts[ai+1].gramMatchObj)
            {
                /* add the superclass list */
                "\t: ";
                for (local scLst = alt.gramMatchObj.getSuperclassList(),
                     local i = 1 ; i <= scLst.length() ; ++i)
                {
                    if (i != 1)
                        ", ";
                    say(reflectionServices.valToSymbol(scLst[i]));
                }

                /* end the grammar definition */
                "\n;\n";
            }

            /* remember the match object for next time */
            lastMatchObj = alt.gramMatchObj;
        }
    }
;

VerbRule(GramProd)
    'gramprod' singleLiteral
    : GramProdAction
    verbPhrase = 'show/showing the grammar for (what)'
;
