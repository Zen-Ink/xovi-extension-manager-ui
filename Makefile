ARCH ?= arm32
JOBS ?= 4
XOVI_REPO ?= $(abspath ../external/xovi)
PACKAGE_ROOT ?= $(abspath ../build/packages)

.PHONY: package
package:
	XOVI_REPO="$(XOVI_REPO)" JOBS="$(JOBS)" bash build.sh $(ARCH)
	mkdir -p "$(PACKAGE_ROOT)/$(ARCH)/xovi-extension-manager-ui"
	cp manifest.json build/$(ARCH)/xovi-extension-manager-ui.so "$(PACKAGE_ROOT)/$(ARCH)/xovi-extension-manager-ui/"
	python3 ../scripts/normalize_package_manifest.py "$(PACKAGE_ROOT)/$(ARCH)/xovi-extension-manager-ui/manifest.json" "$(ARCH)" --binary "$(PACKAGE_ROOT)/$(ARCH)/xovi-extension-manager-ui/xovi-extension-manager-ui.so"
	tar -czf "$(PACKAGE_ROOT)/$(ARCH)/xovi-extension-manager-ui.tar.gz" -C "$(PACKAGE_ROOT)/$(ARCH)/xovi-extension-manager-ui" manifest.json xovi-extension-manager-ui.so
