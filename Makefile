ARCH ?= arm32
NFJ_PLUGIN_DELIVERY_TOOL ?= $(abspath ../../nfj/install.remarkable.nfj/scripts/publish-native-plugin.py)
JOBS ?= $(shell nproc)
XOVI_REPO ?= $(abspath ../../xovi)
PACKAGE_ROOT ?= $(abspath ../build/packages)

.PHONY: package install-repo
package:
	XOVI_REPO="$(XOVI_REPO)" JOBS="$(JOBS)" bash build.sh $(ARCH)
	mkdir -p "$(PACKAGE_ROOT)/$(ARCH)/xovi-extension-manager-ui"
	cp build/$(ARCH)/manifest.json build/$(ARCH)/xovi-extension-manager-ui.so "$(PACKAGE_ROOT)/$(ARCH)/xovi-extension-manager-ui/"
	python3 ../scripts/normalize_package_manifest.py "$(PACKAGE_ROOT)/$(ARCH)/xovi-extension-manager-ui/manifest.json" "$(ARCH)" --binary "$(PACKAGE_ROOT)/$(ARCH)/xovi-extension-manager-ui/xovi-extension-manager-ui.so"
	tar -czf "$(PACKAGE_ROOT)/$(ARCH)/xovi-extension-manager-ui.tar.gz" -C "$(PACKAGE_ROOT)/$(ARCH)/xovi-extension-manager-ui" manifest.json xovi-extension-manager-ui.so

install-repo:
	@case "$(NFJ_INSTALL_REPO)" in /*) ;; *) echo 'NFJ_INSTALL_REPO must be absolute' >&2; exit 2;; esac
	@test -n "$(NFJ_PLUGIN_DELIVERY_TOOL)" || { echo 'NFJ_PLUGIN_DELIVERY_TOOL is required' >&2; exit 2; }
	@case "$(NFJ_PACKAGE_ARCH)" in armv7) arch=arm32;; aarch64) arch=aarch64;; *) echo 'NFJ_PACKAGE_ARCH must be armv7 or aarch64' >&2; exit 2;; esac; \
		$(MAKE) package ARCH="$$arch" && \
		python3 "$(NFJ_PLUGIN_DELIVERY_TOOL)" --repo "$(NFJ_INSTALL_REPO)" --arch "$(NFJ_PACKAGE_ARCH)" --archive "$(PACKAGE_ROOT)/$$arch/xovi-extension-manager-ui.tar.gz"
