# Task: fix my SL-Infinity lighting

The existing Lian Li SL-Infinity support does not change my fans' colours.
One hub, 3/2/1/0 fans on ports 1/2/3/4. Centres and side strips are separate
logical channels: two per physical port. Keep static streaming and existing
hardware effects available. Cooling is managed by Fan Control and must not change.

## Observed vendor/replay traffic

USB 0CF2:A102; lighting interface 1, usage page FF72, usage A1.
Packets below include report ID E0. Hexadecimal values:

- Disable motherboard lighting sync: feature report E0 10 61 00 00 00 00.
- Configure a physical port: feature report E0 10 60 PP NN 00 00, where PP
  is port 1..4 and NN the number of fans. The setup describes the whole port,
  not an independent count for its centre and side channels.
- Colour data: 353-byte output report, E0 then 30+logical-channel (0..7),
  then R,B,G triplets. Existing device LED mapping/brightness should be examined.
- Commit: seven-byte feature report E0, 10+logical-channel, effect, speed,
  direction, brightness, 00. Speed and direction are separate bytes.
  A captured static commit is E0 10 01 00 00 00 00 for logical channel 0.
  Vendor SetEffectSetting also takes separate speed, direction and brightness.
  Static effect is 01; existing
  hardware effect definitions should be preserved.

## Prior hardware observations

Windows hid_write did not produce the verified response; control SET_REPORT
colour output did. Feature reports are required for setup and commit. Setup
once per changed port/count is sufficient, but colour and commit are required
on every update. Removing commits left fans unchanged. Pauses of 20 ms after
disabling lighting sync and 5 ms after subsequent reports worked. MinGW C++
short waits rounded up noticeably on the tested Windows setup; native Windows
waits improved this. Full fan-hub throughput remains about five frames/sec for
six populated logical channels. Do not claim 30 FPS hardware updates.

Static RGB/purple, hardware rainbow and continuous software colours were
physically confirmed with these reports. Other hubs and platforms are unverified.
Do not change fan PWM/RPM sync. Sparse/empty channels and partial 16-LED fan
groups must not prevent later populated channels updating.

## Deliverable

Implement a minimal production fix. Check short and failed reports; do not
commit after a failed colour report, or flood logs on persistent failures.
Preserve successful report bytes and existing hardware modes. Explain validation
limits. Keep local diagnostics outside the production diff. No hardware,
network, other repositories or original finished solutions. Do not commit or
publish. Read and edit only the supplied task directory.
