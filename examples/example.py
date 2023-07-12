import sys
import gi

gi.require_version("Gtk", "4.0")
gi.require_version("Web", "0.0")
from gi.repository import GLib, Gtk, Web


class MyApplication(Gtk.Application):
    def __init__(self):
        super().__init__(application_id="com.mattjakeman.LibWebGTK.Demo")
        GLib.set_application_name('LibWebGTK Python Demo')

    def do_activate(self):
        window = Gtk.ApplicationWindow(application=self, title="LibWeb GTK Demo")
        window.set_default_size(720, 480)

        Web.embed_init()

        web = Web.ContentView()
        web.load("https://mattjakeman.com")

        scroll_area = Gtk.ScrolledWindow()
        scroll_area.set_child(web)

        window.set_child(scroll_area)
        window.present()


app = MyApplication()
exit_status = app.run(sys.argv)
sys.exit(exit_status)
