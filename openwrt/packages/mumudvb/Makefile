# 
# Copyright (C) 2009 Brice DUBOST
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=mumudvb
PKG_VERSION:=1.6.1-openwrt
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=http://mumudvb.braice.net/mumudvb/mumudvb-beta
PKG_BUILD_DIR:=$(BUILD_DIR)/mumudvb
EXTRA_CPPFLAGS+=-std=gnu99


include $(INCLUDE_DIR)/package.mk


define Package/mumudvb
  SECTION:=utils
  CATEGORY:=Utilities
  DEFAULT:=n
  TITLE:=MuMuDVB streaming software
  URL:=http://mumudvb.braice.net/
endef

define Package/mumudvb/description
 MuMuDVB streaming software
endef

define Build/Configure
  $(call Build/Configure/Default,--with-linux-headers=$(LINUX_DIR))
endef

define Package/mumudvb/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/mumudvb $(1)/usr/bin/mumudvb
endef

$(eval $(call BuildPackage,mumudvb))
