#pragma once
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDynamicPropertyChangeEvent>
#include <QElapsedTimer>
#include <QQmlEngine>
#include <QPointer>
#include <QTimer>
#include <QTranslator>
#include <functional>

namespace ManagerI18n {
inline QString currentLanguage() {
    return QCoreApplication::instance()->property("xoviNativeUiLanguage").toString();
}
class LanguageObserver : public QObject {
public:
    LanguageObserver(QObject *parent, std::function<void()> update)
        : QObject(parent), update_(std::move(update)) {}
    bool eventFilter(QObject *, QEvent *event) override {
        if (event->type() == QEvent::DynamicPropertyChange
            && static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName() == "xoviNativeUiLanguage")
            update_();
        return false;
    }
private:
    std::function<void()> update_;
};
class Catalog : public QObject {
public:
    Catalog() : QObject(QCoreApplication::instance()) {
        QCoreApplication::instance()->installEventFilter(new LanguageObserver(this, [this] { synchronize(); }));
        synchronize();
    }
    void synchronize() {
        const auto code = currentLanguage();
        if (code == language_) return;
        language_ = code;
        if (translator_) {
            QCoreApplication::removeTranslator(translator_);
            delete translator_;
        }
        translator_ = new QTranslator(this);
        if (translator_->load(":/xovi/i18n/xovi-extension-manager-ui/xovi-extension-manager-ui_" + code + ".qm"))
            QCoreApplication::installTranslator(translator_);
        qInfo().noquote() << "[manager-i18n] retranslate scheduled engines=" << engines_.size()
                          << "epoch-ms=" << QDateTime::currentMSecsSinceEpoch();
        int engineIndex = 0;
        for (auto engine : engines_) {
            const auto index = engineIndex++;
            if (engine) QTimer::singleShot(0, engine, [engine, index] {
                if (!engine) return;
                QElapsedTimer elapsed;
                elapsed.start();
                qInfo().noquote() << "[manager-i18n] retranslate enter engine=" << index
                                  << "epoch-ms=" << QDateTime::currentMSecsSinceEpoch();
                engine->retranslate();
                qInfo().noquote() << "[manager-i18n] retranslate exit engine=" << index
                                  << "elapsed-ms=" << elapsed.elapsed()
                                  << "epoch-ms=" << QDateTime::currentMSecsSinceEpoch();
            });
        }
    }
    void attach(QQmlEngine *engine) {
        if (engine && !engines_.contains(engine)) engines_.append(engine);
    }
    QString translate(const QString &context, const QString &source) {
        synchronize();
        const auto text = translator_ ? translator_->translate(context.toUtf8().constData(), source.toUtf8().constData()) : QString();
        return text.isEmpty() ? source : text;
    }
private:
    QString language_;
    QTranslator *translator_ = nullptr;
    QList<QPointer<QQmlEngine>> engines_;
};
inline Catalog *service() { static auto *catalog = new Catalog; return catalog; }
inline void attach(QQmlEngine *engine) { service()->attach(engine); }
}
