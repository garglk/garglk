#!/bin/sh

MACPORTS=/opt/local/lib
GARGDIST=build/dist
DYLIBS=support/dylibs
BUNDLE=Gargoyle.app/Contents

rm -rf Gargoyle.app
mkdir -p $BUNDLE/MacOS
mkdir -p $BUNDLE/Frameworks
mkdir -p $BUNDLE/Resources

rm -rf $GARGDIST
jam install

for file in `ls $GARGDIST`
do
for lib in `cat $DYLIBS`
do
install_name_tool -change $MACPORTS/$lib @executable_path/../Frameworks/$lib $GARGDIST/$file
done
install_name_tool -change @executable_path/libgarglk.dylib @executable_path/../Frameworks/libgarglk.dylib $GARGDIST/$file
done
install_name_tool -id @executable_path/../Frameworks/libgarglk.dylib $GARGDIST/libgarglk.dylib

for file in `ls $GARGDIST | grep -v .dylib`
do
cp -f $GARGDIST/$file $BUNDLE/MacOS
done

for lib in `cat $DYLIBS`
do
cp $MACPORTS/$lib $BUNDLE/Frameworks
done

for dylib in `cat $DYLIBS`
do
for lib in `cat $DYLIBS`
do
install_name_tool -change $MACPORTS/$lib @executable_path/../Frameworks/$lib $BUNDLE/Frameworks/$dylib
done
install_name_tool -id @executable_path/../Frameworks/$dylib $BUNDLE/Frameworks/$dylib
done 

cp -f $GARGDIST/libgarglk.dylib $BUNDLE/Frameworks
cp -f garglk/garglk.ini $BUNDLE/Resources
cp -f garglk/*.icns $BUNDLE/Resources
cp -f licenses/* $BUNDLE/Resources
cp -f garglk/Info.plist $BUNDLE

