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


#ifndef LDAPMAPPER_H
#define LDAPMAPPER_H
#include <kldap/ldapobject.h>
#include <KABC/Addressee>

class LDAPMapper
{
public:
    static QStringList requestedFullPayloadAttributes();
    static QStringList requestedLookupPayloadAttributes();
    static KABC::Addressee getAddressee(const KLDAP::LdapObject &obj);
    static QString getStableIdentifier(const KLDAP::LdapObject &obj);
    static QString getTimestamp(const KLDAP::LdapObject &obj);
    enum Attribute {
        UniqueIdentifier
    };
    static QString getAttribute(Attribute attr);
};

#endif
