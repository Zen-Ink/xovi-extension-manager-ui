#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QFont>
#include <QColor>
#include <QQuickWindow>
#include <QUrl>
#include <cstdio>
#include <cstdlib>

static int checkFonts(QQuickItem *item) {
    int count=0;
    if (item->metaObject()->indexOfProperty("lineHeight")>=0 && !item->property("text").toString().isEmpty()) {
        const auto font=item->property("font").value<QFont>();
        if (font.pixelSize()!=36 || font.family()!="reMarkable Sans") {
            std::fprintf(stderr,"Unexpected font on %s: %d, %s\n", qPrintable(item->property("text").toString()), font.pixelSize(), qPrintable(font.family()));
            std::exit(1);
        }
        ++count;
    }
    for(auto *child:item->childItems()) count+=checkFonts(child);
    return count;
}
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    QQmlEngine engine;
    engine.addImportPath(QString::fromLocal8Bit(argv[1]));
    bool warnings=false;
    QObject::connect(&engine,&QQmlEngine::warnings,[&](const QList<QQmlError> &errors){ warnings=true; for(const auto &e:errors) std::fprintf(stderr,"%s\n",qPrintable(e.toString())); });
    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick
import org.xovi.Controls 1.0
Column {
    width: 700
    SettingsBody { text: "Body" }
    SettingsLabel { text: "Label" }
    SettingsTitle { text: "Title" }
    ELabel { text: "Plain" }
    EButton { objectName: "initialCheckedButton"; checked: true; iconName: "power" }
    EButton { objectName: "stateButton"; text: "Button"; iconName: "check" }
    SettingsPanel { width: 700; label: "Panel label"; description: "Panel description" }
    SettingsSidebarItem {
        id: sidebar
        objectName: "typographySidebar"
        width: 700; text: "Sidebar"
        Component.onCompleted: if ("description" in sidebar) sidebar.description = "Sidebar description"
    }
})",QUrl("qrc:/typography-test.qml"));
    QScopedPointer<QObject> object(component.create());
    if(!object){std::fprintf(stderr,"%s\n",qPrintable(component.errorString()));return 1;}
    QCoreApplication::processEvents();
    const int count=checkFonts(qobject_cast<QQuickItem *>(object.data()));
    const auto sidebar=object->findChild<QObject *>("typographySidebar");
    const int expected=sidebar->property("description").isValid() ? 9 : 8;
    if(warnings || count<expected) { std::fprintf(stderr,"labels=%d warnings=%d\n",count,warnings); return 1; }
    sidebar->setProperty("iconSource",QStringLiteral("qrc:/ark/icons/cog"));
    for(bool selected:{true,false}) {
        sidebar->setProperty("highlighted",selected);
        QCoreApplication::processEvents();
        bool found=false;
        for(auto *child:sidebar->findChildren<QObject *>()) {
            if(child->property("source").toString()=="qrc:/ark/icons/cog") {
                found=true;
                if(child->property("color").value<QColor>()!=QColor(selected ? "white" : "black")) return 1;
            }
        }
        if(!found) return 1;
    }
    std::puts("PASS native sidebar icon reverses with selection and restores without custom content");
    auto *initial=object->findChild<QObject *>("initialCheckedButton");
    auto *initialIcon=initial->findChild<QObject *>("buttonIcon");
    if (!initialIcon || initialIcon->property("color").value<QColor>()!=QColor("white")
        || !initialIcon->property("source").toString().endsWith("power.svg")) return 1;
    std::puts("PASS initially selected button has its icon before any interaction");
    auto *button=object->findChild<QObject *>("stateButton");
    auto *icon=button->findChild<QObject *>("buttonIcon");
    auto *label=button->findChild<QObject *>("buttonLabel");
    button->setProperty("enabled",false);
    if (!icon || !label || icon->property("color").value<QColor>()!=QColor("#808080")
            || label->property("color").value<QColor>()!=QColor("#808080")) return 1;
    if (label->property("font").value<QFont>().underline()) return 1;
    button->setProperty("enabled",true);
    if (icon->property("color").value<QColor>()!=QColor("black")) return 1;
    button->setProperty("checked",true);
    if (icon->property("color").value<QColor>()!=QColor("white")) return 1;
    button->setProperty("enabled",false);
    if (icon->property("color").value<QColor>()!=QColor("#808080")) return 1;
    std::puts("PASS disabled icon and text use color and restore when enabled");
    QQmlComponent fixture(&engine);
    fixture.setData(R"(import QtQml
QtObject {
    property var enabledRimeSchemas: [{id:"a",name:"First input method"},{id:"b",name:"Second input method"}]
    property var availableRimeSchemas: [{id:"a",name:"First input method"},{id:"b",name:"Second input method"},{id:"c",name:"Third input method"}]
    property bool rimePendingChanges: false
    property string rimeDeploymentStatus: "idle"
    property string target: ""
    function refreshRimeConfiguration() {}
    function openSettings(owner,page) { target=owner+"/"+page; return {accepted:true} }
})",QUrl("qrc:/rime-fixture.qml"));
    QScopedPointer<QObject> settings(fixture.create());
    QQmlComponent rimeComponent(&engine,QUrl::fromLocalFile(QString::fromLocal8Bit(argv[2])+"/advanced_settings/resources/qml/tabs/RimeTab.qml"));
    QScopedPointer<QObject> rime(rimeComponent.createWithInitialProperties({{"settingsManager",QVariant::fromValue(settings.data())},{"settingsContext",QVariant::fromValue(settings.data())}}));
    if(!rime) { std::fprintf(stderr,"%s\n",qPrintable(rimeComponent.errorString()));return 1; }
    auto *rimeItem=qobject_cast<QQuickItem *>(rime.data());
    QQuickWindow window;window.show();rimeItem->setParentItem(window.contentItem());
    for(int width:{400,700,1200}) {
        window.resize(width,1600);rimeItem->setWidth(width);rimeItem->setHeight(rimeItem->implicitHeight());
        window.grabWindow();
        if(checkFonts(rimeItem)<8 || warnings) return 1;
        auto *open=rime->findChild<QQuickItem *>("openKeyboardSettings");
        if(!open || open->width()>width || open->width()<80) return 1;
        QMetaObject::invokeMethod(open,"clicked");
        if(settings->property("target")!="keyboardcjk/main") return 1;
    }
    rimeItem->setParentItem(nullptr);
    std::puts("PASS IM page uses shared typography at narrow/wide widths and opens Keyboard CJK");
    std::printf("PASS %d labels including native panel/sidebar children use shared font\n",count);
}
