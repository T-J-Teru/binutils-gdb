# Copyright (C) 2021 Free Software Foundation, Inc.

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

import re
import gdb
import gdb.xmethod

def _iterator (pointer, len):
    start = pointer
    end = pointer + len
    while pointer != end:
        yield ('[%d]' % int (pointer - start), pointer.dereference())
        pointer += 1

class pp_generic_array:
    def __init__ (self, type, val):
        self.val = val
        self.type = type

    def to_string (self):
        name = self.val['m_name'].string ()
        return ('%s array \"%s\" with %d elements'
                % (self.type, name, self.val['m_nitems']))

    def children(self):
        return _iterator(self.val['m_items'], self.val['m_nitems'])

    def display_hint (self):
        return 'array'

class pp_generic_map:
    def __init__ (self, key_type, value_type, val):
        self.val = val
        self.key_type = key_type
        self.value_type = value_type

    def to_string (self):
        name = self.val['m_name'].string ()
        return ('%s -> %s map \"%s\" with %d elements'
                % (self.key_type, self.value_type, name,
                   self.val['m_nitems']))

    def children(self):
        start = self.val['m_items']
        end = start + self.val['m_nitems']
        pointer = start
        while pointer != end:
            obj = pointer.dereference ()
            yield ('[%d]' % int (pointer - start), obj['key'])
            yield ('[%d]' % int (pointer - start), obj['value'])
            pointer += 1

    def display_hint (self):
        return 'map'

class pretty_printer_finder:
    def __init__ (self):
        self.dict = {}
        self.dict[re.compile ('^int_array$')] \
            = lambda v : pp_generic_array ('int', v)
        self.dict[re.compile ('^float_array$')] \
            = lambda v : pp_generic_array ('float', v)
        self.dict[re.compile ('^int_int_map$')] \
            = lambda v : pp_generic_map ('int', 'int', v)
        self.dict[re.compile ('^generic_array<int>$')] \
            = lambda v : pp_generic_array ('int', v)
        self.dict[re.compile ('^generic_array<float>$')] \
            = lambda v : pp_generic_array ('float', v)
        self.dict[re.compile ('^generic_map<int, int>$')] \
            = lambda v : pp_generic_map ('int', 'int', v)

    def lookup (self, value):
        type = value.type

        # If it points to a reference, get the reference.
        if type.code == gdb.TYPE_CODE_REF:
            type = type.target ()

        # Get the unqualified type, stripped of typedefs.
        type = type.unqualified ().strip_typedefs ()

        # Get the type name.
        typename = type.tag

        if typename == None:
            return None

        # Iterate over local dictionary of types to determine
        # if a printer is registered for that type.  Return an
        # instantiation of the printer if found.
        for function in self.dict:
            if function.match (typename):
                return self.dict[function] (value)

        # Cannot find a pretty printer.  Return None.
        print ("No PP for %s" % typename)
        return None

pp_finder = pretty_printer_finder ()
gdb.pretty_printers.append (lambda v : pp_finder.lookup (v))

class generic_array_subscript (gdb.xmethod.XMethod):

    class worker (gdb.xmethod.XMethodWorker):
        def get_arg_types (self):
            return [gdb.lookup_type ('int')]

        def get_result_type(self, obj):
            return gdb.lookup_type(self.type)

        def __call__(self, obj, idx):
            idx = int (idx)
            if idx < 0 or idx >= obj['m_nitems']:
                raise gdb.GdbError ("array index %d out of bounds" % idx)
            ptr = obj['m_items'] + idx
            print ("xmethod found it")
            return ptr.dereference ()

        def __init__ (self, type):
            gdb.xmethod.XMethodWorker.__init__ (self)
            self.type = type

    def __init__ (self, type):
        gdb.xmethod.XMethod.__init__ (self, "generic_array<%s>::operator[]"
                                      % type)
        self.type = type

    def get_worker (self, method_name):
        if method_name == 'operator[]':
            return self.worker (self.type)

    def class_tag (self):
        return "generic_array<%s>" % self.type

class generic_array_matcher (gdb.xmethod.XMethodMatcher):
    def __init__ (self):
        gdb.xmethod.XMethodMatcher.__init__ (self, 'generic_array_matcher')
        self.methods = [generic_array_subscript ('int'),
                        generic_array_subscript ('float')]

    def match (self, class_type, method_name):
        if (class_type.tag == 'generic_array<int>'
            or class_type.tag == 'generic_array<float>'):
            workers = []
            for method in self.methods:
                if method.enabled and class_type.tag == method.class_tag ():
                    worker = method.get_worker (method_name)
                    if worker:
                        workers.append (worker)
            return workers
        return None

class generic_map_subscript (gdb.xmethod.XMethod):

    class worker (gdb.xmethod.XMethodWorker):
        def get_arg_types (self):
            return [gdb.lookup_type ('int')]

        def get_result_type(self, obj):
            return gdb.lookup_type(self.type)

        def __call__(self, obj, idx):
            if str (idx.type) != self.k_type:
                raise gdb.GdbError ("invalid key type")
            start = obj['m_items']
            end = start + obj['m_nitems']
            pointer = start
            while pointer != end:
                child = pointer.dereference ()
                if (child['key'] == idx):
                    print ("xmethod found it")
                    return child['value']
                pointer += 1
            raise gdb.GdbError ("invalid map key %s" % idx)

        def __init__ (self, k_type, v_type):
            gdb.xmethod.XMethodWorker.__init__ (self)
            self.k_type = k_type
            self.v_type = v_type

    def __init__ (self, k_type, v_type):
        gdb.xmethod.XMethod.__init__ (self, "generic_map<%s, %s>::operator[]"
                                      % (k_type, v_type))
        self.k_type = k_type
        self.v_type = v_type

    def get_worker (self, method_name):
        if method_name == 'operator[]':
            return self.worker (self.k_type, self.v_type)

    def class_tag (self):
        return "generic_map<%s, %s>" % (self.k_type, self.v_type)

class generic_map_matcher (gdb.xmethod.XMethodMatcher):
    def __init__ (self):
        gdb.xmethod.XMethodMatcher.__init__ (self, 'generic_map_matcher')
        self.methods = [generic_map_subscript ('int', 'int')]

    def match (self, class_type, method_name):
        workers = []
        for method in self.methods:
            if method.enabled and class_type.tag == method.class_tag ():
                worker = method.get_worker (method_name)
                if worker:
                    workers.append (worker)
        return workers

def register_xmethods ():
    gdb.xmethod.register_xmethod_matcher (None, generic_array_matcher ())
    gdb.xmethod.register_xmethod_matcher (None, generic_map_matcher ())
