# Multithreaded-Proxy-Server
This project is the second programming assignment in UNT's Intro to Networks class.

##Instructions
* compile proxy-server.c using 'gcc -pthread'
* run compiled program "./a.out"
* go to proper url on machine and use the port 8080
	* ex: if using cse04.cse.unt.edu to run server, go to cse04.cse.unt.edu:8080
* server will serve the index.html file located in the resources directory
* open another window and try going to www.bing.com
	* you will be redirected to a page that says Status: 401 Access Denied

##Approach and Limitations
I simply took the example code from blackboard and implemented it into my pre-existing code from the first programming assignment. 

##Screenshots:
####Showing how program can load index file and block user from going to a blacklisted url simultaneously
![alt tag](https://raw.githubusercontent.com/caseycarroll42/Multithreaded-Proxy-Server/master/multi-thread-screenshot.png)
