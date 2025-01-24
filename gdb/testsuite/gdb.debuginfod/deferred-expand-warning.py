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

# This script registers a new_objfile event handler that triggers
# symtab expansion by looking up a symbol.  This is used to test
# that on-demand debuginfo downloading correctly handles the case
# where an observer expands symtabs in the newly-downloaded objfile.

import gdb


def new_objfile_handler(event):
    """Handler for new_objfile events.

    When a new objfile is loaded, check if it's the separate debug
    objfile for our test library.  If so, look up a symbol to trigger
    symtab expansion.  See the .exp file for why we do this.
    """
    objfile = event.new_objfile

    # Only trigger for separate debug objfiles (those with an owner).
    if objfile.owner is None:
        return

    # Check if this is the separate debug objfile for our test library.
    # The owner's filename should contain our library name.
    owner_filename = objfile.owner.filename
    if owner_filename is None:
        return

    if "libdeferred-expand-warning" not in owner_filename:
        return

    print(
        "Python observer: triggering symtab expansion for {}".format(objfile.filename)
    )

    # Look up a global symbol from this objfile to trigger symtab
    # expansion.
    sym = gdb.lookup_global_symbol("libdeferred_expand_warning_func")
    if sym is not None:
        print("Python observer: found symbol {}".format(sym.name))
    else:
        raise RuntimeError("No symbol found")


# Register the new objfile event handler.
gdb.events.new_objfile.connect(new_objfile_handler)
print("Python observer: registered new_objfile handler")
