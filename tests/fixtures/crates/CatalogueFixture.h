#pragma once
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

// Never included in the ordinary application build or the shipped catalogue.
inline QByteArray cratesCatalogueFixture() {
    QJsonArray projects,ids;
    const QStringList titles{"Preview · Night courier","Preview · Coastal rally","Preview · Star notes",
        "Preview · Forest path","Preview · Pixel workshop","Preview · Frame tools",
        "Preview · A deliberately long project title to check safe clipping", "Preview · No artwork"};
    const QStringList art{"castlevania","sonic","final-fantasy","zelda","mario","metroid","kirby"};
    for(int i=0;i<titles.size();++i) {
        const QString id="preview-"+QString::number(i);ids<<id;
        const QString image="qrc:/series/"+art.value(i)+".png";
        projects<<QJsonObject{{"id",id},{"title",titles[i]},{"creator","UI test data"},
            {"kind",i==5?"module":i==2||i==4?"app":"game"},
            {"description","Presentation test only. These projects are not published or available to install. The artwork is an original environment from the existing TrainerOS asset pack.\n\nThis paragraph checks readable descriptions and scrolling while the selected image remains visible.\n\nNo rating or installed status is invented."},
            {"hero",i==7?"":image},
            {"screenshots",i==7?QJsonArray{}:QJsonArray{image,"qrc:/series/multiverse.png","qrc:/series/final-fantasy.png"}},
            {"releases",QJsonArray{
                QJsonObject{{"version","1.2-preview"},{"channel","Beta"},{"architecture","ARM64"},{"compatibility",i==5?"incompatible":i==2?"supported":"untested"},{"compatibilityNote","Example compatibility statement for layout validation only."},{"sizeBytes",88080384},{"releasedOn","2026-10-18"},{"notes","Preview release metadata. No downloadable package."}},
                QJsonObject{{"version","1.0-preview"},{"channel","Experimental"},{"architecture","ARM64"},{"compatibility","untested"},{"releasedOn","2026-10-01"},{"notes","Earlier preview. Selecting this entry changes displayed information only."}}
            }}};
    }
    const QJsonObject contents{
        {"workshop",ids}, {"whats-new",QJsonArray{"preview-0","preview-5"}},
        {"adventure",QJsonArray{"preview-0","preview-3"}}, {"tools",QJsonArray{"preview-5"}}
    };
    return QJsonDocument(QJsonObject{{"schema",1},{"projects",projects},{"contents",contents}}).toJson();
}
