VIA_ENABLE = yes
VIAL_ENABLE = yes
VIAL_INSECURE = yes
SRC += via_he.c

# Stock QMK mouse keys instead of the analog mousekey feature.
OPT_DEFS += -DANALOG_MOUSEKEY_ENABLE=0
