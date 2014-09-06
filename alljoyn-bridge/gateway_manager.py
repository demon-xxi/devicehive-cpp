#! /usr/bin/env python

import subprocess
import json
import time
import urllib2

GATEWAY_PATH = '/usr/bin/DH_alljoyn'
INSTANCES_URL = 'http://www.cloud.devicehive.com/devicehive/management/instances/list'

processes = list()
exec_count = 0

def get_service_info(exec_count):
	if True:
		try:
			req = urllib2.Request(INSTANCES_URL)
			opener = urllib2.build_opener()
			f = opener.open(req)
			s = f.read()
		except Exception as ex:
			print 'FAILED to get instance list', ex
			return None
	else:  # fake data
		s = "[{\"channel\":\"com.devicehive.service1\", \"restUrl\":\"http://service1.cloud.devicehive.com\"}, {\"channel\":\"com.devicehive.service2\", \"restUrl\":\"http://service2.devicehive.com\"}]" 
		if exec_count % 2 :
			s = "[{\"channel\":\"com.devicehive.service2\", \"restUrl\":\"http://service2.devicehive.com\"}]"
	return s

def service_exists(name):
	for i in processes:
		if (i['service'] == name):
			i['alive'] = True
			return True
	return False

def start_gateway(service, url):
	log = open('/tmp/DH_alljoyn-'+service+'.log', 'w')
	log_detail = '/tmp/DH_alljoyn-'+service+'.detail.log';
	process = subprocess.Popen([GATEWAY_PATH, '--service', service, '--server', url, '--log', log_detail], shell=False, stdout=log, stderr=log)
	return process

def reset_alive():
	for i in processes:
		i['alive'] = False

def kill_orphans():
	for i in processes[:]:
		if not i['alive']:
			print 'Killing', i['service'], 'PID:', i['process'].pid
			i['process'].kill
			processes.remove(i)

#main
try:
	while True:
		print '='*30
		info_json = get_service_info(exec_count)
		if info_json is None:
			time.sleep(10)
			continue
		info = json.loads(info_json)
		reset_alive()
		for f in info:
			service = f['channel'];
			url = f['restUrl'];
			if not (service_exists(service)):
				process = start_gateway(service, url)
				print 'Service', service, 'has been started, PID:', process.pid
				processes.append({'service':service, 'url':url, 'process':process, 'alive':True })
			else:
				pass #print 'Service', service, 'already exists'
		kill_orphans()
		# TODO: restart dead gateways (check by PID)
		exec_count += 1
		time.sleep(10)
except:
	print 'stopping...'
	reset_alive()
	kill_orphans()
