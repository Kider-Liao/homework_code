# Copyright (c) 2006, 2021, Oracle and/or its affiliates.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

SET(HNUSQL_PLUGIN_STATIC  "hnusql")

SET(HNUSQL_PLUGIN_DYNAMIC "ha_hnu")

SET(HNUSQL_SOURCES  ha_hnu.cc ha_hnu.h transparent_file1.cc transparent_file1.h)


MYSQL_ADD_PLUGIN(hnusql ${HNUSQL_SOURCES} STORAGE_ENGINE MANDATORY RECOMPILE_FOR_EMBEDDED)
