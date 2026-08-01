//---------------------------------------------------------------------------
//		E L E N A   P r o j e c t:  ELENA IDE
//      ELENA IDE main body
//                                              (C)2005-2007, by Alexei Rakov
//---------------------------------------------------------------------------

#include "idecommon.h"
// --------------------------------------------------------------------------
#include "appwindow.h"
#include "idesettings.h"

using namespace _GUI_;
using namespace _ELENA_;

// --- WinAPI initializing ---

void initCommonControls()
{
   INITCOMMONCONTROLSEX icex;
   icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
   icex.dwICC  = ICC_WIN95_CLASSES|ICC_COOL_CLASSES|ICC_BAR_CLASSES|ICC_USEREX_CLASSES|
	   ICC_TAB_CLASSES|ICC_LISTVIEW_CLASSES;

   ::InitCommonControlsEx(&icex);
}

void registerClass(HINSTANCE hInstance, const TCHAR* name, size_t style, HCURSOR cursor = NULL,
				   HBRUSH background = NULL, HICON icon = NULL, TCHAR* menu = NULL)
{
   WNDCLASSEX wndClass;
   wndClass.cbSize = sizeof(wndClass);
   wndClass.style = style;
   wndClass.lpfnWndProc = Window::Window_Proc;
   wndClass.cbClsExtra = 0;
   wndClass.cbWndExtra = 0;
   wndClass.hInstance = hInstance;
   wndClass.hIcon = icon;
   wndClass.hCursor = cursor;
   wndClass.hbrBackground = background;
   wndClass.lpszMenuName = menu;
   wndClass.lpszClassName = name;
   wndClass.hIconSm = 0;

   if (!::RegisterClassEx(&wndClass))	{
      ErrorManager::raiseError(ERR_WNDCLASS_NOT_REGISTERED, APP_WND_CLASS);
   }
}

void registerAppClass(HINSTANCE hInstance)
{
   registerClass(hInstance, APP_WND_CLASS, CS_BYTEALIGNWINDOW | CS_DBLCLKS, NULL, 
	   //::CreateSolidBrush(::GetSysColor(COLOR_MENU)),
      (HBRUSH)COLOR_WINDOW,
	   ::LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON)), 
	   MAKEINTRESOURCE(IDR_MAIN_MENU));
}

void registerFrame(HINSTANCE hInstance)
{
   registerClass(hInstance, EDIT_WND_CLASS, CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS);
}

void registerSplitters(HINSTANCE hInstance)
{
   registerClass(hInstance, VSPLTR_WND_CLASS, CS_HREDRAW | CS_VREDRAW, ::LoadCursor(NULL,IDC_SIZEWE), 
	   (HBRUSH)COLOR_WINDOW);

   registerClass(hInstance, HSPLTR_WND_CLASS, CS_HREDRAW | CS_VREDRAW, ::LoadCursor(NULL,IDC_SIZENS), 
	   (HBRUSH)COLOR_WINDOW);
}

// --- Loading / Saving configuration ---

void loadSettings()
{
   Path       path(Paths::appPath, TEXT("ide.cfg"));
   IniConfigFile file;

   if (file.load(path)) {
      Settings::load(file);
      //if (!Settings::defaultProject.isEmpty()) {
      //   Paths::lastPath.copyPath(Settings::defaultProject);
      //}
   }
}

void loadHistory(AppWindow& appWindow)
{
   Path       path(Paths::appPath, _T("ide.cfg"));
   IniConfigFile file;

   if (file.load(path)) {
      appWindow.loadHistory(file);
   }
}

void saveSettings(AppWindow& appWindow)
{
   Path       path(Paths::appPath, _T("ide.cfg"));
   IniConfigFile file;

   Settings::save(file);
   appWindow.saveHistory(file);

   file.save(path);
}

// --- Load command-line options ---

void setOption(const TCHAR* parameter)
{
   if (parameter[0]!='-') {
      if (Path::checkExtension(parameter, SOURCE_EXTENSION)) {
         Settings::defaultFiles.add(_ELENA_::strdup(parameter));
      }
      else if (Path::checkExtension(parameter, PROJECT_EXTENSION)) {
         Settings::defaultProject.copy(parameter);
      }
   }
   else if (compstr(parameter, _T("-test"))) {
      Settings::testMode = true;
   }
   else if (compstr(parameter, _T("-sclassic"))) {
      Settings::scheme = 1;
   }
   else if (compstr(parameter, _T("-xbytecode"))) {
      Settings::bytecode = true;
      Settings::scheme = 1;
   }
}

void loadCommandLine(char* cmdLine)
{
#ifdef _UNICODE
   wchar_t* cmdWLine = (wchar_t*)malloc((strlen(cmdLine) + 1) * 2);

   ansiToUnicode(cmdLine, cmdWLine, strlen(cmdLine));
   cmdWLine[strlen(cmdLine)] = 0;

   int start = 0;
   for (size_t i = 1 ; i <= wcslen(cmdWLine) ; i++) {
      if (cmdWLine[i]==' ' || cmdWLine[i]==0) {
         String parameter(cmdWLine + start, i - start);

         setOption(parameter);

         start = i + 1;
      }
   }
   freestr(cmdWLine);
#else
   int start = 0;
   for (size_t i = 1 ; i <= strlen(cmdLine) ; i++) {
      if (cmdLine[i]==' ' || cmdLine[i]==0) {
         String parameter(cmdLine + start, i - start);

         setOption(parameter);

         start = i + 1;
      }
   }
#endif
}

// --- WinMain ---

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE, LPSTR cmdLine, int)
{
   Paths::init(_T("..\\src"), _T("..\\lib"));

   initCommonControls();
   registerAppClass(hInstance);
   registerFrame(hInstance);
   registerSplitters(hInstance);

   loadSettings();
   loadCommandLine(cmdLine);

   AppWindow appWindow;
   try
   {
      appWindow.create(hInstance, 0);
      loadHistory(appWindow);   

      appWindow.show(Settings::appMaximized);

      MSG msg;
      msg.wParam = 0;
      while (::GetMessage(&msg, NULL, 0, 0)) {
         if (!appWindow.translateMessage(&msg)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
         }
      }
   }
   catch (_GUI_::Exception&)
   {
      appWindow.saveAllAsDraft();
   }
   catch (...)
   {
      appWindow.saveAllAsDraft();
   }
   saveSettings(appWindow);
   Font::releaseFontCache();
   Settings::clear();
   return 0;
}
