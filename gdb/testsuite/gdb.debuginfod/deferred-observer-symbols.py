# Copyright 2026 Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import gdb


# Look up a symbol NAME in OBJFILE and check we can get its type.  If
# STATIC_P is true then NAME is a static symbol, otherwise it's a
# non-static global.
#
# Print a status message containing the symbol's type, or an error if
# the symbol cannot be found.
def lookup_symbol_in_objfile(objfile, name, static_p):
    try:
        if static_p:
            sym = objfile.lookup_static_symbol(name)
        else:
            sym = objfile.lookup_global_symbol(name)
        if sym is not None:
            # Try to get the symbol's type - this needs full debug info.
            sym_type = sym.type
            if sym_type is not None:
                print("Python observer: found type {}".format(sym_type))
            else:
                print("Python observer: ERROR - symbol type is None")
        else:
            print("Python observer: ERROR - symbol not found")
    except Exception as e:
        print("Python observer: ERROR - exception: {}".format(e))


# New objfile observer.
def new_objfile_handler(event):
    objfile = event.new_objfile

    # Only interested in the separate debug objfile for libdeferred1.
    if objfile.owner is None:
        return
    owner_filename = objfile.owner.filename
    if owner_filename is None or "libdeferred1.so" not in owner_filename:
        return

    print("Python observer: checking symbols in {}".format(objfile.filename))

    lookup_symbol_in_objfile(objfile, "libdeferred1_test", False)
    lookup_symbol_in_objfile(objfile, "flag", True)


gdb.events.new_objfile.connect(new_objfile_handler)
print("Python observer registered")
