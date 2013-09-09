redis_sip_worker
========================


describtion
------------------------
this worker subscribe a redis channel 'valta_sip_out', when receive a message, it send it out 
through sip. and when it receive a message from sip, it publishs the message to redis channel 'valta_sip_in'.

it uses hiredis with libevent to handle interface to redis, and uses pjsip to handle interface to sip. the main
thread goes into libevent and handle passing message to a callback, which then use pjsip_send_im to send out. pjsip
also create a thread underneath to handle passing sip message to on_pager callback, which then use redisCommand to 
publish. 


message format
------------------------
message from redis is a simple json array string,which contain two objects. The first is a valid sip address,
exp. <sip:example@example.com>; the second is a arbitrary string. 

Because in order for a sidekiq worker to handle the message, the massage published to redis is a bit more specific.
it follows sidekiq's convention. it's a json hash, contain four keys, which are 'retry', 'queue', 'class', and 'args'.
'retry' denote weither redis should retry to push, 'queue' specify which queue to push, which must be 'default', 'class'
is the name of the sidekiq worker, and 'args' is the arguments send to the worker, it's a array, whose lenght must be
the same as that of the 'perform' method of the worker.

installation
------------------------

dependencies:
	hiredis: https://github.com/redis/hiredis
	libevent: http://libevent.org
	json-c: https://github.com/json-c/json-c
	pjsip: http://www.pjsip.org


Make sure the dependencies are available on your platform, and run
     $make


Usage
------------------------

	$./sip_worker


More to come
------------------------

current, many parameters are hard coded, like redis server address, sip account and many other
so next release will be make it accept parameters and log file.