SUMMARY = "Automotive BEV application"
DESCRIPTION = "bev-app: application layer that drives libbev-core.so \
against camera/image input and writes BEV output. Installs calibration \
defaults and the systemd unit that keeps the process running."
HOMEPAGE = "https://example.com/bev-project"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${S}/LICENSE;md5=<REPLACE_WITH_ACTUAL_MD5>"

DEPENDS = "opencv bev-core"
RDEPENDS:${PN} = "bev-core"

SRC_URI = " \
    git://example.com/bev-project.git;protocol=https;branch=main \
    file://bev.service \
"
SRCREV = "${AUTOREV}"

S = "${WORKDIR}/git"

inherit cmake systemd

EXTRA_OECMAKE = " \
    -DBUILD_BEV_APP=ON \
    -DBUILD_BEV_TOOLS=OFF \
    -DBUILD_BEV_TESTS=OFF \
"

SYSTEMD_SERVICE:${PN} = "bev.service"
SYSTEMD_AUTO_ENABLE:${PN} = "enable"

do_install:append() {
    # Runtime calibration lives under /etc/bev so it survives application
    # updates/restarts without recalibration (crash-recovery Layer 4).
    install -d ${D}${sysconfdir}/bev
    install -m 0644 ${S}/config/*.yaml ${D}${sysconfdir}/bev/

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/bev.service ${D}${systemd_system_unitdir}/bev.service

    install -d ${D}${localstatedir}/lib/bev/input/front \
               ${D}${localstatedir}/lib/bev/input/rear \
               ${D}${localstatedir}/lib/bev/input/left \
               ${D}${localstatedir}/lib/bev/input/right \
               ${D}${localstatedir}/lib/bev/output
}

FILES:${PN} += " \
    ${sysconfdir}/bev \
    ${localstatedir}/lib/bev \
    ${systemd_system_unitdir}/bev.service \
"
