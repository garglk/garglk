/*
 *   Embedded expression in a single-quoted string in a template 
 */

class Thing: object
    name = ''
    desc = ""
;

Thing template 'name' "desc"?;

knife: Thing '<<color>> knife' "It's a knife with a <<color>> blade. "
    color = '<<one of>>red<<or>>green<<or>>blue<<cycling>>'
;

main(args)
{
    for (local i in 1..6)
        "<<knife.name>>\n";
}
