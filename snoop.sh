ssh protean.io "ssh pi@192.168.1.198 \"sudo collector -i -a | predictor -r /media/training/0.route -m0 -f\"" | ./viewer
