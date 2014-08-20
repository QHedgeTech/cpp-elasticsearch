/*
 * Licensed to cpp-elasticsearch under one or more contributor
 * license agreements. See the NOTICE file distributed with
 * this work for additional information regarding copyright
 * ownership. Elasticsearch licenses this file to you under
 * the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef HTTP_H
#define HTTP_H

#include <string>
#include <mutex>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>

#include "json/json.h"

#define _TEXT_PLAIN "text/plain"
#define _APPLICATION_JSON "application/json"
#define _APPLICATION_URLENCODED "application/x-www-form-urlencoded"

#define EXCEPTION(...) throw Exception(__FILE__, __LINE__, __VA_ARGS__)

class Exception : std::exception {
    public:
        template<typename T>
        Exception(const char* fil, int lin, T const& msg);
        virtual ~Exception() throw() {}

        const char* what() const throw() { return _msg.c_str(); }

    private:
        Exception(){}
        std::string _msg;
};

template<typename T>
Exception::Exception(const char* fil, int lin, T const& msg) {
    _msg = msg;
    std::cerr << "Exception in "<< fil << " l. " << lin << " ->\n";
    std::cerr << msg << std::endl;
}

enum Result {
    OK,
    ERROR,
    MORE_DATA
};

class HTTP {
    public:
        HTTP(std::string url, bool keepAlive);
        ~HTTP();

        /// DEPRECATED
        /// Generic request that parses the result in Json::Object.
        bool request(const char* method, const char* endUrl, const char* data, Json::Object* root, const char* content_type = _APPLICATION_JSON);

        /// Generic request that parses the result in Json::Object.
        unsigned int request(const char* method, const char* endUrl, const char* data, Json::Object* root, Result& result, const char* content_type = _APPLICATION_JSON);

        /// DEPRECATED
        /// Generic request that stores result in the string.
        bool request(const char* method, const char* endUrl, const char* data, std::string& output, const char* content_type = _APPLICATION_JSON);

        /// Generic request that stores result in the string.
        unsigned int request(const char* method, const char* endUrl, const char* data, std::string& output, Result& result, const char* content_type = _APPLICATION_JSON);

        /// Generic get request to node.
        inline unsigned int get(const char* endUrl, const char* data, Json::Object* root){
            Result result;
            return request("GET", endUrl, data, root, result);
        }

        /// Generic head request to node.
        inline unsigned int head(const char* endUrl, const char* data, Json::Object* root){
            Result result;
            return request("HEAD", endUrl, data, root, result);
        }

        /// Generic put request to node.
        inline unsigned int put(const char* endUrl, const char* data, Json::Object* root){
            Result result;
            return request("PUT", endUrl, data, root, result);
        }

        /// Generic post request to node.
        inline unsigned int post(const char* endUrl, const char* data, Json::Object* root){
            Result result;
            return request("POST", endUrl, data, root, result);
        }

        /// Generic delete request to node.
        inline unsigned int remove(const char* endUrl, const char* data, Json::Object* root){
            Result result;
            return request("DELETE", endUrl, data, root, result);
        }

        /// Generic post request to node.
        inline unsigned int rawpost(const char* endUrl, const char* data, Json::Object* root){
            Result result;
            return request("POST", endUrl, data, root, result, _APPLICATION_URLENCODED);
        }

    private:

        /// Returns true if managed to connect.
        bool connect();

        /// Parse the message and split if necessary.
        bool sendMessage(const char* method, const char* endUrl, const char* data, const char* content_type);

        /// Write string on the socketfd.
        bool write(const std::string& outgoing);

        /// Test socket point.
        inline bool connected() const { return (_sockfd >= 0); }

        /// Close the socket.
        void disconnect();

        /// Whole process to read the response from HTTP server.
        unsigned int readMessage(std::string& output, Result& result);

        /// Wait with select then start to read the message.
        unsigned int readMessage(std::string& output, size_t& contentLength, bool& isChunked, Result& result);

        /// Methods to read chunked messages.
        unsigned int parseMessage(std::string& output, size_t& contentLength, bool& isChunked, Result& result);

        /// Append the chunk message to the stream.
        size_t appendChunk(std::string& output, char* msg, size_t msgSize);

        /// Check if the connection is on error state.
        bool error();

        /// Determine if we must reconnect.
        inline bool mustReconnect() const { return (_keepAliveTimeout <= time(NULL) - _lastRequest); }

        std::string _url;
        std::string _urn;
        int _port;
        unsigned int _connection;
        int _sockfd;
        struct sockaddr_in _client;
        bool _keepAlive;
        time_t _keepAliveTimeout;
        time_t _lastRequest;

        /// Mutex for every request.
        std::mutex _requestMutex;
};

#endif // HTTP_H
