# Example for a VS2022/MSVC19.4 dependency build. build.ps1 generates its own profile.
[settings]
os=Windows
arch=x86_64
compiler=msvc
compiler.version=194
compiler.cppstd=23
compiler.runtime=dynamic
build_type=Release
[conf]
tools.cmake.cmaketoolchain:generator=Visual Studio 17 2022
