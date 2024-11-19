import gdb

# This setting was added to Fedora GDB back in 2007.  It controlled
# whether GDB would use the build-id extracted from a core file to
# auto-load the executable if the user had not already loaded an
# executable.
#
# In 2020 this setting was effectively deprecated as the only use of
# the setting's value was removed.  After this GDB would always
# auto-load an executable based on the build-id if no executable was
# already loaded.
#
# For now we maintain this setting for backward compatibility reasons.

class build_id_core_load(gdb.Parameter):
    """This setting is deprecated.  Changing it will have no effect.
    This is maintained only for backwards compatibility.

    When opening a core-file, and no executable is loaded, GDB will
    always try to auto-load a suitable executable using the build-id
    extracted from the core file (if a suitable build-id can be
    found)."""

    def __init__(self):
        self.set_doc = "This setting is deprecated and has no effect."
        self.show_doc = "This setting is deprecated and has no effect."
        self.value = True

        super().__init__("build-id-core-load", gdb.COMMAND_DATA, gdb.PARAM_BOOLEAN)
    def validate(self):
        return True
    def get_set_string(self):
        raise gdb.GdbError("The 'build-id-core-load' setting is deprecated.")

build_id_core_load()
