TEMPLATE = lib
TARGET = xovi-extension-manager-ui
CONFIG += shared plugin no_plugin_name_prefix c++17
QT += core gui qml quick dbus
XOVI_REPO = $$(XOVI_REPO)
isEmpty(XOVI_REPO): XOVI_REPO = $$clean_path($$PWD/xovi)
SOURCES += src/main.cpp src/bridge.cpp src/official-update-capture.cpp $$OUT_PWD/xovi.cpp
INCLUDEPATH += $$OUT_PWD
LIBS += -ldl
HEADERS += src/bridge.h
isEmpty(XOVI_SDK): XOVI_SDK = $$(XOVI_SDK)
isEmpty(XOVI_SDK): XOVI_SDK = $$PWD/sdk
!exists($$XOVI_SDK/xovi_controls.qrc): error("XOVI_SDK must contain the shared controls")
RESOURCES += manager_resources.qrc $$XOVI_SDK/xovi_controls.qrc
xoviextension.target = $$OUT_PWD/xovi.cpp
xoviextension.commands = cd $$PWD && python3 $$XOVI_REPO/util/xovigen.py -o $$OUT_PWD/xovi.cpp -H $$OUT_PWD/xovi-generated.h -m $$OUT_PWD/manifest.json --manifest-entry xovi-extension-manager-ui.so xovi-extension-manager-ui.xovi
xoviextension.depends = $$PWD/xovi-extension-manager-ui.xovi $$PWD/qmd/manager_3_27_x.qmd $$PWD/qmd/manager_3_28_x.qmd $$PWD/qmd/manager_3_29_x.qmd $$PWD/qmd/manager_3_29_orientation_sensor.qmd $$XOVI_REPO/util/xovigen.py
QMAKE_EXTRA_TARGETS += xoviextension
PRE_TARGETDEPS += $$OUT_PWD/xovi.cpp
QMAKE_LFLAGS += -Wl,-Bsymbolic

XOVI_TRANSLATION_ID = xovi-extension-manager-ui
XOVI_TRANSLATION_DIR = $$PWD/translations
RESOURCES += translations/catalogs.qrc
HEADERS += src/ManagerI18n.h
