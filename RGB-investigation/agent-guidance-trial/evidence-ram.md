# Task: duplicate RAM entries on Windows

OpenRGB shows four Corsair Vengeance RGB DDR5 entries although my ASUS ROG
STRIX B850-F GAMING WIFI7 NEO has two physical sticks. Entries 1 and 3 control
the same first stick; 2 and 4 control the same second stick. All entries can
change the physical colours. Fan Control stays running and must remain working.

Investigate the existing Windows PawnIO PIIX4 bus detection/port selection.
Prior diagnostic observations: a failed requested port selection can leave a
handle on its default port. Registering that handle anyway can make two bus
interfaces enumerate the same sticks. Windows SMBus access uses a named mutex
shared with other processes; port selection must respect that access convention.
Inspect how the existing backend serializes normal transactions.

Fix the underlying registration/selection failure rather than hiding duplicate
RAM entries in the UI. Handle mutex wait failure and abandoned ownership,
selection failure and handle cleanup. Preserve successful detection. Other
motherboards are unverified. The prior working behaviour is two independently
controllable sticks including after reboot, but your candidate still requires
its own validation.

Implement only the appropriate production change and explain what you could
and could not verify. Local checks may stay outside the proposed production
diff. No hardware, network, other repositories or completed solutions. Do not
commit or publish. Read and edit only the supplied task directory.
