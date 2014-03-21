#
# Copyright (C) 2009 Brice DUBOST
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mumudvb
PKG_VERSION:=1.7.3
PKG_SOURCE_VERSION:=6d7c5791ba2bca9f801c7218b1eec7f267cabd8f

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=git://github.com/braice/MuMuDVB.git
PKG_SOURCE_SUBDIR:=$(PKG_NAME)
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_SOURCE_SUBDIR)

EXTRA_CFLAGS+=-std=gnu99

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/nls.mk

define Package/mumudvb
  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:=+librt +libpthread +libiconv
  DEFAULT:=n
  TITLE:=MuMuDVB streaming software
  URL:=http://mumudvb.braice.net/
endef

define Package/mumudvb/description
 MuMuDVB streaming software
endef

define Build/Configure
        autoreconf $(PKG_BUILD_DIR) -i -f
        $(call Build/Configure/Default, LIBS=-liconv)
endef

define Package/mumudvb/install
        $(INSTALL_DIR) $(1)/usr/bin
        $(INSTALL_BIN) $(PKG_BUILD_DIR)/src/mumudvb $(1)/usr/bin/mumudvb
endef

$(eval $(call BuildPackage,mumudvb))
