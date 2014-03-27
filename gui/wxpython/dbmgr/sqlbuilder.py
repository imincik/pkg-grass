"""!
@package dbmgr.sqlbuilder

@brief GRASS SQL Builder

Classes:
 - sqlbuilder::SQLFrame

Usage:
@code
python sqlbuilder.py vector_map
@endcode

(C) 2007-2009, 2011 by the GRASS Development Team

This program is free software under the GNU General Public License
(>=v2). Read the file COPYING that comes with GRASS for details.

@author Jachym Cepicky <jachym.cepicky gmail.com> (original author)
@author Martin Landa <landa.martin gmail.com>
@author Hamish Bowman <hamish_b yahoo com>
"""

import os
import sys

if __name__ == "__main__":
    sys.path.append(os.path.join(os.getenv('GISBASE'), 'etc', 'gui', 'wxpython'))
from core import globalvar
import wx

from core.gcmd   import RunCommand, GError
from dbmgr.vinfo import createDbInfoDesc, VectorDBInfo

import grass.script as grass

class SQLFrame(wx.Frame):
    """!SQL Frame class"""
    def __init__(self, parent, title, vectmap, id = wx.ID_ANY,
                 layer = 1, qtype = "select", evtHandler = None):
        
        wx.Frame.__init__(self, parent, id, title)
        
        self.SetIcon(wx.Icon(os.path.join(globalvar.ETCICONDIR, 'grass_sql.ico'),
                             wx.BITMAP_TYPE_ICO))
        
        self.parent = parent
        self.evtHandler = evtHandler

        #
        # variables
        #
        self.vectmap = vectmap # fullname
        if not "@" in self.vectmap:
            self.vectmap = grass.find_file(self.vectmap, element = 'vector')['fullname']
        self.mapname, self.mapset = self.vectmap.split("@", 1)
        
        # db info
        self.layer = layer
        self.dbInfo = VectorDBInfo(self.vectmap)
        self.tablename = self.dbInfo.GetTable(self.layer)
        self.driver, self.database = self.dbInfo.GetDbSettings(self.layer)
        
        self.qtype = qtype      # type of query: SELECT, UPDATE, DELETE, ...
        self.colvalues = []     # array with unique values in selected column

        # set dialog title
        self.SetTitle(_("GRASS SQL Builder (%(type)s): vector map <%(map)s>") % \
                          { 'type' : self.qtype.upper(), 'map' : self.vectmap })
        
        self.panel = wx.Panel(parent = self, id = wx.ID_ANY)

        # statusbar
        self.statusbar = self.CreateStatusBar(number=1)
        self.statusbar.SetStatusText(_("SQL statement not verified"), 0)
       
        self._doLayout()

    def _doLayout(self):
        """!Do dialog layout"""
      
        pagesizer = wx.BoxSizer(wx.VERTICAL)

        
        # dbInfo
        databasebox = wx.StaticBox(parent = self.panel, id = wx.ID_ANY,
                                   label = " %s " % _("Database connection"))
        databaseboxsizer = wx.StaticBoxSizer(databasebox, wx.VERTICAL)
        databaseboxsizer.Add(item=createDbInfoDesc(self.panel, self.dbInfo, layer = self.layer),
                             proportion=1,
                             flag=wx.EXPAND | wx.ALL,
                             border=3)

        #
        # text areas
        #
        # sql box
        sqlbox = wx.StaticBox(parent = self.panel, id = wx.ID_ANY,
                              label = " %s " % _("Query"))
        sqlboxsizer = wx.StaticBoxSizer(sqlbox, wx.VERTICAL)

        self.text_sql = wx.TextCtrl(parent = self.panel, id = wx.ID_ANY,
                                    value = '', size = (-1, 50),
                                    style=wx.TE_MULTILINE)
        if self.qtype.lower() == "select":
            self.text_sql.SetValue("SELECT * FROM %s" % self.tablename)
        self.text_sql.SetInsertionPointEnd()
        self.text_sql.SetToolTipString(_("Example: %s") % "SELECT * FROM roadsmajor WHERE MULTILANE = 'no' OR OBJECTID < 10")
        wx.CallAfter(self.text_sql.SetFocus)

        sqlboxsizer.Add(item = self.text_sql, flag = wx.EXPAND)
        
        #
        # buttons
        #
        self.btn_clear  = wx.Button(parent = self.panel, id = wx.ID_CLEAR)
        self.btn_clear.SetToolTipString(_("Set SQL statement to default"))
        self.btn_verify = wx.Button(parent = self.panel, id = wx.ID_ANY,
                                    label = _("Verify"))
        self.btn_verify.SetToolTipString(_("Verify SQL statement"))
        self.btn_apply  = wx.Button(parent = self.panel, id = wx.ID_APPLY)
        self.btn_apply.SetToolTipString(_("Apply SQL statement and close the dialog"))
        self.btn_close  = wx.Button(parent = self.panel, id = wx.ID_CLOSE)
        self.btn_close.SetToolTipString(_("Close the dialog"))
        
        self.btn_lv = { 'is'    : ['=', ],
                        'isnot' : ['!=', ],
                        'like'  : ['LIKE', ],
                        'gt'    : ['>', ],
                        'ge'    : ['>=', ],
                        'lt'    : ['<', ],
                        'le'    : ['<=', ],
                        'or'    : ['OR', ],
                        'not'   : ['NOT', ],
                        'and'   : ['AND', ],
                        'brac'  : ['()', ],
                        'prc'   : ['%', ] }
        
        for key, value in self.btn_lv.iteritems():
            btn = wx.Button(parent = self.panel, id = wx.ID_ANY,
                            label = value[0])
            self.btn_lv[key].append(btn.GetId())
        
        buttonsizer = wx.FlexGridSizer(cols = 4, hgap = 5, vgap = 5)
        buttonsizer.Add(item = self.btn_clear)
        buttonsizer.Add(item = self.btn_verify)
        buttonsizer.Add(item = self.btn_apply)
        buttonsizer.Add(item = self.btn_close)
        
        buttonsizer2 = wx.GridBagSizer(5, 5)
        buttonsizer2.Add(item = self.FindWindowById(self.btn_lv['is'][1]), pos = (0,0))
        buttonsizer2.Add(item = self.FindWindowById(self.btn_lv['isnot'][1]), pos = (1,0))
        buttonsizer2.Add(item = self.FindWindowById(self.btn_lv['like'][1]), pos = (2, 0))

        buttonsizer2.Add(item = self.FindWindowById(self.btn_lv['gt'][1]), pos = (0, 1))
        buttonsizer2.Add(item = self.FindWindowById(self.btn_lv['ge'][1]), pos = (1, 1))
        buttonsizer2.Add(item = self.FindWindowById(self.btn_lv['or'][1]), pos = (2, 1))

        buttonsizer2.Add(item = self.FindWindowById(self.btn_lv['lt'][1]), pos = (0, 2))
        buttonsizer2.Add(item = self.FindWindowById(self.btn_lv['le'][1]), pos = (1, 2))
        buttonsizer2.Add(item = self.FindWindowById(self.btn_lv['not'][1]), pos = (2, 2))

        buttonsizer2.Add(item = self.FindWindowById(self.btn_lv['brac'][1]), pos = (0, 3))
        buttonsizer2.Add(item = self.FindWindowById(self.btn_lv['prc'][1]), pos = (1, 3))
        buttonsizer2.Add(item = self.FindWindowById(self.btn_lv['and'][1]), pos = (2, 3))
        
        #
        # list boxes (columns, values)
        #
        hsizer = wx.BoxSizer(wx.HORIZONTAL)
        
        columnsbox = wx.StaticBox(parent = self.panel, id = wx.ID_ANY,
                                  label = " %s " % _("Columns"))
        columnsizer = wx.StaticBoxSizer(columnsbox, wx.VERTICAL)
        self.list_columns = wx.ListBox(parent = self.panel, id = wx.ID_ANY,
                                       choices = self.dbInfo.GetColumns(self.tablename),
                                       style = wx.LB_MULTIPLE)
        columnsizer.Add(item = self.list_columns, proportion = 1,
                        flag = wx.EXPAND)

        radiosizer = wx.BoxSizer(wx.HORIZONTAL)
        self.radio_cv = wx.RadioBox(parent = self.panel, id = wx.ID_ANY,
                                    label = " %s " % _("Add on double-click"),
                                    choices = [_("columns"), _("values")])
        self.radio_cv.SetSelection(1) # default 'values'
        radiosizer.Add(item = self.radio_cv, proportion = 1,
                       flag = wx.ALIGN_CENTER_HORIZONTAL | wx.EXPAND, border = 5)

        columnsizer.Add(item = radiosizer, proportion = 0,
                        flag = wx.TOP | wx.EXPAND, border = 5)
        # self.list_columns.SetMinSize((-1,130))
        # self.list_values.SetMinSize((-1,100))

        valuesbox = wx.StaticBox(parent = self.panel, id = wx.ID_ANY,
                                 label = " %s " % _("Values"))
        valuesizer = wx.StaticBoxSizer(valuesbox, wx.VERTICAL)
        self.list_values = wx.ListBox(parent = self.panel, id = wx.ID_ANY,
                                      choices = self.colvalues,
                                      style = wx.LB_MULTIPLE)
        valuesizer.Add(item = self.list_values, proportion = 1,
                       flag = wx.EXPAND)
        
        self.btn_unique = wx.Button(parent = self.panel, id = wx.ID_ANY,
                                    label = _("Get all values"))
        self.btn_unique.Enable(False)
        self.btn_uniquesample = wx.Button(parent = self.panel, id = wx.ID_ANY,
                                          label = _("Get sample"))
        self.btn_uniquesample.Enable(False)

        buttonsizer3 = wx.BoxSizer(wx.HORIZONTAL)
        buttonsizer3.Add(item = self.btn_uniquesample, proportion = 0,
                         flag = wx.ALIGN_CENTER_HORIZONTAL | wx.RIGHT, border = 5)
        buttonsizer3.Add(item = self.btn_unique, proportion = 0,
                         flag = wx.ALIGN_CENTER_HORIZONTAL)

        valuesizer.Add(item = buttonsizer3, proportion = 0,
                       flag = wx.TOP, border = 5)
        
        # hsizer1.Add(wx.StaticText(self.panel,-1, "Unique values: "), border=0, proportion=1)
 
        hsizer.Add(item = columnsizer, proportion = 1,
                   flag = wx.EXPAND)
        hsizer.Add(item = valuesizer, proportion = 1,
                   flag = wx.EXPAND)
        
        self.close_onapply = wx.CheckBox(parent = self.panel, id = wx.ID_ANY,
                                         label = _("Close dialog on apply"))
        self.close_onapply.SetValue(True)
 
        pagesizer.Add(item = databaseboxsizer,
                      flag = wx.ALL | wx.EXPAND, border = 5)
        pagesizer.Add(item = hsizer, proportion = 1,
                      flag = wx.LEFT | wx.RIGHT | wx.BOTTOM | wx.EXPAND, border = 5)
        # pagesizer.Add(self.btn_uniqe,0,wx.ALIGN_LEFT|wx.TOP,border=5)
        # pagesizer.Add(self.btn_uniqesample,0,wx.ALIGN_LEFT|wx.TOP,border=5)
        pagesizer.Add(item = buttonsizer2, proportion = 0,
                      flag = wx.ALIGN_CENTER_HORIZONTAL)
        pagesizer.Add(item = sqlboxsizer, proportion = 0,
                      flag = wx.EXPAND | wx.LEFT | wx.RIGHT, border = 5)
        pagesizer.Add(item = buttonsizer, proportion = 0,
                      flag = wx.ALIGN_RIGHT | wx.ALL, border = 5)
        pagesizer.Add(item = self.close_onapply, proportion = 0,
                      flag = wx.LEFT | wx.RIGHT | wx.BOTTOM | wx.EXPAND, border = 5)

        #
        # bindings
        #
        self.btn_unique.Bind(wx.EVT_BUTTON,       self.OnUniqueValues)
        self.btn_uniquesample.Bind(wx.EVT_BUTTON, self.OnSampleValues)
        
        for key, value in self.btn_lv.iteritems():
            self.FindWindowById(value[1]).Bind(wx.EVT_BUTTON, self.OnAddMark)
        
        self.btn_close.Bind(wx.EVT_BUTTON,       self.OnClose)
        self.btn_clear.Bind(wx.EVT_BUTTON,       self.OnClear)
        self.btn_verify.Bind(wx.EVT_BUTTON,      self.OnVerify)
        self.btn_apply.Bind(wx.EVT_BUTTON,       self.OnApply)

        self.list_columns.Bind(wx.EVT_LISTBOX,   self.OnAddColumn)
        self.list_values.Bind(wx.EVT_LISTBOX,    self.OnAddValue)
        
        self.text_sql.Bind(wx.EVT_TEXT,          self.OnText)
        
        self.panel.SetAutoLayout(True)
        self.panel.SetSizer(pagesizer)
        pagesizer.Fit(self.panel)
        
        self.Layout()
        self.SetMinSize((660, 525))
        self.SetClientSize(self.panel.GetSize())
        self.CenterOnParent()
        
    def OnUniqueValues(self, event, justsample = False):
        """!Get unique values"""
        vals = []
        try:
            idx = self.list_columns.GetSelections()[0]
            column = self.list_columns.GetString(idx)
        except:
            self.list_values.Clear()
            return
        
        self.list_values.Clear()
        
        data = grass.db_select(sql = "SELECT %s FROM %s" % (column, self.tablename),
                               database = self.database,
                               driver = self.driver)
        if not data:
            return
        
        desc = self.dbInfo.GetTableDesc(self.dbInfo.GetTable(self.layer))[column]
        
        i = 0
        for item in sorted(set(map(lambda x: desc['ctype'](x[0]), data))):
            if justsample and i > 255:
                break
            
            if desc['type'] != 'character':
                item = str(item)
            self.list_values.Append(item)
            i += 1
        
    def OnSampleValues(self, event):
        """!Get sample values"""
        self.OnUniqueValues(None, True)

    def OnAddColumn(self, event):
        """!Add column name to the query"""
        idx = self.list_columns.GetSelections()
        for i in idx:
            column = self.list_columns.GetString(i)
            self._add(element = 'column', value = column)
        
        if not self.btn_uniquesample.IsEnabled():
            self.btn_uniquesample.Enable(True)
            self.btn_unique.Enable(True)
        
    def OnAddValue(self, event):
        """!Add value"""
        selection = self.list_values.GetSelections()
        if not selection:
            event.Skip()
            return

        idx = selection[0]
        value = self.list_values.GetString(idx)
        idx = self.list_columns.GetSelections()[0]
        column = self.list_columns.GetString(idx)
        
        ctype = self.dbInfo.GetTableDesc(self.dbInfo.GetTable(self.layer))[column]['type']
        
        if ctype == 'character':
            value = "'%s'" % value
        
        self._add(element = 'value', value = value)

    def OnAddMark(self, event):
        """!Add mark"""
        mark = None
        for key, value in self.btn_lv.iteritems():
            if event.GetId() == value[1]:
                mark = value[0]
                break
        
        self._add(element = 'mark', value = mark)

    def _add(self, element, value):
        """!Add element to the query

        @param element element to add (column, value)
        """
        sqlstr = self.text_sql.GetValue()
        newsqlstr = ''
        if element == 'column':
            if self.radio_cv.GetSelection() == 0: # -> column
                idx1 = len('select')
                idx2 = sqlstr.lower().find('from')
                colstr = sqlstr[idx1:idx2].strip()
                if colstr == '*':
                    cols = []
                else:
                    cols = colstr.split(',')
                if value in cols:
                        cols.remove(value)
                else:
                    cols.append(value)
                
                if len(cols) < 1:
                    cols = ['*',]
                
                newsqlstr = 'SELECT ' + ','.join(cols) + ' ' + sqlstr[idx2:]
            else: # -> where
                newsqlstr = sqlstr
                if sqlstr.lower().find('where') < 0:
                    newsqlstr += ' WHERE'
                
                newsqlstr += ' ' + value
        
        elif element == 'value':
            newsqlstr = sqlstr + ' ' + value
        elif element == 'mark':
            newsqlstr = sqlstr + ' ' + value
        
        if newsqlstr:
            self.text_sql.SetValue(newsqlstr)

    def GetSQLStatement(self):
        """!Return SQL statement"""
        return self.text_sql.GetValue().strip().replace("\n"," ")
    
    def CloseOnApply(self):
        """!Return True if the dialog will be close on apply"""
        return self.close_onapply.IsChecked()
    
    def OnText(self, event):
        """Query string changed"""
        if len(self.text_sql.GetValue()) > 0:
            self.btn_verify.Enable(True)
        else:
            self.btn_verify.Enable(False)
        
    def OnApply(self, event):
        """Apply button pressed"""
        if self.evtHandler:
            self.evtHandler(event = 'apply')
        
        if self.close_onapply.IsChecked():
            self.Destroy()
            
        event.Skip()
    
    def OnVerify(self, event):
        """!Verify button pressed"""
        ret, msg = RunCommand('db.select',
                              getErrorMsg = True,
                              table = self.tablename,
                              sql = self.text_sql.GetValue(),
                              flags = 't',
                              driver = self.driver,
                              database = self.database)
        
        if ret != 0 and msg:
            self.statusbar.SetStatusText(_("SQL statement is not valid"), 0)
            GError(parent = self,
                   message = _("SQL statement is not valid.\n\n%s") % msg)
        else:
            self.statusbar.SetStatusText(_("SQL statement is valid"), 0)
                        
    def OnClear(self, event):
        """!Clear button pressed"""
        if self.qtype.lower() == "select":
            self.text_sql.SetValue("SELECT * FROM %s" % self.tablename)
        else:
            self.text_sql.SetValue("")
    
    def OnClose(self, event):
        """!Close button pressed"""
        if self.evtHandler:
            self.evtHandler(event = 'close')
        
        self.Destroy()
        
        event.Skip()
        
if __name__ == "__main__":
    if len(sys.argv) != 2:
        print >>sys.stderr, __doc__
        sys.exit()
    
    import gettext
    gettext.install('grasswxpy', os.path.join(os.getenv("GISBASE"), 'locale'), unicode=True)
    
    app = wx.App(0)
    sqlb = SQLFrame(parent = None, title = _('SQL Builder'), vectmap = sys.argv[1])
    sqlb.Show()
    
    app.MainLoop()
