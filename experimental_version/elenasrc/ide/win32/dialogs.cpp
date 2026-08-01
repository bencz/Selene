//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      Static dialogs implementation
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "dialogs.h"
#include "idesettings.h"
#include "elenaconst.h"

using namespace _GUI_;
using namespace _ELENA_;

// --- FileDialog ---

TCHAR* FileDialog :: ProjectFilter = _T("ELENA Project file\0*.prj\0All types\0*.*\0\0");
TCHAR* FileDialog :: SourceFilter = _T("ELENA source file\0*.l\0All types\0*.*\0\0");

FileDialog :: FileDialog(Control* owner, const TCHAR* filter, const TCHAR* caption, const TCHAR* initialDir)
{
   _fileName[0] = '\0';

   ZeroMemory(&_struct, sizeof(_struct));
   _struct.lStructSize = sizeof(_struct);
   _struct.hwndOwner = owner->getHandle();
   _struct.hInstance = owner->getInstance();
   _struct.lpstrCustomFilter = (LPTSTR) NULL;
   _struct.nMaxCustFilter = 0L;
   _struct.nFilterIndex = 1L;
   _struct.lpstrFilter = filter;
   _struct.lpstrFile = _fileName;
   _struct.nMaxFile = sizeof(_fileName);
   _struct.lpstrFileTitle = NULL;
   _struct.nMaxFileTitle = 0;
   _struct.lpstrInitialDir = emptystr(initialDir) ? NULL : initialDir;
   _struct.lpstrTitle = caption;
   _struct.nFileOffset  = 0;
   _struct.nFileExtension = 0;
   _struct.lpstrDefExt = NULL;
   _struct.lCustData = 0;
   _struct.lpfnHook = NULL;
   _struct.lpTemplateName = NULL;

   _defaultFlags = OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_LONGNAMES | DS_CENTER | OFN_HIDEREADONLY;
}

bool FileDialog :: openFiles(List<TCHAR*>& files)
{
   _struct.Flags = _defaultFlags | OFN_FILEMUSTEXIST | OFN_ALLOWMULTISELECT;

   files.clear();
   if (::GetOpenFileName(&_struct)) {
      if (emptystr(_fileName + getlength(_fileName) + 1)) {
         files.add(_ELENA_::strdup(_fileName));
      }
      else {
         Path path;
         const TCHAR* p = _fileName + getlength(_fileName) + 1;

         while (!emptystr(p)) {
            path.copy(_fileName);
            path.combine(p);

            files.add(_ELENA_::strdup(path));

            p += getlength(p) + 1;
         }
      }
      return true;
   }
   else return false;
}

const TCHAR* FileDialog :: openFile()
{
   _struct.Flags = _defaultFlags;

   if (::GetOpenFileName(&_struct)) {
      return _fileName;
   }
   else return NULL;
}

bool FileDialog :: saveFile(const TCHAR* defaultExt, Path& path)
{
   _struct.Flags = _defaultFlags | OFN_PATHMUSTEXIST;
   _struct.lpstrDefExt = defaultExt;

   if (::GetSaveFileName(&_struct)) {
      path.copy(_fileName);

      return true;
   }
   else return false;
}

// --- Dialog ---

Dialog :: Dialog(Control* owner)
{
   _owner = owner;
   _self = NULL;
}

BOOL CALLBACK Dialog :: DialogProc(HWND hWnd, size_t message, WPARAM wParam, LPARAM lParam)
{
   Dialog* dialog = (Dialog*)::GetWindowLong(hWnd, GWL_USERDATA);
   switch (message) {
      case WM_INITDIALOG:
         dialog = (Dialog*)lParam;
         dialog->_self = hWnd;
         ::SetWindowLong(hWnd, GWL_USERDATA, (long)lParam);

         dialog->onCreate();

         return 0;
      case WM_COMMAND:
         dialog->doCommand(LOWORD(wParam), HIWORD(wParam));
         return TRUE;
      default:
         return FALSE;
   }
}

void Dialog :: doCommand(int id, int command)
{
   switch (id) {
      case IDOK:
         onOK();
         ::EndDialog(_self, -1);
         break;
      case IDCANCEL:
         ::EndDialog(_self, 0);
         break;
   }
}

int Dialog :: showModal()
{
   return ::DialogBoxParam(_owner->getInstance(), MAKEINTRESOURCE(getDialogID()),
	   _owner->getHandle(), (DLGPROC)DialogProc, (LPARAM)this);
}

void Dialog :: enable(int id, bool enabled)
{
   ::EnableWindow(::GetDlgItem(_self, id), enabled? TRUE : FALSE);
}

void Dialog :: getText(int id, TCHAR** text, int length)
{
   ::SendDlgItemMessage(_self, id, WM_GETTEXT, length, (LPARAM)text);
}

int Dialog :: getIntText(int id)
{
   TCHAR s[13];

   ::SendDlgItemMessage(_self, id, WM_GETTEXT, 12, (LPARAM)s);

   return _ttoi(s);
}

bool Dialog :: getCheckState(int id)
{
   return test(::SendDlgItemMessage(_self, id, BM_GETCHECK, 0, 0), BST_CHECKED);
}

int Dialog :: getComboBoxIndex(int id)
{
   return ::SendDlgItemMessage(_self, id, CB_GETCURSEL, 0, 0);
}

int Dialog :: getListCount(int id)
{
   return ::SendDlgItemMessage(_self, id, LB_GETCOUNT, 0, 0);
}

void Dialog :: getListItem(int id, int index, TCHAR** text)
{
   ::SendDlgItemMessage(_self, id, LB_GETTEXT, index, (LPARAM)text);
}

int Dialog :: getListIndex(int id)
{
   return ::SendDlgItemMessage(_self, id, LB_GETCURSEL, 0, 0);
}

void Dialog :: setText(int id, const TCHAR* text)
{
   ::SendDlgItemMessage(_self, id, WM_SETTEXT, 0, (LPARAM)text);
}

void Dialog :: setIntText(int id, int value)
{
   LocalString<15> s;
   s.appendInt(value);

   ::SendDlgItemMessage(_self, id, WM_SETTEXT, 0, (LPARAM)s.asString());
}

void Dialog :: setCheckState(int id, bool value)
{
   ::SendDlgItemMessage(_self, id, BM_SETCHECK, value ? BST_CHECKED : BST_UNCHECKED, 0);
}

void Dialog :: setComboBoxIndex(int id, int index)
{
   ::SendDlgItemMessage(_self, id, CB_SETCURSEL, index, 0);
}

void Dialog :: setListIndex(int id, int index)
{
   ::SendDlgItemMessage(_self, id, LB_SETCURSEL, index, 0);
}

void Dialog :: setTextLimit(int id, int maxLength)
{
   ::SendDlgItemMessage(_self, id, EM_SETLIMITTEXT, maxLength, 0);
}

void Dialog :: addComboBoxItem(int id, const TCHAR* text)
{
   ::SendDlgItemMessage(_self, id, CB_ADDSTRING, 0, (LPARAM)text);
}

void Dialog :: addListItem(int id, const TCHAR* text)
{
   ::SendDlgItemMessage(_self, id, LB_ADDSTRING, 0, (LPARAM)text);
}

void Dialog :: insertListItem(int id, int index, const TCHAR* text)
{
   ::SendDlgItemMessage(_self, id, LB_INSERTSTRING, index, (LPARAM)text);
}

void Dialog :: removeListItem(int id, int index)
{
   ::SendDlgItemMessage(_self, id, LB_DELETESTRING, index, (LPARAM)0);
}

int  Dialog :: getListSelCount(int id)
{
   return ::SendDlgItemMessage(_self, id, LB_GETSELCOUNT, 0, 0);
}

void Dialog :: getListSelected(int id, int count, int* selected)
{
   ::SendDlgItemMessage(_self, id, LB_GETSELITEMS, count, (LPARAM)selected);
}

void Dialog :: setListSelected(int id, int index, bool toggle)
{
   ::SendDlgItemMessage(_self, id, LB_SETSEL, toggle ? TRUE : FALSE, index);
}

// --- ProjectSettingsDialog ---

void ProjectSettingsDialog :: loadTemplateList()
{
   Path configPath(Paths::appPath, _T("elc.cfg"));
   _ELENA_::IniConfigFile config;
   if (!config.load(configPath))
      return;

   const TCHAR* curTemplate = Settings::project.getTemplate();

   int current = 0;
   for (_ELENA_::ConfigCategoryIterator it = config.getCategoryIt(_T("templates")) ; !it.Eof() ; it++, current++) {
      addComboBoxItem(IDC_SETTINGS_TEPMPLATE, it.key());

      if (compstr(curTemplate, it.key()))
         setComboBoxIndex(IDC_SETTINGS_TEPMPLATE, current);
   }
}

void ProjectSettingsDialog :: onCreate()
{
   setTextLimit(IDC_SETTINGS_ENTRY, IDENTIFIER_LEN);
   setTextLimit(IDC_SETTINGS_PACKAGE, IDENTIFIER_LEN);

   setText(IDC_SETTINGS_PACKAGE, Settings::project.getPackage());
   setText(IDC_SETTINGS_ENTRY, Settings::project.getStartSymbol());
   setText(IDC_SETTINGS_TARGET, Settings::project.getTarget());
   setText(IDC_SETTINGS_OUTPUT, Settings::project.getOutputPath());
   setText(IDC_SETTINGS_ARGUMENT, Settings::project.getArguments());
   setText(IDC_SETTINGS_OPTIONS, Settings::project.getOptions());

   setCheckState(IDC_SETTINGS_DEBUG, Settings::project.getDebugInfoEnabled());
   setCheckState(IDC_SETTINGS_WARN_REF, Settings::project.getBoolSetting(_T("warn:unresolved")));

   addComboBoxItem(IDC_SETTINGS_TYPE, _T("ELENA Library"));
   addComboBoxItem(IDC_SETTINGS_TYPE, _T("Win32 Console"));
   addComboBoxItem(IDC_SETTINGS_TYPE, _T("Win32 GUI"));

   loadTemplateList();

   setComboBoxIndex(IDC_SETTINGS_TYPE, Settings::project.getType());
   onProjectTypeChanged();
}

void ProjectSettingsDialog :: onOK()
{
   TCHAR path[MAX_PATH + 1];
   getText(IDC_SETTINGS_TARGET, (TCHAR**)(&path), MAX_PATH);

   if (!emptystr(path)) {
      if (Path::checkExtension(path, NULL)) {
         _tcscat(path, _T(".exe"));
      }
      Settings::project.setTarget(path);
   }
   else Settings::project.setTarget(NULL);

   getText(IDC_SETTINGS_ARGUMENT, (TCHAR**)(&path), MAX_PATH);
   Settings::project.setArguments(path);

   getText(IDC_SETTINGS_OUTPUT, (TCHAR**)(&path), MAX_PATH);
   Settings::project.setOutputPath(path);

   getText(IDC_SETTINGS_OPTIONS, (TCHAR**)(&path), MAX_PATH);
   Settings::project.setOptions(path);

   TCHAR name[IDENTIFIER_LEN + 1] ;
   getText(IDC_SETTINGS_ENTRY, (TCHAR**)(&name), IDENTIFIER_LEN);
   Settings::project.setStartSymbol(name);

   getText(IDC_SETTINGS_PACKAGE, (TCHAR**)(&name), IDENTIFIER_LEN);
   Settings::project.setPackage(name);

   if (getComboBoxIndex(IDC_SETTINGS_TEPMPLATE) != -1) {
      getText(IDC_SETTINGS_TEPMPLATE, (TCHAR**)(&name), IDENTIFIER_LEN);
      Settings::project.setTemplate(name);
   }

   Settings::project.setType(getComboBoxIndex(IDC_SETTINGS_TYPE));

   Settings::project.setDebugInfoEnabled(getCheckState(IDC_SETTINGS_DEBUG));

   Settings::project.setBoolSetting(_T("warn:unresolved"), getCheckState(IDC_SETTINGS_WARN_REF));
}

void ProjectSettingsDialog :: doCommand(int id, int msg)
{
   if (id==IDC_SETTINGS_TYPE) {
      onProjectTypeChanged();
   }
   else Dialog::doCommand(id, msg);
}

void ProjectSettingsDialog :: onProjectTypeChanged()
{
   int index = getComboBoxIndex(IDC_SETTINGS_TYPE);
   if (index == 0) {
      setText(IDC_SETTINGS_ENTRY, NULL);
      setText(IDC_SETTINGS_TARGET, NULL);
      setCheckState(IDC_SETTINGS_DEBUG, false);

      enable(IDC_SETTINGS_ENTRY, false);
      enable(IDC_SETTINGS_TARGET, false);
      enable(IDC_SETTINGS_DEBUG, false);
      enable(IDC_SETTINGS_ARGUMENT, false);
   }
   else {
      enable(IDC_SETTINGS_ENTRY, true);
      enable(IDC_SETTINGS_TARGET, true);
      enable(IDC_SETTINGS_DEBUG, true);
      enable(IDC_SETTINGS_ARGUMENT, true);
   }
}

// --- ProjectForwardsDialog ---

bool ProjectForwardsDialog :: validateItem(TCHAR* &text)
{
   // trim space
   while (text[0]==' ') text++;
   while (getlength(text) > 0 && text[getlength(text) - 1]==' ') text[getlength(text) - 1] = 0;

   if (emptystr(text))
      return false;
   else if (chrpos(text, '=')==-1) {
      MsgBox::show(_owner->getHandle(), _T("The forward should have the following structure: <forward name>=<full class name>\n(e.g. 'integer=std'basic'integer)"), MB_ICONERROR);
	  return false;
   }
   else return true;
}

void ProjectForwardsDialog :: addItem()
{
   TCHAR item[IDENTIFIER_LEN * 2 + 1];

   getText(IDC_FORWARDS_EDIT, (TCHAR**)(&item), IDENTIFIER_LEN * 2);

   TCHAR* s = item;
   if (validateItem(s)) {
      addListItem(IDC_FORWARDS_LIST, s);
	  _changed = true;
   }
   setText(IDC_FORWARDS_EDIT, NULL);
   _current = -1;
   _changed = true;
}

void ProjectForwardsDialog :: getItem()
{
   _current = getListIndex(IDC_FORWARDS_LIST);

   TCHAR item[IDENTIFIER_LEN * 2 + 1];

   getListItem(IDC_FORWARDS_LIST, _current, (TCHAR**)(&item));
   setText(IDC_FORWARDS_EDIT, item);
}

void ProjectForwardsDialog :: editItem()
{
   if (_current != -1) {
      TCHAR item[IDENTIFIER_LEN * 2 + 1];

      getText(IDC_FORWARDS_EDIT, (TCHAR**)(&item), IDENTIFIER_LEN * 2);

      TCHAR* s = item;
      if (validateItem(s)) {
         removeListItem(IDC_FORWARDS_LIST, _current);
         insertListItem(IDC_FORWARDS_LIST, _current, s);
	     _changed = true;
      }
      setText(IDC_FORWARDS_EDIT, NULL);
      _current = -1;
	  _changed = true;
   }
}

void ProjectForwardsDialog :: deleteItem()
{
   int index = _current = getListIndex(IDC_FORWARDS_LIST);

   removeListItem(IDC_FORWARDS_LIST, index);
   setText(IDC_FORWARDS_EDIT, NULL);

   _current = -1;
   _changed = true;
}


void ProjectForwardsDialog :: onCreate()
{
   ConfigCategoryIterator forwards = Settings::project.Forwards();
   String item;
   while (!forwards.Eof()) {
      item.copy(forwards.key());
      item.append('=');
      item.append((TCHAR*)*forwards);

      addListItem(IDC_FORWARDS_LIST, item);

      forwards++;
   }
}

void ProjectForwardsDialog :: onOK()
{
   if (_changed) {
      Settings::project.clearForwards();

      int count = getListCount(IDC_FORWARDS_LIST);
      TCHAR item[IDENTIFIER_LEN * 2 + 1];
      String name;
      for (int i = 0 ; i < count ; i++) {
         getListItem(IDC_FORWARDS_LIST, i, (TCHAR**)(&item));

         int pos = chrpos(item, '=');
         name.copy(item, pos);

         Settings::project.addForward(name, item + pos + 1);
      }
   }
}

void ProjectForwardsDialog :: doCommand(int id, int command)
{
   switch (id) {
      case IDC_FORWARDS_ADD:
         addItem();
         break;
      case IDC_FORWARDS_REPLACE:
         editItem();
         break;
      case IDC_FORWARDS_DELETE:
         deleteItem();
         break;
      case IDC_FORWARDS_LIST:
         if (command==LBN_DBLCLK) {
            getItem();
         }
         break;
	  case IDC_FORWARDS_SAVE:
	     onOK();
         ::EndDialog(_self, -1);
	     break;
	  case IDOK:
	     break;
	  default:
	     Dialog::doCommand(id, command);
   }
}

// --- GoToLineDialog ---

void GoToLineDialog :: onCreate()
{
   setIntText(IDC_GOTOLINE_LINENUMBER, _number);
}

void GoToLineDialog :: onOK()
{
   _number = getIntText(IDC_GOTOLINE_LINENUMBER);
}

// --- WindowsDialog ---

void WindowsDialog :: onCreate()
{
   for (int i = 0 ; i < _windowTabs->getCount() ; i++) {
	  addListItem(IDC_WINDOWS_LIST, _windowTabs->getTabName(i));
   }
   setListSelected(IDC_WINDOWS_LIST, _windowTabs->getCurrentIndex(), true);
}

void WindowsDialog :: doCommand(int id, int command)
{
   switch (id) {
      case IDC_WINDOWS_LIST:
         if (command==LBN_SELCHANGE) {
            onListChange();
         }
         break;
      case IDC_WINDOWS_CLOSE:
         onClose();
         ::EndDialog(_self, -2);
         break;
	  default:
	     Dialog::doCommand(id, command);
   }
}

void WindowsDialog :: onListChange()
{
    enable(IDOK, (getListSelCount(IDC_WINDOWS_LIST) == 1));
}

void WindowsDialog :: onOK()
{
   _windowTabs->selectTab(getListIndex(IDC_WINDOWS_LIST));
   int index = 0;
   getListSelected(IDC_WINDOWS_LIST, 1, &index);

   _windowTabs->selectTab(index);
}

void WindowsDialog :: onClose()
{
   int count = getListSelCount(IDC_WINDOWS_LIST);
   int* selected = (int*)malloc(sizeof(INT) * count);
   getListSelected(IDC_WINDOWS_LIST, count, selected);

   for (int i = 0 ; i < count; i++) {
      _closedTabs->add(selected[i]);
   }
   free(selected);
}

// --- EditorSettings ---

void EditorSettings :: onCreate()
{
   addComboBoxItem(IDC_EDITOR_COLORSCHEME, TEXT("Default"));
   addComboBoxItem(IDC_EDITOR_COLORSCHEME, TEXT("Classic"));

   setComboBoxIndex(IDC_EDITOR_COLORSCHEME, Settings::scheme);

   setCheckState(IDC_EDITOR_LINENUMBERFLAG, Settings::lineNumberVisible);
   setCheckState(IDC_EDITOR_USETAB, Settings::tabCharUsing);
   setCheckState(IDC_EDITOR_HIGHLIGHSYNTAXFLAG, Settings::highlightSyntax);
   setCheckState(IDC_EDITOR_UNICODEFILES, (Settings::defaultEncoding == feUTF16));
   if (!Settings::highlightSyntax)
      enable(IDC_EDITOR_COLORSCHEME, false);

   TCHAR size[3];
   _itot(Settings::tabSize, size, 10);
   setText(IDC_EDITOR_TABSIZE, size);

   setCheckState(IDC_EDITOR_REMEMBERPATH, Settings::lastPathRemember);
   setCheckState(IDC_EDITOR_REMEMBERPROJECT, Settings::lastProjectRemember);
}

void EditorSettings :: doCommand(int id, int msg)
{
   if (id==IDC_EDITOR_HIGHLIGHSYNTAXFLAG) {
      onEditorHighlightSyntaxChanged();
   }
   else Dialog::doCommand(id, msg);
}

void EditorSettings :: onEditorHighlightSyntaxChanged()
{
   enable(IDC_EDITOR_COLORSCHEME, getCheckState(IDC_EDITOR_HIGHLIGHSYNTAXFLAG));
}

void EditorSettings :: onOK()
{
   Settings::scheme = getComboBoxIndex(IDC_EDITOR_COLORSCHEME);
   Settings::lineNumberVisible = getCheckState(IDC_EDITOR_LINENUMBERFLAG);
   Settings::tabCharUsing = getCheckState(IDC_EDITOR_USETAB);
   Settings::highlightSyntax = getCheckState(IDC_EDITOR_HIGHLIGHSYNTAXFLAG);
   Settings::defaultEncoding = getCheckState(IDC_EDITOR_UNICODEFILES) ? feUTF16 : feAnsi;
   Settings::lastPathRemember = getCheckState(IDC_EDITOR_REMEMBERPATH);
   Settings::lastProjectRemember = getCheckState(IDC_EDITOR_REMEMBERPROJECT);

   TCHAR size[12];
   getText(IDC_EDITOR_TABSIZE, (TCHAR**)(&size), 11);
   Settings::tabSize = _ttoi(size);
   if (Settings::tabSize <= 0 && Settings::tabSize > 20) {
      Settings::tabSize = 4;
   }
}

// --- FindDialog ---

int FindDialog :: showModal(bool replaceMode)
{
   _replaceMode = replaceMode;

   return Dialog::showModal();
}

void FindDialog :: onCreate()
{
   setText(IDC_FIND_TEXT, _text);
   if (_replaceMode) {
      setText(IDC_REPLACE_TEXT, _newText);
   }
   setCheckState(IDC_FIND_CASE, _matchCase);
   setCheckState(IDC_FIND_WHOLE, _wholeWord);
}

void FindDialog :: onOK()
{
   TCHAR s[200];

   getText(IDC_FIND_TEXT, (TCHAR**)(&s), 200);
   _text.copy(s);

   if (_replaceMode) {
      s[0] = 0;
      getText(IDC_REPLACE_TEXT, (TCHAR**)(&s), 200);
      _newText.copy(s);
   }
   _matchCase = getCheckState(IDC_FIND_CASE);
   _wholeWord = getCheckState(IDC_FIND_WHOLE);
}

// --- AboutDialog ---

void AboutDialog :: onCreate()
{
   setText(IDC_ABOUT_LICENCE_TEXT, APACHE_LICENSE2);
   setText(IDC_ABOUT_HOME, ELENA_HOMEPAGE);
}
