TEMPLATE = lib
TARGET = xovi-extension-manager-ui
CONFIG += shared plugin no_plugin_name_prefix c++17
QT += core gui qml quick
XOVI_REPO = $$(XOVI_REPO)
isEmpty(XOVI_REPO): XOVI_REPO = $$clean_path($$PWD/xovi)
SOURCES += src/main.cpp src/bridge.cpp xovi.cpp
HEADERS += src/bridge.h
RESOURCES += manager_resources.qrc sdk/xovi_controls.qrc
xoviextension.target = $$PWD/xovi.cpp
xoviextension.commands = cd $$PWD && python3 $$XOVI_REPO/util/xovigen.py -o xovi.cpp -H xovi.h xovi-extension-manager-ui.xovi
xoviextension.depends = $$PWD/xovi-extension-manager-ui.xovi $$PWD/qmd/manager_3_27_x.qmd $$PWD/qmd/manager_3_28_x.qmd $$XOVI_REPO/util/xovigen.py
QMAKE_EXTRA_TARGETS += xoviextension
PRE_TARGETDEPS += $$PWD/xovi.cpp
QMAKE_LFLAGS += -Wl,-Bsymbolic

XOVI_TRANSLATION_ID = xovi-extension-manager-ui
XOVI_TRANSLATION_DIR = $$PWD/translations
include($$clean_path($$PWD/sdk/i18n.pri))
