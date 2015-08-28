#! /usr/bin/env bash
$EXTRACTRC `find . -name \*.ui` >> rc.cpp
$XGETTEXT `find . -name \*.h -o -name \*.cpp | grep -v '/tests/'` -o $podir/akonadi_ldap_resource.pot
rm -f rc.cpp
