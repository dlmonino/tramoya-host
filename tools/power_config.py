"""Keep incremental firmware builds on the USB-safe CPU power policy."""
import re


POWER_CONFIG = {
    "CONFIG_PM_ENABLE": "y",
    "CONFIG_PM_DFS_INIT_AUTO": "n",
    "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_40": "n",
    "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_80": "n",
    "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_160": "y",
    "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240": "n",
    "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ": "160",
}
SETTING = re.compile(r"^(CONFIG_\w+)=(.*)$|^# (CONFIG_\w+) is not set$")


def read_config(text):
    values = {}
    for line in text.splitlines():
        match = SETTING.fullmatch(line)
        if match:
            key, value, disabled = match.groups()
            values[key or disabled] = value if key else "n"
    return values


def prepare_power_config(path):
    """IDF defaults do not override an existing sdkconfig; update only our
    power settings. Fresh build directories still use sdkconfig.defaults."""
    if not path.exists():
        return
    original = path.read_text()
    values = read_config(original)
    if all(values.get(key, "n") == value for key, value in POWER_CONFIG.items()):
        return
    lines = []
    for line in original.splitlines():
        match = SETTING.fullmatch(line)
        if match and (match[1] or match[3]) in POWER_CONFIG:
            continue
        lines.append(line)
    lines.extend(f"# {key} is not set" if value == "n" else f"{key}={value}"
                 for key, value in POWER_CONFIG.items())
    updated = "\n".join(lines) + "\n"
    if updated != original:
        path.write_text(updated)


def validate_power_config(path):
    values = read_config(path.read_text())
    for key, expected in POWER_CONFIG.items():
        # Hidden, inapplicable Kconfig choices can be omitted by IDF.
        actual = values.get(key, "n")
        if actual != expected:
            raise ValueError(f"{path}: {key}={actual}, expected {expected}; "
                             "rebuild with tools/build.py")
