# Fully static build (CRT and libraries), Release only: the debug Qt is the dynamic one
# from C:\Qt, and vcpkg builds only what goes into the release exe.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)
