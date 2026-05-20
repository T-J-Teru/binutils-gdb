# Copyright (C) 2026 Free Software Foundation, Inc.
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
from gdb.unwinder import Unwinder

test_debugging = False


def debug_log(msg):
    global test_debugging
    if not test_debugging:
        return
    print(msg)


flush_cache_at_levels = {}


class cache_flush_unwinder(Unwinder):
    def __init__(self):
        super().__init__("cache_flush_unwinder")

    def __call__(self, pending_frame):
        global flush_cache_at_levels

        level = pending_frame.level()
        debug_log("Cache flushing unwinder at level %d" % (level))

        if level in flush_cache_at_levels:
            if flush_cache_at_levels[level] > 0:
                debug_log(" '-> Flushing the frame cache")
                gdb.invalidate_cached_frames()
                flush_cache_at_levels[level] -= 1

        # This unwinder never claims any frames.
        return None


gdb.unwinder.register_unwinder(None, cache_flush_unwinder(), True)
