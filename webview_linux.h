//
// ====================================================================
//
// This implementation uses webkit2gtk backend. It requires gtk+3.0 and
// webkit2gtk-4.0 libraries. Proper compiler flags can be retrieved via:
//
//   pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0
//
// ====================================================================
//
#include <JavaScriptCore/JavaScript.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

namespace webview {

class gtk_webkit_engine {
public:
  gtk_webkit_engine(int width,int height,bool hide,bool debug) {
    m_hide = hide;
    gtk_init_check(0, NULL);
    m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_resize(GTK_WINDOW(m_window), width, height);
    g_signal_connect(G_OBJECT(m_window), "destroy",
                     G_CALLBACK(+[](GtkWidget *, gpointer arg) {
                       static_cast<gtk_webkit_engine *>(arg)->terminate();
                     }),
                     this);

    if(m_hide){
      g_signal_connect(G_OBJECT(m_window), "delete-event",  G_CALLBACK(gtk_webkit_engine::hideWindow), this);
    }

    // Initialize webview widget
    m_webview = webkit_web_view_new();
    WebKitUserContentManager *manager =
        webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(m_webview));
    g_signal_connect(manager, "script-message-received::external",
                     G_CALLBACK(+[](WebKitUserContentManager *,
                                    WebKitJavascriptResult *r, gpointer arg) {
                       auto *w = static_cast<gtk_webkit_engine *>(arg);
#if WEBKIT_MAJOR_VERSION >= 2 && WEBKIT_MINOR_VERSION >= 22
                       JSCValue *value =
                           webkit_javascript_result_get_js_value(r);
                       char *s = jsc_value_to_string(value);
#else
                       JSGlobalContextRef ctx =
                           webkit_javascript_result_get_global_context(r);
                       JSValueRef value = webkit_javascript_result_get_value(r);
                       JSStringRef js = JSValueToStringCopy(ctx, value, NULL);
                       size_t n = JSStringGetMaximumUTF8CStringSize(js);
                       char *s = g_new(char, n);
                       JSStringGetUTF8CString(js, s, n);
                       JSStringRelease(js);
#endif
                       w->on_message(s);
                       g_free(s);
                     }),
                     this);
    webkit_user_content_manager_register_script_message_handler(manager,
                                                                "external");
    init("window.external={invoke:function(s){window.webkit.messageHandlers."
         "external.postMessage(s);}}");

    gtk_container_add(GTK_CONTAINER(m_window), GTK_WIDGET(m_webview));
    gtk_widget_grab_focus(GTK_WIDGET(m_webview));

    WebKitSettings *settings =
        webkit_web_view_get_settings(WEBKIT_WEB_VIEW(m_webview));
    webkit_settings_set_javascript_can_access_clipboard(settings, true);
    if (debug) {
      webkit_settings_set_enable_write_console_messages_to_stdout(settings,
                                                                  true);
      webkit_settings_set_enable_developer_extras(settings, true);
    }
    gtk_window_set_position( GTK_WINDOW(m_window), GTK_WIN_POS_CENTER_ALWAYS );
    gtk_widget_show_all(m_window);
  }

  GdkPixbuf * create_pixbuf(const gchar *filename)
  {
      GdkPixbuf *pixbuf;
      GError *error = NULL;
      pixbuf = gdk_pixbuf_new_from_file(filename, &error);
      if(!pixbuf)
      {
          fprintf(stderr,"%s\n",error->message);
          g_error_free(error);
      }
      return pixbuf;
  }


  void set_icon(const std::string icon) {

    std::string temp_file_name ="/tmp/icon_xxxxxxxxxx";;
    FILE *fd = fopen((char*)temp_file_name.c_str(),"w+");

    if (fd == NULL) {
		  printf("failed to create icon file %s: %s\n", temp_file_name.c_str(), strerror(errno));
      return;
    }
    printf("size=%d\n",(int)icon.size());
    ssize_t written = fwrite(icon.data(), icon.size(),icon.size(),fd);
    fclose(fd);
    gtk_window_set_icon(GTK_WINDOW(m_window),create_pixbuf(temp_file_name.c_str()));

  }


  void *window() { return (void *)m_window; }
  void run() { gtk_main(); }
  void terminate() { gtk_main_quit(); }

  void hide(){
      g_idle_add(GSourceFunc(hideWindowMain),this);
  }

  static gboolean hideWindowMain(gpointer arg){
      auto *engine = static_cast<gtk_webkit_engine *>(arg);
      gtk_widget_hide(engine->m_window);
      return false;
  }

  static gboolean hideWindow(GtkWidget *window, GdkEvent *event,gpointer arg){

       gtk_widget_hide(window);
       return true;
  }

  static gboolean showWindowMain(gpointer arg){
      auto *engine = static_cast<gtk_webkit_engine *>(arg);
      gtk_window_set_position( GTK_WINDOW(engine->m_window), GTK_WIN_POS_CENTER_ALWAYS );
      gtk_widget_show_all(engine->m_window);
      gtk_window_present(GTK_WINDOW(engine->m_window));
      return false;
  }

  void show(){
      g_idle_add(GSourceFunc(showWindowMain),this);
  }


  void dispatch(std::function<void()> f) {
    g_idle_add_full(G_PRIORITY_HIGH_IDLE, (GSourceFunc)([](void *f) -> int {
                      (*static_cast<dispatch_fn_t *>(f))();
                      return G_SOURCE_REMOVE;
                    }),
                    new std::function<void()>(f),
                    [](void *f) { delete static_cast<dispatch_fn_t *>(f); });
  }

  void set_title(const std::string title) {
    gtk_window_set_title(GTK_WINDOW(m_window), title.c_str());
  }

  void set_size(int width, int height, int hints) {
    gtk_window_set_resizable(GTK_WINDOW(m_window), hints != WEBVIEW_HINT_FIXED);
    if (hints == WEBVIEW_HINT_NONE) {
      gtk_window_resize(GTK_WINDOW(m_window), width, height);
    } else if (hints == WEBVIEW_HINT_FIXED) {
      gtk_widget_set_size_request(m_window, width, height);
    } else {
      GdkGeometry g;
      g.min_width = g.max_width = width;
      g.min_height = g.max_height = height;
      GdkWindowHints h =
          (hints == WEBVIEW_HINT_MIN ? GDK_HINT_MIN_SIZE : GDK_HINT_MAX_SIZE);
      // This defines either MIN_SIZE, or MAX_SIZE, but not both:
      gtk_window_set_geometry_hints(GTK_WINDOW(m_window), nullptr, &g, h);
    }
  }

  void navigate(const std::string url) {
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(m_webview), url.c_str());
  }

  void init(const std::string js) {
    WebKitUserContentManager *manager =
        webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(m_webview));
    webkit_user_content_manager_add_script(
        manager, webkit_user_script_new(
                     js.c_str(), WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                     WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START, NULL, NULL));
  }

  void eval(const std::string js) {
    webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(m_webview), js.c_str(), NULL,
                                   NULL, NULL);
  }
private:
  virtual void on_message(const std::string msg) = 0;
  GtkWidget *m_window;
  GtkWidget *m_webview;
  bool m_hide;
};

using browser_engine = gtk_webkit_engine;

} // namespace webview
