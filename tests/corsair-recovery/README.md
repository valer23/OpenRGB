# Corsair startup recovery

The owner observed receiver `1B1C:1BA6` present in Windows but absent from OpenRGB
following startup. The startup log rejected unknown capabilities; an awake manual
rescan registered the same receiver. This does not establish the exact earlier
PID reply, only that failed initial identification had no later retry.

The Windows custom-build fix watches ordinary Raw Input only when that receiver
failed identification. It opens no HID reader and sends no polling requests.
A fresh physical mouse event must match the failed control interface's container.
The detection worker retries only that exact interface, revalidates current
identity/age after metadata and immediately before detector I/O, honors disabled
settings, and skips already registered receivers. Busy detection is never blocked
or overwritten. Expired events, cleanup and replug invalidate queued recovery.

The standard detector and registration path still perform initialization and load
the existing active profile. Recovery unsubscribes from Raw Input once no missing
receiver remains. It does not change the native sleep/lighting/battery driver,
ASUS services, Fan Control, profiles, or the installed app until deployment.
Other platforms retain their existing behavior.

Run `./tests/corsair-recovery/Run-Tests.ps1` for deterministic policy/worker/queue
checks and the existing real-driver plus 20,000-step packet comparison. The Windows
observer is compiled against a fake detection consumer: it contains no HID/device
command implementation. An explicit `-ReceiverControlPath` additionally tests
same-process mouse/page registration conflicts and replacement-registration
preservation using Windows metadata only. `corsair_windows_shadow_test.exe
--shadow <control-path>` observes normal physical input for at most two minutes,
records no input coordinates/buttons and sends zero device commands.

Hardware validation must use an isolated OpenRGB instance after the installed one
has exited. A generated, private trial detector may defer its first initialization
without device commands to exercise missing-at-startup recovery. Such test-only
code is not part of the production source, executable or installer.
