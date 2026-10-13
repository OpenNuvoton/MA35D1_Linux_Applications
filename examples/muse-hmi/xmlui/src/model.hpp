// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <QDomDocument>
#include <QMap>
#include <QString>
#include "vendor/json.hpp"
using J=nlohmann::json;
struct Screen {
 QString id,title,question;
 QDomDocument xml;
 J defaults=J::object(),fields=J::object();
};
class Catalog {
 QMap<QString,QDomElement> components;
 QDomDocument library;
 QDomElement expand(QDomDocument&,const QDomElement&,const J&,const QString&,int) const;
public:
 QMap<QString,Screen> screens;
 explicit Catalog(const QString& directory);
 J describe() const;
 J bind(const QString&,const J&) const;
 QDomDocument instantiate(const QString&,const J&) const;
 static QDomDocument layer(const QDomDocument&,const QString&);
};
