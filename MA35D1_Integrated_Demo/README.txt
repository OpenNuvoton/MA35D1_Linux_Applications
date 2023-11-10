Demo video:
https://www.youtube.com/watch?v=Z3CKdRFPZ7I 

KWS/Linux:
Add RTP KWS Linux driver
Refer to dts/ma35d1.dtsi to add rtp_kws driver node
Copy rtp_kws.c, Kconfig, Makefile to ${Your Linux TOPDIR}/drivers/input/misc/
Enter Linux menuconfig -> enable rtp-fws
re-compile Linux kernel-> bitbake linux-ma35d1 -C compile
re-pack blob -> bitbake nvt-image-qt5 -c cleanall && bitbake nvt-image-qt5

KWS/TF-A:
Refer to fdts/ma35d1.dtsi and fdts/ma35d1-cpu800-wb-256m.dts to assign I2S/DMA to RTP M4
re-compile tf-a -> bitbake tf-a-ma35d1 -C compile
re-pack blob -> bitbake nvt-image-qt5 -c cleanall && bitbake nvt-image-qt5

recipes-demo:
Install Integrated Demo in Yocto:
Copy recipes-demo to Yocto-TOPDIR/meta-ma35d1/
Open ${YOCTO TOP DIR}/build/local.conf and add "integrateddemo" in CORE_IMAGE_EXTRA_INSTALL variable
e.x. CORE_IMAGE_EXTRA_INSTALL += "integrateddemo"
bitbake nvt-image-qt5
 
Qt/MA35D1_DEMO:
Application Source code

toolchain for compiling MA35D1_DEMO source code:
Refer to Chapter "Setup Compiler by Yocto" in https://www.nuvoton.com/export/resource-files/en-us--MA35D1_Yocto_Quick_Start_Rev1.00.pdf to setup the compiler for MA35

Add opencv into toolchain:
Unzip toolchain/opencv2.zip to /usr/local/oecore-x86_64/sysroots/aarch64-poky-linux/usr/local/include/
Unzip toolchain/lib.zip to /usr/local/oecore-x86_64/sysroots/aarch64-poky-linux/usr/local/

