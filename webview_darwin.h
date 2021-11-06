//
// ====================================================================
//
// This implementation uses Cocoa WKWebView backend on macOS. It is
// written using ObjC runtime and uses WKWebView class as a browser runtime.
// You should pass "-framework Webkit" flag to the compiler.
//
// ====================================================================
//

#include <CoreGraphics/CoreGraphics.h>
#include <objc/objc-runtime.h>
#include <iostream>

namespace webview {


#define CLASS(clazz) (id)objc_getClass(clazz)
#define METHOD(method) sel_registerName(method)
#define NSTR(string) ((id(*)(id, SEL, const char *))objc_msgSend)((id)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), string)

class cocoa_wkwebview_engine {
public:
  cocoa_wkwebview_engine(int width,int height,bool hide,bool debug) {
      m_app = ((id(*)(id, SEL))objc_msgSend)(CLASS("WebViewApp"), METHOD("alloc"));
      m_app =  ((id(*)(id, SEL,int,int,BOOL,BOOL))objc_msgSend)(m_app, METHOD("initApp:height:hide:debug:"),width,height,hide,debug);
      
      Class cls = objc_allocateClassPair((Class)objc_getClass("NSObject"), "NSWebviewResponder", 0);
      class_addProtocol(cls, objc_getProtocol("MessageDelegate"));

      class_addMethod(cls, METHOD("onMessage:"),
                      (IMP)(+[](id self, SEL, id msg) {
                        auto w = (cocoa_wkwebview_engine *)objc_getAssociatedObject(self, "webview");
                        assert(w);
                        w->on_message(((const char *(*)(id, SEL))objc_msgSend)(((id(*)(id, SEL))objc_msgSend)(msg, METHOD("body")),METHOD("UTF8String")));
                      }),
                      "v@:@@");

      objc_registerClassPair(cls);

      id delegate = ((id(*)(id, SEL))objc_msgSend)((id)cls, METHOD("new"));
      objc_setAssociatedObject(delegate, "webview", (id)this,OBJC_ASSOCIATION_ASSIGN);
      ((void (*)(id, SEL, id))objc_msgSend)(m_app, METHOD("setDelegate:"),delegate);
  }
  ~cocoa_wkwebview_engine() { close(); }
  void *window() { return (void *)m_app; }

  void show() {
      ((void (*)(id, SEL))objc_msgSend)(m_app, METHOD("show"));
  }

  void hide() {
      ((void (*)(id, SEL))objc_msgSend)(m_app, METHOD("hide"));
  }
  void terminate() {
    close();
    ((void (*)(id, SEL))objc_msgSend)(m_app, METHOD("terminate"));
  }
  void run() {
    ((void (*)(id, SEL))objc_msgSend)(m_app, METHOD("run"));
  }
  void dispatch(std::function<void()> f) {
    dispatch_async_f(dispatch_get_main_queue(), new dispatch_fn_t(f),
                     (dispatch_function_t)([](void *arg) {
                       auto f = static_cast<dispatch_fn_t *>(arg);
                       (*f)();
                       delete f;
                     }));
  }

  void set_icon(const std::string icon) {
  }

  void set_title(const std::string title) {
    ((void (*)(id, SEL, id))objc_msgSend)(m_app, METHOD("setTitle:"),NSTR(title.c_str()));
  }
  void set_size(int width, int height, int hints) {
      ((void (*)(id, SEL, int,int,int))objc_msgSend)(m_app, METHOD("setSize:height:hints:"),width,height,hints);

  }
  void navigate(const std::string url) {
    ((void (*)(id, SEL, id))objc_msgSend)(m_app, METHOD("navigate:"),NSTR(url.c_str()));
  }
  void init(const std::string js) {
      ((void (*)(id, SEL, id))objc_msgSend)(m_app, METHOD("initJS:"),NSTR(js.c_str()));

  }
  void eval(const std::string js) {
      ((void (*)(id, SEL, id))objc_msgSend)(m_app, METHOD("evalJS:"),NSTR(js.c_str()));
  }

private:
  virtual void on_message(const std::string msg) = 0;
  void close() { ((void (*)(id, SEL))objc_msgSend)(m_app, METHOD("close")); }
  id m_app;

};

using browser_engine = cocoa_wkwebview_engine;

} // namespace webview