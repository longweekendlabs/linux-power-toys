Name:           linux-power-toys
Version:        0.2
Release:        0.1.preview%{?dist}
Summary:        Desktop utilities for KDE Plasma
License:        LicenseRef-Local-Preview AND MIT
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  cmake(Qt6Widgets)
BuildRequires:  cmake(Qt6DBus)
BuildRequires:  cmake(KF6StatusNotifierItem)
BuildRequires:  cmake(KF6WindowSystem)
Requires:       powerdevil
Requires:       /usr/bin/powerprofilesctl
Requires:       polkit
Requires:       python3
Requires:       libevdev
Requires:       systemd-libs

%description
Desktop toolbox with Always Awake, Power and Touchpad Edges for KDE Plasma.

%prep
%autosetup

%build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=%{_prefix} -DCMAKE_INSTALL_SYSCONFDIR=%{_sysconfdir} \
  -DCMAKE_INSTALL_LIBEXECDIR=%{_libexecdir} -DCMAKE_INSTALL_LOCALSTATEDIR=%{_localstatedir}
cmake --build build %{?_smp_mflags}

%install
DESTDIR=%{buildroot} cmake --install build

%files
%license assets/awake/NOTICE helpers/LICENSE
%{_bindir}/linux-power-toys
%{_datadir}/applications/org.longweekendlabs.LinuxPowerToys.desktop
%{_sysconfdir}/xdg/autostart/org.longweekendlabs.LinuxPowerToys.desktop
%{_libexecdir}/linux-power-toys/
%{_datadir}/polkit-1/actions/org.longweekendlabs.LinuxPowerToys.policy
%dir %{_localstatedir}/lib/linux-power-toys
