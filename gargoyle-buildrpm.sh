#!/bin/sh
# This scripts creates and build a simple RPM package
#
# Prerequisites:
#  - rpm-build, make and gcc (as it's a c file) packages must be installed
#

# Holds the name of the root directory containing the necessary structure to
# build RPM packages.
RPM_ROOT_DIR=~/rpm_factory
PKG_NAME=gargoyle-free
PKG_TAR=/tmp/${PKG_NAME}.tar.gz
PKG_DIR=/tmp/${PKG_NAME}

mv ./Jamrules ./JamrulesFULL
mv ./JamrulesRPM ./Jamrules

if [ -d "./build/dist" ]
then
    rm -rfv ./build/dist
fi
if [ -d "./build/linux.release" ]
then
    rm -rfv ./build/linux.release
fi
#rm -rfv ./build/dist
#rm -rfv ./build/linux.release
jam
jam install

mkdir -p ${PKG_DIR}
cp -v ./build/dist/* ${PKG_DIR}
cp -v ./garglk/garglk.ini ${PKG_DIR}
cp -v ./garglk/gargoyle-house.png ${PKG_DIR}

pushd ${PKG_DIR}/..
tar czvf ${PKG_NAME}.tar.gz ${PKG_NAME}
popd

# Recreate the root directory and its structure if necessary
mkdir -p ${RPM_ROOT_DIR}/{SOURCES,BUILD,RPMS,SPECS,SRPMS,tmp}
pushd  $RPM_ROOT_DIR
cp ${PKG_TAR} ${RPM_ROOT_DIR}/SOURCES/

# Creating a basic spec file
cat << __EOF__ > ${RPM_ROOT_DIR}/SPECS/${PKG_NAME}.spec
Summary: This package contains a build of the gargoyle interactive fiction interpreter.  This build is based on sources available at https://github.com/garglk/garglk
Name: $PKG_NAME
Version: 1.0
Release: 0
License: GPL
Packager: interactivefiction
Group: Development/Tools
Source: %{name}.tar.gz
BuildRequires: coreutils
BuildRoot: ${RPM_ROOT_DIR}/tmp/%{name}-%{version}
Provides: libgarglk.so()(64bit)

%global debug_package %{nil}

%description
%{summary}

%prep
%setup -n ${PKG_NAME}

%build
echo -e '#!/bin/sh\n%{_libdir}/gargoyle/gargoyle &\nexit' > gargoyle-free
chmod +x gargoyle-free
cat > %{name}.desktop <<'EOF'
[Desktop Entry]
Name=gargoyle-free
Comment=Interactive fiction interpreter / player supporting many formats.
Exec=gargoyle-free
Icon=/usr/share/icons/gargoyle/gargoyle-house.png
Categories=Application;Graphics;X-Red-Hat-Base;
Type=Application
Terminal=0
X-Desktop-File-Install-Version=0.3
EOF

%install
mkdir -p "%{buildroot}%{_sysconfdir}/"
mkdir -p "%{buildroot}%{_libdir}/gargoyle/"
mkdir -p "%{buildroot}%{_bindir}/"
mkdir -p "%{buildroot}%{_datadir}/applications/"
mkdir -p "%{buildroot}%{_datadir}/icons/gargoyle/"
cp -v garglk.ini "%{buildroot}%{_sysconfdir}/"
cp -v libgarglk.so "%{buildroot}%{_libdir}/"
cp -v advsys "%{buildroot}%{_libdir}/gargoyle/"
cp -v geas "%{buildroot}%{_libdir}/gargoyle/"
cp -v magnetic "%{buildroot}%{_libdir}/gargoyle/"
cp -v agility "%{buildroot}%{_libdir}/gargoyle/"
cp -v git "%{buildroot}%{_libdir}/gargoyle/"
cp -v nitfol "%{buildroot}%{_libdir}/gargoyle/"
cp -v alan2 "%{buildroot}%{_libdir}/gargoyle/"
cp -v glulxe "%{buildroot}%{_libdir}/gargoyle/"
cp -v scare "%{buildroot}%{_libdir}/gargoyle/"
cp -v alan3 "%{buildroot}%{_libdir}/gargoyle/"
cp -v hugo "%{buildroot}%{_libdir}/gargoyle/"
cp -v scott "%{buildroot}%{_libdir}/gargoyle/"
cp -v bocfel "%{buildroot}%{_libdir}/gargoyle/"
cp -v jacl "%{buildroot}%{_libdir}/gargoyle/"
cp -v tadsr "%{buildroot}%{_libdir}/gargoyle/"
cp -v frotz "%{buildroot}%{_libdir}/gargoyle/"
cp -v level9 "%{buildroot}%{_libdir}/gargoyle/"
cp -v gargoyle "%{buildroot}%{_libdir}/gargoyle/"
cp -v gargoyle-free "%{buildroot}%{_bindir}/"
cp -v %{name}.desktop "%{buildroot}%{_datadir}/applications/"
cp -v gargoyle-house.png "%{buildroot}%{_datadir}/icons/gargoyle/" 

%files
%{_sysconfdir}/garglk.ini
%{_libdir}/libgarglk.so
%{_libdir}/gargoyle/advsys
%{_libdir}/gargoyle/geas
%{_libdir}/gargoyle/magnetic
%{_libdir}/gargoyle/agility
%{_libdir}/gargoyle/git
%{_libdir}/gargoyle/nitfol
%{_libdir}/gargoyle/alan2
%{_libdir}/gargoyle/glulxe
%{_libdir}/gargoyle/scare
%{_libdir}/gargoyle/alan3
%{_libdir}/gargoyle/hugo
%{_libdir}/gargoyle/scott
%{_libdir}/gargoyle/bocfel
%{_libdir}/gargoyle/jacl
%{_libdir}/gargoyle/tadsr
%{_libdir}/gargoyle/frotz
%{_libdir}/gargoyle/level9
%{_libdir}/gargoyle/gargoyle
%{_bindir}/gargoyle-free
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/gargoyle/gargoyle-house.png

%clean
%if "%{clean}" != ""
  rm -rfv %{_topdir}/BUILD/%{name}
%endif

%post
chmod 755 -R /opt/${PKG_NAME}
__EOF__

rpmbuild -v -bb --define "_topdir ${RPM_ROOT_DIR}" SPECS/${PKG_NAME}.spec
popd

mv ./Jamrules ./JamrulesRPM
mv ./JamrulesFULL ./Jamrules
