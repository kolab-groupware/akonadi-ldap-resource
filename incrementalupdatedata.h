/*
 * Copyright (C) 2014 Klaralvdalens Datakonsult AB <info@kdab.com>
 *     Author: Kevin Krammer <kevin.krammer@kdab.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef INCREMENTALUPDATEDATA_H
#define INCREMENTALUPDATEDATA_H

#include <QList>
#include <QString>

namespace KLDAP {
    class LdapObject;
}

struct GroupUpdate
{
    GroupUpdate(const KLDAP::LdapObject &obj);

    QString id;
    QString name;
    QString timestamp;
};

typedef QList<GroupUpdate> GroupUpdateList;

#endif // INCREMENTALUPDATEDATA_H
