#!/usr/bin/python

from flask import * 
import os
app = Flask(__name__)


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

@app.route('/')
def index():
	return view(params=model(request)) 

@app.route('/route')
def route():
	os.system('collector -r -m/media/training')
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

	os.system('collector -i -a | predictor -r /media/training/0.route -m' + str(pwm_msk))

	return view(params=m) 

app.run(host='0.0.0.0')
url_for('static', filename='bootstrap.min.css')
