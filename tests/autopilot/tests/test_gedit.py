
import os
import time
import autopilot.emulators.bamf
from unity.tests import UnityTestCase
from testtools.matchers import NotEquals
from autopilot.matchers import Eventually
from autopilot.introspection.gtk import GtkIntrospectionTestMixin
from unity.emulators.unity import Unity
from unity.emulators.panel import UnityPanel



def find_process(process_name):
    """Returns a psutil process similar to a Popen process."""

    def process_name_equals(process):
        if process.name == process_name:
            return True
        else:
            return False
    processes = filter(process_name_equals, psutil.get_process_list())
    try:
        return processes[0]
    except IndexError:
        return None


class GeditTestCase(UnityTestCase, GtkIntrospectionTestMixin):


    def add_autopilot_to_gtk_modules(self):
        """Prepare the application, or environment to launch with
        autopilot-support.
        """
    
        modules = os.getenv('GTK_MODULES', '').split(':')
        if 'autopilot' not in modules:
            modules.append('autopilot')
            os.putenv('GTK_MODULES', ':'.join(modules))
    
    
    def start_app_window(self, app_name, files=[], locale=None):
        """Add 'autopilot' to the GTK_MODULES env var."""
    
        self.add_autopilot_to_gtk_modules()
        unity_app = super(GeditTestCase, self).start_app_window(
            app_name, files, locale)
        return unity_app


    def setUp(self):
        super(GeditTestCase, self).setUp()
        self.panel_controller = Unity.get_root_instance().panels
        # Open gedit
        #self.app = self.start_app_window('Text Editor')
        #self.app.set_focus()
        #self.assertTrue(self.app.is_focused)
        self.app = self.launch_test_application('gedit')


    def test_file_exit(self):
        # Open the file menu
        # FIXME get rid of this sleep--how to add "wait for" here?
        time.sleep(3)
        panel = self.panel_controller.get_active_panel()
        file_menu = panel.menus.get_menu_by_label('_File')
        self.assertThat(file_menu, NotEquals(None))
        file_menu.mouse_click()

        # Guess where the 'Exit' item is
        # FIXME get rid of this sleep
        time.sleep(5)

        menu_item = self.app.select_single('GtkAccelLabel', label='_Quit')
        self.assertTrue(menu_item, NotEquals(None))
        self.mouse.move_to_object(menu_item)
        self.click()

        # Make sure we exited
        # FIXME get rid of this sleep
        time.sleep(1)
        self.assertTrue(not self.app_is_running('Text Editor'))
