//
// ====================================================================
//
// This implementation uses Win32 API to create a native window. It can
// use either EdgeHTML or Edge/Chromium backend as a browser engine.
//
// ====================================================================
//

#define WIN32_LEAN_AND_MEAN
//#define WINVER  0x0605
#include <Shlwapi.h>
#include <codecvt>
#include <stdlib.h>
#include <windows.h>
#include <wrl.h>
#include <winuser.h>
#include <string>
#include <locale>
#include <iostream>
#include "webview2.h"


namespace webview {

using msg_cb_t = std::function<void(const std::string)>;

// Common interface for EdgeHTML and Edge/Chromium
class browser {
public:
  virtual ~browser() = default;
  virtual bool embed(HWND, bool, msg_cb_t) = 0;
  virtual void navigate(const std::string url) = 0;
  virtual void eval(const std::string js) = 0;
  virtual void init(const std::string js) = 0;
  virtual void resize(HWND) = 0;
};

//
// Edge/Chromium browser engine
//
class edge_chromium : public browser {
public:
  bool embed(HWND wnd, bool debug, msg_cb_t cb) override {
    m_debug = debug;
    CoInitialize(0);
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
    flag.test_and_set();
    char currentExePath[MAX_PATH];
    GetModuleFileNameA(NULL, currentExePath, MAX_PATH);
    char *currentExeName = PathFindFileNameA(currentExePath);
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> wideCharConverter;
    std::wstring userDataFolder = wideCharConverter.from_bytes(std::getenv("APPDATA"));
    std::wstring currentExeNameW = wideCharConverter.from_bytes(currentExeName);
    HRESULT res = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, (userDataFolder + L"/" + currentExeNameW).c_str(), nullptr,
        new webview2_com_handler(wnd, cb,
                                 [&](ICoreWebView2Controller *controller) {
                                   m_controller = controller;
                                   m_controller->get_CoreWebView2(&m_webview);
                                   m_webview->AddRef();
                                   flag.clear();
                                 }));

    if (res != S_OK) {
      CoUninitialize();
      return false;
    }
    MSG msg = {};
    while (flag.test_and_set() && GetMessage(&msg, NULL, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    ICoreWebView2Settings* Settings;
    m_webview->get_Settings(&Settings);
    Settings->put_AreDevToolsEnabled(m_debug);

    init("window.external={invoke:s=>window.chrome.webview.postMessage(s)}");
    return true;
  }

  void resize(HWND wnd) override {
    if (m_controller == nullptr) {
      return;
    }
    RECT bounds;
    GetClientRect(wnd, &bounds);
    m_controller->put_Bounds(bounds);
  }

  void navigate(const std::string url) override {
    auto wurl = to_lpwstr(url);
    m_webview->Navigate(wurl);
    delete[] wurl;
  }

  void init(const std::string js) override {
    LPCWSTR wjs = to_lpwstr(js);
    m_webview->AddScriptToExecuteOnDocumentCreated(wjs, nullptr);
    delete[] wjs;
  }

  void eval(const std::string js) override {
    LPCWSTR wjs = to_lpwstr(js);
    m_webview->ExecuteScript(wjs, nullptr);
    delete[] wjs;
  }

private:
  LPWSTR to_lpwstr(const std::string s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    wchar_t *ws = new wchar_t[n];
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, ws, n);
    return ws;
  }
  bool m_debug;
  ICoreWebView2 *m_webview = nullptr;
  ICoreWebView2Controller *m_controller = nullptr;

  class webview2_com_handler
      : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
        public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
        public ICoreWebView2WebMessageReceivedEventHandler,
        public ICoreWebView2PermissionRequestedEventHandler {
    using webview2_com_handler_cb_t = std::function<void(ICoreWebView2Controller *)>;

  public:
    webview2_com_handler(HWND hwnd, msg_cb_t msgCb,
                         webview2_com_handler_cb_t cb)
        : m_window(hwnd), m_msgCb(msgCb), m_cb(cb) {}
    ULONG STDMETHODCALLTYPE AddRef() { return 1; }
    ULONG STDMETHODCALLTYPE Release() { return 1; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID *ppv) {
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res, ICoreWebView2Environment *env) {
      if(res != 0x0){
        return res;
      }
      env->CreateCoreWebView2Controller(m_window, this);
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT res,ICoreWebView2Controller *controller) {

      if(res != 0x0){
        return res;
      }
      controller->AddRef();

      ICoreWebView2 *webview;
      ::EventRegistrationToken token;
      controller->get_CoreWebView2(&webview);
      webview->add_WebMessageReceived(this, &token);
      webview->add_PermissionRequested(this, &token);

      m_cb(controller);
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args) {
      LPWSTR message;
      args->TryGetWebMessageAsString(&message);

      std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> wideCharConverter;
      m_msgCb(wideCharConverter.to_bytes(message));
      sender->PostWebMessageAsString(message);

      CoTaskMemFree(message);
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *sender,ICoreWebView2PermissionRequestedEventArgs *args) {
      COREWEBVIEW2_PERMISSION_KIND kind;
      args->get_PermissionKind(&kind);
      if (kind == COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ) {
        args->put_State(COREWEBVIEW2_PERMISSION_STATE_ALLOW);
      }
      return S_OK;
    }

  private:
    HWND m_window;
    msg_cb_t m_msgCb;
    webview2_com_handler_cb_t m_cb;
  };
};

class win32_edge_engine {
public:
  win32_edge_engine(int width,int height,bool hide,bool debug) {
    m_hide = hide;
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    HICON icon = (HICON)LoadImage(hInstance, IDI_WINLOGO, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(WNDCLASSEX));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.hInstance = hInstance;
    wc.lpszClassName = "webview";
    wc.hIcon = icon;
    wc.hIconSm = icon;
    wc.hbrBackground = CreateSolidBrush(RGB(255,255,255));
    wc.lpfnWndProc =
        (WNDPROC)(+[](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> int {
          auto w = (win32_edge_engine *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
          switch (msg) {
          case WM_SIZE:
            w->m_browser->resize(hwnd);
            break;
          case WM_CLOSE:
            if(w->m_hide){
              ShowWindow(w->m_window, SW_HIDE);
            }else{
              DestroyWindow(hwnd);
            }
            break;
          case WM_DESTROY:
            w->terminate();
            break;
          case WM_GETMINMAXINFO: {
            auto lpmmi = (LPMINMAXINFO)lp;
            if (w == nullptr) {
              return 0;
            }
            if (w->m_maxsz.x > 0 && w->m_maxsz.y > 0) {
              lpmmi->ptMaxSize = w->m_maxsz;
              lpmmi->ptMaxTrackSize = w->m_maxsz;
            }
            if (w->m_minsz.x > 0 && w->m_minsz.y > 0) {
              lpmmi->ptMinTrackSize = w->m_minsz;
            }
          } break;
          default:
            return DefWindowProc(hwnd, msg, wp, lp);
          }
          return 0;
        });
    RegisterClassEx(&wc);
    m_window = CreateWindow("webview", "", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                            CW_USEDEFAULT, width, height, nullptr, nullptr,
                            GetModuleHandle(nullptr), nullptr);
    SetWindowLongPtr(m_window, GWLP_USERDATA, (LONG_PTR)this);

    int scrWidth  = GetSystemMetrics(SM_CXSCREEN);
    int scrHeight = GetSystemMetrics(SM_CYSCREEN);
    RECT rect;
    rect.right =  width;// 设置窗口宽高
    rect.bottom = height;
    rect.left = (scrWidth - rect.right) / 2;
    rect.top = (scrHeight - rect.bottom) / 2;

    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, 0);
    MoveWindow(m_window,rect.left, rect.top, rect.right, rect.bottom,  1);// 居中
    //SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE);
    ShowWindow(m_window, SW_SHOW);
    UpdateWindow(m_window);
    SetFocus(m_window);

    auto cb = std::bind(&win32_edge_engine::on_message, this, std::placeholders::_1);
    bool flag = m_browser->embed(m_window, debug, cb);
    if(!flag){
      MessageBox(NULL,TEXT("can't load webview2,please install webveiw2 runtime"),TEXT("Alert"),MB_OK);
      exit(0);
    }
    m_browser->resize(m_window);
  }

  void run() {
    MSG msg;
    BOOL res;
    while ((res = GetMessage(&msg, nullptr, 0, 0)) != -1) {
      
      if (msg.hwnd) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        continue;
      }
      if (msg.message == WM_APP) {
        auto f = (dispatch_fn_t *)(msg.lParam);
        (*f)();
        delete f;
      } else if (msg.message == WM_QUIT) {
        return;
      }
    }
  }
  void *window() { return (void *)m_window; }
  void hide() {
    ShowWindow(m_window, SW_HIDE);
  }
  void show() {
    ShowWindow(m_window, SW_SHOW);
  }
  void terminate() { PostQuitMessage(0); }
  void dispatch(dispatch_fn_t f) {
    PostThreadMessage(m_main_thread, WM_APP, 0, (LPARAM) new dispatch_fn_t(f));
  }

  void set_icon(const std::string icon) {
  }

  void set_title(const std::string title) {
    SetWindowText(m_window, title.c_str());
  }

  void set_size(int width, int height, int hints) {
    auto style = GetWindowLong(m_window, GWL_STYLE);
    if (hints == WEBVIEW_HINT_FIXED) {
      style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    } else {
      style |= (WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    SetWindowLong(m_window, GWL_STYLE, style);

    if (hints == WEBVIEW_HINT_MAX) {
      m_maxsz.x = width;
      m_maxsz.y = height;
    } else if (hints == WEBVIEW_HINT_MIN) {
      m_minsz.x = width;
      m_minsz.y = height;
    } else {

      int scrWidth  = GetSystemMetrics(SM_CXSCREEN);
      int scrHeight = GetSystemMetrics(SM_CYSCREEN);
      RECT rect;
      rect.right =  width;// 设置窗口宽高
      rect.bottom = height;
      rect.left = (scrWidth - rect.right) / 2;
      rect.top = (scrHeight - rect.bottom) / 2;

      AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, 0);
      MoveWindow(m_window,rect.left, rect.top, rect.right, rect.bottom,  1);// 居中
      m_browser->resize(m_window);
    }
  }

  void navigate(const std::string url) { m_browser->navigate(url); }
  void eval(const std::string js) { m_browser->eval(js); }
  void init(const std::string js) { m_browser->init(js); }

private:
  virtual void on_message(const std::string msg) = 0;
  bool m_hide;
  HWND m_window;
  POINT m_minsz = POINT{0, 0};
  POINT m_maxsz = POINT{0, 0};
  DWORD m_main_thread = GetCurrentThreadId();
  std::unique_ptr<webview::browser> m_browser = std::unique_ptr<webview::edge_chromium>(new webview::edge_chromium());
};

using browser_engine = win32_edge_engine;
} // namespace webview