#!/bin/bash
PWD=$(pwd)
scp -rv ~/source/knulli-distribution/output/h700/build/batocera-emulationstation-quick_resume_mode/emulationstation root@RG35XXSP:~/emulationstation.new
ssh root@RG35XXSP "./es-kill.sh && echo \"killing ES...\" && sleep 7.5 && cp -rv ~/emulationstation.new /usr/bin/emulationstation && batocera-save-overlay && reboot"
cd "$PWD"
echo
echo "Deployment complete."
echo
