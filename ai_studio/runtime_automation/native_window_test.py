import unittest
from unittest import mock

import native_window


class NativeWindowTest(unittest.TestCase):
    def test_non_windows_is_explicit_noop(self):
        with mock.patch.object(native_window.os, "name", "posix"):
            self.assertFalse(native_window.resize_and_park_client(123, 1080, 1920))

    def test_missing_process_is_explicit_noop(self):
        self.assertFalse(native_window.resize_and_park_client(None, 1080, 1920))


if __name__ == "__main__":
    unittest.main()
