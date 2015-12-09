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


#include "http.h"

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <iostream>
#include <netdb.h>
#include <unistd.h>
#include <cassert>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <algorithm>

#include <fcntl.h>

/** Returns true on success, or false if there was an error */
bool SetSocketBlockingEnabled(int fd, bool blocking) {
   if (fd < 0) return false;

   int flags = fcntl(fd, F_GETFL, 0);

   if (flags < 0)
       return false;

   flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);

   return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
}

int to_int(const std::string& str){
    int numb;
    std::istringstream ( str ) >> numb;
    return numb;
}

HTTP::HTTP(std::string uri, bool keepAlive)
: _connection(0),
  _sockfd(-1),
  _keepAlive(keepAlive),
  _keepAliveTimeout(60),
  _lastRequest(0)
{
    // Remove http protocol if set.
    size_t pos = uri.find("http://");
    if( pos != std::string::npos)
        uri = uri.substr(pos + 7);

    // Extract the URN
    pos = uri.find("/");
    if(pos != std::string::npos){
        _url = uri.substr(0, pos);
        _urn = uri.substr(pos);
    } else {
        _url = uri;
        _urn = "/";
    }

    //std::cout << "_urn " << _urn << std::endl;
    //std::cout << "_url " << _url << std::endl;

    // Extract the port if it's in the domain name.
    pos = _url.find(":");
    if(pos != std::string::npos){
        _port = to_int(_url.substr(pos + 1));
        _url = _url.substr(0, pos);
    } else {
        _port = 80;
    }

    struct hostent* host = gethostbyname(_url.c_str());

    if ( (host == NULL) || (host->h_addr == NULL) )
        EXCEPTION("Error retrieving DNS information.");

    // print information about this host:
    struct in_addr **addr_list;
    printf("Official name is: %s", host->h_name);
    printf(", IP addresses: ");
    addr_list = (struct in_addr **)host->h_addr_list;
    for(int i = 0; addr_list[i] != NULL; i++)
        printf("%s ", inet_ntoa(*addr_list[i]));

    if(_keepAlive)
        printf(", session keep alive connection.\n");
    else
        printf(", session is not keep alive connection.\n");

    bzero(&_client, sizeof(_client));

    _client.sin_family = AF_INET;
    _client.sin_port = htons( _port );
    memcpy(&_client.sin_addr, host->h_addr, host->h_length);
}

HTTP::~HTTP() {
    // Set the socket free.
    disconnect();
}


// Returns true if managed to connect.
bool HTTP::connect(){

    if( ++_connection > 5 )
        return false;

    // If socket point already present. Close connection.
    if(_sockfd >= 0)
        close(_sockfd);

    /* Create a socket point */
    _sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (_sockfd < 0)
        EXCEPTION("Error creating socket.");

    // Set socket non-bloking
    int flags = fcntl(_sockfd, F_GETFL, 0);
    fcntl(_sockfd, F_SETFL, flags | O_NONBLOCK);

    /* Now connect to the server */
    int n = ::connect(_sockfd, (struct sockaddr*)&_client, sizeof(_client));
    if( n < 0 && errno != EINPROGRESS)
        EXCEPTION("Failed to connect to host.");

    assert(errno == EINPROGRESS);
    errno = 0;

    if(n == 0){
        _connection = 0;
        return true;
    }

    fd_set			rset, wset;
    struct timeval	tval;

    FD_ZERO(&rset);

    FD_SET(_sockfd, &rset);
    wset = rset;
    tval.tv_sec = 5;
    tval.tv_usec = 0;

    if ( (n = select(_sockfd+1, &rset, &wset, NULL, &tval)) == 0) {
        close(_sockfd);		/* timeout */
        errno = ETIMEDOUT;
        EXCEPTION("Failed to connect to host.");
    }

    if (FD_ISSET(_sockfd, &rset) || FD_ISSET(_sockfd, &wset)) {
        int errorValue;
        socklen_t len = sizeof(errorValue);
        if (getsockopt(_sockfd, SOL_SOCKET, SO_ERROR, &errorValue, &len) < 0)
            EXCEPTION("Solaris pending error");

        errno = errorValue;
        if(error()) {
            close(_sockfd);		/* just in case */
            EXCEPTION("error set by getsockopt.");
        }
    } else
        EXCEPTION("select error: sockfd not set");

    if(error()) {
        close(_sockfd);		/* just in case */
        EXCEPTION("error set by select.");
    }

    _connection = 0;

    return true;
}

void HTTP::disconnect() {

    if(_sockfd >= 0)
        close(_sockfd);

    _sockfd = -1;

}

// Check if the connection is on error state.
bool HTTP::error() {

    // no error
    if( errno == 0 )
        return false;

    if( errno == EWOULDBLOCK || errno == EAGAIN )
        return false;

    // Socket is already connected
    if( errno == EISCONN )
        printf("Socket is already connected\n");

    if(errno == EINVAL )
        printf("Exception caught while reading socket - Invalid argument: _sfd = %i\n", _sockfd);

    if( errno == ECONNREFUSED )
        printf("Couldn't connect, connection refused.\n");

    if( errno == EINPROGRESS )
        printf("This returns is often see for large ElasticSearch requests.\n");

    printf("Exception caught while reading socket - %s.\n",strerror(errno));

    // reset errno
    errno = 0;
    disconnect();
    return true;
}

// Get Json Object on web server.
bool HTTP::request(const char* method, const char* endUrl, const char* data, Json::Object* jOutput, const char* content_type){
    Result result;
    request(method, endUrl, data, jOutput, result, content_type);
    return (result == OK);
}

// Get Json Object on web server.
unsigned int HTTP::request(const char* method, const char* endUrl, const char* data, Json::Object* jOutput, Result& result, const char* content_type){

    unsigned int statusCode = 0;

    std::string output;
    statusCode = request(method, endUrl, data, output, result, content_type);
    if(result != OK) {

        // Give a second chance.
        disconnect();

        statusCode = request(method, endUrl, data, output, result, content_type);
        if(result != OK)
            return statusCode;
    }

    try {
        if (jOutput && output.size()) {
            jOutput->addMember(output.c_str(), output.c_str() + output.size());
        }
    }
    catch(Exception& e){
        printf("parser() failed in Getter. Exception caught: %s\n", e.what());
        result = ERROR;
        return statusCode;
    }
    catch(std::exception& e){
        printf("parser() failed in Getter. std::exception caught: %s\n", e.what());
        throw std::exception(e);
    }
    catch(...){
        printf("parser() failed in Getter.\n");
        EXCEPTION("Unknown exception.");
    }

    if (jOutput) {
		jOutput->addMemberByKey("status", statusCode);
	}

    result = OK;
    return statusCode;
}

// Parse the message and split if necessary.
bool HTTP::sendMessage(const char* method, const char* endUrl, const char* data, const char* content_type){

    // Make the request type.
    std::string requestString(method);

    assert(requestString == "POST" || requestString == "DELETE" || requestString == "GET" || requestString == "PUT" || requestString == "HEAD");

    // Concatenate the page.
    requestString += std::string(" ");
    requestString += _urn;

    if(endUrl != 0) {

        if(_urn.back() != '/' )
            requestString += std::string("/");

        requestString += std::string(endUrl);
    }

    requestString += std::string(" HTTP/1.1\r\n");

    // Concatenate the host.
    requestString += std::string("Host: ");
    requestString += _url;
    requestString += std::string("\r\n");
    requestString += std::string("Accept: */*\r\n");
    if(_keepAlive)
        requestString += std::string("Connection: Keep-Alive\r\n");
    //requestString += "Connection: close\r\n";

    // If no data, send the header and return.
    if(data == 0){
        requestString += std::string("\r\n");

        if(!write(requestString))
            return false;

        return true;
    }

    assert( data != 0);
    // If data decide to split it or not.

    // Concatenate the content.
    requestString += std::string("Content-Type: ");
    requestString += std::string(content_type);
    requestString += std::string("\r\n");

    size_t dataSize = strlen(data);

    assert(!error());

    // If size is small enough, send as one message with the header.
    if(dataSize < 1024) {
        requestString += std::string("Content-length: ");
        requestString += std::to_string(dataSize);
        requestString += std::string("\r\n\r\n");

        requestString += std::string(data);

        if(!write(requestString))
            return false;

        return true;
    }

    assert(dataSize >= 1024);
    // If size is high then send the header and the rest as chunked message.
    requestString += std::string("Transfer-Encoding: chunked\r\n\r\n");

    if(!write(requestString))
        return false;

    size_t totalSent = 0;
    while(totalSent < dataSize){
        size_t chunkSize = std::min(dataSize - totalSent, (size_t)1024);

        std::stringstream chunkSizeString;
        chunkSizeString << std::hex << chunkSize;

        std::string chunk(chunkSizeString.str());
        chunk += "\r\n";
        chunk += std::string(data + totalSent, chunkSize);
        chunk += "\r\n";

        #if !defined(NDEBUG) && VERBOSE >= 4
        show(chunk.c_str(), chunk.length(), __LINE__);
        #endif

        if(!write(chunk))
            return false;

        totalSent += chunkSize;
    }

    // Final chunk message
    if(!write("0\r\n\r\n"))
        return false;

    return true;
}

// Write string on the socketfd.
bool HTTP::write(const std::string& outgoing) {

    assert( !error() );

    if(!connected() && !connect())
        EXCEPTION("Cannot write, we're not connected.");

    assert( !error() );

    ssize_t writeReturn = ::write(_sockfd, outgoing.c_str(), outgoing.length());

    if(writeReturn == 0) {
        if(!connect())
            EXCEPTION("write returned 0 and we could not reconnect.");

        write(outgoing);
    }

    if( writeReturn < 0 ){
        error();
        EXCEPTION(outgoing);
    }

    if( outgoing.length() != (size_t)writeReturn ){
        error();
        EXCEPTION("we did not write everything we wanted to write.");
    }

    assert( !error() );

    return true;
}


bool HTTP::request(const char* method, const char* endUrl, const char* data, std::string& output, const char* content_type){
    Result result;
    request(method, endUrl, data, output, result, content_type);
    return (result == OK);
}

unsigned int HTTP::request(const char* method, const char* endUrl, const char* data, std::string& output, Result& result, const char* content_type){

    /// Example of request.
    /// "POST /test.php HTTP/1.0\r\n"
    /// "Host: www.mariequantier.com\r\n"
    /// "Content-Type: application/json\r\n"
    /// "Content-length: 36\r\n\r\n"
    /// Then the content.
    ///
    /// Where /test.php is the URN and www.mariequantier.com is the URL.

    // Lock guard for every request.
    std::lock_guard<std::mutex> lock(_requestMutex);

    // If this instance does not keep-alive the connection, we must reconnect each time.
    if( !connected() || (connected() && !_keepAlive) || mustReconnect() ){
        if(!connect())
            EXCEPTION("Cannot reconnect.");
    }

    assert( !error() );
    assert(output.empty());

    unsigned int statusCode = 0;

    if(!sendMessage(method, endUrl, data, content_type)) {
        result = ERROR;
        return statusCode;
    }

    statusCode = readMessage(output, result);
    if(result != OK) {

        // Clear ouput in case we didn't get the full response.
        if(!output.empty())
            output.clear();
    }

    if(_keepAlive)
        _lastRequest = time(NULL);
    else
        // not keep-alive session.
        disconnect();

    // Format string output.
    /*
    if(output.size() >= 2){
        if(output[output.size() - 2] == '\r' && output[output.size() - 1] == '\n'){
            output[output.size() - 2] = '\0';
            output.resize(output.size() - 1);
        }
    } */

    result = OK;
    return statusCode;
}

// Whole process to read the response from HTTP server.
unsigned int HTTP::readMessage(std::string& output, Result& result) {

    unsigned int statusCode = 0;

    // Need to loop (recursion may fail because pile up over the stack for large requests.
    size_t contentLength = 0;
    bool isChunked = false;
    do {
        statusCode = readMessage(output, contentLength, isChunked, result);
    } while(result == MORE_DATA);

    return statusCode;
}

// Wait with select then start to read the message.
unsigned int HTTP::readMessage(std::string& output, size_t& contentLength, bool& isChunked, Result& result) {

    unsigned int statusCode = 0;

    /// First, use select() with a timeout value to determine when the file descriptor is ready to be read.
    assert( !error() );
    assert( _sockfd >= 0 );

    int fd = _sockfd;

    // Time value before timeout.
    timeval tval = {40, 0};

    // Declare file descriptors.
    fd_set readSet, errorSet;

    FD_ZERO( &readSet);
    FD_ZERO( &errorSet);
    FD_SET( fd, &readSet);
    FD_SET( fd, &errorSet);

    assert( fd >= 0 );

    int ret = select( fd + 1, &readSet, 0, &errorSet, &tval );

    assert( !error() );

    // Is error ?
    if(ret < 0) {
        disconnect();
        result = ERROR;
        return statusCode;
    }

    // Is timeout ?
    if(ret == 0) {
        result = ERROR;
        return statusCode;
    }

    // Check error on socket
    if(FD_ISSET( fd, &errorSet)) {
        error();
        result = ERROR;
        return statusCode;
    }

    // Is read ?
    if(FD_ISSET( fd, &readSet)) {
        // Parse message.
        return parseMessage(output, contentLength, isChunked, result);
    }

    result = OK;
    return statusCode;
}

// Append char* to output.
size_t HTTP::appendChunk(std::string& output, char* msg, size_t msgSize) {
    assert(msgSize > 0);

    #if !defined(NDEBUG) && VERBOSE >= 4
    show(msg, msgSize, __LINE__);
    #endif

    char* afterSize;
    size_t chunkSize = strtol(msg, &afterSize, 16);

    if(error())
        return 0;

    if(chunkSize == 0)
        return 0;


    // Move after the /r/n characters.
    afterSize += 2;

    assert(msgSize + msg > afterSize + 2);

    // Append the result to the output; remove the \r\n as line separator.
    if(chunkSize + afterSize + 2 <= msgSize + msg)
        // The chunksize is smaller, we take only the given size.
        output.append(afterSize, chunkSize);
    else{
        // If the chunksize is higher, we take only what we got in the answer.
        output.append(afterSize, msgSize - (afterSize - msg));
    }

    return chunkSize;
}

// Whole process to read the response from HTTP server.
unsigned int HTTP::parseMessage(std::string& output, size_t& contentLength, bool& isChunked, Result& result) {

        unsigned int statusCode = 0;

        // Socket is ready for reading.
        char recvline[4096];
        ssize_t readSize = read(_sockfd, recvline, 4095);

        if(readSize <= 0) {

            if(!connect()) {
                result = ERROR;
                return statusCode;
            }

            if( readSize < 0 ) {
                if(errno != (EWOULDBLOCK | EAGAIN) )
                    EXCEPTION("read error on socket");
                errno = 0;
            }

            result = MORE_DATA;
            return statusCode;
        }

        assert(readSize <= 4095);

        // If we already got the header from a non chunked message.
        if(contentLength > 0) {
            // If we have the content length, append the result to the output.
            output.append(recvline, readSize);
        }

        // If we already got the header from a chunked message.
        if(contentLength == 0 && isChunked) {

            // We already tested that the readSize is not 0.
            contentLength = appendChunk(output, recvline, readSize);

            // Append the message to the output.
            if( contentLength == 0 ) {
                result = OK;
                return statusCode;
            }
        }

        // If this method is called with no contentLength, parse the header.
        if(contentLength == 0 && !isChunked) {

            char* endStatus = strstr(recvline,"\r\n");

            if(endStatus == NULL) {
                disconnect();
                result = ERROR;
                return statusCode;
            }

            // Extract and interpret status line.
            std::stringstream status( std::string(recvline, endStatus) );

            if (!status) {
                disconnect();
                result = ERROR;
                return statusCode;
            }

            std::string httpVersion;
            status >> httpVersion;

            if (httpVersion.substr(0, 5) != "HTTP/") {
                disconnect();
                result = ERROR;
                return statusCode;
            }

            status >> statusCode;

            // Extract status message.
            std::stringstream statusMessage;
            status >> statusMessage.rdbuf();

            // Handle the different status' response.
            switch(statusCode) {

                // If created, then continue.
                case 201:
                    break;

                // If ok, then continue.
                case 200:
                    break;

                // If found, then continue.
                case 302:
                    break;

                // Bad Request
                case 400:
                    std::cout << "Status: Bad Request, you must reconsidered your request." << std::endl;
                    disconnect();
                    result = ERROR;
                    return statusCode;

                // If forbidden, it's over.
                case 403:
                    std::cout << "Status: Forbidden, you must reconsidered your request." << std::endl;
                    disconnect();
                    result = ERROR;
                    return statusCode;

                // If 404 then check the message and continue to get the complete response.
                case 404:
                    std::cout << " 404 but statusMessage is not \"Not Found\"." << std::endl;
                    disconnect();
                    break;

                // If 500 then print the message and break.
                case 500:
                    std::cout << " 500 but statusMessage is not \"Internal Server Error\"." << std::endl;
                    disconnect();
                    result = ERROR;
                    return statusCode;

                // If unhandled state, return false.
                default:
                    std::cout << "Weird status code: " << statusCode << std::endl;
                    disconnect();
                    result = ERROR;
                    return statusCode;
            }

            // Extract and interpret the header.
            char* endHeader = strstr(endStatus+2,"\r\n\r\n");
            if(endHeader == NULL) {
                disconnect();
                result = ERROR;
                return statusCode;
            }
            size_t headerSize = endHeader + 4 - recvline;

            // Extract and interpret the header.
            char* contentLenghtPos = strstr(endStatus+2,"Content-Length:");

            // If we've got the content-length.
            if(contentLenghtPos == NULL){

                // If the message is chunked, restart the method with the flag on.
                if( strstr(endStatus+2,"Transfer-Encoding: chunked") != NULL ){
                    // Set the transfer as chunked.
                    isChunked = true;

                    // If we did not get the size of the chunk read message as a chunk, go on reading.
                    if(readSize - headerSize <= 0) {
                        result = MORE_DATA;
                        return statusCode;
                    }

                    // If the message content the length, parse the size.
                    contentLength = appendChunk(output, endHeader + 4, readSize - headerSize);

                    if(contentLength == 0) {
                        result = OK;
                        return statusCode;
                    }

                } else {
                    disconnect();
                    result = ERROR;
                    return statusCode;
                }
            }
            // We received the content-length.
            else {
                contentLength = atoi(contentLenghtPos + 15);

                // Error due to conversion may set errno.
                if(error()) {
                    result = ERROR;
                    return statusCode;
                }

                // Copy content.
                assert( endHeader - recvline + contentLength + 4 >= (unsigned int)readSize);
                output.append(endHeader+4, readSize - headerSize);
            }
        }

        while( output.length() <  contentLength) {
            char nextContent[4096];
            readSize = read(_sockfd, nextContent, 4095);

            // When we did not receive the data yet. Wait with select.
            if( readSize == 0 ) {
                result = MORE_DATA;
                return statusCode;
            }

            // When there is nothing more to read but the output is incomplete, wait with select.
            if( readSize < 0 ) {

                if(errno != (EWOULDBLOCK | EAGAIN) )
                    EXCEPTION("read error on socket");
                errno = 0;
                result = MORE_DATA;
                return statusCode;
            }

            output.append(nextContent, readSize);
        }

        // If chunked but no size, we return until we receive the answer.
        if(isChunked && contentLength == 0) {
            result = MORE_DATA;
            return statusCode;
        }

    result = OK;
    return statusCode;
}


