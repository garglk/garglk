#charset "us-ascii"

foo: PreinitObject
    execute()
    {
        setVal('testing');
    }

    setVal(val)
    {
        self.val = val;
    }

    val = nil

    bad = (self.val)
    good()  { return val; }
;

main(args)
{
    local x = /*nbsp:*/ 1;
    "Testing nbsp: [ ]: x = <<x>>\n";
}
