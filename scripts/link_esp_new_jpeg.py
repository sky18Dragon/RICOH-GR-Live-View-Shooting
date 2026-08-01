"""Link Espressif's prebuilt esp_new_jpeg archive for the selected SoC.

The ESP Component Registry package ships headers and target-specific static
libraries, but no PlatformIO build metadata for those archives.
"""

Import("env")

from os.path import join


component_dir = join(
    env.subst("$PROJECT_LIBDEPS_DIR"),
    env.subst("$PIOENV"),
    "esp_new_jpeg",
)
target = env.BoardConfig().get("build.mcu", "esp32s3")

env.Append(
    CPPPATH=[join(component_dir, "include")],
    LIBPATH=[join(component_dir, "lib", target)],
    LIBS=["esp_new_jpeg"],
)
