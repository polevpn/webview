# webview for golang and c/c++

A tiny cross-platform webview library for C/C++/Golang to build modern cross-platform GUIs. 

The goal of the project is to create a common HTML5 UI abstraction layer for the most widely used platforms. 

It supports two-way JavaScript bindings (to call JavaScript from C/C++/Go and to call C/C++/Go from JavaScript).

It uses Cocoa/WebKit on macOS, gtk-webkit2 on Linux and Edge on Windows 10.

## Webview for Go developers

If you are interested in writing Webview apps in C/C++, [skip to the next section](#webview-for-cc-developers).

### Getting started

Install Webview library with `go get`:

```
$ go get github.com/polevpn/webview
```

Import the package and start using it:

```go
package main

import "github.com/polevpn/webview"

func main() {
	debug := true
	w := webview.New(false,true)
	defer w.Destroy()
	w.SetTitle("Minimal webview example")
	w.SetSize(800, 600, webview.HintNone)
	w.Navigate("https://en.m.wikipedia.org/wiki/Main_Page")
	w.Run()
}
```

To build the app use the following commands:

```bash
# Linux
$ go build -o webview-example && ./webview-example

# MacOS uses app bundles for GUI apps
$ mkdir -p example.app/Contents/MacOS
$ go build -o example.app/Contents/MacOS/example
$ open example.app # Or click on the app in Finder

# Windows requires special linker flags for GUI apps.
# It's also recommended to use (mingw) compiler for CGo
# https://github.com/niXman/mingw-builds-binaries/releases use win32-seh-msvcrt-rt_v10 version
$ go build -ldflags="-H windowsgui" -o webview-example.exe
```

### Distributing webview apps

On Linux you get a standalone executable. It will depend on GTK3 and GtkWebkit2, so if you distribute your app in DEB or RPM format include those dependencies. An application icon can be specified by providing a `.desktop` file.

On MacOS you are likely to ship an app bundle. Make the following directory structure and just zip it:

```
example.app
└── Contents
    ├── Info.plist
    ├── MacOS
    |   └── example
    └── Resources
        └── example.icns
```

Here, `Info.plist` is a [property list file](https://developer.apple.com/library/content/documentation/General/Reference/InfoPlistKeyReference/Articles/AboutInformationPropertyListFiles.html) and `*.icns` is a special icon format. You may convert PNG to icns [online](https://iconverticons.com/online/).

On Windows you probably would like to have a custom icon for your executable. It can be done by providing a resource file, compiling it and linking with it. Typically, `windres` utility is used to compile resources. Also, on Windows, `WebView2Loader.dll` must be placed into the same directory with your app executable.

## Webview for C/C++ developers

### C++:
```c
// main.cc
#include "webview.h"
#ifdef WIN32
int WINAPI WinMain(HINSTANCE hInt, HINSTANCE hPrevInst, LPSTR lpCmdLine,
                   int nCmdShow) {
#else
int main() {
#endif
  webview::webview w(false,true, nullptr);
  w.set_title("Minimal example");
  w.set_size(480, 320, WEBVIEW_HINT_NONE);
  w.navigate("https://en.m.wikipedia.org/wiki/Main_Page");
  w.run();
  return 0;
}
```
Build it:

```bash
# Linux
$ g++ main.cc `pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0` -o webview-example
# MacOS
$ g++ main.cc -std=c++11 -framework WebKit -o webview-example
# Windows (x64)
$ g++ main.cc -std=c++11 -mwindows -o webview-example.exe -I./libwebview2/build/native/include -Wl,-Bstatic -lstdc++ -lgcc_eh -lpthread -Wl,-Bdynamic -lole32 -lShlwapi -L./libwebview2/build/native/x64 -lWebView2Loader
```

### C:
```c
// main .c
#include "webview.h"
#ifdef WIN32
int WINAPI WinMain(HINSTANCE hInt, HINSTANCE hPrevInst, LPSTR lpCmdLine,
                   int nCmdShow) {
#else
int main() {
#endif
	webview_t w = webview_create(0,0, NULL);
	webview_set_title(w, "Webview Example");
	webview_set_size(w, 480, 320, WEBVIEW_HINT_NONE);
	webview_navigate(w, "https://en.m.wikipedia.org/wiki/Main_Page");
	webview_run(w);
	webview_destroy(w);
	return 0;
}
```
Build it:

```bash
# Linux
$ g++ main.c `pkg-config --cflags --libs gtk+-3.0 webkit2gtk-4.0` -o webview-example
# MacOS
$ g++ main.c -std=c++11 -framework WebKit -o webview-example
# Windows (x64)
$ g++ main.c -mwindows -o webview-example.exe -I./libwebview2/build/native/include -lole32 -lShlwapi -L./libwebview2/build/native/x64 -lWebView2Loader
```

On Windows it is possible to use webview library directly when compiling with cgo, but WebView2Loader.dll is still required. To use MinGW you may dynamically link prebuilt webview.dll (this approach is used in Cgo bindings).

Full C/C++ API is described at the top of the `webview.h` file.

## Notes

Execution on OpenBSD requires `wxallowed` [mount(8)](https://man.openbsd.org/mount.8) option.
For Ubuntu Users run `sudo apt install webkit2gtk-4.0`(Try with webkit2gtk-4.0-dev if webkit2gtk-4.0 is not found) to install webkit2gtk-4.0 related items.
FreeBSD is also supported, to install webkit2 run `pkg install webkit2-gtk3`.

## License

Code is distributed under MIT license, feel free to use it in your proprietary
projects as well.

