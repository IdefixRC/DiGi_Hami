"""
DiGi Hami - automatic filesystem upload
=======================================

PlatformIO / pioarduino post-action hook.

After the firmware is successfully flashed to the ESP32 (the "upload"
target), this immediately uploads the SPIFFS filesystem image as well.
That image contains ``data/animation.gif`` - the hamster animation - which
the firmware reads at runtime.

The net effect: pressing **Upload** once in pioarduino gives you a fully
working DiGi Hami, with no separate "Upload Filesystem Image" step.

Fallback (README step 4.7): if the animation ever fails to upload this way
(for example the board re-enumerates too slowly after the firmware flash),
just run it by hand:

    * pioarduino sidebar -> Project Tasks -> ... -> Platform ->
      "Upload Filesystem Image", or
    * a terminal:  pio run -t uploadfs

This hook only *adds* behaviour to "upload"; it never modifies the
"uploadfs" target itself, so the manual path is always available.
"""

Import("env")  # noqa: F821  (injected by PlatformIO/pioarduino)


def _upload_filesystem(source, target, env):
    print("DiGi Hami: firmware uploaded - now uploading the animation "
          "(SPIFFS image with data/animation.gif)...")
    # Run the filesystem upload for this same environment, in a child
    # process, using the same Python/PlatformIO that is running this build.
    # "uploadfs" does not trigger the "upload" target, so this does not
    # recurse.
    result = env.Execute(
        env.VerboseAction(
            '"$PYTHONEXE" -m platformio run --environment "$PIOENV" '
            '--target uploadfs',
            "Uploading filesystem image (data/animation.gif)",
        )
    )
    if result != 0:
        print("DiGi Hami: automatic animation upload failed. Flash it "
              "manually - see README step 4.7 (Upload Filesystem Image / "
              "'pio run -t uploadfs').")


env.AddPostAction("upload", _upload_filesystem)  # noqa: F821
