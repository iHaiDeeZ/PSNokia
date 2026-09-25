#!/bin/bash
# Builds PCSL, the CLDC VM and MIDP for the PS4, in that order.
D="$(dirname "$0")"
bash "$D/build_pcsl.sh" && bash "$D/build_cldc.sh" && bash "$D/build_midp.sh"
