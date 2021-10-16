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

#define NSBackingStoreBuffered 2

#define NSWindowStyleMaskResizable 8
#define NSWindowStyleMaskMiniaturizable 4
#define NSWindowStyleMaskTitled 1
#define NSWindowStyleMaskClosable 2

#define WKUserScriptInjectionTimeAtDocumentStart 0

namespace webview {


#define CLASS(clazz) (id)objc_getClass(clazz)
#define METHOD(method) sel_registerName(method)
#define NSTR(string) ((id(*)(id, SEL, const char *))objc_msgSend)((id)objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), string)

class cocoa_wkwebview_engine {
public:
  cocoa_wkwebview_engine(int width,int height,bool hide,bool debug) {

    m_hide = hide;
    // Application
    id app = ((id(*)(id, SEL))objc_msgSend)(CLASS("NSApplication"),METHOD("sharedApplication"));
    ((void (*)(id, SEL, long))objc_msgSend)(app, METHOD("setActivationPolicy:"), YES);

    // Delegate
    Class cls = objc_allocateClassPair((Class)objc_getClass("NSResponder"), "NSWebviewResponder", 0);
    class_addProtocol(cls, objc_getProtocol("NSTouchBarProvider"));

    if(m_hide) {
      class_addMethod(cls, METHOD("applicationShouldTerminateAfterLastWindowClosed:"), (IMP)(+[](id, SEL, id) -> BOOL { return NO; }), "c@:@");
    }else{
      class_addMethod(cls, METHOD("applicationShouldTerminateAfterLastWindowClosed:"), (IMP)(+[](id, SEL, id) -> BOOL { return YES; }), "c@:@");
    }
    class_addMethod(cls, METHOD("userContentController:didReceiveScriptMessage:"),
                    (IMP)(+[](id self, SEL, id, id msg) {
                      auto w = (cocoa_wkwebview_engine *)objc_getAssociatedObject(self, "webview");
                      assert(w);
                      w->on_message(((const char *(*)(id, SEL))objc_msgSend)(((id(*)(id, SEL))objc_msgSend)(msg, METHOD("body")),METHOD("UTF8String")));
                    }),
                    "v@:@@");

    objc_registerClassPair(cls);

    auto delegate = ((id(*)(id, SEL))objc_msgSend)((id)cls, METHOD("new"));
    objc_setAssociatedObject(delegate, "webview", (id)this,OBJC_ASSOCIATION_ASSIGN);
    ((void (*)(id, SEL, id))objc_msgSend)(app, METHOD("setDelegate:"),delegate);

    m_window = ((id(*)(id, SEL))objc_msgSend)(CLASS("NSWindow"), METHOD("alloc"));
    m_window =  ((id(*)(id, SEL, CGRect, int, unsigned long, int))objc_msgSend)(
            m_window, METHOD("initWithContentRect:styleMask:backing:defer:"),
            CGRectMake(0, 0, width, height), 0, NSBackingStoreBuffered, 0);

    m_controller = ((id(*)(id, SEL))objc_msgSend)(CLASS("NSWindowController"), METHOD("alloc"));
    m_controller =  ((id(*)(id, SEL,id))objc_msgSend)(m_controller, METHOD("initWithWindow:"),m_window);

    // Webview
    auto config = ((id(*)(id, SEL))objc_msgSend)(CLASS("WKWebViewConfiguration"), METHOD("new"));
    m_manager = ((id(*)(id, SEL))objc_msgSend)(config, METHOD("userContentController"));
    m_webview = ((id(*)(id, SEL))objc_msgSend)(CLASS("WKWebView"), METHOD("alloc"));

    if (debug) {
      // Equivalent Obj-C:
      // [[config preferences] setValue:@YES forKey:@"developerExtrasEnabled"];
      ((id(*)(id, SEL, id, id))objc_msgSend)(
          ((id(*)(id, SEL))objc_msgSend)(config, METHOD("preferences")),METHOD("setValue:forKey:"),
          ((id(*)(id, SEL, BOOL))objc_msgSend)(CLASS("NSNumber"),METHOD("numberWithBool:"), 1),NSTR("developerExtrasEnabled"));
    }

    // Equivalent Obj-C:
    // [[config preferences] setValue:@YES forKey:@"fullScreenEnabled"];
    ((id(*)(id, SEL, id, id))objc_msgSend)(
        ((id(*)(id, SEL))objc_msgSend)(config, METHOD("preferences")),METHOD("setValue:forKey:"),
        ((id(*)(id, SEL, BOOL))objc_msgSend)(CLASS("NSNumber"),METHOD("numberWithBool:"), 1),NSTR("fullScreenEnabled"));

    // Equivalent Obj-C:
    // [[config preferences] setValue:@YES forKey:@"javaScriptCanAccessClipboard"];
    ((id(*)(id, SEL, id, id))objc_msgSend)(
        ((id(*)(id, SEL))objc_msgSend)(config, METHOD("preferences")),METHOD("setValue:forKey:"),
        ((id(*)(id, SEL, BOOL))objc_msgSend)(CLASS("NSNumber"),METHOD("numberWithBool:"), 1),NSTR("javaScriptCanAccessClipboard"));

    // Equivalent Obj-C:
    // [[config preferences] setValue:@YES forKey:@"DOMPasteAllowed"];
    ((id(*)(id, SEL, id, id))objc_msgSend)(
        ((id(*)(id, SEL))objc_msgSend)(config, METHOD("preferences")),METHOD("setValue:forKey:"),
        ((id(*)(id, SEL, BOOL))objc_msgSend)(CLASS("NSNumber"),METHOD("numberWithBool:"), 1),NSTR("DOMPasteAllowed"));

    ((void (*)(id, SEL, CGRect, id))objc_msgSend)(m_webview, METHOD("initWithFrame:configuration:"), CGRectMake(0, 0, 0, 0),config);
    ((void (*)(id, SEL, id, id))objc_msgSend)(m_manager, METHOD("addScriptMessageHandler:name:"), delegate,NSTR("external"));

    init(R"script(window.external = { invoke: function(s) {window.webkit.messageHandlers.external.postMessage(s);},};)script");

    ((void (*)(id, SEL, id))objc_msgSend)(m_window, METHOD("setContentView:"),m_webview);
    ((void (*)(id, SEL, id))objc_msgSend)(m_window, METHOD("makeKeyAndOrderFront:"),nullptr);
  }
  ~cocoa_wkwebview_engine() { close(); }
  void *window() { return (void *)m_window; }

  void show() {
    dispatch_async(dispatch_get_main_queue(),^{

        id app = ((id(*)(id, SEL))objc_msgSend)(CLASS("NSApplication"),METHOD("sharedApplication"));
        ((void (*)(id, SEL, BOOL))objc_msgSend)(app, METHOD("activateIgnoringOtherApps:"), YES);
        ((void (*)(id, SEL, id))objc_msgSend)(m_controller, METHOD("showWindow:"),m_window);
        ((void (*)(id, SEL, id))objc_msgSend)(m_window, METHOD("makeKeyAndOrderFront:"),nullptr);
    });  
  }

  void hide() {
    dispatch_async(dispatch_get_main_queue(),^{
        ((void (*)(id, SEL, id))objc_msgSend)(m_window, METHOD("orderOut:"),m_window);
    });  
  }
  void terminate() {
    close();
    ((void (*)(id, SEL, id))objc_msgSend)(CLASS("NSApp"), METHOD("terminate:"),nullptr);
  }
  void run() {
    id app = ((id(*)(id, SEL))objc_msgSend)(CLASS("NSApplication"),METHOD("sharedApplication"));
    dispatch([&]() {
      ((void (*)(id, SEL, BOOL))objc_msgSend)(app, METHOD("activateIgnoringOtherApps:"), 1);
    });
    ((void (*)(id, SEL))objc_msgSend)(app, METHOD("run"));
  }
  void dispatch(std::function<void()> f) {
    dispatch_async_f(dispatch_get_main_queue(), new dispatch_fn_t(f),
                     (dispatch_function_t)([](void *arg) {
                       auto f = static_cast<dispatch_fn_t *>(arg);
                       (*f)();
                       delete f;
                     }));
  }
  void set_title(const std::string title) {
    ((void (*)(id, SEL, id))objc_msgSend)(m_window, METHOD("setTitle:"),NSTR(title.c_str()));
  }
  void set_size(int width, int height, int hints) {
    auto style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
    if (hints != WEBVIEW_HINT_FIXED) {
      style = style | NSWindowStyleMaskResizable;
    }
    ((void (*)(id, SEL, unsigned long))objc_msgSend)(m_window, METHOD("setStyleMask:"), style);

    if (hints == WEBVIEW_HINT_MIN) {
      ((void (*)(id, SEL, CGSize))objc_msgSend)(m_window, METHOD("setContentMinSize:"), CGSizeMake(width, height));
    } else if (hints == WEBVIEW_HINT_MAX) {
      ((void (*)(id, SEL, CGSize))objc_msgSend)(m_window, METHOD("setContentMaxSize:"), CGSizeMake(width, height));
    } else {
      ((void (*)(id, SEL, CGRect, BOOL, BOOL))objc_msgSend)(m_window, METHOD("setFrame:display:animate:"),CGRectMake(0, 0, width, height), 1, 0);
    }
    ((void (*)(id, SEL))objc_msgSend)(m_window, METHOD("center"));
  }
  void navigate(const std::string url) {
    auto nsurl = ((id(*)(id, SEL, id))objc_msgSend)(CLASS("NSURL"), METHOD("URLWithString:"),NSTR(url.c_str()));
    auto request = ((id(*)(id, SEL, id))objc_msgSend)(CLASS("NSURLRequest"),METHOD("requestWithURL:"), nsurl);
    ((void (*)(id, SEL, id))objc_msgSend)(m_webview, METHOD("loadRequest:"),request);
  }
  void init(const std::string js) {
    // Equivalent Obj-C:
    // [m_manager addUserScript:[[WKUserScript alloc] initWithSource:[NSString stringWithUTF8String:js.c_str()] injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]]
    ((void (*)(id, SEL, id))objc_msgSend)(
        m_manager, METHOD("addUserScript:"),
        ((id(*)(id, SEL, id, long, BOOL))objc_msgSend)(
            ((id(*)(id, SEL))objc_msgSend)(CLASS("WKUserScript"), METHOD("alloc")),
            METHOD("initWithSource:injectionTime:forMainFrameOnly:"),
            NSTR(js.c_str()),
            WKUserScriptInjectionTimeAtDocumentStart, 1));
  }
  void eval(const std::string js) {
    ((void (*)(id, SEL, id, id))objc_msgSend)(m_webview, METHOD("evaluateJavaScript:completionHandler:"),NSTR(js.c_str()),nullptr);
  }

private:
  virtual void on_message(const std::string msg) = 0;
  void close() { ((void (*)(id, SEL))objc_msgSend)(m_window, METHOD("close")); }
  bool m_hide;
  id m_window;
  id m_webview;
  id m_manager;
  id m_controller;
};

using browser_engine = cocoa_wkwebview_engine;

} // namespace webview