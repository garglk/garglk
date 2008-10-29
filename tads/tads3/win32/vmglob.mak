# $Header$
#
# makefile header - global variable configuration (see vmglob.h)
#
# Note that VMGLOB_PARAM is the configuration where it is easiest to
# make coding errors, so during testing this setting should always be
# used to ensure that the code keeps working in this configuration.
# However, the other configurations are faster (VMGLOB_STRUCT is the
# fastest), so release builds should use one of the others when there
# is not another reason to use the parameter-passing mechanism (the
# parameter-passing mechanism is only required when the VM engine is
# going to be used as a shared library on systems without OS-level
# isolation of per-process globals, or when multiple T3 images are
# to be loaded simultaneously within the same process).

#
# ONLY ONE OF THE FOLLOWING SHOULD BE DEFINED.  Comment out the others.
#

# compile with globals as a global static structure
#T3_GLOBAL_CONFIG=VMGLOB_STRUCT

# compile with globals in a structure with a global pointer to the structure
#T3_GLOBAL_CONFIG=VMGLOB_POINTER

# compile with globals in a structure passed as a parameter to all
# functions that require access to globals (directly or because they
# call functions that require access to globals)
#T3_GLOBAL_CONFIG=VMGLOB_PARAM

!ifdef COMPILE_FOR_DEBUG
T3_GLOBAL_CONFIG=VMGLOB_PARAM
!else
T3_GLOBAL_CONFIG=VMGLOB_VARS
!endif
