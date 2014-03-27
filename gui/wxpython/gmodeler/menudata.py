"""!
@package gmodeler.menudata

@brief wxGUI Graphical Modeler - menu data

Classes:
 - menudata::ModelerData

(C) 2010-2011 by the GRASS Development Team

This program is free software under the GNU General Public License
(>=v2). Read the file COPYING that comes with GRASS for details.

@author Martin Landa <landa.martin gmail.com>
"""

import os

from core                 import globalvar
from core.menudata        import MenuData

class ModelerData(MenuData):
    def __init__(self, filename = None):
        if not filename:
            gisbase = os.getenv('GISBASE')
	    filename = os.path.join(globalvar.ETCWXDIR, 'xml', 'menudata_modeler.xml')
        
        MenuData.__init__(self, filename)
