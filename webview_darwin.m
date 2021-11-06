#define WEBVIEW_HINT_NONE 0  // Width and height are default size
#define WEBVIEW_HINT_MIN 1   // Width and height are minimum bounds
#define WEBVIEW_HINT_MAX 2   // Width and height are maximum bounds
#define WEBVIEW_HINT_FIXED 3 // Window size can not be changed by a user

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <WebKit/WebKit.h>

@interface AppWebView : WKWebView

@end

@implementation AppWebView

- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    if ([event modifierFlags] & NSEventModifierFlagCommand) {
        // The command key is the ONLY modifier key being pressed.
        if ([[event charactersIgnoringModifiers] isEqualToString:@"x"]) {
            
            return [NSApp sendAction:@selector(cut:) to:[[self window] firstResponder] from:self];
        } else if ([[event charactersIgnoringModifiers] isEqualToString:@"c"]) {
            return [NSApp sendAction:@selector(copy:) to:[[self window] firstResponder] from:self];
        } else if ([[event charactersIgnoringModifiers] isEqualToString:@"v"]) {
            return [NSApp sendAction:@selector(paste:) to:[[self window] firstResponder] from:self];
        } else if ([[event charactersIgnoringModifiers] isEqualToString:@"a"]) {
            return [NSApp sendAction:@selector(selectAll:) to:[[self window] firstResponder] from:self];
        }
    }
    return [super performKeyEquivalent:event];
}

@end


@protocol MessageDelegate <NSObject>

-(void)onMessage:(id)message;

@end

@interface WebViewApp : NSObject<NSApplicationDelegate,WKScriptMessageHandler> {
    NSWindow  *_window;
    NSWindowController  *_controller;
    AppWebView  *_webview;
    WKUserContentController *_manager;
    BOOL _hide;
    id<MessageDelegate> _delegate;
}

-(id) initApp:(NSInteger)width height:(NSInteger)height hide:(BOOL)hide debug:(BOOL)debug;
-(void) close;
-(void) hide;
-(void) show;
-(void) run;
-(void) terminate;
-(void)setDelegate:(id<MessageDelegate>)delegate;
-(void)setTitle:(NSString*)title;
-(void) setSize:(NSInteger)width height:(NSInteger)height hints:(NSInteger)hints;
-(void) initJS:(NSString*)js;
-(void) evalJS:(NSString*)js;
-(void) navigate:(NSString *)url;
-(BOOL) applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender;
-(void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message;
@end


@implementation WebViewApp {
}

-(id) initApp:(NSInteger)width height:(NSInteger)height hide:(BOOL)hide debug:(BOOL)debug {
   
    id app =  [NSApplication sharedApplication];
    [app setActivationPolicy:YES];
    
    _hide = hide;
    
    [app setDelegate:self];
    
    _window = [[NSWindow alloc] initWithContentRect:CGRectMake(0, 0, width, height) styleMask:0 backing:NSBackingStoreBuffered defer:NO];
        
    _controller = [[NSWindowController alloc]initWithWindow:_window];

     WKWebViewConfiguration *config = [[WKWebViewConfiguration alloc] init];

     WKPreferences *preference = [[WKPreferences alloc]init];
    
    if (debug) {
        [preference setValue:@YES forKey:@"developerExtrasEnabled"];
    }
    [preference setValue:@YES forKey:@"fullScreenEnabled"];
    [preference setValue:@YES forKey:@"javaScriptCanAccessClipboard"];
    [preference setValue:@YES forKey:@"DOMPasteAllowed"];
    config.preferences = preference;
    _manager = [[WKUserContentController alloc] init];
    [_manager addScriptMessageHandler:self  name:@"external"];
    config.userContentController = _manager;
     
    _webview = [[AppWebView alloc] initWithFrame:CGRectMake(0, 0, 0, 0) configuration:config];

    [self initJS:@"window.external = { invoke: function(s) {window.webkit.messageHandlers.external.postMessage(s)}}"];

    [_window setContentView:_webview];
    [_window makeKeyAndOrderFront:nil];

    return self;
    
    // Application
};


-(void) initJS:(NSString *)js {
    dispatch_async(dispatch_get_main_queue(),^{
    [self->_manager addUserScript:[[WKUserScript alloc] initWithSource:js injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]];
    });

}

-(void) evalJS:(NSString *)js{
    dispatch_async(dispatch_get_main_queue(),^{
        [self->_webview evaluateJavaScript:js completionHandler:nil];
    });
}

-(void) setDelegate:(id<MessageDelegate>)delegate{
    _delegate = delegate;
}

-(void) setTitle:(NSString *)title{
    [_window setTitle:title];

}
-(void) setSize:(NSInteger)width height:(NSInteger)height hints:(NSInteger)hints{
    NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
    if (hints != WEBVIEW_HINT_FIXED) {
      style = style | NSWindowStyleMaskResizable;
    }
    
    [_window setStyleMask:style];
    
    
    if (hints == WEBVIEW_HINT_MIN) {
        [_window setContentMinSize:CGSizeMake(width, height)];
    }else if (hints == WEBVIEW_HINT_MAX){
        [_window setContentMaxSize:CGSizeMake(width, height)];
    }else{
        [_window setFrame:CGRectMake(0, 0, width, height) display:YES animate:NO];
    }
    
    [_window center];
}


-(void) close {
    dispatch_async(dispatch_get_main_queue(),^{
        [self->_window close];
    });
}
-(void) hide {
    dispatch_async(dispatch_get_main_queue(),^{
        [self->_window orderOut:self->_window];
    });
}

-(void) show{
    dispatch_async(dispatch_get_main_queue(),^{
        id app =  [NSApplication sharedApplication];
        [app activateIgnoringOtherApps:YES];
        [self->_controller showWindow:self->_window];
        [self->_window makeKeyAndOrderFront:nil];
    });
}
-(void) navigate:(NSString *)url{

    id request =  [NSURLRequest requestWithURL:[NSURL URLWithString:url]];
    [_webview loadRequest:request];
}

-(void) terminate {
    id app =  [NSApplication sharedApplication];
    [self close];
    [app terminate:nil];
}
-(void) run {
    
    id app =  [NSApplication sharedApplication];
    [app activateIgnoringOtherApps:YES];
    [app run];
}


-(BOOL) applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender{
    return !_hide;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message {
    
    if (_delegate != nil){
        [_delegate onMessage:message];
    }
}

@end;
