modify kf(A a1, B b1)
{
    "\n!!Extern Modified!! f(A a1=<<a1.name>>, B b1=<<b1.name>>) ";

    "\n[ inherited:";
    inherited(a1, b1);
    "\n.. back from inherited!]";

    "\n[ replaced:";
    replaced(a1, b1);
    "\n.. back from replaced!]";
}

modify kf(C c1, C c2)
{
    "\n!!Extern Modified!! f(C c1=<<c1.name>>, C c2=<<c2.name>>)";

    "\n[ inherited:";
    inherited(c1, c2);
    "\n.. back from inherited!]";

    "\n[ replaced:";
    replaced(c1, c2);
    "\n.. back from replaced!]";
}
    