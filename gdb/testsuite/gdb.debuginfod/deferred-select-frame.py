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

# Python script that invalidates the frame cache when the separate
# debug objfile for lib-deferred-select-frame.so is loaded.
#
# This simulates an observer that resets frame-related state when new
# debug info arrives.  The invalidation happens during select_frame's
# call to find_compunit_symtab_for_pc, after selected_frame has
# already been set.

import gdb

def new_objfile_handler(event):
    objfile = event.new_objfile

    if objfile.owner is None:
        return

    owner_filename = objfile.owner.filename
    if owner_filename is None:
        return

    if ("lib1-deferred-select-frame.so" not in owner_filename
        and "lib2-deferred-select-frame.so" not in owner_filename):
        return

    gdb.invalidate_cached_frames()
    print("Python new_objfile handler: frame cache invalidated")

gdb.events.new_objfile.connect(new_objfile_handler)
print("Python registered new_objfile handler")
