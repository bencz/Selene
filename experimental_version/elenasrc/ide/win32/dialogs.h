//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//		Static dialogs header
//                                              (C)2005-2009, by Alexei Rakov
//---------------------------------------------------------------------------

#ifndef dialogsH
#define dialogsH

#include "window.h"
#include "tabbar.h"

namespace _GUI_
{

// --- FileDialog ---
class FileDialog
{
public:
   static TCHAR* ProjectFilter;
   static TCHAR* SourceFilter;

private:
   OPENFILENAME _struct;
   TCHAR        _fileName[MAX_PATH * 8];        // ??
   int          _defaultFlags;

public:
   const TCHAR* openFile();
   bool openFiles(_ELENA_::List<TCHAR*>& files);
   bool saveFile(const TCHAR* defaultExt, _ELENA_::Path& path);

   FileDialog(Control* owner, const TCHAR* filter, const TCHAR* caption, const TCHAR* initialDir = NULL);
};

// --- Dialog ---

class Dialog
{
protected:
   HWND     _self;
   Control* _owner;

   void enable(int id, bool enabled);

   void getText(int id, TCHAR** text, int length);
   int  getIntText(int id);
   bool getCheckState(int id);
   int  getComboBoxIndex(int id);
   int  getListCount(int id);
   void getListItem(int id, int index, TCHAR** text);
   int  getListIndex(int id);
   int  getListSelCount(int id);
   void getListSelected(int id, int count, int* selected);

   void setText(int id, const TCHAR* text);
   void setIntText(int id, int value);
   void setCheckState(int id, bool value);
   void setComboBoxIndex(int id, int index);
   void setListIndex(int id, int index);
   void setListSelected(int id, int index, bool toggle);
   void setTextLimit(int id, int maxLength);

   void addComboBoxItem(int id, const TCHAR* text);
   void addListItem(int id, const TCHAR* text);

   void insertListItem(int id, int index, const TCHAR* text);
   void removeListItem(int id, int index);

   virtual int getDialogID() const = 0;

   virtual void onCreate() = 0;
   virtual void onOK() = 0;

   virtual void doCommand(int id, int command);

public:
   static BOOL CALLBACK DialogProc(HWND hwnd, size_t message, WPARAM wParam, LPARAM lParam);

   virtual int showModal();

   Dialog(Control* owner);
};

// --- ProjectSettingsDialog ---

class ProjectSettingsDialog : public Dialog
{
   virtual int getDialogID() const { return IDD_SETTINGS; }

   void loadTemplateList();

   virtual void onCreate();
   virtual void onOK();
   virtual void onProjectTypeChanged();

   virtual void doCommand(int id, int command);

public:   
   ProjectSettingsDialog(Control* owner) : Dialog(owner) {}
};

// --- ProjectForwardsDialog ---

class ProjectForwardsDialog : public Dialog
{
   bool _changed;
   int  _current;

   bool validateItem(TCHAR* &text);

   void addItem();
   void getItem();
   void editItem();
   void deleteItem();

   virtual int getDialogID() const { return IDD_FORWARDS; }

   virtual void onCreate();
   virtual void onOK();

   virtual void doCommand(int id, int command);

public:   
   ProjectForwardsDialog(Control* owner)
      : Dialog(owner) 
   { 
      _changed = false; 
	  _current = -1;
   }
};

// --- GoToLineDialog ---

class GoToLineDialog : public Dialog
{
   int _number;

   virtual int getDialogID() const { return IDD_GOTOLINE; }

   virtual void onCreate();
   virtual void onOK();

public:
   int getLineNumber() const { return _number; }

   GoToLineDialog(Control* owner, int number)
      : Dialog(owner) 
   {
      _number = number;
   }
};

// --- WindowsDialog ---

class WindowsDialog : public Dialog
{
   TabBar*             _windowTabs;
   _ELENA_::List<int>* _closedTabs;

   virtual int getDialogID() const { return IDD_WINDOWS; }

   virtual void doCommand(int id, int command);
   virtual void onOK();

   void onListChange();
   void onCreate();
   void onClose();

public:
   WindowsDialog(Control* owner, TabBar* windowTabs, _ELENA_::List<int>* closedTabs)
      : Dialog(owner) 
   {
      _windowTabs = windowTabs;
      _closedTabs = closedTabs;
   }
};

// --- EditorSettings ---

class EditorSettings : public Dialog
{
   virtual int getDialogID() const { return IDD_EDITOR_SETTINGS; }

   virtual void doCommand(int id, int command);
   virtual void onCreate();
   virtual void onOK();
   virtual void onEditorHighlightSyntaxChanged();

public:
   EditorSettings(Control* owner)
      : Dialog(owner)
   {
   }
};

// --- FindDialog ---

class FindDialog : public Dialog
{
   bool            _replaceMode;

   _ELENA_::String _text;
   _ELENA_::String _newText;
   bool            _matchCase;
   bool            _wholeWord;

   virtual int getDialogID() const { return _replaceMode ? IDD_EDITOR_REPLACE : IDD_EDITOR_FIND; }

   virtual void onCreate();
   virtual void onOK();

public:
   const TCHAR* getTextToFind() const { return _text; }
   const TCHAR* getTextToReplace() const { return _newText; }

   bool isMatchCase() const { return _matchCase; }

   bool isWholeWord() const { return _wholeWord; }

   virtual int showModal()
   {
      return showModal(false);
   } 
   virtual int showModal(bool replaceMode);

   FindDialog(Control* owner)
      : Dialog(owner) 
   {
      _matchCase = false;
      _wholeWord = false;
   }
};

// --- AboutDialog ---

class AboutDialog : public Dialog
{
   virtual int getDialogID() const { return IDD_ABOUT; }

   virtual void onCreate();
   virtual void onOK() {}

public:
   AboutDialog(Control* owner)
      : Dialog(owner) 
   { 
   }
};

} // _GUI_

#endif // dialogsH
