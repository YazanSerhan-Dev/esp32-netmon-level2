# Linux Scripts

This folder contains helper scripts to install and remove the Linux-side NetMon collector
in a repeatable and production-safe way.

## Scripts

### install.sh
Installs the NetMon collector runtime on the machine.

What it does:
- Creates required directories (`/opt/netmon`, `/var/log/netmon`)
- Copies the `server/` code into `/opt/netmon`
- Installs the systemd unit file: `netmon-collector.service`
- Enables and starts the service
- Installs the logrotate configuration

Run:
```bash
./linux/scripts/install.sh
```

---

### uninstall.sh
Removes the NetMon collector service and related configuration.

Default behavior (safe):
- Stops and disables the systemd service
- Removes systemd unit file
- Removes logrotate configuration
- Keeps application and log directories

Run:
```bash
./linux/scripts/uninstall.sh
```

Full cleanup (dangerous – deletes app + logs):
```bash
./linux/scripts/uninstall.sh --purge
```

---

## Notes
- These scripts require sudo privileges.
- Collector logs can be viewed using:
```bash
journalctl -u netmon-collector -f
```
- Log files are written to:
  - `/var/log/netmon/metrics.log`
  - `/var/log/netmon/latest.json`
