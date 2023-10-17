# Copyright 2023 Free Software Foundation, Inc.

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
import gdb.missing_debug
import rpm

# Track all the RPMs suggested during a single debug session so we
# don't suggest the same RPM twice.  This is only cleared when the
# main executable is changed.
__missing_rpms = {}

# Track any missing RPMs that have been discovered since the last time
# the prompt was displayed.  RPMs in here are also present in the
# __MISSING_RPMS dictionary, but this dictionary is cleared each time
# the prompt is shown.
__suggest_rpms = {}

# Lookup RPMs that might provide the debug information for FILENAME,
# which is a string containing the path to an object file GDB could
# not find any debug information for.
#
# If a possible RPM is found then this is added to the globals
# __MISSING_RPMS and __SUGGEST_RPMS, which are used elsewhere in this
# script.
def find_suggestions(filename):
    ts = rpm.TransactionSet()

    mi = ts.dbMatch(rpm.RPMDBI_BASENAMES, filename)
    for h in mi:
        # Build the debuginfo package name.
        obj = h.format("%{name}-debuginfo-%{version}-%{release}.%{arch}")

        # Check to see if the package is installed.
        mi2 = ts.dbMatch(rpm.RPMDBI_LABEL, str(obj))
        if len(mi2) > 0:
            continue

        # Now build the name of the package FILENAME came from.
        obj = h.format("%{name}-%{version}-%{release}.%{arch}")
        rpm_name = str(obj)
        if not rpm_name in __missing_rpms:
            __suggest_rpms[rpm_name] = True
            __missing_rpms[rpm_name] = True


# A missing debug handler class.  Just forwards the name of the
# objfile for which we are missing debug information to
# find_suggestions.
class RPMSuggestionHandler(gdb.missing_debug.MissingDebugHandler):
    def __init__(self):
        super().__init__("rpm-suggestions")

    def __call__(self, objfile):
        find_suggestions(objfile.filename)
        return False


# Called when GDB encounters an objfile that is missing debug
# information.  The EVENT contains a reference to the objfile in
# question.
def missing_debuginfo(event):
    find_suggestions(event.objfile.filename)


# Called before GDB displays its prompt.  If the global __SUGGEST_RPMS
# dictionary is not empty, then this hook prints treats the keys of
# this dictionary as strings which are the names of RPMs.  This hook
# formats each RPM name into a suggested debuginfo-install command and
# suggests this to the user.
def before_prompt():
    global __suggest_rpms

    if len(__suggest_rpms) > 0:
        for p in __suggest_rpms.keys():
            print("Missing debuginfo, try: debuginfo-install " + p)
        __suggest_rpms = {}


# Called when the executable within a progrm space is changed.  Clear
# the lists of RPM suggestions.  We only clear the previous suggestion
# list when the executable really changes.  If the user simply
# recompiles the executable, then we don't both clearing this list.
def executable_changed_handler(event):
    global __missing_rpms
    global __suggest_rpms

    if not event.reload:
        __missing_rpms = {}
        __suggest_rpms = {}


# Attach to the required GDB events.
gdb.events.executable_changed.connect(executable_changed_handler)
gdb.events.before_prompt.connect(before_prompt)

# Register the missing debug handler with GDB.
gdb.missing_debug.register_handler(None, RPMSuggestionHandler())
