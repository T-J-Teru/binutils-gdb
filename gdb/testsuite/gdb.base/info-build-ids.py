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

# Setting ARG_PASSING_MODE controls how Inferior.build_ids() is
# called:
#
#   'named' -- call using a named parameter.
#
#   'default' -- call using the default parameter value, when
#                possible.
#
#   'unnamed' -- call without using parameter name.
arg_passing_mode = "named"


class ShowBuildIds(gdb.Command):
    def __init__(self):
        gdb.Command.__init__(self, "info py-build-ids", gdb.COMMAND_DATA)

    def invoke(self, args, from_tty):
        argv = gdb.string_to_argv(args)

        all_files = False
        if len(argv) > 0 and argv[0] == "-all-files":
            all_files = True
            del argv[0]

        if len(argv) > 0:
            args = " ".join(argv)
            raise gdb.GdbError(f"Junk at end of command: {args}")

        global arg_passing_mode
        if arg_passing_mode == "named":
            list = gdb.selected_inferior().build_ids(all_files=all_files)
        elif arg_passing_mode == "unnamed":
            list = gdb.selected_inferior().build_ids(all_files)
        elif arg_passing_mode == "default":
            if not all_files:
                list = gdb.selected_inferior().build_ids()
            else:
                list = gdb.selected_inferior().build_ids(True)
        else:
            raise gdb.GdbError(f"unknown arg_passing_mode value '{arg_passing_mode}'")

        if len(list) == 0:
            num = gdb.selected_inferior().num
            if all_files:
                print(f"There are no named files in inferior {num}.")
            else:
                print(f"There are no files with a build-id in inferior {num}.")
            return

        longest_build_id = 8
        for id_and_filename in list:
            if (id_and_filename[1] is not None) and (
                len(id_and_filename[1]) > longest_build_id
            ):
                longest_build_id = len(id_and_filename[1])

        print(f"%-{longest_build_id}s File" % "Build Id")
        for id_and_filename in list:
            build_id = ""
            if id_and_filename[1] is not None:
                build_id = id_and_filename[1]
            print(f"{build_id:{longest_build_id}s} {id_and_filename[0]}")


ShowBuildIds()
