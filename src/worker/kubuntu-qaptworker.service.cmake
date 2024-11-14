[Unit]
Description=KUbuntu qaptworker daemon service

[Service]
Type=dbus
BusName=@QAPT_WORKER_RDN_VERSIONED@
ExecStart=/usr/bin/qaptworker@QAPT_WORKER_VERSION@
StandardOutput=syslog
# Can not detect all package script or install actions.
CapabilityBoundingSet=~
MemoryLimit=8G
