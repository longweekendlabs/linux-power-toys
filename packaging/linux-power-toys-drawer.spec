%if 0%{?fedora} == 45
%global plasma_version 6.8.3
%global plasma_release 1.fc45
%global qt_release 6.11.2-3.fc45
%else
%global plasma_version 6.8.2
%global plasma_release 1.fc44
%global qt_release 6.11.2-3.fc44
%endif
%global trayroot %{_libdir}/linux-power-toys/plasma

Name:           linux-power-toys-drawer
Version:        0.2
Release:        0.2.preview%{?dist}
Summary:        Tray Icon Drawer integration for Fedora KDE Plasma %{plasma_version}
License:        GPL-2.0-or-later AND LGPL-2.0-or-later
Source0:        linux-power-toys-%{version}.tar.gz
Source1:        https://download.kde.org/stable/plasma/%{plasma_version}/plasma-workspace-%{plasma_version}.tar.xz

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  patch
BuildRequires:  ninja-build
BuildRequires:  extra-cmake-modules
BuildRequires:  libplasma-devel = %{plasma_version}-%{plasma_release}
BuildRequires:  qt6-qtbase-devel = %{qt_release}
BuildRequires:  qt6-qtbase-private-devel = %{qt_release}
BuildRequires:  qt6-qtdeclarative-devel = %{qt_release}
BuildRequires:  cmake(KF6Config)
BuildRequires:  cmake(KF6CoreAddons)
BuildRequires:  cmake(KF6IconThemes)
BuildRequires:  cmake(KF6I18n)
BuildRequires:  cmake(KF6KIO)
BuildRequires:  cmake(KF6JobWidgets)
BuildRequires:  cmake(KF6Service)
BuildRequires:  cmake(KF6ItemModels)
BuildRequires:  cmake(KF6WindowSystem)
BuildRequires:  cmake(KF6XmlGui)
BuildRequires:  cmake(KF6StatusNotifierItem)
Requires:       linux-power-toys = %{version}-%{release}
Requires:       plasma-workspace%{?_isa} = %{plasma_version}-%{plasma_release}
Requires:       plasma-workspace-libs%{?_isa} = %{plasma_version}-%{plasma_release}
Requires:       libplasma%{?_isa} = %{plasma_version}-%{plasma_release}
Requires:       qt6-qtbase-gui%{?_isa} = %{qt_release}
Requires:       qt6-qtdeclarative%{?_isa} = %{qt_release}

%description
System Tray applet built for Fedora KDE with drag and drop between the panel
and the overflow drawer. The integration loads when the Plasma session starts.

%prep
%setup -q -n linux-power-toys-%{version} -a 1
patch --batch --forward --fuzz=0 -p1 -d plasma-workspace-%{plasma_version} \
  -i "$PWD/plasma/plasma-workspace-6.8.2-tray-drawer.patch"
test -f plasma-workspace-%{plasma_version}/applets/systemtray/qml/DrawerDropTarget.qml

%build
cmake -S plasma -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DPLASMA_VERSION=%{plasma_version} -DCMAKE_SKIP_RPATH=ON \
  -DPLASMA_WORKSPACE_SOURCE="$PWD/plasma-workspace-%{plasma_version}"
cmake --build build --target org.kde.plasma.systemtray systemtrayplugin %{?_smp_mflags}

%install
install -Dpm755 build/lib/plasma/applets/org.kde.plasma.systemtray.so \
  %{buildroot}%{trayroot}/plugins/plasma/applets/org.kde.plasma.systemtray.so
module=org/kde/plasma/private/systemtray
install -Dpm755 "build/bin/$module/libsystemtrayplugin.so" \
  "%{buildroot}%{trayroot}/qml/$module/libsystemtrayplugin.so"
install -Dpm644 "build/bin/$module/qmldir" \
  "%{buildroot}%{trayroot}/qml/$module/qmldir"
install -pm644 "build/bin/$module/systemtrayplugin.qmltypes" \
  "%{buildroot}%{trayroot}/qml/$module/"
sed 's|@LIBDIR@|%{_libdir}|g' packaging/50-linux-power-toys-drawer.conf.in > drawer.conf
install -Dpm644 drawer.conf \
  %{buildroot}%{_userunitdir}/plasma-plasmashell.service.d/50-linux-power-toys-drawer.conf

%files
%license plasma-workspace-%{plasma_version}/LICENSES/GPL-2.0-or-later.txt
%license plasma-workspace-%{plasma_version}/LICENSES/LGPL-2.0-or-later.txt
%{trayroot}
%dir %{_userunitdir}/plasma-plasmashell.service.d
%{_userunitdir}/plasma-plasmashell.service.d/50-linux-power-toys-drawer.conf
