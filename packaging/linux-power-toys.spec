Name:           linux-power-toys
Version:        0.1
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

%description
Native toolbox and Always Awake utility for the KDE Plasma session.

%prep
%autosetup

%build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=%{_prefix} -DCMAKE_INSTALL_SYSCONFDIR=%{_sysconfdir}
cmake --build build %{?_smp_mflags}

%install
DESTDIR=%{buildroot} cmake --install build

%files
%license assets/awake/NOTICE
%{_bindir}/linux-power-toys
%{_datadir}/applications/org.longweekendlabs.LinuxPowerToys.desktop
%{_sysconfdir}/xdg/autostart/org.longweekendlabs.LinuxPowerToys.desktop
