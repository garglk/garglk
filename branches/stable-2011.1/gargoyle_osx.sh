#!/bin/sh

MACPORTS=/opt/local/lib
GARGDIST=build/dist
DYLIBS=support/dylibs
BUNDLE=Gargoyle.app/Contents

rm -rf Gargoyle.app
mkdir -p $BUNDLE/MacOS
mkdir -p $BUNDLE/Frameworks
mkdir -p $BUNDLE/Resources
mkdir -p $BUNDLE/PlugIns

rm -rf $GARGDIST
jam -sUNIVERSAL=yes install

for file in `ls $GARGDIST`
do
for lib in `cat $DYLIBS`
do
install_name_tool -change $MACPORTS/$lib @executable_path/../Frameworks/$lib $GARGDIST/$file
done
install_name_tool -change @executable_path/libgarglk.dylib @executable_path/../Frameworks/libgarglk.dylib $GARGDIST/$file
done
install_name_tool -id @executable_path/../Frameworks/libgarglk.dylib $GARGDIST/libgarglk.dylib

for file in `ls $GARGDIST | grep -v .dylib | grep -v gargoyle`
do
cp -f $GARGDIST/$file $BUNDLE/PlugIns
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

cp -f garglk/launcher.plist $BUNDLE/Info.plist
cp -f $GARGDIST/gargoyle $BUNDLE/MacOS/Gargoyle
cp -f $GARGDIST/libgarglk.dylib $BUNDLE/Frameworks
cp -f garglk/launchmac.nib $BUNDLE/Resources/MainMenu.nib
cp -f garglk/garglk.ini $BUNDLE/Resources
cp -f garglk/*.icns $BUNDLE/Resources
cp -f licenses/* $BUNDLE/Resources

mkdir $BUNDLE/Resources/Fonts
cp fonts/LiberationMono*.ttf $BUNDLE/Resources/Fonts
cp fonts/LinLibertine*.otf $BUNDLE/Resources/Fonts

hdiutil create -ov -srcfolder Gargoyle.app/ gargoyle-2011.1-mac.dmg
