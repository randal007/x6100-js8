# X6100 diagnostics for Windows

`x6100-diag.ps1` collects what we need to track down a problem on a radio
running the X6100 JS8 firmware, over the USB cable. It **only reads**: it
changes no setting, writes nothing on the radio and never transmits by
itself.

What it collects: firmware and BASE versions, the transmit settings (TX
power, TX codec gain, band output gain corrections, the FT8/JS8 learned
drive level, TX filter, CESSB), the app log and kernel messages. If you
agree, it also records the app log while *you* press TUNE, talk on SSB and
send a JS8 CQ, and asks what your power meter showed. Everything goes into
one text file on your Desktop (`x6100-diag-<date>.txt`): send that file
back.

## Running it

1. Power the radio on with the JS8 SD card in and wait for the main screen.
2. Connect a USB-C cable from the PC to the radio's **DEV** port (not HOST).
   Windows should add two COM ports ("USB Serial Device"); the script finds
   the right one itself. If Device Manager shows an unknown device instead,
   install WCH's CH343SER driver from wch-ic.com.
3. Download `x6100-diag.ps1`, right-click it > **Properties** > tick
   **Unblock** > OK (Windows blocks downloaded scripts).
4. Right-click it > **Run with PowerShell**. Or, in a PowerShell window in
   the download folder:

   ```
   powershell -ExecutionPolicy Bypass -File .\x6100-diag.ps1
   ```

   Options: `-Port COM7` to pick the port yourself, `-SkipTx` to skip the
   transmit tests.

For the transmit tests use a dummy load (or a clear frequency) and put any
amplifier on standby.

If the radio is running the stock Xiegu firmware (no SD card), the script
stops without typing anything.
