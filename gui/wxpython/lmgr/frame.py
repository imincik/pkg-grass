"""!
@package lmgr::frame

@brief Layer Manager - main menu, layer management toolbar, notebook
control for display management and access to command console.

Classes:
 - frame::GMFrame

(C) 2006-2012 by the GRASS Development Team

This program is free software under the GNU General Public License
(>=v2). Read the file COPYING that comes with GRASS for details.

@author Michael Barton (Arizona State University)
@author Jachym Cepicky (Mendel University of Agriculture)
@author Martin Landa <landa.martin gmail.com>
@author Vaclav Petras <wenzeslaus gmail.com> (menu customization)
"""

import sys
import os
import tempfile
import stat
import platform
try:
    import xml.etree.ElementTree as etree
except ImportError:
    import elementtree.ElementTree as etree # Python <= 2.4

from core import globalvar
import wx
import wx.aui
try:
    import wx.lib.agw.flatnotebook   as FN
except ImportError:
    import wx.lib.flatnotebook   as FN

sys.path.append(os.path.join(globalvar.ETCDIR, "python"))
from grass.script          import core as grass

from core.gcmd             import Command, RunCommand, GError, GMessage
from core.settings         import UserSettings
from core.utils            import SetAddOnPath
from gui_core.preferences  import MapsetAccess, PreferencesDialog, EVT_SETTINGS_CHANGED
from lmgr.layertree        import LayerTree, LMIcons
from lmgr.menudata         import ManagerData
from gui_core.widgets      import GNotebook
from modules.mcalc_builder import MapCalcFrame
from dbmgr.manager         import AttributeManager
from core.workspace        import ProcessWorkspaceFile, ProcessGrcFile, WriteWorkspaceFile
from gui_core.goutput      import GMConsole
from gui_core.dialogs      import DxfImportDialog, GdalImportDialog, MapLayersDialog
from gui_core.dialogs      import LocationDialog, MapsetDialog, CreateNewVector, GroupDialog
from modules.ogc_services  import WMSDialog
from modules.colorrules    import RasterColorTable, VectorColorTable
from gui_core.menu         import Menu
from gmodeler.model        import Model
from gmodeler.frame        import ModelFrame
from psmap.frame           import PsMapFrame
from core.debug            import Debug
from gui_core.ghelp        import MenuTreeWindow, AboutWindow
from modules.extensions    import InstallExtensionWindow, UninstallExtensionWindow
from lmgr.toolbars         import LMWorkspaceToolbar, LMDataToolbar, LMToolsToolbar
from lmgr.toolbars         import LMMiscToolbar, LMVectorToolbar, LMNvizToolbar
from lmgr.pyshell          import PyShellWindow
from gui_core.forms        import GUI
from gcp.manager           import GCPWizard
from nviz.main             import haveNviz

class GMFrame(wx.Frame):
    """!Layer Manager frame with notebook widget for controlling GRASS
    GIS. Includes command console page for typing GRASS (and other)
    commands, tree widget page for managing map layers.
    """
    def __init__(self, parent, id = wx.ID_ANY, title = _("GRASS GIS Layer Manager"),
                 workspace = None,
                 size = globalvar.GM_WINDOW_SIZE, style = wx.DEFAULT_FRAME_STYLE, **kwargs):
        self.parent    = parent
        self.baseTitle = title
        self.iconsize  = (16, 16)
        
        wx.Frame.__init__(self, parent = parent, id = id, size = size,
                          style = style, **kwargs)
                          
        self.SetTitle(self.baseTitle)
        self.SetName("LayerManager")

        self.SetIcon(wx.Icon(os.path.join(globalvar.ETCICONDIR, 'grass.ico'), wx.BITMAP_TYPE_ICO))

        self._auimgr = wx.aui.AuiManager(self)

        # initialize variables
        self.disp_idx      = 0            # index value for map displays and layer trees
        self.curr_page     = None         # currently selected page for layer tree notebook
        self.curr_pagenum  = None         # currently selected page number for layer tree notebook
        self.workspaceFile = workspace    # workspace file
        self.workspaceChanged = False     # track changes in workspace
        self.georectifying = None         # reference to GCP class or None
        self.gcpmanagement = None         # reference to GCP class or None
        
        # list of open dialogs
        self.dialogs        = dict()
        self.dialogs['preferences'] = None
        self.dialogs['atm'] = list()
        
        # creating widgets
        self._createMenuBar()
        self.statusbar = self.CreateStatusBar(number = 1)
        self.notebook  = self._createNoteBook()
        self.toolbars  = { 'workspace' : LMWorkspaceToolbar(parent = self),
                           'data'      : LMDataToolbar(parent = self),
                           'tools'     : LMToolsToolbar(parent = self),
                           'misc'      : LMMiscToolbar(parent = self),
                           'vector'    : LMVectorToolbar(parent = self),
                           'nviz'      : LMNvizToolbar(parent = self)}
        
        self._toolbarsData = { 'workspace' : ("toolbarWorkspace",     # name
                                              _("Workspace Toolbar"), # caption
                                              1),                     # row
                               'data'      : ("toolbarData",
                                              _("Data Toolbar"),
                                              1),
                               'misc'      : ("toolbarMisc",
                                              _("Misc Toolbar"),
                                              2),
                               'tools'     : ("toolbarTools",
                                              _("Tools Toolbar"),
                                              2),
                               'vector'    : ("toolbarVector",
                                              _("Vector Toolbar"),
                                              2),
                               'nviz'      : ("toolbarNviz",
                                              _("3D view Toolbar"),
                                              2),                                            
                               }
        if sys.platform == 'win32':
            self._toolbarsList = ('workspace', 'data',
                                  'vector', 'tools', 'misc', 'nviz')
        else:
            self._toolbarsList = ('data', 'workspace',
                                  'nviz', 'misc', 'tools', 'vector')
        for toolbar in self._toolbarsList:
            name, caption, row = self._toolbarsData[toolbar]
            self._auimgr.AddPane(self.toolbars[toolbar],
                                 wx.aui.AuiPaneInfo().
                                 Name(name).Caption(caption).
                                 ToolbarPane().Top().Row(row).
                                 LeftDockable(False).RightDockable(False).
                                 BottomDockable(False).TopDockable(True).
                                 CloseButton(False).Layer(2).
                                 BestSize((self.toolbars[toolbar].GetBestSize())))
        
        self._auimgr.GetPane('toolbarNviz').Hide()
        # bindings
        self.Bind(wx.EVT_CLOSE,    self.OnCloseWindow)
        self.Bind(wx.EVT_KEY_DOWN, self.OnKeyDown)

        # minimal frame size
        self.SetMinSize((500, 400))

        # AUI stuff
        self._auimgr.AddPane(self.notebook, wx.aui.AuiPaneInfo().
                             Left().CentrePane().BestSize((-1,-1)).Dockable(False).
                             CloseButton(False).DestroyOnClose(True).Row(1).Layer(0))

        self._auimgr.Update()

        wx.CallAfter(self.notebook.SetSelectionByName, 'layers')
        
        # use default window layout ?
        if UserSettings.Get(group = 'general', key = 'defWindowPos', subkey = 'enabled'):
            dim = UserSettings.Get(group = 'general', key = 'defWindowPos', subkey = 'dim')
            try:
               x, y = map(int, dim.split(',')[0:2])
               w, h = map(int, dim.split(',')[2:4])
               self.SetPosition((x, y))
               self.SetSize((w, h))
            except:
                pass
        else:
            self.Centre()
        
        self.Layout()
        self.Show()
        
        # load workspace file if requested
        if self.workspaceFile:
            # load given workspace file
            if self.LoadWorkspaceFile(self.workspaceFile):
                self.SetTitle(self.baseTitle + " - " +  os.path.basename(self.workspaceFile))
            else:
                self.workspaceFile = None
        else:
            # start default initial display
            self.NewDisplay(show = False)

        # show map display widnow
        # -> OnSize() -> UpdateMap()
        if self.curr_page and not self.curr_page.maptree.mapdisplay.IsShown():
            self.curr_page.maptree.mapdisplay.Show()
        
        # redirect stderr to log area    
        self.goutput.Redirect()
        
        # fix goutput's pane size (required for Mac OSX)`
        self.goutput.SetSashPosition(int(self.GetSize()[1] * .8))
        
        self.workspaceChanged = False
        
        # start with layer manager on top
        if self.curr_page:
            self.curr_page.maptree.mapdisplay.Raise()
        wx.CallAfter(self.Raise)
        
    def _createMenuBar(self):
        """!Creates menu bar"""
        self.menubar = Menu(parent = self, data = ManagerData())
        self.SetMenuBar(self.menubar)
        self.menucmd = self.menubar.GetCmd()
        
    def _createTabMenu(self):
        """!Creates context menu for display tabs.
        
        Used to rename display.
        """
        menu = wx.Menu()
        item = wx.MenuItem(menu, id = wx.ID_ANY, text = _("Rename Map Display"))
        menu.AppendItem(item)
        self.Bind(wx.EVT_MENU, self.OnRenameDisplay, item)
        
        return menu
        
    def _setCopyingOfSelectedText(self):
        copy = UserSettings.Get(group = 'manager', key = 'copySelectedTextToClipboard', subkey = 'enabled')
        self.goutput.SetCopyingOfSelectedText(copy)
    
    def IsPaneShown(self, name):
        """!Check if pane (toolbar, ...) of given name is currently shown"""
        if self._auimgr.GetPane(name).IsOk():
            return self._auimgr.GetPane(name).IsShown()
        return False
    
    def _createNoteBook(self):
        """!Creates notebook widgets"""
        self.notebook = GNotebook(parent = self, style = globalvar.FNPageDStyle)
        # create displays notebook widget and add it to main notebook page
        cbStyle = globalvar.FNPageStyle
        if globalvar.hasAgw:
            self.gm_cb = FN.FlatNotebook(self, id = wx.ID_ANY, agwStyle = cbStyle)
        else:
            self.gm_cb = FN.FlatNotebook(self, id = wx.ID_ANY, style = cbStyle)
        self.gm_cb.SetTabAreaColour(globalvar.FNPageColor)
        menu = self._createTabMenu()
        self.gm_cb.SetRightClickMenu(menu)
        self.notebook.AddPage(page = self.gm_cb, text = _("Map layers"), name = 'layers')
        
        # create 'command output' text area
        self.goutput = GMConsole(self)
        self.notebook.AddPage(page = self.goutput, text = _("Command console"), name = 'output')
        self._setCopyingOfSelectedText()
        
        # create 'search module' notebook page
        if not UserSettings.Get(group = 'manager', key = 'hideTabs', subkey = 'search'):
            self.search = MenuTreeWindow(parent = self)
            self.notebook.AddPage(page = self.search, text = _("Search module"), name = 'search')
        else:
            self.search = None
        
        # create 'python shell' notebook page
        if not UserSettings.Get(group = 'manager', key = 'hideTabs', subkey = 'pyshell'):
            self.pyshell = PyShellWindow(parent = self)
            self.notebook.AddPage(page = self.pyshell, text = _("Python shell"), name = 'pyshell')
        else:
            self.pyshell = None
        
        # bindings
        self.gm_cb.Bind(FN.EVT_FLATNOTEBOOK_PAGE_CHANGED,    self.OnCBPageChanged)
        self.notebook.Bind(FN.EVT_FLATNOTEBOOK_PAGE_CHANGED, self.OnPageChanged)
        self.gm_cb.Bind(FN.EVT_FLATNOTEBOOK_PAGE_CLOSING,    self.OnCBPageClosed)
        
        return self.notebook
            
    def AddNvizTools(self):
        """!Add nviz notebook page"""
        Debug.msg(5, "GMFrame.AddNvizTools()")
        if not haveNviz:
            return
        
        from nviz.main import NvizToolWindow
        
        # show toolbar
        self._auimgr.GetPane('toolbarNviz').Show()
        # reorder other toolbars
        for pos, toolbar in enumerate(('toolbarVector', 'toolbarTools', 'toolbarMisc','toolbarNviz')):
            self._auimgr.GetPane(toolbar).Row(2).Position(pos)
        self._auimgr.Update()
        
        # create nviz tools tab
        self.nviz = NvizToolWindow(parent = self,
                                   display = self.curr_page.maptree.GetMapDisplay())
        idx = self.notebook.GetPageIndexByName('layers')
        self.notebook.InsertPage(indx = idx + 1, page = self.nviz, text = _("3D view"), name = 'nviz')
        self.notebook.SetSelectionByName('nviz')
        
        
    def RemoveNvizTools(self):
        """!Remove nviz notebook page"""
        # if more mapwindow3D were possible, check here if nb page should be removed
        self.notebook.SetSelectionByName('layers')
        self.notebook.DeletePage(self.notebook.GetPageIndexByName('nviz'))

        # hide toolbar
        self._auimgr.GetPane('toolbarNviz').Hide()
        for pos, toolbar in enumerate(('toolbarVector', 'toolbarTools', 'toolbarMisc')):
            self._auimgr.GetPane(toolbar).Row(2).Position(pos)
        self._auimgr.Update()
    
    def WorkspaceChanged(self):
        """!Update window title"""
        if not self.workspaceChanged:
            self.workspaceChanged = True
        
        if self.workspaceFile:
            self.SetTitle(self.baseTitle + " - " +  os.path.basename(self.workspaceFile) + '*')
        
    def OnLocationWizard(self, event):
        """!Launch location wizard"""
        from location_wizard.wizard import LocationWizard
        from location_wizard.dialogs import RegionDef
        
        gWizard = LocationWizard(parent = self,
                                 grassdatabase = grass.gisenv()['GISDBASE'])
        location = gWizard.location
        
        if location !=  None:
            dlg = wx.MessageDialog(parent = self,
                                   message = _('Location <%s> created.\n\n'
                                               'Do you want to switch to the '
                                               'new location?') % location,
                                   caption=_("Switch to new location?"),
                                   style = wx.YES_NO | wx.NO_DEFAULT |
                                   wx.ICON_QUESTION | wx.CENTRE)
            
            ret = dlg.ShowModal()
            dlg.Destroy()
            if ret == wx.ID_YES:
                if RunCommand('g.mapset', parent = self,
                              location = location,
                              mapset = 'PERMANENT') != 0:
                    return
                
                GMessage(parent = self,
                         message = _("Current location is <%(loc)s>.\n"
                                     "Current mapset is <%(mapset)s>.") % \
                             { 'loc' : location, 'mapset' : 'PERMANENT' })

                # code duplication with gis_set.py
                dlg = wx.MessageDialog(parent = self,
                               message = _("Do you want to set the default "
                                           "region extents and resolution now?"),
                               caption = _("Location <%s> created") % location,
                               style = wx.YES_NO | wx.NO_DEFAULT | wx.ICON_QUESTION)
                dlg.CenterOnScreen()
                if dlg.ShowModal() == wx.ID_YES:
                    dlg.Destroy()
                    defineRegion = RegionDef(self, location = location)
                    defineRegion.CenterOnScreen()
                    defineRegion.ShowModal()
                    defineRegion.Destroy()
                else:
                    dlg.Destroy()
        
    def OnSettingsChanged(self, event):
        """!Here can be functions which have to be called after EVT_SETTINGS_CHANGED. 
        Now only set copying of selected text to clipboard (in goutput).
        """
        ### self._createMenuBar() # bug when menu is re-created on the fly
        self._setCopyingOfSelectedText()
        
    def OnGCPManager(self, event):
        """!Launch georectifier module
        """
        GCPWizard(self)

    def OnGModeler(self, event):
        """!Launch Graphical Modeler"""
        win = ModelFrame(parent = self)
        win.CentreOnScreen()
        
        win.Show()
        
    def OnPsMap(self, event):
        """!Launch Cartographic Composer
        """
        win = PsMapFrame(parent = self)
        win.CentreOnScreen()
        
        win.Show()
        
    def OnDone(self, cmd, returncode):
        """Command execution finised"""
        if hasattr(self, "model"):
            self.model.DeleteIntermediateData(log = self.goutput)
            del self.model
        self.SetStatusText('')
        
    def OnRunModel(self, event):
        """!Run model"""
        filename = ''
        dlg = wx.FileDialog(parent = self, message =_("Choose model to run"),
                            defaultDir = os.getcwd(),
                            wildcard = _("GRASS Model File (*.gxm)|*.gxm"))
        if dlg.ShowModal() == wx.ID_OK:
            filename = dlg.GetPath()
        
        if not filename:
            dlg.Destroy()
            return
        
        self.model = Model()
        self.model.LoadModel(filename)
        self.model.Run(log = self.goutput, onDone = self.OnDone, parent = self)
        
        dlg.Destroy()
        
    def OnMapsets(self, event):
        """!Launch mapset access dialog
        """
        dlg = MapsetAccess(parent = self, id = wx.ID_ANY)
        dlg.CenterOnScreen()
        
        if dlg.ShowModal() == wx.ID_OK:
            ms = dlg.GetMapsets()
            RunCommand('g.mapsets',
                       parent = self,
                       mapset = '%s' % ','.join(ms))
            
    def OnCBPageChanged(self, event):
        """!Page in notebook (display) changed"""
        self.curr_page   = self.gm_cb.GetCurrentPage()
        self.curr_pagenum = self.gm_cb.GetSelection()
        try:
            self.curr_page.maptree.mapdisplay.SetFocus()
            self.curr_page.maptree.mapdisplay.Raise()
        except:
            pass
        
        event.Skip()

    def OnPageChanged(self, event):
        """!Page in notebook changed"""
        page = event.GetSelection()
        if page == self.notebook.GetPageIndexByName('output'):
            # remove '(...)'
            self.notebook.SetPageText(page, _("Command console"))
            wx.CallAfter(self.goutput.ResetFocus)
        self.SetStatusText('', 0)
        
        event.Skip()

    def OnCBPageClosed(self, event):
        """!Page of notebook closed
        Also close associated map display
        """
        if UserSettings.Get(group = 'manager', key = 'askOnQuit', subkey = 'enabled'):
            maptree = self.curr_page.maptree
            
            if self.workspaceFile:
                message = _("Do you want to save changes in the workspace?")
            else:
                message = _("Do you want to store current settings "
                            "to workspace file?")
            
            # ask user to save current settings
            if maptree.GetCount() > 0:
                name = self.gm_cb.GetPageText(self.curr_pagenum)
                dlg = wx.MessageDialog(self,
                                       message = message,
                                       caption = _("Close Map Display %s") % name,
                                       style = wx.YES_NO | wx.YES_DEFAULT |
                                       wx.CANCEL | wx.ICON_QUESTION | wx.CENTRE)
                ret = dlg.ShowModal()
                if ret == wx.ID_YES:
                    if not self.workspaceFile:
                        self.OnWorkspaceSaveAs()
                    else:
                        self.SaveToWorkspaceFile(self.workspaceFile)
                elif ret == wx.ID_CANCEL:
                    event.Veto()
                    dlg.Destroy()
                    return
                dlg.Destroy()

        self.gm_cb.GetPage(event.GetSelection()).maptree.Map.Clean()
        self.gm_cb.GetPage(event.GetSelection()).maptree.Close(True)

        self.curr_page = None

        event.Skip()

    def GetLayerTree(self):
        """!Get current layer tree"""
        if self.curr_page:
            return self.curr_page.maptree
        return None
    
    def GetLogWindow(self):
        """!Get widget for command output"""
        return self.goutput
    
    def GetMenuCmd(self, event):
        """!Get GRASS command from menu item

        Return command as a list"""
        layer = None
        
        if event:
            cmd = self.menucmd[event.GetId()]
        
        try:
            cmdlist = cmd.split(' ')
        except: # already list?
            cmdlist = cmd
        
        # check list of dummy commands for GUI modules that do not have GRASS
        # bin modules or scripts. 
        if cmd in ['vcolors', 'r.mapcalc', 'r3.mapcalc']:
            return cmdlist

        try:
            layer = self.curr_page.maptree.layer_selected
            name = self.curr_page.maptree.GetPyData(layer)[0]['maplayer'].name
            type = self.curr_page.maptree.GetPyData(layer)[0]['type']
        except:
            layer = None

        if layer and len(cmdlist) == 1: # only if no paramaters given
            if (type == 'raster' and cmdlist[0][0] == 'r' and cmdlist[0][1] != '3') or \
                    (type == 'vector' and cmdlist[0][0] == 'v'):
                input = GUI().GetCommandInputMapParamKey(cmdlist[0])
                if input:
                    cmdlist.append("%s=%s" % (input, name))
        
        return cmdlist

    def RunMenuCmd(self, event = None, cmd = []):
        """!Run command selected from menu"""
        if event:
            cmd = self.GetMenuCmd(event)
        self.goutput.RunCmd(cmd, switchPage = False)

    def OnMenuCmd(self, event = None, cmd = []):
        """!Parse command selected from menu"""
        if event:
            cmd = self.GetMenuCmd(event)
        GUI(parent = self).ParseCommand(cmd)
        
    def OnVDigit(self, event):
        """!Start vector digitizer
        """
        if not self.curr_page:
            self.MsgNoLayerSelected()
            return
        
        tree = self.GetLayerTree()
        layer = tree.layer_selected
        # no map layer selected
        if not layer:
            self.MsgNoLayerSelected()
            return
        
        # available only for vector map layers
        try:
            mapLayer = tree.GetPyData(layer)[0]['maplayer']
        except:
            mapLayer = None
        
        if not mapLayer or mapLayer.GetType() != 'vector':
            GMessage(parent = self,
                     message = _("Selected map layer is not vector."))
            return
        
        if mapLayer.GetMapset() != grass.gisenv()['MAPSET']:
            GMessage(parent = self,
                     message = _("Editing is allowed only for vector maps from the "
                                 "current mapset."))
            return
        
        if not tree.GetPyData(layer)[0]:
            return
        dcmd = tree.GetPyData(layer)[0]['cmd']
        if not dcmd:
            return
        
        tree.OnStartEditing(None)
        
    def OnRunScript(self, event):
        """!Run script"""
        # open dialog and choose script file
        dlg = wx.FileDialog(parent = self, message = _("Choose script file to run"),
                            defaultDir = os.getcwd(),
                            wildcard = _("Python script (*.py)|*.py|Bash script (*.sh)|*.sh"))
        
        filename = None
        if dlg.ShowModal() == wx.ID_OK:
            filename = dlg.GetPath()
        
        if not filename:
            return False
        
        if not os.path.exists(filename):
            GError(parent = self,
                   message = _("Script file '%s' doesn't exist. "
                               "Operation canceled.") % filename)
            return

        # check permission
        if not os.access(filename, os.X_OK):
            dlg = wx.MessageDialog(self,
                                   message = _("Script <%s> is not executable. "
                                               "Do you want to set the permissions "
                                               "that allows you to run this script "
                                               "(note that you must be the owner of the file)?" % \
                                                   os.path.basename(filename)),
                                   caption = _("Set permission?"),
                                   style = wx.YES_NO | wx.YES_DEFAULT | wx.ICON_QUESTION)
            if dlg.ShowModal() != wx.ID_YES:
                return
            dlg.Destroy()
            try:
                mode = stat.S_IMODE(os.lstat(filename)[stat.ST_MODE])
                os.chmod(filename, mode | stat.S_IXUSR)
            except OSError:
                GError(_("Unable to set permission. Operation canceled."), parent = self)
                return
        
        # check GRASS_ADDON_PATH
        addonPath = os.getenv('GRASS_ADDON_PATH', [])
        if addonPath:
            addonPath = addonPath.split(os.pathsep)
        dirName = os.path.dirname(filename)
        if dirName not in addonPath:
            addonPath.append(dirName)
            dlg = wx.MessageDialog(self,
                                   message = _("Directory '%s' is not defined in GRASS_ADDON_PATH. "
                                               "Do you want add this directory to GRASS_ADDON_PATH?") % \
                                       dirName,
                                   caption = _("Update Addons path?"),
                                   style = wx.YES_NO | wx.YES_DEFAULT | wx.ICON_QUESTION)
            if dlg.ShowModal() == wx.ID_YES:
                SetAddOnPath(os.pathsep.join(addonPath))
        
        self.goutput.WriteCmdLog(_("Launching script '%s'...") % filename)
        self.goutput.RunCmd([filename], switchPage = True)
        
    def OnChangeLocation(self, event):
        """Change current location"""
        dlg = LocationDialog(parent = self)
        if dlg.ShowModal() == wx.ID_OK:
            location, mapset = dlg.GetValues()
            dlg.Destroy()
            
            if not location or not mapset:
                GError(parent = self,
                       message = _("No location/mapset provided. Operation canceled."))
                return # this should not happen
            
            if RunCommand('g.mapset', parent = self,
                          location = location,
                          mapset = mapset) != 0:
                return # error reported
            
            # close workspace
            self.OnWorkspaceClose()
            self.OnWorkspaceNew()
            GMessage(parent = self,
                     message = _("Current location is <%(loc)s>.\n"
                                 "Current mapset is <%(mapset)s>.") % \
                         { 'loc' : location, 'mapset' : mapset })
        
    def OnCreateMapset(self, event):
        """!Create new mapset"""
        dlg = wx.TextEntryDialog(parent = self,
                                 message = _('Enter name for new mapset:'),
                                 caption = _('Create new mapset'))
        
        if dlg.ShowModal() ==  wx.ID_OK:
            mapset = dlg.GetValue()
            if not mapset:
                GError(parent = self,
                       message = _("No mapset provided. Operation canceled."))
                return
            
            ret = RunCommand('g.mapset',
                             parent = self,
                             flags = 'c',
                             mapset = mapset)
            if ret == 0:
                GMessage(parent = self,
                         message = _("Current mapset is <%s>.") % mapset)
            
    def OnChangeMapset(self, event):
        """Change current mapset"""
        dlg = MapsetDialog(parent = self)
        
        if dlg.ShowModal() == wx.ID_OK:
            mapset = dlg.GetMapset()
            dlg.Destroy()
            
            if not mapset:
                GError(parent = self,
                       message = _("No mapset provided. Operation canceled."))
                return
            
            if RunCommand('g.mapset',
                          parent = self,
                          mapset = mapset) == 0:
                GMessage(parent = self,
                         message = _("Current mapset is <%s>.") % mapset)
        
    def OnNewVector(self, event):
        """!Create new vector map layer"""
        dlg = CreateNewVector(self, log = self.goutput,
                              cmd = (('v.edit',
                                      { 'tool' : 'create' },
                                      'map')))
        
        if not dlg:
            return
        
        name = dlg.GetName(full = True)
        if name and dlg.IsChecked('add'):
            # add layer to map layer tree
            self.curr_page.maptree.AddLayer(ltype = 'vector',
                                            lname = name,
                                            lcmd = ['d.vect', 'map=%s' % name])
        dlg.Destroy()
        
    def OnSystemInfo(self, event):
        """!Print system information"""
        vInfo = grass.version()
        
        # GDAL/OGR
        try:
            from osgeo import gdal
            gdalVersion = gdal.__version__
        except:
            try:
                gdalVersion = grass.Popen(['gdalinfo', '--version'], stdout = grass.PIPE).communicate()[0].rstrip('\n')
            except:
                gdalVersion = _("unknown")
        # PROJ4
        try:
            projVersion = RunCommand('proj', getErrorMsg = True)[1].splitlines()[0]
        except:
            projVersion = _("unknown")
        # check also OSGeo4W on MS Windows
        if sys.platform == 'win32' and \
                not os.path.exists(os.path.join(os.getenv("GISBASE"), "WinGRASS-README.url")):
            osgeo4w = ' (OSGeo4W)'
        else:
            osgeo4w = ''
        
        self.goutput.WriteCmdLog(_("System Info"))
        self.goutput.WriteLog("%s: %s\n"
                              "%s: %s\n"
                              "%s: %s (%s)\n"
                              "GDAL/OGR: %s\n"
                              "PROJ4: %s\n"
                              "Python: %s\n"
                              "wxPython: %s\n"
                              "%s: %s%s\n"% (_("GRASS version"), vInfo['version'],
                                           _("GRASS SVN Revision"), vInfo['revision'],
                                           _("GIS Library Revision"), vInfo['libgis_revision'], vInfo['libgis_date'].split(' ', 1)[0],
                                           gdalVersion, projVersion,
                                           platform.python_version(),
                                           wx.__version__,
                                           _("Platform"), platform.platform(), osgeo4w),
                              switchPage = True)
        self.goutput.WriteCmdLog(' ')
    
    def OnAboutGRASS(self, event):
        """!Display 'About GRASS' dialog"""
        win = AboutWindow(self)
        win.CentreOnScreen()
        win.Show(True)  

    def _popupMenu(self, data):
        """!Create popup menu
        """
        point = wx.GetMousePosition()
        menu = wx.Menu()
        
        for key, handler in data:
            if key is None:
                menu.AppendSeparator()
                continue
            item = wx.MenuItem(menu, wx.ID_ANY, LMIcons[key].GetLabel())
            item.SetBitmap(LMIcons[key].GetBitmap(self.iconsize))
            menu.AppendItem(item)
            self.Bind(wx.EVT_MENU, handler, item)
        
        # create menu
        self.PopupMenu(menu)
        menu.Destroy()

    def OnImportMenu(self, event):
        """!Import maps menu (import, link)
        """
        self._popupMenu((('rastImport',    self.OnImportGdalLayers),
                         ('vectImport',    self.OnImportOgrLayers)))
        
    def OnWorkspaceNew(self, event = None):
        """!Create new workspace file

        Erase current workspace settings first
        """
        Debug.msg(4, "GMFrame.OnWorkspaceNew():")
        
        # start new map display if no display is available
        if not self.curr_page:
            self.NewDisplay()
        
        maptree = self.curr_page.maptree
        
        # ask user to save current settings
        if self.workspaceFile and self.workspaceChanged:
            self.OnWorkspaceSave()
        elif self.workspaceFile is None and maptree.GetCount() > 0:
             dlg = wx.MessageDialog(self, message = _("Current workspace is not empty. "
                                                    "Do you want to store current settings "
                                                    "to workspace file?"),
                                    caption = _("Create new workspace?"),
                                    style = wx.YES_NO | wx.YES_DEFAULT | \
                                        wx.CANCEL | wx.ICON_QUESTION)
             ret = dlg.ShowModal()
             if ret == wx.ID_YES:
                 self.OnWorkspaceSaveAs()
             elif ret == wx.ID_CANCEL:
                 dlg.Destroy()
                 return
             
             dlg.Destroy()
        
        # delete all items
        maptree.DeleteAllItems()
        
        # add new root element
        maptree.root = maptree.AddRoot("Map Layers")
        self.curr_page.maptree.SetPyData(maptree.root, (None,None))
        
        # no workspace file loaded
        self.workspaceFile = None
        self.workspaceChanged = False
        self.SetTitle(self.baseTitle)
        
    def OnWorkspaceOpen(self, event = None):
        """!Open file with workspace definition"""
        dlg = wx.FileDialog(parent = self, message = _("Choose workspace file"),
                            defaultDir = os.getcwd(), wildcard = _("GRASS Workspace File (*.gxw)|*.gxw"))

        filename = ''
        if dlg.ShowModal() == wx.ID_OK:
            filename = dlg.GetPath()

        if filename == '':
            return

        Debug.msg(4, "GMFrame.OnWorkspaceOpen(): filename=%s" % filename)
        
        # delete current layer tree content
        self.OnWorkspaceClose()
        
        self.LoadWorkspaceFile(filename)

        self.workspaceFile = filename
        self.SetTitle(self.baseTitle + " - " +  os.path.basename(self.workspaceFile))

    def LoadWorkspaceFile(self, filename):
        """!Load layer tree definition stored in GRASS Workspace XML file (gxw)

        @todo Validate against DTD
        
        @return True on success
        @return False on error
        """
        # dtd
        dtdFilename = os.path.join(globalvar.ETCWXDIR, "xml", "grass-gxw.dtd")
        
        # parse workspace file
        try:
            gxwXml = ProcessWorkspaceFile(etree.parse(filename))
        except Exception, e:
            GError(parent = self,
                   message = _("Reading workspace file <%s> failed.\n"
                                    "Invalid file, unable to parse XML document.") % filename)
            return
        
        busy = wx.BusyInfo(message = _("Please wait, loading workspace..."),
                           parent = self)
        wx.Yield()

        #
        # load layer manager window properties
        #
        if not UserSettings.Get(group = 'general', key = 'workspace',
                                subkey = ['posManager', 'enabled']):
            if gxwXml.layerManager['pos']:
                self.SetPosition(gxwXml.layerManager['pos'])
            if gxwXml.layerManager['size']:
                self.SetSize(gxwXml.layerManager['size'])
        
        #
        # start map displays first (list of layers can be empty)
        #
        displayId = 0
        mapdisplay = list()
        for display in gxwXml.displays:
            mapdisp = self.NewDisplay(name = display['name'], show = False)
            mapdisplay.append(mapdisp)
            maptree = self.gm_cb.GetPage(displayId).maptree
            
            # set windows properties
            mapdisp.SetProperties(render = display['render'],
                                  mode = display['mode'],
                                  showCompExtent = display['showCompExtent'],
                                  alignExtent = display['alignExtent'],
                                  constrainRes = display['constrainRes'],
                                  projection = display['projection']['enabled'])

            if display['projection']['enabled']:
                if display['projection']['epsg']:
                    UserSettings.Set(group = 'display', key = 'projection', subkey = 'epsg',
                                     value = display['projection']['epsg'])
                    if display['projection']['proj']:
                        UserSettings.Set(group = 'display', key = 'projection', subkey = 'proj4',
                                         value = display['projection']['proj'])
            
            # set position and size of map display
            if not UserSettings.Get(group = 'general', key = 'workspace', subkey = ['posDisplay', 'enabled']):
                if display['pos']:
                    mapdisp.SetPosition(display['pos'])
                if display['size']:
                    mapdisp.SetSize(display['size'])
                    
            # set extent if defined
            if display['extent']:
                w, s, e, n = display['extent']
                region = maptree.Map.region = maptree.Map.GetRegion(w = w, s = s, e = e, n = n)
                mapdisp.GetWindow().ResetZoomHistory()
                mapdisp.GetWindow().ZoomHistory(region['n'],
                                                region['s'],
                                                region['e'],
                                                region['w'])
                
            mapdisp.Show()
            
            displayId += 1
    
        maptree = None 
        selected = [] # list of selected layers
        # 
        # load list of map layers
        #
        for layer in gxwXml.layers:
            display = layer['display']
            maptree = self.gm_cb.GetPage(display).maptree
            
            newItem = maptree.AddLayer(ltype = layer['type'],
                                       lname = layer['name'],
                                       lchecked = layer['checked'],
                                       lopacity = layer['opacity'],
                                       lcmd = layer['cmd'],
                                       lgroup = layer['group'],
                                       lnviz = layer['nviz'],
                                       lvdigit = layer['vdigit'])
            
            if layer.has_key('selected'):
                if layer['selected']:
                    selected.append((maptree, newItem))
                else:
                    maptree.SelectItem(newItem, select = False)
            
        for maptree, layer in selected:
            if not maptree.IsSelected(layer):
                maptree.SelectItem(layer, select = True)
                maptree.layer_selected = layer
                
        busy.Destroy()
            
        for idx, mdisp in enumerate(mapdisplay):
            mdisp.MapWindow2D.UpdateMap()
            # nviz
            if gxwXml.displays[idx]['viewMode'] == '3d':
                mdisp.AddNviz()
                self.nviz.UpdateState(view = gxwXml.nviz_state['view'],
                                              iview = gxwXml.nviz_state['iview'],
                                              light = gxwXml.nviz_state['light'])
                mdisp.MapWindow3D.constants = gxwXml.nviz_state['constants']
                for idx, constant in enumerate(mdisp.MapWindow3D.constants):
                    mdisp.MapWindow3D.AddConstant(constant, idx + 1)
                for page in ('view', 'light', 'fringe', 'constant', 'cplane'):
                    self.nviz.UpdatePage(page)
                self.nviz.UpdateSettings()
                mdisp.toolbars['map'].combo.SetSelection(1)
            
            mdisp.Show() # show mapdisplay
        
        return True
    
    def OnWorkspaceLoadGrcFile(self, event):
        """!Load map layers from GRC file (Tcl/Tk GUI) into map layer tree"""
        dlg = wx.FileDialog(parent = self, message = _("Choose GRC file to load"),
                            defaultDir = os.getcwd(), wildcard = _("Old GRASS Workspace File (*.grc)|*.grc"))

        filename = ''
        if dlg.ShowModal() == wx.ID_OK:
            filename = dlg.GetPath()

        if filename == '':
            return

        Debug.msg(4, "GMFrame.OnWorkspaceLoadGrcFile(): filename=%s" % filename)

        # start new map display if no display is available
        if not self.curr_page:
            self.NewDisplay()

        busy = wx.BusyInfo(message = _("Please wait, loading workspace..."),
                           parent = self)
        wx.Yield()

        maptree = None
        for layer in ProcessGrcFile(filename).read(self):
            maptree = self.gm_cb.GetPage(layer['display']).maptree
            newItem = maptree.AddLayer(ltype = layer['type'],
                                       lname = layer['name'],
                                       lchecked = layer['checked'],
                                       lopacity = layer['opacity'],
                                       lcmd = layer['cmd'],
                                       lgroup = layer['group'])

            busy.Destroy()
            
        if maptree:
            # reverse list of map layers
            maptree.Map.ReverseListOfLayers()

    def OnWorkspaceSaveAs(self, event = None):
        """!Save workspace definition to selected file"""
        dlg = wx.FileDialog(parent = self, message = _("Choose file to save current workspace"),
                            defaultDir = os.getcwd(), wildcard = _("GRASS Workspace File (*.gxw)|*.gxw"), style = wx.FD_SAVE)

        filename = ''
        if dlg.ShowModal() == wx.ID_OK:
            filename = dlg.GetPath()

        if filename == '':
            return False

        # check for extension
        if filename[-4:] != ".gxw":
            filename += ".gxw"

        if os.path.exists(filename):
            dlg = wx.MessageDialog(self, message = _("Workspace file <%s> already exists. "
                                                     "Do you want to overwrite this file?") % filename,
                                   caption = _("Save workspace"), style = wx.YES_NO | wx.YES_DEFAULT | wx.ICON_QUESTION)
            if dlg.ShowModal() != wx.ID_YES:
                dlg.Destroy()
                return False

        Debug.msg(4, "GMFrame.OnWorkspaceSaveAs(): filename=%s" % filename)

        self.SaveToWorkspaceFile(filename)
        self.workspaceFile = filename
        self.SetTitle(self.baseTitle + " - " + os.path.basename(self.workspaceFile))

    def OnWorkspaceSave(self, event = None):
        """!Save file with workspace definition"""
        if self.workspaceFile:
            dlg = wx.MessageDialog(self, message = _("Workspace file <%s> already exists. "
                                                   "Do you want to overwrite this file?") % \
                                       self.workspaceFile,
                                   caption = _("Save workspace"), style = wx.YES_NO | wx.YES_DEFAULT | wx.ICON_QUESTION)
            if dlg.ShowModal() == wx.ID_NO:
                dlg.Destroy()
            else:
                Debug.msg(4, "GMFrame.OnWorkspaceSave(): filename=%s" % self.workspaceFile)
                self.SaveToWorkspaceFile(self.workspaceFile)
                self.SetTitle(self.baseTitle + " - " + os.path.basename(self.workspaceFile))
                self.workspaceChanged = False
        else:
            self.OnWorkspaceSaveAs()

    def SaveToWorkspaceFile(self, filename):
        """!Save layer tree layout to workspace file
        
        Return True on success, False on error
        """
        tmpfile = tempfile.TemporaryFile(mode = 'w+b')
        try:
            WriteWorkspaceFile(lmgr = self, file = tmpfile)
        except StandardError, e:
            GError(parent = self,
                   message = _("Writing current settings to workspace file "
                               "failed."))
            return False
        
        try:
            mfile = open(filename, "w")
            tmpfile.seek(0)
            for line in tmpfile.readlines():
                mfile.write(line)
        except IOError:
            GError(parent = self,
                   message = _("Unable to open file <%s> for writing.") % filename)
            return False
        
        mfile.close()
        
        return True
    
    def OnWorkspaceClose(self, event = None):
        """!Close file with workspace definition
        
        If workspace has been modified ask user to save the changes.
        """
        Debug.msg(4, "GMFrame.OnWorkspaceClose(): file=%s" % self.workspaceFile)
        
        self.OnDisplayCloseAll()
        self.workspaceFile = None
        self.workspaceChanged = False
        self.SetTitle(self.baseTitle)
        self.disp_idx = 0
        self.curr_page = None
        
    def OnDisplayClose(self, event = None):
        """!Close current map display window
        """
        if self.curr_page and self.curr_page.maptree.mapdisplay:
            self.curr_page.maptree.mapdisplay.OnCloseWindow(event)
        
    def OnDisplayCloseAll(self, event = None):
        """!Close all open map display windows
        """
        displays = list()
        for page in range(0, self.gm_cb.GetPageCount()):
            displays.append(self.gm_cb.GetPage(page).maptree.mapdisplay)
        
        for display in displays:
            display.OnCloseWindow(event)
        
    def OnRenameDisplay(self, event):
        """!Change Map Display name"""
        name = self.gm_cb.GetPageText(self.curr_pagenum)
        dlg = wx.TextEntryDialog(self, message = _("Enter new name:"),
                                 caption = _("Rename Map Display"), defaultValue = name)
        if dlg.ShowModal() == wx.ID_OK:
            name = dlg.GetValue()
            self.gm_cb.SetPageText(page = self.curr_pagenum, text = name)
            mapdisplay = self.curr_page.maptree.mapdisplay
            mapdisplay.SetTitle(_("GRASS GIS Map Display: %(name)s  - Location: %(loc)s") % \
                                     { 'name' : name,
                                       'loc' : grass.gisenv()["LOCATION_NAME"] })
        dlg.Destroy()
        
    def RulesCmd(self, event):
        """!Launches dialog for commands that need rules input and
        processes rules
        """
        cmd = self.GetMenuCmd(event)
        
        if cmd[0] == 'r.colors':
            ctable = RasterColorTable(self)
        else:
            ctable = VectorColorTable(self, attributeType = 'color')
        ctable.CentreOnScreen()
        ctable.Show()
        
    def OnXTermNoXMon(self, event):
        """!
        Run commands that need xterm
        """
        self.OnXTerm(event, need_xmon = False)
        
    def OnXTerm(self, event, need_xmon = True):
        """!
        Run commands that need interactive xmon

        @param need_xmon True to start X monitor
        """
        # unset display mode
        if os.getenv('GRASS_RENDER_IMMEDIATE'):
            del os.environ['GRASS_RENDER_IMMEDIATE']
        
        if need_xmon:
            # open next available xmon
            xmonlist = []
            
            # make list of xmons that are not running
            ret = RunCommand('d.mon',
                             flags = 'L',
                             read = True)
            
            for line in ret.split('\n'):               
                line = line.strip()
                if line.startswith('x') and 'not running' in line:
                    xmonlist.append(line[0:2])
            
            # find available xmon
            xmon = xmonlist[0]
            
            # bring up the xmon
            cmdlist = ['d.mon', xmon]
            p = Command(cmdlist, wait=False)
        
        # run the command        
        command = self.GetMenuCmd(event)
        command = ' '.join(command)
        
        gisbase = os.environ['GISBASE']
        
        if sys.platform == "win32":
            runbat = os.path.join(gisbase,'etc','grass-run.bat')
            cmdlist = ["start", runbat, runbat, command]
        else:
            if sys.platform == "darwin":
                xtermwrapper = os.path.join(gisbase,'etc','grass-xterm-mac')
            else:
                xtermwrapper = os.path.join(gisbase,'etc','grass-xterm-wrapper')
            
            grassrun = os.path.join(gisbase,'etc','grass-run.sh')
            cmdlist = [xtermwrapper, '-e', grassrun, command]
        
        p = Command(cmdlist, wait=False)
        
        # reset display mode
        os.environ['GRASS_RENDER_IMMEDIATE'] = 'TRUE'
        
    def OnEditImageryGroups(self, event, cmd = None):
        """!Show dialog for creating and editing groups.
        """
        dlg = GroupDialog(self)
        dlg.CentreOnScreen()
        dlg.Show()
        
    def OnInstallExtension(self, event):
        """!Install extension from GRASS Addons SVN repository"""
        win = InstallExtensionWindow(self, size = (650, 550))
        win.CentreOnScreen()
        win.Show()

    def OnUninstallExtension(self, event):
        """!Uninstall extension"""
        win = UninstallExtensionWindow(self, size = (650, 300))
        win.CentreOnScreen()
        win.Show()

    def OnPreferences(self, event):
        """!General GUI preferences/settings
        """
        if not self.dialogs['preferences']:
            dlg = PreferencesDialog(parent = self)
            self.dialogs['preferences'] = dlg
            self.dialogs['preferences'].CenterOnScreen()
            
            dlg.Bind(EVT_SETTINGS_CHANGED, self.OnSettingsChanged)
        
        self.dialogs['preferences'].ShowModal()
        
    def OnHelp(self, event):
        """!Show help
        """
        self.goutput.RunCmd(['g.manual','-i'])
        
    def OnHistogram(self, event):
        """!Init histogram display canvas and tools
        """
        from modules.histogram import HistogramFrame
        win = HistogramFrame(self)
        
        win.CentreOnScreen()
        win.Show()
        win.Refresh()
        win.Update()
        
    def OnProfile(self, event):
        """!Launch profile tool
        """
        win = profile.ProfileFrame(parent = self)
        
        win.CentreOnParent()
        win.Show()
        win.Refresh()
        win.Update()
        
    def OnMapCalculator(self, event, cmd = ''):
        """!Init map calculator for interactive creation of mapcalc statements
        """
        if event:
            try:
                cmd = self.GetMenuCmd(event)
            except KeyError:
                cmd = ['r.mapcalc']
        
        win = MapCalcFrame(parent = self,
                           cmd = cmd[0])
        win.CentreOnScreen()
        win.Show()
        
    def OnVectorCleaning(self, event, cmd = ''):
        """!Init interactive vector cleaning
        """
        from modules.vclean import VectorCleaningFrame
        win = VectorCleaningFrame(parent = self)
        win.CentreOnScreen()
        win.Show()
        
    def OnImportDxfFile(self, event, cmd = None):
        """!Convert multiple DXF layers to GRASS vector map layers"""
        dlg = DxfImportDialog(parent = self)
        dlg.CentreOnScreen()
        dlg.Show()

    def OnImportGdalLayers(self, event, cmd = None):
        """!Convert multiple GDAL layers to GRASS raster map layers"""
        dlg = GdalImportDialog(parent = self)
        dlg.CentreOnScreen()
        dlg.Show()

    def OnLinkGdalLayers(self, event, cmd = None):
        """!Link multiple GDAL layers to GRASS raster map layers"""
        dlg = GdalImportDialog(parent = self, link = True)
        dlg.CentreOnScreen()
        dlg.Show()
        
    def OnImportOgrLayers(self, event, cmd = None):
        """!Convert multiple OGR layers to GRASS vector map layers"""
        dlg = GdalImportDialog(parent = self, ogr = True)
        dlg.CentreOnScreen()
        dlg.Show()
        
    def OnLinkOgrLayers(self, event, cmd = None):
        """!Links multiple OGR layers to GRASS vector map layers"""
        dlg = GdalImportDialog(parent = self, ogr = True, link = True)
        dlg.CentreOnScreen()
        dlg.Show()
        
    def OnImportWMS(self, event, cmd = None):
        """!Import data from OGC WMS server"""
        dlg = WMSDialog(parent = self, service = 'wms')
        dlg.CenterOnScreen()
        
        if dlg.ShowModal() == wx.ID_OK: # -> import layers
            layers = dlg.GetLayers()
            
            if len(layers.keys()) > 0:
                for layer in layers.keys():
                    cmd = ['r.in.wms',
                           'mapserver=%s' % dlg.GetSettings()['server'],
                           'layers=%s' % layer,
                           'output=%s' % layer,
                           'format=png',
                           '--overwrite']
                    styles = ','.join(layers[layer])
                    if styles:
                        cmd.append('styles=%s' % styles)
                    self.goutput.RunCmd(cmd, switchPage = True)

                    self.curr_page.maptree.AddLayer(ltype = 'raster',
                                                    lname = layer,
                                                    lcmd = ['d.rast', 'map=%s' % layer],
                                                    multiple = False)
            else:
                self.goutput.WriteWarning(_("Nothing to import. No WMS layer selected."))
                
                
        dlg.Destroy()
        
    def OnShowAttributeTable(self, event, selection = None):
        """!Show attribute table of the given vector map layer
        """
        if not self.curr_page:
            self.MsgNoLayerSelected()
            return
        
        tree = self.GetLayerTree()
        layer = tree.layer_selected
        # no map layer selected
        if not layer:
            self.MsgNoLayerSelected()
            return
        
        # available only for vector map layers
        try:
            maptype = tree.GetPyData(layer)[0]['maplayer'].type
        except:
            maptype = None
        
        if not maptype or maptype != 'vector':
            GMessage(parent = self,
                          message = _("Selected map layer is not vector."))
            return
        
        if not tree.GetPyData(layer)[0]:
            return
        dcmd = tree.GetPyData(layer)[0]['cmd']
        if not dcmd:
            return
        
        busy = wx.BusyInfo(message = _("Please wait, loading attribute data..."),
                           parent = self)
        wx.Yield()
        
        dbmanager = AttributeManager(parent = self, id = wx.ID_ANY,
                                     size = wx.Size(500, 300),
                                     item = layer, log = self.goutput,
                                     selection = selection)
        
        busy.Destroy()
        
        # register ATM dialog
        self.dialogs['atm'].append(dbmanager)
        
        # show ATM window
        dbmanager.Show()
        
    def OnNewDisplayWMS(self, event = None):
        """!Create new layer tree and map display instance"""
        self.NewDisplayWMS()

    def OnNewDisplay(self, event = None):
        """!Create new layer tree and map display instance"""
        self.NewDisplay()

    def NewDisplay(self, name = None, show = True):
        """!Create new layer tree, which will
        create an associated map display frame

        @param name name of new map display
        @param show show map display window if True

        @return reference to mapdisplay intance
        """
        Debug.msg(1, "GMFrame.NewDisplay(): idx=%d" % self.disp_idx)
        
        # make a new page in the bookcontrol for the layer tree (on page 0 of the notebook)
        self.pg_panel = wx.Panel(self.gm_cb, id = wx.ID_ANY, style = wx.EXPAND)
        if name:
            dispName = name
        else:
            dispName = "Display " + str(self.disp_idx + 1)
        self.gm_cb.AddPage(self.pg_panel, text = dispName, select = True)
        self.curr_page = self.gm_cb.GetCurrentPage()
        
        # create layer tree (tree control for managing GIS layers)  and put on new notebook page
        self.curr_page.maptree = LayerTree(self.curr_page, id = wx.ID_ANY, pos = wx.DefaultPosition,
                                           size = wx.DefaultSize, style = wx.TR_HAS_BUTTONS |
                                           wx.TR_LINES_AT_ROOT| wx.TR_HIDE_ROOT |
                                           wx.TR_DEFAULT_STYLE| wx.NO_BORDER | wx.FULL_REPAINT_ON_RESIZE,
                                           idx = self.disp_idx, lmgr = self, notebook = self.gm_cb,
                                           auimgr = self._auimgr, showMapDisplay = show)
        
        # layout for controls
        cb_boxsizer = wx.BoxSizer(wx.VERTICAL)
        cb_boxsizer.Add(self.curr_page.maptree, proportion = 1, flag = wx.EXPAND, border = 1)
        self.curr_page.SetSizer(cb_boxsizer)
        cb_boxsizer.Fit(self.curr_page.maptree)
        self.curr_page.Layout()
        self.curr_page.maptree.Layout()
        
        # use default window layout
        if UserSettings.Get(group = 'general', key = 'defWindowPos', subkey = 'enabled'):
            dim = UserSettings.Get(group = 'general', key = 'defWindowPos', subkey = 'dim')
            idx = 4 + self.disp_idx * 4
            try:
                x, y = map(int, dim.split(',')[idx:idx + 2])
                w, h = map(int, dim.split(',')[idx + 2:idx + 4])
                self.curr_page.maptree.mapdisplay.SetPosition((x, y))
                self.curr_page.maptree.mapdisplay.SetSize((w, h))
            except:
                pass
        
        self.disp_idx += 1
        
        return self.curr_page.maptree.mapdisplay
    
    def OnAddMaps(self, event = None):
        """!Add selected map layers into layer tree"""
        dialog = MapLayersDialog(parent = self, title = _("Add selected map layers into layer tree"))
        
        if dialog.ShowModal() != wx.ID_OK:
            dialog.Destroy()
            return
        
        # start new map display if no display is available
        if not self.curr_page:
            self.NewDisplay()
            
        maptree = self.curr_page.maptree
        
        for layerName in dialog.GetMapLayers():
            ltype = dialog.GetLayerType(cmd = True)
            if ltype == 'rast':
                cmd = ['d.rast', 'map=%s' % layerName]
                wxType = 'raster'
            elif ltype == 'rast3d':
                cmd = ['d.rast3d', 'map=%s' % layerName]
                wxType = '3d-raster'
            elif ltype == 'vect':
                cmd = ['d.vect', 'map=%s' % layerName]
                wxType = 'vector'
            else:
                GError(parent = self,
                       message = _("Unsupported map layer type <%s>.") % ltype)
                return
            
            newItem = maptree.AddLayer(ltype = wxType,
                                       lname = layerName,
                                       lchecked = False,
                                       lopacity = 1.0,
                                       lcmd = cmd,
                                       lgroup = None)
        dialog.Destroy()
        
    def OnAddRaster(self, event):
        """!Add raster map layer"""
        # start new map display if no display is available
        if not self.curr_page:
            self.NewDisplay(show = True)
        
        self.notebook.SetSelectionByName('layers')
        self.curr_page.maptree.AddLayer('raster')
        
    def OnAddRasterMisc(self, event):
        """!Create misc raster popup-menu"""
        # start new map display if no display is available
        if not self.curr_page:
            self.NewDisplay(show = True)
        
        self._popupMenu((('addRast3d', self.OnAddRaster3D),
                         (None, None),
                         ('addRgb',    self.OnAddRasterRGB),
                         ('addHis',    self.OnAddRasterHIS),
                         (None, None),
                         ('addShaded', self.OnAddRasterShaded),
                         (None, None),
                         ('addRArrow', self.OnAddRasterArrow),
                         ('addRNum',   self.OnAddRasterNum)))
        
        # show map display
        self.curr_page.maptree.mapdisplay.Show()
        
    def OnAddVector(self, event):
        """!Add vector map to the current layer tree"""
        # start new map display if no display is available
        if not self.curr_page:
            self.NewDisplay(show = True)
        
        self.notebook.SetSelectionByName('layers')
        self.curr_page.maptree.AddLayer('vector')

    def OnAddVectorMisc(self, event):
        """!Create misc vector popup-menu"""
        # start new map display if no display is available
        if not self.curr_page:
            self.NewDisplay(show = True)

        self._popupMenu((('addThematic', self.OnAddVectorTheme),
                         ('addChart',    self.OnAddVectorChart)))
        
        # show map display
        self.curr_page.maptree.mapdisplay.Show()

    def OnAddVectorTheme(self, event):
        """!Add thematic vector map to the current layer tree"""
        self.notebook.SetSelectionByName('layers')
        self.curr_page.maptree.AddLayer('thememap')

    def OnAddVectorChart(self, event):
        """!Add chart vector map to the current layer tree"""
        self.notebook.SetSelectionByName('layers')
        self.curr_page.maptree.AddLayer('themechart')

    def OnAddOverlay(self, event):
        """!Create decoration overlay menu""" 
        # start new map display if no display is available
        if not self.curr_page:
            self.NewDisplay(show = True)

        self._popupMenu((('addGrid',     self.OnAddGrid),
                         ('addLabels',   self.OnAddLabels),
                         ('addGeodesic', self.OnAddGeodesic),
                         ('addRhumb',    self.OnAddRhumb),
                         (None, None),
                         ('addCmd',      self.OnAddCommand)))
        
        # show map display
        self.curr_page.maptree.mapdisplay.Show()
        
    def OnAddRaster3D(self, event):
        """!Add 3D raster map to the current layer tree"""
        self.notebook.SetSelectionByName('layers')
        self.curr_page.maptree.AddLayer('3d-raster')

    def OnAddRasterRGB(self, event):
        """!Add RGB raster map to the current layer tree"""
        self.notebook.SetSelectionByName('layers')
        self.curr_page.maptree.AddLayer('rgb')

    def OnAddRasterHIS(self, event):
        """!Add HIS raster map to the current layer tree"""
        self.notebook.SetSelectionByName('layers')
        self.curr_page.maptree.AddLayer('his')

    def OnAddRasterShaded(self, event):
        """!Add shaded relief raster map to the current layer tree"""
        self.notebook.SetSelectionByName('layers')
        self.curr_page.maptree.AddLayer('shaded')

    def OnAddRasterArrow(self, event):
        """!Add flow arrows raster map to the current layer tree"""
        self.notebook.SetSelectionByName('layers')
        tree = self.curr_page.maptree
        resolution = tree.GetMapDisplay().GetProperty('resolution')
        if not resolution:
            dlg = self.MsgDisplayResolution()
            if dlg.ShowModal() == wx.ID_YES:
                tree.GetMapDisplay().SetProperty('resolution', True)
            dlg.Destroy()

        self.curr_page.maptree.AddLayer('rastarrow')

    def OnAddRasterNum(self, event):
        """!Add cell number raster map to the current layer tree"""
        self.notebook.SetSelectionByName('layers')
        tree = self.curr_page.maptree
        resolution = tree.GetMapDisplay().GetProperty('resolution')
        if not resolution:
            limitText = _("Note that cell values can only be displayed for "
                          "regions of less than 10,000 cells.")
            dlg = self.MsgDisplayResolution(limitText)
            if dlg.ShowModal() == wx.ID_YES:
                tree.GetMapDisplay().SetProperty('resolution', True)
            dlg.Destroy()

        # region = tree.GetMap().GetCurrentRegion()
        # if region['cells'] > 10000:
        #   GMessage(message = "Cell values can only be displayed for regions of < 10,000 cells.", parent = self)
        self.curr_page.maptree.AddLayer('rastnum')

    def OnAddCommand(self, event):
        """!Add command line map layer to the current layer tree"""
        # start new map display if no display is available
        if not self.curr_page:
            self.NewDisplay(show = True)

        self.notebook.SetSelectionByName('layers')
        self.curr_page.maptree.AddLayer('command')

        # show map display
        self.curr_page.maptree.mapdisplay.Show()

    def OnAddGroup(self, event):
        """!Add layer group"""
        # start new map display if no display is available
        if not self.curr_page:
            self.NewDisplay(show = True)

        self.notebook.SetSelectionByName('layers')
        self.curr_page.maptree.AddLayer('group')

        # show map display
        self.curr_page.maptree.mapdisplay.Show()

    def OnAddGrid(self, event):
        """!Add grid map layer to the current layer tree"""
        self.notebook.SetSelectionByName('layers')
        self.curr_page.maptree.AddLayer('grid')

    def OnAddGeodesic(self, event):
        """!Add geodesic line map layer to the current layer tree"""
        self.notebook.SetSelectionByName('layers')
        self.curr_page.maptree.AddLayer('geodesic')

    def OnAddRhumb(self, event):
        """!Add rhumb map layer to the current layer tree"""
        self.notebook.SetSelectionByName('layers')
        self.curr_page.maptree.AddLayer('rhumb')

    def OnAddLabels(self, event):
        """!Add vector labels map layer to the current layer tree"""
        # start new map display if no display is available
        if not self.curr_page:
            self.NewDisplay(show = True)

        self.notebook.SetSelectionByName('layers')
        self.curr_page.maptree.AddLayer('labels')

        # show map display
        self.curr_page.maptree.mapdisplay.Show()

    def OnDeleteLayer(self, event):
        """!Remove selected map layer from the current layer Tree
        """
        if not self.curr_page or not self.curr_page.maptree.layer_selected:
            self.MsgNoLayerSelected()
            return

        if UserSettings.Get(group = 'manager', key = 'askOnRemoveLayer', subkey = 'enabled'):
            layerName = ''
            for item in self.curr_page.maptree.GetSelections():
                name = str(self.curr_page.maptree.GetItemText(item))
                idx = name.find('(opacity')
                if idx > -1:
                    layerName += '<' + name[:idx].strip(' ') + '>,\n'
                else:
                    layerName += '<' + name + '>,\n'
            layerName = layerName.rstrip(',\n')
            
            if len(layerName) > 2: # <>
                message = _("Do you want to remove map layer(s)\n%s\n"
                            "from layer tree?") % layerName
            else:
                message = _("Do you want to remove selected map layer(s) "
                            "from layer tree?")

            dlg = wx.MessageDialog (parent = self, message = message,
                                    caption = _("Remove map layer"),
                                    style  =  wx.YES_NO | wx.YES_DEFAULT | wx.ICON_QUESTION)

            if dlg.ShowModal() != wx.ID_YES:
                dlg.Destroy()
                return
            
            dlg.Destroy()

        for layer in self.curr_page.maptree.GetSelections():
            if self.curr_page.maptree.GetPyData(layer)[0]['type'] == 'group':
                self.curr_page.maptree.DeleteChildren(layer)
            self.curr_page.maptree.Delete(layer)
        
    def OnKeyDown(self, event):
        """!Key pressed"""
        kc = event.GetKeyCode()
        
        if event.ControlDown():
            if kc == wx.WXK_TAB:
                # switch layer list / command output
                if self.notebook.GetSelection() == self.notebook.GetPageIndexByName('layers'):
                    self.notebook.SetSelectionByName('output')
                else:
                    self.notebook.SetSelectionByName('layers')
        
        try:
            ckc = chr(kc)
        except ValueError:
            event.Skip()
            return
        
        if event.CtrlDown():
            if kc == 'R':
                self.OnAddRaster(None)
            elif kc == 'V':
                self.OnAddVector(None)
        
        event.Skip()

    def OnCloseWindow(self, event):
        """!Cleanup when wxGUI is quitted"""
        # save command protocol if actived
        if self.goutput.btnCmdProtocol.GetValue():
            self.goutput.CmdProtocolSave()
        
        if not self.curr_page:
            self._auimgr.UnInit()
            self.Destroy()
            return
        
        # save changes in the workspace
        maptree = self.curr_page.maptree
        if self.workspaceChanged and \
                UserSettings.Get(group = 'manager', key = 'askOnQuit', subkey = 'enabled'):
            if self.workspaceFile:
                message = _("Do you want to save changes in the workspace?")
            else:
                message = _("Do you want to store current settings "
                            "to workspace file?")
            
            # ask user to save current settings
            if maptree.GetCount() > 0:
                dlg = wx.MessageDialog(self,
                                       message = message,
                                       caption = _("Quit GRASS GUI"),
                                       style = wx.YES_NO | wx.YES_DEFAULT |
                                       wx.CANCEL | wx.ICON_QUESTION | wx.CENTRE)
                ret = dlg.ShowModal()
                if ret == wx.ID_YES:
                    if not self.workspaceFile:
                        self.OnWorkspaceSaveAs()
                    else:
                        self.SaveToWorkspaceFile(self.workspaceFile)
                elif ret == wx.ID_CANCEL:
                    # when called from menu, it gets CommandEvent and not CloseEvent
                    if hasattr(event, 'Veto'):
                        event.Veto()
                    dlg.Destroy()
                    return
                dlg.Destroy()
        
        # don't ask any more...
        UserSettings.Set(group = 'manager', key = 'askOnQuit', subkey = 'enabled',
                         value = False)
        
        self.OnDisplayCloseAll()
        
        self.gm_cb.DeleteAllPages()
        
        self._auimgr.UnInit()
        self.Destroy()
        
    def MsgNoLayerSelected(self):
        """!Show dialog message 'No layer selected'"""
        wx.MessageBox(parent = self,
                      message = _("No map layer selected. Operation canceled."),
                      caption = _("Message"),
                      style = wx.OK | wx.ICON_INFORMATION | wx.CENTRE)
                      
    def MsgDisplayResolution(self, limitText = None):
        """!Returns dialog for d.rast.num, d.rast.arrow
            when display resolution is not constrained
            
        @param limitText adds a note about cell limit
        """
        message = _("Display resolution is currently not constrained to "
                    "computational settings. "
                    "It's suggested to constrain map to region geometry. "
                    "Do you want to constrain "
                    "the resolution?")
        if limitText:
            message += "\n\n%s" % _(limitText)
        dlg = wx.MessageDialog(parent = self,
                               message = message,
                               caption = _("Constrain map to region geometry?"),
                               style = wx.YES_NO | wx.YES_DEFAULT | wx.ICON_QUESTION | wx.CENTRE)
        return dlg
