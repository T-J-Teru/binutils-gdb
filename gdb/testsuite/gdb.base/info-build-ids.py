# Copyright (C) 2025 Free Software Foundation, Inc.
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


class ShowBuildIds(gdb.Command):
    def __init__(self):
        gdb.Command.__init__(self, "info py-build-ids", gdb.COMMAND_DATA)

    def invoke(self, args, from_tty):
        list = gdb.selected_inferior().build_ids()

        if len(list) == 0:
            print("There are no files with a build-id.")
            return

        longest_build_id = 0
        for id_and_filename in list:
            if len(id_and_filename[1]) > longest_build_id:
                longest_build_id = len(id_and_filename[1])

        print(f"%-{longest_build_id}s File" % "Build Id")
        for id_and_filename in list:
            print(f"{id_and_filename[1]:{longest_build_id}s} {id_and_filename[0]}")


ShowBuildIds()
