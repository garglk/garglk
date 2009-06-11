main(args)
{
    obj.m1(1, 2, &m2, 4, 5);
}

obj: object
    m1(a, b, prop, [args])
    {
        local e = f1(b);
        f2(a, a);
        
        try
        {
            return self.(prop)(args...);
        }
        finally
        {
            f3();
            f1(e);
        }
    }

    m2(m, n)
    {
    }
;

f1(x)
{
    return x;
}
f2(y, z)
{
}
f3()
{
}
