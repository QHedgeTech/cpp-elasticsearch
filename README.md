cpp-elasticsearch
=================

C++ Client for Elasticsearch
----------------------------

cpp-elasticsearch is a small C++ API to elasticsearch, it aims at becomming the official one. 

For this purpose, contributors must respect the following rules:
- Master branch must be in line with latest Elasticsearch release
- Be in line with https://github.com/elasticsearch/elasticsearch/tree/master/rest-api-spec
- Document the code
- Included in travis CI

Once the required features will be added, new features will come like:
- Load balancing
- Connection pooling
- Library package
- Performance monitoring

A documentation is stil to come as well as a "get started" page and continous intragration project with Travis.

## Get Source and Build ##

```
git clone https://github.com/QHedgeTech/cpp-elasticsearch.git
cd cpp-elasticsearch

#build example getstarted
scons project=getstarted mode=debug

#start example getstarted
example/bin/getstarted-debug-gnu

#clean example
scons -c project=getstarted


```
For debug builds, use "scons mode=debug"

Warning
-------

- Actual version is minimal and has been developped for the needs of Q-Hedge Technologies.
- Features of C++11 are used in this code.

Dependencies
------------

The current version works on Linux/MacOS platform and is POSIX compliant without any dependency. The code may still be hacked to be used on other platform or with third party tools: any JSON parser, or libcurl for the connection for instance.

The JSON parser and the HTTP connection classes are not the purpose of this project. However, they are provided so this project can work as a stand-alone tool. They must be bug free  sufficient (if not optimal) for the elasticsearch client.
