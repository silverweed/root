# Author: Danilo Piparo, Massimiliano Galli CERN  08/2018
# Author: Vincenzo Eduardo Padulano CERN/UPV 03/2022

################################################################################
# Copyright (C) 1995-2022, Rene Brun and Fons Rademakers.                      #
# All rights reserved.                                                         #
#                                                                              #
# For the licensing terms see $ROOTSYS/LICENSE.                                #
# For the list of contributors see $ROOTSYS/README/CREDITS.                    #
################################################################################

r'''
\pythondoc RFile

TODO: document RFile

\code{.py}
# TODO code example
\endcode

\endpythondoc
'''

from . import pythonization


def _RFile_Get(self, namecycle):
    """
    Allow access to objects through the method Get().
    """

    import cppyy

    key = self.GetKey(namecycle)
    if key:
        class_name = key.GetClassName()
        address = self.GetObjectChecked(namecycle, class_name)
        return cppyy.bind_object(address, class_name)
    # no key? for better or worse, call normal Get()
    return self._Get(namecycle)

def _RFileExit(obj, exc_type, exc_val, exc_tb):
    """
    Close the RFile object.
    Signature and return value are imposed by Python, see
    https://docs.python.org/3/library/stdtypes.html#typecontextmanager.
    """
    obj.Close()
    return False


@pythonization('RFile', ns="ROOT::Experimental")
def pythonize_rfile(klass):
    # Pythonization for __enter__ and __exit__ methods
    # These make RFile usable in a `with` statement as a context manager
    klass.__enter__ = lambda rfile: rfile
    klass.__exit__ = _RFileExit
