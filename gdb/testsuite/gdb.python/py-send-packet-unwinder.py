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


# Count the number of times TestUnwinder.__call__ is called.
TestUnwinder_Call_Count = 0


# A frame unwinder that never claims any frame, but does send a packet
# when sniffing the frame.  As the packet is only sent to the
# currently selected inferior then we never need to switch thread to
# send the packet, and so we should never need to flush the frame
# cache.
class TestUnwinder(Unwinder):
    def __init__(self):
        Unwinder.__init__(self, "send packet unwinder")

    call_count = 0

    def __call__(self, pending_frame):
        TestUnwinder.call_count += 1

        gdb.selected_inferior().connection.send_packet("vMustReplyEmpty")

        # This unwinder never claims any frames.
        return None


gdb.unwinder.register_unwinder(None, TestUnwinder(), True)

print("Sourcing complete.")
