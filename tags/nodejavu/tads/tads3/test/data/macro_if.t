//#define IfnG(glb, args...) \
    if (args#ifempty#!#glb##Global.prop##glb args#ifnempty# != args#)

#define IfnG(glb, args...) \
    if (IfnG1(args)glb##Global.prop##glb IfnG2(args))
#define IfnG1(args...) args#ifempty#!#
#define IfnG2(args...) args#ifnempty# != args#


//#define IfnG(glb, args...) \
//    if (args#ifempty#!#IfnGVar(glb)args#ifnempty# != args#)

//#define IfnGVar(glb) \
//    glb##Global.prop##glb


IfnG(Debug);

IfnG(Debug, a, b);

