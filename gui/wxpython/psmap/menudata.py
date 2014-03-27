"""!
@package ps.menudata

@brief wxPsMap - menu entries

Classes:
 - menudata::PsMapData

(C) 2011 by the GRASS Development Team

This program is free software under the GNU General Public License
(>=v2). Read the file COPYING that comes with GRASS for details.

@author Anna Kratochvilova <kratochanna gmail.com>
"""

import os

from core                 import globalvar
from core.menudata        import MenuData

class PsMapData(MenuData):
    def __init__(self, path = None):
        """!Menu for Cartographic Composer (psmap.py)
        
        @path path to XML to be read (None for menudata_psmap.xml)
        """
        if not path:
            gisbase = os.getenv('GISBASE')
            global etcwxdir
        path = os.path.join(globalvar.ETCWXDIR, 'xml', 'menudata_psmap.xml')
        
        MenuData.__init__(self, path)
