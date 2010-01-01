
#
# Copyright (C) 2009 Brice DUBOST
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
#
# !!!! This is a generated file !!!! 
#

DVB_MENU:=DVB support

#
# General section
#

define KernelPackage/dvb-core
  SUBMENU:=$(DVB_MENU)
  TITLE:=DVB core support
  DEPENDS:=@LINUX_2_6 +kmod-i2c-core
  KCONFIG:= \
	CONFIG_DVB_CORE \
	CONFIG_DVB=y \
	CONFIG_DVB_CAPTURE_DRIVERS=y \
	MEDIA_TUNER_CUSTOMIZE=y \
	DVB_FE_CUSTOMISE=y \
	CONFIG_CRC32 
  FILES:=$(LINUX_DIR)/drivers/media/dvb/dvb-core/dvb-core.$(LINUX_KMOD_SUFFIX)
  AUTOLOAD:=$(call AutoLoad,50,dvb-core)
endef

define KernelPackage/dvb-core/description
 Kernel module for DVB support
endef

$(eval $(call KernelPackage,dvb-core))

define KernelPackage/dvb-usb
  SUBMENU:=$(DVB_MENU)
  TITLE:=DVB USB Support
  DEPENDS:=@USB_SUPPORT +kmod-dvb-core +kmod-usb-core
  KCONFIG:= \
	CONFIG_DVB_USB \
	CONFIG_INPUT=y
  FILES:=$(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb.$(LINUX_KMOD_SUFFIX)
#  AUTOLOAD:=$(call AutoLoad,55,dvb-usb)
endef

define KernelPackage/dvb-usb/description
 Kernel module for DVB USB devices. Note you have to select at least a device.
endef

$(eval $(call KernelPackage,dvb-usb))

#
# Devices section
#


define KernelPackage/dvb-usb-a800
  SUBMENU:=$(DVB_MENU)
  TITLE:=AVerMedia AverTV DVB-T USB 2.0 
  KCONFIG:= CONFIG_DVB_USB_A800 \
	CONFIG_DVB_DIB3000MC \
	CONFIG_DVB_TUNER_MT2060
  DEPENDS:=+kmod-dvb-usb
  FILES:= $(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-dibusb-common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-a800.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dib3000mc.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dibx000_common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/mt2060.$(LINUX_KMOD_SUFFIX)
endef

define KernelPackage/dvb-usb-a800/description
 Say Y here to support the AVerMedia AverTV DVB-T USB 2.0 .
The following modules will be compiled for this device :  dvb-usb-dibusb-common dvb-usb-a800 dib3000mc dibx000_common mt2060
You have to put the firmware files in the download dir : dvb-usb-avertv-a800-02.fw
They can be found in the package http://packages.ubuntu.com/jaunty/linux-firmware .
endef

DVB_USB_A800_FW_0:=dvb-usb-avertv-a800-02.fw

define KernelPackage/dvb-usb-a800/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_A800_FW_0) $(1)/lib/firmware/
endef

$(eval $(call KernelPackage,dvb-usb-a800))

define KernelPackage/dvb-usb-dibusb-mb
  SUBMENU:=$(DVB_MENU)
  TITLE:=DiBcom USB DVB-T devices  
  KCONFIG:= CONFIG_DVB_USB_DIBUSB_MB \
	CONFIG_DVB_DIB3000MB \
	CONFIG_DVB_TUNER_MT2060
  DEPENDS:=+kmod-dvb-usb
  FILES:= $(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-dibusb-common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-dibusb-mb.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dib3000mb.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/mt2060.$(LINUX_KMOD_SUFFIX)
endef

define KernelPackage/dvb-usb-dibusb-mb/description
 Say Y here to support the DiBcom USB DVB-T devices  .
The following modules will be compiled for this device :  dvb-usb-dibusb-common dvb-usb-dibusb-mb dib3000mb mt2060
You have to put the firmware files in the download dir : dvb-usb-dibusb-5.0.0.11.fw dvb-usb-dibusb-an2235-01.fw dvb-usb-adstech-usb2-02.fw dvb-usb-dibusb-6.0.0.8.fw
They can be found in the package http://packages.ubuntu.com/jaunty/linux-firmware .
endef

DVB_USB_DIBUSB_MB_FW_0:=dvb-usb-dibusb-5.0.0.11.fw
DVB_USB_DIBUSB_MB_FW_1:=dvb-usb-dibusb-an2235-01.fw
DVB_USB_DIBUSB_MB_FW_2:=dvb-usb-adstech-usb2-02.fw
DVB_USB_DIBUSB_MB_FW_3:=dvb-usb-dibusb-6.0.0.8.fw

define KernelPackage/dvb-usb-dibusb-mb/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_DIBUSB_MB_FW_0) $(1)/lib/firmware/
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_DIBUSB_MB_FW_1) $(1)/lib/firmware/
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_DIBUSB_MB_FW_2) $(1)/lib/firmware/
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_DIBUSB_MB_FW_3) $(1)/lib/firmware/
endef

$(eval $(call KernelPackage,dvb-usb-dibusb-mb))

define KernelPackage/dvb-usb-dibusb-mc
  SUBMENU:=$(DVB_MENU)
  TITLE:=DiBcom USB DVB-T devices  
  KCONFIG:= CONFIG_DVB_USB_DIBUSB_MC \
	CONFIG_DVB_DIB3000MC \
	CONFIG_DVB_TUNER_MT2060
  DEPENDS:=+kmod-dvb-usb
  FILES:= $(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-dibusb-common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-dibusb-mc.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dib3000mc.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dibx000_common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/mt2060.$(LINUX_KMOD_SUFFIX)
endef

define KernelPackage/dvb-usb-dibusb-mc/description
 Say Y here to support the DiBcom USB DVB-T devices  .
The following modules will be compiled for this device :  dvb-usb-dibusb-common dvb-usb-dibusb-mc dib3000mc dibx000_common mt2060
You have to put the firmware files in the download dir : dvb-usb-dibusb-6.0.0.8.fw
They can be found in the package http://packages.ubuntu.com/jaunty/linux-firmware .
endef

DVB_USB_DIBUSB_MC_FW_0:=dvb-usb-dibusb-6.0.0.8.fw

define KernelPackage/dvb-usb-dibusb-mc/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_DIBUSB_MC_FW_0) $(1)/lib/firmware/
endef

$(eval $(call KernelPackage,dvb-usb-dibusb-mc))

define KernelPackage/dvb-usb-dib0700
  SUBMENU:=$(DVB_MENU)
  TITLE:=DiBcom DiB0700 USB DVB devices 
  KCONFIG:= CONFIG_DVB_USB_DIB0700 \
	CONFIG_DVB_DIB7000P \
	CONFIG_DVB_DIB7000M \
	CONFIG_DVB_DIB3000MC \
	CONFIG_DVB_TUNER_MT2060
  DEPENDS:=+kmod-dvb-usb
  FILES:= $(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-dib0700.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dib7000p.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dibx000_common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dib7000m.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dibx000_common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dib3000mc.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dibx000_common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/mt2060.$(LINUX_KMOD_SUFFIX)
endef

define KernelPackage/dvb-usb-dib0700/description
 Say Y here to support the DiBcom DiB0700 USB DVB devices .
The following modules will be compiled for this device :  dvb-usb-dib0700 dib7000p dibx000_common dib7000m dibx000_common dib3000mc dibx000_common mt2060
You have to put the firmware files in the download dir : dvb-usb-dib0700-01.fw
They can be found in the package http://packages.ubuntu.com/jaunty/linux-firmware .
endef

DVB_USB_DIB0700_FW_0:=dvb-usb-dib0700-01.fw

define KernelPackage/dvb-usb-dib0700/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_DIB0700_FW_0) $(1)/lib/firmware/
endef

$(eval $(call KernelPackage,dvb-usb-dib0700))

define KernelPackage/dvb-usb-umt-010
  SUBMENU:=$(DVB_MENU)
  TITLE:=HanfTek UMT-010 DVB-T USB2.0 
  KCONFIG:= CONFIG_DVB_USB_UMT_010 \
	CONFIG_DVB_DIB3000MC \
	CONFIG_DVB_TUNER_MT2060
  DEPENDS:=+kmod-dvb-usb
  FILES:= $(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-dibusb-common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-umt-010.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dib3000mc.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dibx000_common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/mt2060.$(LINUX_KMOD_SUFFIX)
endef

define KernelPackage/dvb-usb-umt-010/description
 Say Y here to support the HanfTek UMT-010 DVB-T USB2.0 .
The following modules will be compiled for this device :  dvb-usb-dibusb-common dvb-usb-umt-010 dib3000mc dibx000_common mt2060
You have to put the firmware files in the download dir : dvb-usb-umt-010-02.fw
They can be found in the package http://packages.ubuntu.com/jaunty/linux-firmware .
endef

DVB_USB_UMT_010_FW_0:=dvb-usb-umt-010-02.fw

define KernelPackage/dvb-usb-umt-010/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_UMT_010_FW_0) $(1)/lib/firmware/
endef

$(eval $(call KernelPackage,dvb-usb-umt-010))

define KernelPackage/dvb-usb-cxusb
  SUBMENU:=$(DVB_MENU)
  TITLE:=Conexant USB2.0 hybrid reference design 
  KCONFIG:= CONFIG_DVB_USB_CXUSB \
	CONFIG_DVB_CX22702 \
	CONFIG_DVB_LGDT330X \
	CONFIG_DVB_TUNER_LGH06XF \
	CONFIG_DVB_MT352 \
	CONFIG_DVB_ZL10353
  DEPENDS:=+kmod-dvb-usb
  FILES:= $(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-cxusb.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/cx22702.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/lgdt330x.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/lgh06xf.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/mt352.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/zl10353.$(LINUX_KMOD_SUFFIX)
endef

define KernelPackage/dvb-usb-cxusb/description
 Say Y here to support the Conexant USB2.0 hybrid reference design .
The following modules will be compiled for this device :  dvb-usb-cxusb cx22702 lgdt330x lgh06xf mt352 zl10353
You have to put the firmware files in the download dir : dvb-usb-bluebird-01.fw
They can be found in the package http://packages.ubuntu.com/jaunty/linux-firmware .
endef

DVB_USB_CXUSB_FW_0:=dvb-usb-bluebird-01.fw

define KernelPackage/dvb-usb-cxusb/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_CXUSB_FW_0) $(1)/lib/firmware/
endef

$(eval $(call KernelPackage,dvb-usb-cxusb))

define KernelPackage/dvb-usb-m920x
  SUBMENU:=$(DVB_MENU)
  TITLE:=Uli m920x DVB-T USB2.0 
  KCONFIG:= CONFIG_DVB_USB_M920X \
	CONFIG_DVB_MT352 \
	CONFIG_DVB_TUNER_QT1010
  DEPENDS:=+kmod-dvb-usb
  FILES:= $(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-m920x.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/mt352.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/qt1010.$(LINUX_KMOD_SUFFIX)
endef

define KernelPackage/dvb-usb-m920x/description
 Say Y here to support the Uli m920x DVB-T USB2.0 .
The following modules will be compiled for this device :  dvb-usb-m920x mt352 qt1010
You have to put the firmware files in the download dir : dvb-usb-megasky-02.fw
They can be found in the package http://packages.ubuntu.com/jaunty/linux-firmware .
endef

DVB_USB_M920X_FW_0:=dvb-usb-megasky-02.fw

define KernelPackage/dvb-usb-m920x/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_M920X_FW_0) $(1)/lib/firmware/
endef

$(eval $(call KernelPackage,dvb-usb-m920x))

define KernelPackage/dvb-usb-gl861
  SUBMENU:=$(DVB_MENU)
  TITLE:=Genesys Logic GL861 USB2.0 
  KCONFIG:= CONFIG_DVB_USB_GL861 \
	CONFIG_DVB_ZL10353 \
	CONFIG_DVB_TUNER_QT1010
  DEPENDS:=+kmod-dvb-usb
  FILES:= $(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-gl861.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/zl10353.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/qt1010.$(LINUX_KMOD_SUFFIX)
endef

define KernelPackage/dvb-usb-gl861/description
 Say Y here to support the Genesys Logic GL861 USB2.0 .
The following modules will be compiled for this device :  dvb-usb-gl861 zl10353 qt1010

endef



$(eval $(call KernelPackage,dvb-usb-gl861))

define KernelPackage/dvb-usb-au6610
  SUBMENU:=$(DVB_MENU)
  TITLE:=Alcor Micro AU6610 USB2.0 
  KCONFIG:= CONFIG_DVB_USB_AU6610 \
	CONFIG_DVB_ZL10353 \
	CONFIG_DVB_TUNER_QT1010
  DEPENDS:=+kmod-dvb-usb
  FILES:= $(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-au6610.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/zl10353.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/qt1010.$(LINUX_KMOD_SUFFIX)
endef

define KernelPackage/dvb-usb-au6610/description
 Say Y here to support the Alcor Micro AU6610 USB2.0 .
The following modules will be compiled for this device :  dvb-usb-au6610 zl10353 qt1010

endef



$(eval $(call KernelPackage,dvb-usb-au6610))

define KernelPackage/dvb-usb-digitv
  SUBMENU:=$(DVB_MENU)
  TITLE:=Nebula Electronics uDigiTV DVB-T USB2.0 
  KCONFIG:= CONFIG_DVB_USB_DIGITV \
	CONFIG_DVB_NXT6000 \
	CONFIG_DVB_MT352
  DEPENDS:=+kmod-dvb-usb
  FILES:= $(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-digitv.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/nxt6000.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/mt352.$(LINUX_KMOD_SUFFIX)
endef

define KernelPackage/dvb-usb-digitv/description
 Say Y here to support the Nebula Electronics uDigiTV DVB-T USB2.0 .
The following modules will be compiled for this device :  dvb-usb-digitv nxt6000 mt352
You have to put the firmware files in the download dir : dvb-usb-digitv-02.fw
They can be found in the package http://packages.ubuntu.com/jaunty/linux-firmware .
endef

DVB_USB_DIGITV_FW_0:=dvb-usb-digitv-02.fw

define KernelPackage/dvb-usb-digitv/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_DIGITV_FW_0) $(1)/lib/firmware/
endef

$(eval $(call KernelPackage,dvb-usb-digitv))

define KernelPackage/dvb-usb-vp7045
  SUBMENU:=$(DVB_MENU)
  TITLE:=TwinhanDTV Alpha/MagicBoxII, DNTV tinyUSB2, Beetle
  KCONFIG:= CONFIG_DVB_USB_VP7045
  DEPENDS:=+kmod-dvb-usb
  FILES:= $(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-vp7045.$(LINUX_KMOD_SUFFIX)
endef

define KernelPackage/dvb-usb-vp7045/description
 Say Y here to support the TwinhanDTV Alpha/MagicBoxII, DNTV tinyUSB2, Beetle.
The following modules will be compiled for this device :  dvb-usb-vp7045
You have to put the firmware files in the download dir : dvb-usb-vp7045-01.fw
They can be found in the package http://packages.ubuntu.com/jaunty/linux-firmware .
endef

DVB_USB_VP7045_FW_0:=dvb-usb-vp7045-01.fw

define KernelPackage/dvb-usb-vp7045/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_VP7045_FW_0) $(1)/lib/firmware/
endef

$(eval $(call KernelPackage,dvb-usb-vp7045))

define KernelPackage/dvb-usb-vp702x
  SUBMENU:=$(DVB_MENU)
  TITLE:=TwinhanDTV StarBox and clones DVB-S USB2.0 
  KCONFIG:= CONFIG_DVB_USB_VP702X
  DEPENDS:=+kmod-dvb-usb
  FILES:= $(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-vp702x.$(LINUX_KMOD_SUFFIX)
endef

define KernelPackage/dvb-usb-vp702x/description
 Say Y here to support the TwinhanDTV StarBox and clones DVB-S USB2.0 .
The following modules will be compiled for this device :  dvb-usb-vp702x
You have to put the firmware files in the download dir : dvb-usb-vp702x-02.fw
They can be found in the package http://packages.ubuntu.com/jaunty/linux-firmware .
endef

DVB_USB_VP702X_FW_0:=dvb-usb-vp702x-02.fw

define KernelPackage/dvb-usb-vp702x/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_VP702X_FW_0) $(1)/lib/firmware/
endef

$(eval $(call KernelPackage,dvb-usb-vp702x))

define KernelPackage/dvb-usb-gp8psk
  SUBMENU:=$(DVB_MENU)
  TITLE:=GENPIX 8PSK->USB module 
  KCONFIG:= CONFIG_DVB_USB_GP8PSK
  DEPENDS:=+kmod-dvb-usb
  FILES:= $(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-gp8psk.$(LINUX_KMOD_SUFFIX)
endef

define KernelPackage/dvb-usb-gp8psk/description
 Say Y here to support the GENPIX 8PSK->USB module .
The following modules will be compiled for this device :  dvb-usb-gp8psk
You have to put the firmware files in the download dir : dvb-usb-gp8psk-01.fw
They can be found in the package http://packages.ubuntu.com/jaunty/linux-firmware .
endef

DVB_USB_GP8PSK_FW_0:=dvb-usb-gp8psk-01.fw

define KernelPackage/dvb-usb-gp8psk/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_GP8PSK_FW_0) $(1)/lib/firmware/
endef

$(eval $(call KernelPackage,dvb-usb-gp8psk))

define KernelPackage/dvb-usb-nova-t-usb2
  SUBMENU:=$(DVB_MENU)
  TITLE:=Hauppauge WinTV-NOVA-T usb2 DVB-T USB2.0 
  KCONFIG:= CONFIG_DVB_USB_NOVA_T_USB2 \
	CONFIG_DVB_DIB3000MC \
	CONFIG_DVB_TUNER_MT2060
  DEPENDS:=+kmod-dvb-usb
  FILES:= $(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-dibusb-common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-nova-t-usb2.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dib3000mc.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dibx000_common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/mt2060.$(LINUX_KMOD_SUFFIX)
endef

define KernelPackage/dvb-usb-nova-t-usb2/description
 Say Y here to support the Hauppauge WinTV-NOVA-T usb2 DVB-T USB2.0 .
The following modules will be compiled for this device :  dvb-usb-dibusb-common dvb-usb-nova-t-usb2 dib3000mc dibx000_common mt2060
You have to put the firmware files in the download dir : dvb-usb-nova-t-usb2-02.fw
They can be found in the package http://packages.ubuntu.com/jaunty/linux-firmware .
endef

DVB_USB_NOVA_T_USB2_FW_0:=dvb-usb-nova-t-usb2-02.fw

define KernelPackage/dvb-usb-nova-t-usb2/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_NOVA_T_USB2_FW_0) $(1)/lib/firmware/
endef

$(eval $(call KernelPackage,dvb-usb-nova-t-usb2))

define KernelPackage/dvb-usb-ttusb2
  SUBMENU:=$(DVB_MENU)
  TITLE:=Pinnacle 400e DVB-S USB2.0 
  KCONFIG:= CONFIG_DVB_USB_TTUSB2 \
	CONFIG_DVB_TDA10086 \
	CONFIG_DVB_LNBP21 \
	CONFIG_DVB_TDA826X
  DEPENDS:=+kmod-dvb-usb
  FILES:= $(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-ttusb2.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/tda10086.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/lnbp21.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/tda826x.$(LINUX_KMOD_SUFFIX)
endef

define KernelPackage/dvb-usb-ttusb2/description
 Say Y here to support the Pinnacle 400e DVB-S USB2.0 .
The following modules will be compiled for this device :  dvb-usb-ttusb2 tda10086 lnbp21 tda826x
You have to put the firmware files in the download dir : dvb-usb-pctv-400e-01.fw
They can be found in the package http://packages.ubuntu.com/jaunty/linux-firmware .
endef

DVB_USB_TTUSB2_FW_0:=dvb-usb-pctv-400e-01.fw

define KernelPackage/dvb-usb-ttusb2/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_TTUSB2_FW_0) $(1)/lib/firmware/
endef

$(eval $(call KernelPackage,dvb-usb-ttusb2))

define KernelPackage/dvb-usb-dtt200u
  SUBMENU:=$(DVB_MENU)
  TITLE:=WideView WT-200U and WT-220U  DVB-T USB2.0  
  KCONFIG:= CONFIG_DVB_USB_DTT200U
  DEPENDS:=+kmod-dvb-usb
  FILES:= $(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-dtt200u.$(LINUX_KMOD_SUFFIX)
endef

define KernelPackage/dvb-usb-dtt200u/description
 Say Y here to support the WideView WT-200U and WT-220U  DVB-T USB2.0  .
The following modules will be compiled for this device :  dvb-usb-dtt200u
You have to put the firmware files in the download dir : dvb-usb-dtt200u-01.fw dvb-usb-wt220u-02.fw dvb-usb-wt220u-fc03.fw dvb-usb-wt220u-zl0353-01.fw
They can be found in the package http://packages.ubuntu.com/jaunty/linux-firmware .
endef

DVB_USB_DTT200U_FW_0:=dvb-usb-dtt200u-01.fw
DVB_USB_DTT200U_FW_1:=dvb-usb-wt220u-02.fw
DVB_USB_DTT200U_FW_2:=dvb-usb-wt220u-fc03.fw
DVB_USB_DTT200U_FW_3:=dvb-usb-wt220u-zl0353-01.fw

define KernelPackage/dvb-usb-dtt200u/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_DTT200U_FW_0) $(1)/lib/firmware/
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_DTT200U_FW_1) $(1)/lib/firmware/
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_DTT200U_FW_2) $(1)/lib/firmware/
	$(INSTALL_DATA) $(DL_DIR)/$(DVB_USB_DTT200U_FW_3) $(1)/lib/firmware/
endef

$(eval $(call KernelPackage,dvb-usb-dtt200u))
