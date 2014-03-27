"""!
@package modules.extensions

@brief GRASS Addons extensions management classes

Classes:
 - extensions::InstallExtensionWindow
 - extensions::ExtensionTree
 - extensions::UninstallExtensionWindow
 - extensions::CheckListExtension

(C) 2008-2011 by the GRASS Development Team

This program is free software under the GNU General Public License
(>=v2). Read the file COPYING that comes with GRASS for details.

@author Martin Landa <landa.martin gmail.com>
"""

import os
import sys

import wx
import wx.lib.mixins.listctrl as listmix
try:
    import wx.lib.agw.customtreectrl as CT
except ImportError:
    import wx.lib.customtreectrl as CT
import wx.lib.flatnotebook as FN

import grass.script as grass
from grass.script import task as gtask

from core             import globalvar
from core.gcmd        import GError, RunCommand
from core.utils       import SetAddOnPath
from gui_core.forms   import GUI
from gui_core.widgets import ItemTree
from gui_core.ghelp   import SearchModuleWindow

class InstallExtensionWindow(wx.Frame):
    def __init__(self, parent, id = wx.ID_ANY,
                 title = _("Fetch & install extension from GRASS Addons"), **kwargs):
        self.parent = parent
        self.options = dict() # list of options
        
        wx.Frame.__init__(self, parent = parent, id = id, title = title, **kwargs)
        self.SetIcon(wx.Icon(os.path.join(globalvar.ETCICONDIR, 'grass.ico'), wx.BITMAP_TYPE_ICO))
        
        self.panel = wx.Panel(parent = self, id = wx.ID_ANY)

        self.repoBox = wx.StaticBox(parent = self.panel, id = wx.ID_ANY,
                                    label = " %s " % _("Repository"))
        self.treeBox = wx.StaticBox(parent = self.panel, id = wx.ID_ANY,
                                    label = " %s " % _("List of extensions"))
        
        self.repo = wx.TextCtrl(parent = self.panel, id = wx.ID_ANY)
        self.fullDesc = wx.CheckBox(parent = self.panel, id = wx.ID_ANY,
                                    label = _("Fetch full info including description and keywords"))
        self.fullDesc.SetValue(True)
        
        self.search = SearchModuleWindow(parent = self.panel)
        self.search.SetSelection(0) 
        
        self.tree   = ExtensionTree(parent = self.panel, log = parent.GetLogWindow())
        
        self.optionBox = wx.StaticBox(parent = self.panel, id = wx.ID_ANY,
                                      label = " %s " % _("Options"))
        
        task = gtask.parse_interface('g.extension.py')
        
        ignoreFlags = ['l', 'c', 'g', 'a', 'f', 'quiet', 'verbose']
        if sys.platform == 'win32':
            ignoreFlags.append('d')
            ignoreFlags.append('i')
        
        for f in task.get_options()['flags']:
            name = f.get('name', '')
            desc = f.get('label', '')
            if not desc:
                desc = f.get('description', '')
            if not name and not desc:
                continue
            if name in ignoreFlags:
                continue
            self.options[name] = wx.CheckBox(parent = self.panel, id = wx.ID_ANY,
                                             label = desc)
        self.repo.SetValue(task.get_param(value = 'svnurl').get('default',
                                                                'http://svn.osgeo.org/grass/grass-addons'))
        
        self.statusbar = self.CreateStatusBar(number = 1)
        
        self.btnFetch = wx.Button(parent = self.panel, id = wx.ID_ANY,
                                  label = _("&Fetch"))
        self.btnFetch.SetToolTipString(_("Fetch list of available modules from GRASS Addons SVN repository"))
        self.btnClose = wx.Button(parent = self.panel, id = wx.ID_CLOSE)
        self.btnInstall = wx.Button(parent = self.panel, id = wx.ID_ANY,
                                    label = _("&Install"))
        self.btnInstall.SetToolTipString(_("Install selected add-ons GRASS module"))
        self.btnInstall.Enable(False)
        self.btnCmd = wx.Button(parent = self.panel, id = wx.ID_ANY,
                                label = _("Command dialog"))
        self.btnCmd.SetToolTipString(_('Open %s dialog') % 'g.extension.py')

        self.btnClose.Bind(wx.EVT_BUTTON, self.OnCloseWindow)
        self.btnFetch.Bind(wx.EVT_BUTTON, self.OnFetch)
        self.btnInstall.Bind(wx.EVT_BUTTON, self.OnInstall)
        self.btnCmd.Bind(wx.EVT_BUTTON, self.OnCmdDialog)
        self.tree.Bind(wx.EVT_TREE_ITEM_ACTIVATED, self.OnItemActivated)
        self.tree.Bind(wx.EVT_TREE_SEL_CHANGED,    self.OnItemSelected)
        self.search.Bind(wx.EVT_TEXT_ENTER,        self.OnShowItem)
        self.search.Bind(wx.EVT_TEXT,              self.OnUpdateStatusBar)

        self._layout()

    def _layout(self):
        """!Do layout"""
        sizer = wx.BoxSizer(wx.VERTICAL)
        repoSizer = wx.StaticBoxSizer(self.repoBox, wx.VERTICAL)
        repo1Sizer = wx.BoxSizer(wx.HORIZONTAL)
        repo1Sizer.Add(item = self.repo, proportion = 1,
                      flag = wx.ALL | wx.ALIGN_CENTER_VERTICAL, border = 1)
        repo1Sizer.Add(item = self.btnFetch, proportion = 0,
                      flag = wx.ALL | wx.ALIGN_CENTER_VERTICAL, border = 1)
        repoSizer.Add(item = repo1Sizer,
                      flag = wx.EXPAND)
        repoSizer.Add(item = self.fullDesc)
        
        findSizer = wx.BoxSizer(wx.HORIZONTAL)
        findSizer.Add(item = self.search, proportion = 1)
        
        treeSizer = wx.StaticBoxSizer(self.treeBox, wx.HORIZONTAL)
        treeSizer.Add(item = self.tree, proportion = 1,
                      flag = wx.ALL | wx.EXPAND, border = 1)

        # options
        optionSizer = wx.StaticBoxSizer(self.optionBox, wx.VERTICAL)
        for key in self.options.keys():
            optionSizer.Add(item = self.options[key], proportion = 0)
        
        btnSizer = wx.BoxSizer(wx.HORIZONTAL)
        btnSizer.Add(item = self.btnCmd, proportion = 0,
                     flag = wx.RIGHT, border = 5)
        btnSizer.AddSpacer(10)
        btnSizer.Add(item = self.btnClose, proportion = 0,
                     flag = wx.RIGHT, border = 5)
        btnSizer.Add(item = self.btnInstall, proportion = 0)
        
        sizer.Add(item = repoSizer, proportion = 0,
                  flag = wx.ALL | wx.EXPAND, border = 3)
        sizer.Add(item = findSizer, proportion = 0,
                  flag = wx.LEFT | wx.RIGHT | wx.BOTTOM | wx.EXPAND, border = 3)
        sizer.Add(item = treeSizer, proportion = 1,
                  flag = wx.LEFT | wx.RIGHT | wx.BOTTOM | wx.EXPAND, border = 3)
        sizer.Add(item = optionSizer, proportion = 0,
                        flag = wx.LEFT | wx.RIGHT | wx.BOTTOM | wx.EXPAND, border = 3)
        sizer.Add(item = btnSizer, proportion = 0,
                  flag = wx.ALIGN_RIGHT | wx.ALL, border = 5)
        
        self.panel.SetSizer(sizer)
        sizer.Fit(self.panel)
        
        self.Layout()

    def _getCmd(self):
        item = self.tree.GetSelected()
        if not item or not item.IsOk():
            return ['g.extension.py']
        
        name = self.tree.GetItemText(item)
        if not name:
            GError(_("Extension not defined"), parent = self)
            return
        flags = list()
        for key in self.options.keys():
            if self.options[key].IsChecked():
                flags.append('-%s' % key)
        
        return ['g.extension.py'] + flags + ['extension=' + name,
                                             'svnurl=' + self.repo.GetValue().strip()]
    
    def OnUpdateStatusBar(self, event):
        """!Update statusbar text"""
        element = self.search.GetSelection()
        if not self.tree.IsLoaded():
            self.SetStatusText(_("Fetch list of available extensions by clicking on 'Fetch' button"), 0)
            return
        
        self.tree.SearchItems(element = element,
                              value = event.GetString())
        
        nItems = len(self.tree.itemsMarked)
        if event.GetString():
            self.SetStatusText(_("%d items match") % nItems, 0)
        else:
            self.SetStatusText("", 0)
        
        event.Skip()
    
    def OnCloseWindow(self, event):
        """!Close window"""
        self.Destroy()

    def OnFetch(self, event):
        """!Fetch list of available extensions"""
        wx.BeginBusyCursor()
        self.SetStatusText(_("Fetching list of modules from GRASS-Addons SVN (be patient)..."), 0)
        self.tree.Load(url = self.repo.GetValue().strip(), full = self.fullDesc.IsChecked())
        self.SetStatusText("", 0)
        wx.EndBusyCursor()

    def OnItemActivated(self, event):
        item = event.GetItem()
        data = self.tree.GetPyData(item)
        if data and 'command' in data:
            self.OnInstall(event = None)
        
    def OnInstall(self, event):
        """!Install selected extension"""
        log = self.parent.GetLogWindow()
        log.RunCmd(self._getCmd(), onDone = self.OnDone)
        
    def OnDone(self, cmd, returncode):
        if returncode == 0:
            if not os.getenv('GRASS_ADDON_PATH'):
                SetAddOnPath()
            
            globalvar.UpdateGRASSAddOnCommands()
            log = self.parent.GetLogWindow()
            log.GetPrompt().SetFilter(None)
    
    def OnItemSelected(self, event):
        """!Item selected"""
        item = event.GetItem()
        self.tree.itemSelected = item
        data = self.tree.GetPyData(item)
        if data is None:
            self.SetStatusText('', 0)
            self.btnInstall.Enable(False)
        else:
            self.SetStatusText(data.get('description', ''), 0)
            self.btnInstall.Enable(True)

    def OnShowItem(self, event):
        """!Show selected item"""
        self.tree.OnShowItem(event)
        if self.tree.GetSelected():
            self.btnInstall.Enable()
        else:
            self.btnInstall.Enable(False)

    def OnCmdDialog(self, event):
        """!Shows command dialog"""
        GUI(parent = self).ParseCommand(cmd = self._getCmd())
        
class ExtensionTree(ItemTree):
    """!List of available extensions"""
    def __init__(self, parent, log, id = wx.ID_ANY,
                 ctstyle = CT.TR_HIDE_ROOT | CT.TR_FULL_ROW_HIGHLIGHT | CT.TR_HAS_BUTTONS |
                 CT.TR_LINES_AT_ROOT | CT.TR_SINGLE,
                 **kwargs):
        self.parent = parent # GMFrame
        self.log    = log
        
        super(ExtensionTree, self).__init__(parent, id, ctstyle = ctstyle, **kwargs)
        
        self._initTree()
        
    def _initTree(self):
        for prefix in ('display', 'database',
                       'general', 'imagery',
                       'misc', 'postscript', 'paint',
                       'raster', 'raster3d', 'sites', 'vector', 'wxGUI', 'other'):
            self.AppendItem(parentId = self.root,
                            text = prefix)
        self._loaded = False
        
    def _expandPrefix(self, c):
        name = { 'd'  : 'display',
                 'db' : 'database',
                 'g'  : 'general',
                 'i'  : 'imagery',
                 'm'  : 'misc',
                 'ps' : 'postscript',
                 'p'  : 'paint',
                 'r'  : 'raster',
                 'r3' : 'raster3d',
                 's'  : 'sites',
                 'v'  : 'vector',
                 'wx' : 'wxGUI',
                 ''   : 'other' }
        
        if c in name:
            return name[c]
        
        return c
    
    def _findItem(self, text):
        """!Find item"""
        item = self.GetFirstChild(self.root)[0]
        while item and item.IsOk():
            if text == self.GetItemText(item):
                return item
            
            item = self.GetNextSibling(item)
        
        return None
    
    def Load(self, url, full = False):
        """!Load list of extensions"""
        self.DeleteAllItems()
        self.root = self.AddRoot(_("Menu tree"))
        self._initTree()
        
        if full:
            flags = 'g'
        else:
            flags = 'l'
        ret = RunCommand('g.extension.py', read = True, parent = self,
                         svnurl = url,
                         flags = flags, quiet = True)
        if not ret:
            return
        
        mdict = dict()
        for line in ret.splitlines():
            if full:
                try:
                    key, value = line.split('=', 1)
                except ValueError:
                    key = 'name'
                    value = line
                
                if key == 'name':
                    try:
                        prefix, name = value.split('.', 1)
                    except ValueError:
                        prefix = ''
                        name = value
                    if prefix not in mdict:
                        mdict[prefix] = dict()
                    mdict[prefix][name] = dict()
                else:
                    mdict[prefix][name][key] = value
            else:
                try:
                    prefix, name = line.strip().split('.', 1)
                except:
                    prefix = ''
                    name = line.strip()
                
                if self._expandPrefix(prefix) == prefix:
                    prefix = ''
                    
                if prefix not in mdict:
                    mdict[prefix] = dict()
                    
                mdict[prefix][name] = { 'command' : prefix + '.' + name }
        
        for prefix in mdict.keys():
            prefixName = self._expandPrefix(prefix)
            item = self._findItem(prefixName)
            names = mdict[prefix].keys()
            names.sort()
            for name in names:
                if prefix:
                    text = prefix + '.' + name
                else:
                    text = name
                new = self.AppendItem(parentId = item,
                                      text = text)
                data = dict()
                for key in mdict[prefix][name].keys():
                    data[key] = mdict[prefix][name][key]
                
                self.SetPyData(new, data)
        
        self._loaded = True

    def IsLoaded(self):
        """Check if items are loaded"""
        return self._loaded

class UninstallExtensionWindow(wx.Frame):
    def __init__(self, parent, id = wx.ID_ANY,
                 title = _("Uninstall GRASS Addons extensions"), **kwargs):
        self.parent = parent
        
        wx.Frame.__init__(self, parent = parent, id = id, title = title, **kwargs)
        self.SetIcon(wx.Icon(os.path.join(globalvar.ETCICONDIR, 'grass.ico'), wx.BITMAP_TYPE_ICO))
        
        self.panel = wx.Panel(parent = self, id = wx.ID_ANY)

        self.extBox = wx.StaticBox(parent = self.panel, id = wx.ID_ANY,
                                   label = " %s " % _("List of installed extensions"))
        
        self.extList = CheckListExtension(parent = self.panel)

        # buttons
        self.btnUninstall = wx.Button(parent = self.panel, id = wx.ID_ANY,
                                    label = _("&Uninstall"))
        self.btnUninstall.SetToolTipString(_("Uninstall selected AddOns extensions"))
        self.btnCmd = wx.Button(parent = self.panel, id = wx.ID_ANY,
                                label = _("Command dialog"))
        self.btnCmd.SetToolTipString(_('Open %s dialog') % 'g.extension')
        self.btnClose = wx.Button(parent = self.panel, id = wx.ID_CLOSE)
        
        self.btnUninstall.Bind(wx.EVT_BUTTON, self.OnUninstall)
        self.btnCmd.Bind(wx.EVT_BUTTON, self.OnCmdDialog)
        self.btnClose.Bind(wx.EVT_BUTTON, self.OnCloseWindow)
        
        self._layout()
        
    def _layout(self):
        """!Do layout"""
        sizer = wx.BoxSizer(wx.VERTICAL)
        
        extSizer = wx.StaticBoxSizer(self.extBox, wx.HORIZONTAL)
        extSizer.Add(item = self.extList, proportion = 1,
                     flag = wx.ALL | wx.EXPAND, border = 1)
        
        btnSizer = wx.BoxSizer(wx.HORIZONTAL)
        btnSizer.Add(item = self.btnCmd, proportion = 0,
                     flag = wx.RIGHT, border = 5)
        btnSizer.AddSpacer(10)
        btnSizer.Add(item = self.btnClose, proportion = 0,
                     flag = wx.RIGHT, border = 5)
        btnSizer.Add(item = self.btnUninstall, proportion = 0)
        
        sizer.Add(item = extSizer, proportion = 1,
                  flag = wx.ALL | wx.EXPAND, border = 3)
        sizer.Add(item = btnSizer, proportion = 0,
                  flag = wx.ALIGN_RIGHT | wx.ALL, border = 5)
        
        self.panel.SetSizer(sizer)
        sizer.Fit(self.panel)
        
        self.Layout()

    def OnCloseWindow(self, event):
        """!Close window"""
        self.Destroy()

    def OnUninstall(self, event):
        """!Uninstall selected extensions"""
        log = self.parent.GetLogWindow()
        eList = self.extList.GetExtensions()
        if not eList:
            GError(_("No extension selected for removal. "
                     "Operation canceled."),
                   parent = self)
            return
        
        for ext in eList:
            files = RunCommand('g.extension.py', parent = self, read = True, quiet = True,
                               extension = ext, operation = 'remove').splitlines()
            dlg = wx.MessageDialog(parent = self,
                                   message = _("List of files to be removed:\n%(files)s\n\n"
                                               "Do you want really to remove <%(ext)s> extension?") % \
                                       { 'files' : os.linesep.join(files), 'ext' : ext },
                                   caption = _("Remove extension"),
                                   style = wx.YES_NO | wx.NO_DEFAULT | wx.ICON_QUESTION)
            
            if dlg.ShowModal() ==  wx.ID_YES:
                RunCommand('g.extension.py', flags = 'f', parent = self, quiet = True,
                           extension = ext, operation = 'remove')
        
        self.extList.LoadData()
        
        # update prompt
        globalvar.UpdateGRASSAddOnCommands(eList)
        log = self.parent.GetLogWindow()
        log.GetPrompt().SetFilter(None)
        
    def OnCmdDialog(self, event):
        """!Shows command dialog"""
        GUI(parent = self).ParseCommand(cmd = ['g.extension.py'])

class CheckListExtension(wx.ListCtrl, listmix.ListCtrlAutoWidthMixin, listmix.CheckListCtrlMixin):
    """!List of mapset/owner/group"""
    def __init__(self, parent):
        self.parent = parent
        
        wx.ListCtrl.__init__(self, parent, id = wx.ID_ANY,
                             style = wx.LC_REPORT)
        listmix.CheckListCtrlMixin.__init__(self)
        
        # setup mixins
        listmix.ListCtrlAutoWidthMixin.__init__(self)

        self.InsertColumn(0, _('Extension'))
        self.LoadData()
        
    def LoadData(self):
        """!Load data into list"""
        self.DeleteAllItems()
        for ext in RunCommand('g.extension.py',
                              quiet = True, parent = self, read = True,
                              flags = 'a').splitlines():
            if ext:
                self.InsertStringItem(sys.maxint, ext)

    def GetExtensions(self):
        """!Get extensions to be un-installed
        """
        extList = list()
        for i in range(self.GetItemCount()):
            if self.IsChecked(i):
                name = self.GetItemText(i)
                if name:
                    extList.append(name)
        
        return extList
