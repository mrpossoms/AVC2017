#!/usr/bin/python

from flask import * 
from threading import Thread
import os
import time

app = Flask(__name__)

CURRENT_THREAD = None
MEDIA='/var/training'

def view(params=None):
	return render_template('index.html', model=params)

def model(request):
	model = {}

	for prop in ['steering', 'throttle', 'goodness', 'badness']:
		try:
			model[prop] = request.args.get(prop, '')
		except NameError:
			print(prop + " not found")

	return model

def collector():
	os.system('collector -r -m%s' % MEDIA)

def predictor(pwm_msk):
	os.system('collector -i -a | predictor -r%s/0.route -m%s' % (MEDIA, str(pwm_msk)))
	
def cal_color(goodbad, name):
	os.system('collector -i -a | %s -n%s' % (goodbad, name))

@app.route('/')
def index():
	return view(params=model(request)) 

@app.route('/route')
def route():
	CURRENT_THREAD = Thread(target=collector)
	CURRENT_THREAD.start()
	return view(params=model(request)) 

@app.route('/training')
def training():
	return redirect('/') 


@app.route('/calgood')
def good():
    m = model(request)
    name = m['goodness']
    CURRENT_THREAD = Thread(target=cal_color, args=('good', name))
    CURRENT_THREAD.start()
    return redirect('/')


@app.route('/calbad')
def bad():
    m = model(request)
    name = m['badness']
    CURRENT_THREAD = Thread(target=cal_color, args=('bad', name))
    CURRENT_THREAD.start()
    return redirect('/')


@app.route('/clearbad')
def clear_bad():
	os.system('rm -rf /var/predictor/color/bad')
	os.system('mkdir /var/predictor/color/bad')
	return redirect('/')


@app.route('/cleargood')
def clear_good():
	os.system('rm -rf /var/predictor/color/good')
	os.system('mkdir /var/predictor/color/good')
	return redirect('/')


@app.route('/run')
def run():
	m = model(request)
	
	pwm_msk = 0x6
	
	if m['steering']:
		print('Control steering')
		pwm_msk &= 0x2

	if m['throttle']:
		print('Control throttle')
		pwm_msk &= 0x4

	print(str(pwm_msk))
	CURRENT_THREAD = Thread(target=predictor, args=(pwm_msk,))
	CURRENT_THREAD.start()

	return redirect('/') 


@app.route('/reboot')
def reboot():
	os.system('reboot 1 &')
	return redirect('/') 


@app.route('/shutdown')
def shutdown():
	os.system('shutdown 0 &')
	return redirect('/') 


@app.route('/stop')
def stop():
	os.system('killall -s2 collector predictor good bad')

	return redirect('/') 

try:
    os.chdir('/root')

    with open('/sys/class/leds/led0/brightness', 'a', 0) as fp:
            for _ in range(1, 10):
                    fp.write('1')
                    time.sleep(0.25)
                    fp.write('0')
    time.sleep(0.25)
except PermissionError:
    print("Couldn't cd to /root")

app.run(host='0.0.0.0')
url_for('static', filename='bootstrap.min.css')
