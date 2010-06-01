
#
# Darwin has install_name magic to find the dylib.
# ELF has no such provision, which makes it necessary
# to set LD_LIBRARY_PATH.
#

if [ `uname` != Darwin ]
then
    abspath=`readlink -f $0`	# get the full path of this script
    dirpath=`dirname $abspath`	# get directory part
    if [ -z "$LD_LIBRARY_PATH" ]
    then
        export LD_LIBRARY_PATH=$dirpath
    else
        export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$dirpath
    fi
else
    dirpath=`dirname $0`
fi

if [ x"$dirpath" == x ]
then
    dirpath=.
fi

#
# Check the arguments
#

if [ x"$1" == x ]
then
    echo "usage: gargoyle <gamefile>"
    exit 1
fi

if [ ! -e "$1" ]
then
    echo "gargoyle: Cannot access file: $1"
    exit 1
fi

#
# Switch on the extension.
# We assume that blorb always contains Glulx code,
# unless it has the extensions zlb or zblorb.
#

lowpath=`basename "$1" | tr A-Z a-z`

case "$lowpath" in
    *.taf ) $dirpath/scare "$1" ;;
    *.dat ) $dirpath/advsys "$1" ;;
    *.agx ) $dirpath/agility "$1" ;;
    *.d\$\$ ) $dirpath/agility "$1" ;;
    *.acd ) $dirpath/alan2 "$1" ;;
    *.a3c ) $dirpath/alan3 "$1" ;;
    *.asl ) $dirpath/geas "$1" ;;
    *.cas ) $dirpath/geas "$1" ;;
    *.ulx ) $dirpath/git "$1" ;;
    *.hex ) $dirpath/hugo "$1" ;;
    *.jacl ) $dirpath/jacl "$1" ;;
    *.j2  ) $dirpath/jacl "$1" ;;
    *.l9  ) $dirpath/level9 "$1" ;;
    *.sna ) $dirpath/level9 "$1" ;;
    *.mag ) $dirpath/magnetic "$1" ;;
    *.gam ) $dirpath/tadsr "$1" ;;
    *.t3  ) $dirpath/tadsr "$1" ;;
    *.z6  ) $dirpath/nitfol "$1" ;;
    *.z?  ) $dirpath/frotz "$1" ;;

    *.blb    ) $dirpath/git "$1" ;;
    *.blorb  ) $dirpath/git "$1" ;;
    *.glb    ) $dirpath/git "$1" ;;
    *.gblorb ) $dirpath/git "$1" ;;
    *.zlb    ) $dirpath/frotz "$1" ;;
    *.zblorb ) $dirpath/frotz "$1" ;;

    * ) echo "gargoyle: Unknown file type: $1" ;;
esac

