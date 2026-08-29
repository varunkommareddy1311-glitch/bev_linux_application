SUMMARY = "Reusable Bird's-Eye-View (BEV) processing core library"
DESCRIPTION = "libbev-core.so: calibration, perspective transform, \
stitching and health-monitoring logic for the automotive surround-view \
BEV application. Contains no application-layer or Yocto-specific logic."
HOMEPAGE = "https://example.com/bev-project"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${S}/LICENSE;md5=<REPLACE_WITH_ACTUAL_MD5>"

DEPENDS = "opencv"

# For a monorepo checked out alongside this layer, point SRC_URI at the
# repository (git, local tarball, or bare directory) that contains the
# top-level CMakeLists.txt / include / src described in the project README.
SRC_URI = "git://example.com/bev-project.git;protocol=https;branch=main"
SRCREV = "${AUTOREV}"

S = "${WORKDIR}/git"

inherit cmake

EXTRA_OECMAKE = " \
    -DBUILD_BEV_APP=OFF \
    -DBUILD_BEV_TOOLS=OFF \
    -DBUILD_BEV_TESTS=OFF \
"

FILES:${PN} = "${libdir}/libbev-core.so.*"
FILES:${PN}-dev = "${includedir}/bev ${libdir}/libbev-core.so"

# Calibration YAML files are packaged separately with bev-app (they are
# runtime data owned by the application deployment, not the library).
