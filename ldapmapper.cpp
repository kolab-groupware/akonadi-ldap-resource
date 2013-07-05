/*
 * Copyright (C) 2013  Christian Mollekopf <mollekopf@kolabsys.com>
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
#include "ldapmapper.h"

QStringList LDAPMapper::requestedAttributes()
{
    QStringList requestedAttributes;
    requestedAttributes << "dn" << "uid" << "cn" << "givenName" << "sn" << "mail" << "alias" << "displayName" << "nsuniqueid" << "modifyTimestamp";
    return requestedAttributes;
}


KABC::Addressee LDAPMapper::getAddressee(const KLDAP::LdapObject& obj)
{
    KABC::Addressee addressee;
    addressee.setUid(obj.value("nsuniqueid"));
    addressee.setName(obj.value("cn"));
    addressee.setGivenName(obj.value("givenName"));
    addressee.setFamilyName(obj.value("sn"));
    addressee.setFormattedName(obj.value("displayName"));
    QStringList email(obj.value("mail"));
    foreach(const QByteArray &e, obj.values("alias")) {
        email << e;
    }
    addressee.setEmails(email);
    return addressee;
}
