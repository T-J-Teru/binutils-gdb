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
import gdb.missing_objfile

# These modules are all system modules, and should be available on any
# correctly setup Python install.
import sys
import os
import subprocess
import re
from threading import Thread
import time

try:
    import rpm
except ModuleNotFoundError:
    # The "RPM suggestions" mechanism will not work without the (python)
    # rpm module.  Inform the user of this, but wait to do so until
    # just prior to printing the GDB prompt.  If we do it right away,
    # the message typically appears before the version and copyright
    # info, which is easily missed by many users.  Additionally, it
    # seems that several other packages which parse GDB version info
    # are confused by an early error message regarding a missing
    # python3-rpm package, so waiting to print the error allows those
    # applications to work as they used to.
    def before_prompt():
        print(
            ("\nUnable to load the Python 'rpm' module.  Lack of this module disables\n"
             "the RPM suggestions mechanism which recommends shell commands for\n"
             "installing missing debuginfo packages.  To enable this functionality,\n"
             "please install the python3-rpm package."),
            file=sys.stderr
        )
        gdb.events.before_prompt.disconnect(before_prompt)

    gdb.events.before_prompt.connect(before_prompt)

    # Implement 'info rpm-suggestions' when the 'rpm' module is not
    # available.  Just throws an error.
    def info_rpm_suggestions():
        raise gdb.GdbError("rpm-suggestions are disabled as the Python 'rpm' module is not installed")
else:
    # Track all the RPMs suggested during a single debug session so we
    # don't suggest the same RPM twice.  This is only cleared when the
    # main executable is changed.
    __missing_rpms = {}

    # Track any missing RPMs that have been discovered since the last time
    # the prompt was displayed.  RPMs in here are also present in the
    # __MISSING_RPMS dictionary, but this dictionary is cleared each time
    # the prompt is shown.
    __suggest_rpms = {}

    # Track all the build-ids suggested during a single debug session so we
    # don't suggest installing using the same build-id twice.  This is only
    # cleared when the main executable is changed.
    __missing_build_ids = {}

    # Track any build-ids that have been discovered missing since the last
    # time the prompt was displayed.  Build-ids in here are also present in
    # the __MISSING_BUILD_IDS dictionary, but this dictionary is cleared
    # each time the prompt is shown.
    __suggest_build_ids = {}

    # The build-id to RPM lookup is very slow.  This cache maps
    # build-ids to the set of RPM we can suggest installing.  The key
    # is the build-id string, and the value is a list of RPM names, or
    # None if there was an error with the build-id to RPM lookup.
    #
    # This cache is never cleared, even when the executable is
    # changed.  The build-ids should be unique, so a build-id lookup
    # should be good for the lifetime of the session.
    __build_id_lookup_cache = {}

    # When searching for an RPM given a build-id, if the search takes
    # too long, then a message is printed to the user.  We only print
    # the message once between each GDB prompt.  This flag is set True
    # when the message is printed, and reset to False when a prompt is
    # displayed.
    __searching_message_printed = False

    # Add a suggestion to install RPM_NAME (the full name of an RPM)
    # to the list of suggestions to make the next time a prompt is
    # displayed.
    def add_rpm_suggestion(rpm_name):
        global __missing_rpms
        global __suggest_rpms

        if not rpm_name in __missing_rpms:
            __suggest_rpms[rpm_name] = True
            __missing_rpms[rpm_name] = True

    # Return True if RPM_NAME is installed, where RPM_NAME is the full
    # name of an RPM.
    def is_rpm_installed(ts, rpm_name):
        res = ts.dbMatch(rpm.RPMDBI_LABEL, rpm_name)
        return len(res) > 0

    # Add a suggestion to install RPMs based on BUILD_ID, a string
    # containing a build-id, to the list of suggestions to make the next
    # time a prompt is displayed.
    def add_build_id_suggestion(build_id):
        global __missing_build_ids
        global __suggest_build_ids

        if not build_id in __missing_build_ids:
            __suggest_build_ids[build_id] = True
            __missing_build_ids[build_id] = True

    # Return true if '/usr/lib' is in the debug-file-directory list.
    # System packages install their debug information into /usr/lib,
    # so if GDB isn't looking in that directory, then there's no
    # reason to try and figure out a suitable RPM to install.
    def using_suitable_debug_file_directory():
        debug_file_directories = gdb.parameter("debug-file-directory")
        for d in debug_file_directories.split(os.pathsep):
            if d[:8] == "/usr/lib":
                return True
        return False

    # Return True if rpm-suggestion is disabled for any reason.
    def rpm_suggestion_is_disabled():
        # If /usr/lib/ is not being used to find debug information
        # then there's no point offering any RPMs as GDB would not
        # find the newly installed content.
        if not using_suitable_debug_file_directory():
            return True

        # Is 'rpm-suggestion enabled' set to 'off'?
        if not param_rpm_suggestion_enabled.value:
            return True

        return False


    # Lookup RPMs that might provide the debug information for FILENAME,
    # which is a string containing the path to an object file GDB could
    # not find any debug information for.
    #
    # If a possible RPM is found then this is added to the globals
    # __MISSING_RPMS and __SUGGEST_RPMS, which are used elsewhere in this
    # script.
    def find_debug_suggestions(filename):
        if rpm_suggestion_is_disabled():
            return

        ts = rpm.TransactionSet()
        mi = ts.dbMatch(rpm.RPMDBI_BASENAMES, filename)
        for h in mi:
            # Build the debuginfo package name.
            obj = h.format("%{name}-debuginfo-%{version}-%{release}.%{arch}")
            rpm_name = str(obj)

            # Check to see if the package is installed.
            if is_rpm_installed(ts, rpm_name):
                continue

            add_rpm_suggestion(rpm_name)


    # Return a string which is the filename of the filename
    # corresponding to BUILD_ID in one of the two locations under
    # /usr/lib.
    #
    # When DEBUG_P is True, return a filename within:
    # /usr/lib/debug/.build-id/ and when DEBUG_P is False, return a
    # filename within: /usr/lib/.build-id/.
    #
    # The SUFFIX string is appended to the generated filename.
    def build_id_to_usr_lib_filename(build_id, debug_p, suffix = ""):
        key = build_id[:2]
        rst = build_id[2:]

        filename = '/usr/lib'
        if debug_p:
            filename += '/debug'
        filename += '/.build-id/' + key + '/' + rst + suffix

        return filename

    # A regexp object used to match against the output of `dnf`.  This
    # is initialised the first time it is needed.
    find_objfile_suggestions_re = None

    # Given BUILD_ID, a string containing a build-id, run a `dnf`
    # command to figure out if any RPMs can provide a file with that
    # build-id.
    #
    # If any suitable RPMs are found then `add_rpm_suggestion` is called
    # to register the suggestion.
    #
    # This function is pretty slow, which is a shame, as the results
    # returned are much nicer than just telling the user to try the
    # lookup command for themselves.
    def find_objfile_suggestions_in_thread(build_id):
        global find_objfile_suggestions_re

        if find_objfile_suggestions_re is None:
            find_objfile_suggestions_re = re.compile("^(.*)-debuginfo-(.*) : Debug information for package (.*)$")

        result = subprocess.run(['dnf', "--enablerepo=*debug*", '--nogpgcheck', '-C', 'provides',
                                 build_id_to_usr_lib_filename(build_id, True)],
                                capture_output=True, timeout=30)

        lines = result.stdout.decode('utf-8').splitlines()
        ts = rpm.TransactionSet()

        for l in lines:
            m = find_objfile_suggestions_re.match(l)
            if not m:
                continue
            if m.group(1) != m.group(3):
                return

            main_rpm = m.group(1) + '-' + m.group(2)
            dbg_rpm = m.group(1) + '-debuginfo-' + m.group(2)

            if not is_rpm_installed(ts, main_rpm):
                add_rpm_suggestion(main_rpm)

            if not is_rpm_installed(ts, dbg_rpm):
                add_rpm_suggestion(dbg_rpm)

        return

    # Call `find_objfile_suggestions_in_thread` is a separate thread,
    # then wait for the thread to complete.  Don't touch any shared
    # state while waiting for the thread to complete.  There's no
    # locking on any of our caches, and the worker thread will add RPM
    # suggestions as it wants.
    #
    # If thre thread takes too long to complete then print a message
    # to the user telling them what's going on.
    def find_objfile_suggestions_slow_core(build_id):
        global __searching_message_printed

        thread = Thread(target = find_objfile_suggestions_in_thread, args = (build_id, ))
        thread.start()
        start = time.time_ns()

        while thread.is_alive ():
            time.sleep(0.05)
            now = time.time_ns ()
            if not __searching_message_printed and (now - start > 1000000000):
                print("Searching for packages to install that could improve debugging...")
                __searching_message_printed = True

        thread.join()


    # Given BUILD_ID, a string containing a build-id, lookup suitable
    # RPMs that could be installed to provide a file with the required
    # build-id.
    #
    # Any suitable RPMs are recorded by calling `add_rpm_suggestion`, and
    # will be printed before the next prompt.
    def find_objfile_suggestions_slow(build_id):
        global __build_id_lookup_cache
        global __suggest_rpms

        # The code to lookup an RPM given only a build-id is pretty
        # slow.  Cache the results to try and reduce the UI delays.
        if build_id in __build_id_lookup_cache:
            rpms = __build_id_lookup_cache[build_id]
            if rpms is not None:
                for r in rpms:
                    add_rpm_suggestion(r)
            return

        # Make sure the cache entry exists before we do the lookup.
        # If, for any reason, the lookup raises an exception, then
        # having a cache entry will prevent us retrying this lookup in
        # the future.
        __build_id_lookup_cache[build_id] = None

        # Now do the lookup.  This is the slow part.
        find_objfile_suggestions_slow_core(build_id)

        # Fill in the cache, for a given build-id which RPMs were
        # suggested.
        rpms = []
        for r in __suggest_rpms:
            rpms.append(r)
        __build_id_lookup_cache[build_id] = rpms


    # Given BUILD_ID, a string containing a build-id, just record that we
    # should advise the user to try installing RPMs based on this build-id.
    def find_objfile_suggestions_fast(build_id):
        add_build_id_suggestion(build_id)

    # Given BUILD_ID, a string containing a build-id, which GDB has failed
    # to find, possibly record some information so that we can, at the next
    # prompt, give some RPM installation advice to the user.
    #
    # We have two different strategies for RPM lookup based on a build-id,
    # one approach is that we actually lookup the RPMs and only suggest
    # something if there is a suitable RPM.  However, this is pretty slow,
    # and users will probably find this annoying.
    #
    # So we also have a fast way.  With this approach we just record the
    # build-id that was missing and tell the user to try installing based on
    # the build-id.  The downside with this is that if there is no RPM for
    # that build-id, then the user will try the command, but nothing will
    # install.
    def find_objfile_suggestions(build_id):
        if rpm_suggestion_is_disabled():
            return

        if param_rpm_suggestion_build_id_mode.fast_mode():
            find_objfile_suggestions_fast(build_id)
        else:
            find_objfile_suggestions_slow(build_id)

    # A missing debug handler class.  Just forwards the name of the
    # objfile for which we are missing debug information to
    # find_debug_suggestions.
    class RPM_MissingDebugHandler(gdb.missing_debug.MissingDebugHandler):
        def __init__(self):
            super().__init__("rpm-suggestions")

        def __call__(self, objfile):
            find_debug_suggestions(objfile.filename)
            return False

    # A missing objfile handler class.  Just forwards the build-id of
    # the objfile that is missing to find_objfile_suggestions.
    class RPM_MissingObjfileHandler(gdb.missing_objfile.MissingObjfileHandler):
        def __init__(self):
            super().__init__("rpm-suggestions")

        def __call__(self, pspace, build_id, filename):
            find_objfile_suggestions(build_id)
            return False

    # Take a non-empty list of RPM names and print a command line a
    # user could run to install these RPMs.
    def print_rpm_suggestions(rpm_name_list):
        print("Missing rpms, try: dnf --enablerepo='*debug*' install " + ' '.join(rpm_name_list))

    # Take a non-empty list of build-id strings and print a series of
    # lines that a user could run to instll the RPMs that provide
    # files with this build-id.
    #
    # The printed commands will also install the corresponding debug
    # packages for the executable with the given build-id.
    def print_build_id_suggestions(build_id_list):
        for build_id in build_id_list:
            print("Missing file(s), try: dnf --enablerepo='*debug*' install "
                  + build_id_to_usr_lib_filename(build_id, False)
                  + ' '
                  + build_id_to_usr_lib_filename(build_id, True, ".debug"))

    # Called before GDB displays its prompt.  If the global __SUGGEST_RPMS
    # dictionary is not empty, then this hook prints the keys of this
    # dictionary as strings which are the names of RPMs.  This hook formats
    # each RPM name into a suggested 'dnf install' command and suggests this
    # to the user.
    #
    # Additionally, if the global __SUGGEST_BUILD_IDS dictionary is not
    # empty, then this hook uses the keys of this dictionary as build-ids
    # that were found to be missing, and formats these into some file based
    # 'dnf install' suggestions to the user.
    def before_prompt():
        global __suggest_rpms
        global __suggest_build_ids
        global __searching_message_printed

        # We allow the searching message to be printed just once
        # between prompts.
        __searching_message_printed = False

        if len(__suggest_rpms) > 0:
            print_rpm_suggestions(__suggest_rpms.keys())
            __suggest_rpms = {}

        if len(__suggest_build_ids) > 0:
            print_build_id_suggestions(__suggest_build_ids.keys())
            __suggest_build_ids = {}

    # Called when the executable within a progrm space is changed.  Clear
    # the lists of RPM suggestions.  We only clear the previous suggestion
    # list when the executable really changes.  If the user simply
    # recompiles the executable, then we don't both clearing this list.
    def executable_changed_handler(event):
        global __missing_rpms
        global __suggest_rpms
        global __suggest_build_ids
        global __missing_build_ids

        if not event.reload:
            __missing_rpms = {}
            __suggest_rpms = {}
            __missing_build_ids = {}
            __suggest_build_ids = {}


    # Attach to the required GDB events.
    gdb.events.executable_changed.connect(executable_changed_handler)
    gdb.events.before_prompt.connect(before_prompt)

    # Register the missing debug and missing objfile handlers with GDB.
    gdb.missing_debug.register_handler(None, RPM_MissingDebugHandler())
    gdb.missing_objfile.register_handler(None, RPM_MissingObjfileHandler())

    # Implement the core of 'info rpm-suggestions'.  Reprint all rpm
    # suggestions.
    def info_rpm_suggestions():
        global __missing_rpms
        global __missing_build_ids

        if len(__missing_rpms) == 0 and len(__missing_build_ids) == 0:
            print("No RPM suggestions have been made so far.")
            return

        if len(__missing_rpms) > 0:
            print_rpm_suggestions(__missing_rpms.keys())
        if len(__missing_build_ids) > 0:
            print_build_id_suggestions(__missing_build_ids.keys())

####################################################################
# The following code is outside the 'else' block of the attempt to #
# load the 'rpm' module.  Nothing after this point depends on the  #
# 'rpm' module.                                                    #
####################################################################

# The 'set rpm-suggestion' prefix command.
class rpm_suggestion_set_prefix(gdb.Command):
    """Prefix command for 'set' rpm-suggestion related commands."""

    def __init__(self):
        super().__init__("set rpm-suggestion", gdb.COMMAND_NONE,
                         gdb.COMPLETE_NONE, True)

# The 'show rpm-suggestion' prefix command.
class rpm_suggestion_show_prefix(gdb.Command):
    """Prefix command for 'show' rpm-suggestion related commands."""

    def __init__(self):
        super().__init__("show rpm-suggestion", gdb.COMMAND_NONE,
                         gdb.COMPLETE_NONE, True)

# The 'set/show rpm-suggestion enabled' command.
class rpm_suggestion_enabled(gdb.Parameter):
    """
    When 'on' GDB will search for RPMS that might provide additional
    debug information, or provide missing executables or shared
    libraries (when opening a core file), and will make suggestions
    about what should be installed.

    When 'off' GDB will not make these suggestions.
    """

    set_doc = "Set whether to perform rpm-suggestion."
    show_doc = "Show whether rpm-suggestion is enabled."

    def __init__(self):
        super().__init__("rpm-suggestion enabled", gdb.COMMAND_NONE, gdb.PARAM_BOOLEAN)
        self.value = True

# The 'set/show rpm-suggestion enabled' command.
class rpm_suggestion_build_id_mode(gdb.Parameter):
    """
    When set to 'fast' (the default), GDB doesn't try to map a build-id to
    an actual RPM, instead, GDB just suggests a command based on the
    build-id which might install some RPMs, if there are any RPMs that
    supply that build-id.  However, it is equally possible that there are no
    suitable RPMs, and nothing will install.  This approach has almost zero
    overhead.

    When set to 'slow', GDB first does the build-id to RPM check itself, and
    only if something is found are RPMs installation commands suggested.
    The suggested command will include the name of the RPM to install.  This
    approach is considerably slower as querying the RPM database for the RPM
    that supplies a specific file is slow.
    """

    set_doc = "Set how build-id based rpm suggestions should be performed."
    show_doc = "Show how build-id based rpm suggestions shoud be performed."

    def __init__(self):
        super().__init__("rpm-suggestion build-id-mode",
                         gdb.COMMAND_NONE, gdb.PARAM_ENUM, ["fast", "slow"])
        self.value = "fast"

    def fast_mode(self):
        return self.value == "fast"

# The 'info rpm-suggestions' command.
class rpm_suggestion_info(gdb.Command):
    """Relist any RPM installation suggestions that have been made
    since the executable was last changed."""
    def __init__(self):
        super().__init__("info rpm-suggestions", gdb.COMMAND_NONE, gdb.COMPLETE_NONE)

    def invoke(self, args, from_tty):
        if args != "":
            raise gdb.GdbError("unexpected arguments: %s" % args)

        info_rpm_suggestions ()


# Create the 'set/show rpm-suggestion' commands.
rpm_suggestion_set_prefix()
rpm_suggestion_show_prefix()
param_rpm_suggestion_enabled = rpm_suggestion_enabled()
param_rpm_suggestion_build_id_mode = rpm_suggestion_build_id_mode()

# Create the 'info rpm-suggestions' commands.
rpm_suggestion_info()
