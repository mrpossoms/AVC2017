#!/usr/bin/python

from flask import * 
from threading import Thread
import os

app = Flask(__name__)

CURRENT_THREAD = None

def view(params=None):
	return render_template('index.html', model=params)

def model(request):
	model = {}

	for prop in ['steering', 'throttle']:
		try:
			model[prop] = request.args.get(prop, '')
		except NameError:
			print(prop + " not found")

	return model

def collector():
	os.system('collector -r -m/media/training')

def predictor(pwm_msk):
	os.system('collector -i -a | predictor -r /media/training/0.route -m' + str(pwm_msk))
	

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
	return view(params=model(request)) 

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

	return view(params=m) 

@app.route('/stop')
def stop():
	os.system('killall collector predictor')

	return view(params=model(request)) 

app.run(host='0.0.0.0')
url_for('static', filename='bootstrap.min.css')
