#
# Copyright (C) 2009 Brice DUBOST
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

DVB_MENU:=DVB support

define KernelPackage/dvb-core
  SUBMENU:=$(DVB_MENU)
  TITLE:=DVB core support
  DEPENDS:=@LINUX_2_6 +kmod-i2c-core
  KCONFIG:= \
	CONFIG_DVB_CORE \
	CONFIG_DVB_CAPTURE_DRIVERS=y \
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
  DEPENDS:=@USB_SUPPORT +kmod-dvb-core +kmod-usb-core +kmod-i2c-core
  KCONFIG:= \
	CONFIG_DVB_USB \
	CONFIG_INPUT=y
  FILES:=$(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb.$(LINUX_KMOD_SUFFIX)
#  AUTOLOAD:=$(call AutoLoad,55,dvb-usb)
endef

define KernelPackage/dvb-usb/description
 Kernel module for DVB USB devices. Note you have to select a device.
endef

$(eval $(call KernelPackage,dvb-usb))

define KernelPackage/dvb-usb-a800
  SUBMENU:=$(DVB_MENU)
  TITLE:=AVerMedia AverTV DVB-T USB 2.0 (A800) receiver
  KCONFIG:= \
	CONFIG_DVB_USB_A800 \
	CONFIG_DVB_DIB3000MC \
	CONFIG_MEDIA_TUNER_MT2060
  DEPENDS:=@USB_SUPPORT +kmod-dvb-core +kmod-dvb-usb +kmod-usb-core +kmod-i2c-core
  FILES:= \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dvb-pll.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-dibusb-common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-a800.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dibx000_common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dib3000mc.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/common/tuners/mt2060.$(LINUX_KMOD_SUFFIX)
#  AUTOLOAD:=$(call Autoload,60, \
#	dvb-pll \
#	dibx000_common \
#	dib3000mc \
#	mt2060 \
#	dvb-usb-dibusb-common \
#	dvb-usb-a800 \
#  )
endef

A800_FW:=dvb-usb-avertv-a800-02.fw

define KernelPackage/dvb-usb-a800/description
 Say Y here to support the AVerMedia AverTV DVB-T USB 2.0 (A800) receiver. 
The firmware will be downloaded from http://www.linuxtv.org/downloads/firmware/
If the download fail put the firmware $(A800_FW) in the openwrt download directory.
endef

define Download/dvb-usb-a800
  FILE:=$(A800_FW)
  URL:=http://www.linuxtv.org/downloads/firmware/
endef


define KernelPackage/dvb-usb-a800/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(A800_FW) $(1)/lib/firmware/
endef

$(eval $(call Download,dvb-usb-a800))
$(eval $(call KernelPackage,dvb-usb-a800))

define KernelPackage/dvb-usb-af9015
  SUBMENU:=$(DVB_MENU)
  TITLE:=Afatech AF9015 DVB-T USB2.0 support
  KCONFIG:= \
	CONFIG_DVB_USB_AF9015 \
	CONFIG_DVB_AF9013 \
	CONFIG_DVB_PLL \
	CONFIG_MEDIA_TUNER_MT2060 \
	CONFIG_MEDIA_TUNER_QT1010 \
	CONFIG_MEDIA_TUNER_TDA18271 \
	CONFIG_MEDIA_TUNER_MXL5005S
  DEPENDS:=@USB_SUPPORT +kmod-dvb-core +kmod-dvb-usb +kmod-usb-core +kmod-i2c-core
  FILES:= \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dvb-pll.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-af9015.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/af9013.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/common/tuners/mt2060.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/common/tuners/qt1010.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/common/tuners/tda18271.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/common/tuners/mxl5005s.$(LINUX_KMOD_SUFFIX)
endef

AF9015_FW:=dvb-usb-af9015.fw

define KernelPackage/dvb-usb-af9015/description
 Supported devices : 
*  Afatech AF9015 DVB-T USB2.0 stick
*  Leadtek WinFast DTV Dongle Gold
*  Pinnacle PCTV 71e
*  KWorld PlusTV Dual DVB-T Stick (DVB-T 399U)
*  DigitalNow TinyTwin DVB-T Receiver
*  TwinHan AzureWave AD-TU700(704J)
*  TerraTec Cinergy T USB XE
*  KWorld PlusTV Dual DVB-T PCI (DVB-T PC160-2T)
*  AVerMedia AVerTV DVB-T Volar X
*  Xtensions XD-380
*  MSI DIGIVOX Duo
*  Fujitsu-Siemens Slim Mobile USB DVB-T
*  Telestar Starstick 2
*  AVerMedia A309
*  MSI Digi VOX mini III
The firmware will be downloaded from http://www.otit.fi/~crope/v4l-dvb/, version 4.65.0
If the download fail put the firmware $(AF9015_FW) in the openwrt download directory.
endef


define Download/dvb-usb-af9015
  FILE:=$(AF9015_FW)
  URL:=http://www.otit.fi/~crope/v4l-dvb/af9015/af9015_firmware_cutter/firmware_files/4.65.0/
  MD5SUM:=532b8e1eabd3b4e9f8ca084b767e4470
endef

define KernelPackage/dvb-usb-af9015/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(AF9015_FW) $(1)/lib/firmware/
endef

$(eval $(call Download,dvb-usb-af9015))
$(eval $(call KernelPackage,dvb-usb-af9015))



define KernelPackage/dvb-usb-dibusb-mc
  SUBMENU:=$(DVB_MENU)
  TITLE:=DiBcom USB DVB-T (DiB3000M-C/P based devices)
  KCONFIG:= \
	CONFIG_DVB_USB_DIBUSB_MC \
	CONFIG_DVB_DIB3000MC \
	CONFIG_MEDIA_TUNER_MT2060
  DEPENDS:=@USB_SUPPORT +kmod-dvb-core +kmod-dvb-usb +kmod-usb-core +kmod-i2c-core
  FILES:= \
	$(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-dibusb-common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-mc.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dibx000_common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dib3000mc.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/common/tuners/mt2060.$(LINUX_KMOD_SUFFIX)
endef

DIBUSB-MC_FW:=dvb-usb-dibusb-6.0.0.8.fw

define KernelPackage/dvb-usb-dibusb-mc/description
 Supported devices : 
*  DiBcom USB2.0 DVB-T reference design (MOD3000P)
*  Artec T1 USB2.0 TVBOX (please check the warm ID)
*  LITE-ON USB2.0 DVB-T Tuner
*  MSI Digivox Mini SL
*  GRAND - USB2.0 DVB-T adapter
*  Artec T14 - USB2.0 DVB-T
*  Leadtek - USB2.0 Winfast DTV dongle

The firmware will be downloaded from http://www.linuxtv.org/downloads/firmware/
If the download fail put the firmware $(DIBUSB-MC_FW) in the openwrt download directory.
endef


define Download/dvb-usb-dibusb-mc
  FILE:=$(DIBUSB-MC_FW)
  URL:=http://www.linuxtv.org/downloads/firmware/
endef

define KernelPackage/dvb-usb-dibusb-mc/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(DIBUSB-MC_FW) $(1)/lib/firmware/
endef

$(eval $(call Download,dvb-usb-dibusb-mc))
$(eval $(call KernelPackage,dvb-usb-dibusb-mc))



define KernelPackage/dvb-usb-dibusb-mb
  SUBMENU:=$(DVB_MENU)
  TITLE:=DiBcom USB DVB-T (DiB3000M-B based devices)
  KCONFIG:= \
	CONFIG_DVB_USB_DIBUSB_MB \
	CONFIG_DVB_PLL \
	CONFIG_DVB_DIB3000MB \
	CONFIG_MEDIA_TUNER_MT2060
  DEPENDS:=@USB_SUPPORT +kmod-dvb-core +kmod-dvb-usb +kmod-usb-core +kmod-i2c-core
  FILES:= \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dvb-pll.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-dibusb-common.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/dvb-usb/dvb-usb-mb.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/dvb/frontends/dib3000mb.$(LINUX_KMOD_SUFFIX) \
	$(LINUX_DIR)/drivers/media/common/tuners/mt2060.$(LINUX_KMOD_SUFFIX)
endef

DIBUSB-MB_FW1:=dvb-usb-dibusb-5.0.0.11.fw
DIBUSB-MB_FW2:=dvb-usb-dibusb-6.0.0.8.fw

define KernelPackage/dvb-usb-dibusb-mb/description
 Supported devices : 
*  AVerMedia AverTV DVBT USB1.1
*  Compro Videomate DVB-U2000 - DVB-T USB1.1 (please confirm to linux-dvb)
*  DiBcom USB1.1 DVB-T reference design (MOD3000)
*  KWorld V-Stream XPERT DTV - DVB-T USB1.1
*  Grandtec USB1.1 DVB-T
*  TwinhanDTV USB-Ter USB1.1 / Magic Box I / HAMA USB1.1 DVB-T device
*  Artec T1 USB1.1 TVBOX with AN2135
*  VideoWalker DVB-T USB
*  Artec T1 USB2.0

Supported but you have to add the firmware by hand : 
*  KWorld Xpert DVB-T USB2.0
*  KWorld/ADSTech Instant DVB-T USB2.0
*  Artec T1 USB1.1 TVBOX with AN2235

The firmware will be downloaded from http://www.linuxtv.org/downloads/firmware/
If the download fail put the firmware $(DIBUSB-MB_FW1) $(DIBUSB-MB_FW2) in the openwrt download directory.
endef


define Download/dvb-usb-dibusb-mb1
  FILE:=$(DIBUSB-MB_FW1)
  URL:=http://www.linuxtv.org/downloads/firmware/
endef
define Download/dvb-usb-dibusb-mb2
  FILE:=$(DIBUSB-MB_FW2)
  URL:=http://www.linuxtv.org/downloads/firmware/
endef

define KernelPackage/dvb-usb-dibusb-mb/install
	$(INSTALL_DIR) $(1)/lib/firmware
	$(INSTALL_DATA) $(DL_DIR)/$(DIBUSB-MB_FW1) $(1)/lib/firmware/
	$(INSTALL_DATA) $(DL_DIR)/$(DIBUSB-MB_FW2) $(1)/lib/firmware/
endef

$(eval $(call Download,dvb-usb-dibusb-mb1))
$(eval $(call Download,dvb-usb-dibusb-mb2))
$(eval $(call KernelPackage,dvb-usb-dibusb-mb))
