g(a, b, c)
{
    local i = a + 1;
    local j = b + 1;
    local k = c + 1;

    "g: i=<<i>>, j=<<j>>, k=<<k>>\b";

    dumpStack('g 1');

    local cnt = 0;
    local af1 = new function(a)
    {
        local sum = 0;
        a.forEach({ k: sum += k, cnt += 1, dumpStack('af1-nested') });

        i = sum;
        dumpStack('af1 - done');
    };
    af1([1, 2, 3, 4, 5]);
    dumpStack('g 2');
}
