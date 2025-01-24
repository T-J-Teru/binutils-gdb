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
# debug objfile for lib-deferred-frame-cache.so is loaded.  This
# simulates an observer that needs to reset frame-related state when
# new debug info arrives.

import gdb

# Guard to only invalidate once.
_already_invalidated = False

def new_objfile_handler(event):
    """Called when a new objfile is loaded.

    When the separate debug objfile for our library is loaded,
    invalidate the frame cache.  This happens during the crash
    handling when GDB is trying to establish the current frame.
    If lookup_selected_frame doesn't handle this correctly, GDB
    will hit an assertion failure.
    """
    global _already_invalidated

    if _already_invalidated:
        return

    objfile = event.new_objfile

    # Only interested in the separate debug objfile (has an owner).
    if objfile.owner is None:
        return

    owner_filename = objfile.owner.filename
    if owner_filename is None or "lib-deferred-frame-cache.so" not in owner_filename:
        return

    _already_invalidated = True

    print("Python new_objfile handler: invalidating frame cache")

    # This calls reinit_frame_cache() which clears selected_frame.
    # We're inside the crash handling call chain where GDB is trying
    # to establish the current frame.  The call sequence is:
    #   handle_inferior_event -> ... -> get_selected_frame()
    #   -> lookup_selected_frame() -> select_frame()
    #   -> find_compunit_symtab_for_pc() -> download -> here
    #
    # After this, selected_frame is nullptr.  Without the fix in
    # lookup_selected_frame, get_selected_frame would hit an assertion.
    gdb.invalidate_cached_frames()

    print("Python new_objfile handler: frame cache invalidated")

gdb.events.new_objfile.connect(new_objfile_handler)
print("Python registered new_objfile handler")
