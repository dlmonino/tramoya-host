"""Regression coverage for upgrading cached pre-power-management builds."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from power_config import prepare_power_config, read_config, validate_power_config


class PowerConfigTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.config = Path(self.temp.name) / "sdkconfig"

    def test_fresh_build_uses_defaults(self):
        prepare_power_config(self.config)
        self.assertFalse(self.config.exists())
        defaults = read_config((Path(__file__).resolve().parents[2] /
                                "sdkconfig.defaults").read_text())
        self.assertEqual(defaults["CONFIG_PM_ENABLE"], "y")
        self.assertEqual(defaults["CONFIG_PM_DFS_INIT_AUTO"], "n")
        self.assertEqual(defaults["CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_160"], "y")

    def test_migrates_old_clocks_and_preserves_board_and_app_settings(self):
        for clock in (80, 160, 240):
            with self.subTest(clock=clock):
                unrelated = ('# User configuration\n'
                             'CONFIG_ESPTOOLPY_FLASHSIZE="16MB"\n'
                             'CONFIG_SPIRAM_MODE_OCT=y\n'
                             'CONFIG_WKAL_AP_SSID="My PS5"\n')
                self.config.write_text(unrelated +
                    '# CONFIG_PM_ENABLE is not set\nCONFIG_PM_DFS_INIT_AUTO=y\n'
                    f'CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_{clock}=y\n'
                    f'CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ={clock}\n')
                prepare_power_config(self.config)
                validate_power_config(self.config)
                self.assertTrue(self.config.read_text().startswith(unrelated))
                values = read_config(self.config.read_text())
                enabled = [key for key, value in values.items()
                           if key.startswith("CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_")
                           and value == "y"]
                self.assertEqual(enabled, ["CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_160"])

    def test_disabled_assignment_and_duplicate_old_keys(self):
        self.config.write_text('CONFIG_PM_ENABLE=n\nCONFIG_PM_ENABLE=n\n')
        prepare_power_config(self.config)
        validate_power_config(self.config)
        self.assertEqual(self.config.read_text().count("CONFIG_PM_ENABLE="), 1)

    def test_repeated_build_does_not_rewrite_config(self):
        self.config.write_text("# CONFIG_PM_ENABLE is not set\n")
        prepare_power_config(self.config)
        # IDF omits unavailable choices such as the FPGA-only 40 MHz entry.
        self.config.write_text(self.config.read_text().replace(
            "# CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_40 is not set\n", ""))
        before = self.config.stat().st_mtime_ns
        contents = self.config.read_bytes()
        prepare_power_config(self.config)
        self.assertEqual(self.config.read_bytes(), contents)
        self.assertEqual(self.config.stat().st_mtime_ns, before)
        validate_power_config(self.config)

    def test_rejects_unsafe_or_disabled_resolved_configuration(self):
        self.config.write_text("")
        prepare_power_config(self.config)
        valid = self.config.read_text()
        for old, new in (
            ("CONFIG_PM_ENABLE=y", "# CONFIG_PM_ENABLE is not set"),
            ("# CONFIG_PM_DFS_INIT_AUTO is not set", "CONFIG_PM_DFS_INIT_AUTO=y"),
            ("CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=160", "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=240"),
        ):
            with self.subTest(setting=old):
                self.config.write_text(valid.replace(old, new))
                with self.assertRaises(ValueError):
                    validate_power_config(self.config)


if __name__ == "__main__":
    unittest.main()
