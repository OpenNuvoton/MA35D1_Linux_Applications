SUMMARY = "MA35 Integrated demo program." 
RV = "1.0-0"
LICENSE = "CLOSED"
inherit systemd
SYSTEMD_AUTO_ENABLE = "enable"
SYSTEMD_SERVICE_${PN} = "run-before-login-prompt.service"
SRC_URI += "file://demo.tar.gz"



do_package_qa[noexec] = "1"
do_install() {      
    install -d ${D}/${base_libdir}/optee_armtz/
    cp -rf ${WORKDIR}/demo/lib/optee_armtz/* ${D}/${base_libdir}/optee_armtz/

    install -d ${D}/${systemd_unitdir}/system
    install -m 0644 ${WORKDIR}/demo/lib/systemd/system/run-before-login-prompt.service ${D}${systemd_unitdir}/system
                                
    install -d ${D}/${base_prefix}/opt/
    cp -rf ${WORKDIR}/demo/opt/* ${D}${base_prefix}/opt/
    install -m 0755 ${WORKDIR}/demo/opt/init.sh ${D}${base_prefix}/opt/
    install -m 0755 ${WORKDIR}/demo/opt/ma35d1_demo.sh ${D}${base_prefix}/opt/

    install -d ${D}/${sysconfdir}/.baresip
    cp -rf ${WORKDIR}/demo/etc/.baresip/* ${D}${sysconfdir}/.baresip
    
    install -d ${D}/${datadir}
    cp -rf ${WORKDIR}/demo/usr/share/* ${D}${datadir}/
    
    install -d ${D}/${libdir}
    cp -rf ${WORKDIR}/demo/usr/lib/* ${D}${libdir}/
    
    install -d ${D}/${includedir}
    cp -rf ${WORKDIR}/demo/usr/include/* ${D}${includedir}/
    
    install -d ${D}/${bindir}
    cp -rf ${WORKDIR}/demo/usr/bin/* ${D}${bindir}/ 
}

FILES_${PN} = "${systemd_unitdir}/system/* ${sysconfdir} ${base_libdir}/* ${datadir}/* ${includedir}/* ${bindir}/* ${libdir}/* ${base_prefix}/opt/*"
