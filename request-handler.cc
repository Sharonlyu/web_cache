/**
 * File: request-handler.cc
 * ------------------------
 * Provides the implementation for the HTTPRequestHandler class.
 */

#include "request-handler.h"
#include "response.h"
#include <socket++/sockstream.h> // for sockbuf, iosockstream
#include "ostreamlock.h"
#include "watchset.h"
#include "cache.h"


using namespace std;
static const string kDefaultProtocol = "HTTP/1.0";
const size_t kTimeout = 5;
const size_t kBridgeBufferSize = 1 << 16;



HTTPRequestHandler::HTTPRequestHandler() {
  handlers["GET"] = &HTTPRequestHandler::handleRequest;
  handlers["POST"] = &HTTPRequestHandler::handleRequest;
  handlers["HEAD"] = &HTTPRequestHandler::handleRequest;
  handlers["CONNECT"] = &HTTPRequestHandler::handleCONNECTRequest;
  strikeSet.addFrom("blocked-domains.txt");
}

void HTTPRequestHandler::serviceRequest(const pair<int, string>& connection) noexcept {
  sockbuf sb(connection.first);
  iosockstream ss(&sb);
  try {
    HTTPRequest request;
    request.ingestRequestLine(ss);
    request.ingestHeader(ss, connection.second);
    request.ingestPayload(ss);


    if (strikeSet.contains(request.getServer())) {
        
        HTTPResponse response;
        response.setResponseCode(403);
        response.setProtocol(kDefaultProtocol);
        response.setPayload("Forbidden Content");
        ss << response << flush;

    }
    
    auto found = handlers.find(request.getMethod());
    if (found == handlers.cend()) throw UnsupportedMethodExeption(request.getMethod());
    (this->*(found->second))(request, ss);
    
  } catch (const HTTPBadRequestException &bre) {
    handleBadRequestError(ss, bre.what());
    
  } catch (const UnsupportedMethodExeption& ume) {
    handleUnsupportedMethodError(ss, ume.what());
    
  } catch (...) {}
   
  
}

int HTTPRequestHandler::createSocket(const HTTPRequest& request) {
    int socketD = createClientSocket(request.getServer(), request.getPort());
        
    if (socketD == kClientSocketError) {
        cerr << "Server could not be reached."<<endl;
        cerr << "Abborting"<<endl;

    }
    return socketD;
    
}


void HTTPRequestHandler::handleRequest(const HTTPRequest& request, class iosockstream& ss) {
    //lock hash operation
    size_t hashCode = cache.hashRequest(request);
    lock_guard<mutex> hashLockLg(cache.lockArr[hashCode % numLocks]);
    
    HTTPResponse response;
    if (cache.containsCacheEntry(request, response)){
        ss << response;
        ss.flush();
        return;

    }
   
    int socketD = createSocket(request);
    sockbuf socketBuffer(socketD);
    iosockstream clientStream(&socketBuffer);
    
    clientStream << request;
    clientStream.flush();
    
    response.ingestResponseHeader(clientStream);
    if(request.getMethod() != "HEAD") response.ingestPayload(clientStream);

    if (cache.shouldCache(request, response)){
        
        cache.cacheEntry(request, response);
    }
    ss << response;
    ss.flush();
}








void HTTPRequestHandler::manageClientServerBridge(iosockstream& client, iosockstream& server) {
  // get embedded descriptors leading to client and origin server
  int clientfd = client.rdbuf()->sd();
  int serverfd = server.rdbuf()->sd();

  // monitor both descriptors for any activity
  ProxyWatchset watchset(kTimeout);
  watchset.add(clientfd);
  watchset.add(serverfd);

  // map each descriptor to its surrounding iosockstream and the one
  // surrounding the descriptor on the other side of the bridge we're building
  map<int, pair<iosockstream *, iosockstream *>> streams;
  streams[clientfd] = make_pair(&client, &server);
  streams[serverfd] = make_pair(&server, &client);
  cout << oslock << buildTunnelString(client, server) << "Establishing HTTPS tunnel" << endl << osunlock;

  while (!streams.empty()) {
    int fd = watchset.wait();
    if (fd == -1) break; // return value of -1 means we timed out
    iosockstream& from = *streams[fd].first;
    iosockstream& to = *streams[fd].second;
    char buffer[kBridgeBufferSize];
    from.read(buffer, 1); // attempt to read one byte to see if we have one
    if (from.eof() || from.fail() || from.gcount() == 0) {
       // in here? that's because the watchset detected EOF instead of an unread byte
       watchset.remove(fd);
       streams.erase(fd);
       continue;
    }
    to.write(buffer, 1);
    // read all available bytes from the
    // source and transport them to the other side of the bridge
    
    while (true) {
        size_t bytesRead = from.readsome(buffer, sizeof(buffer));
        
        if (bytesRead == 0) break;
        if (from.eof() || from.fail() || from.gcount() == 0) {
            watchset.remove(fd);
            streams.erase(fd);
            break;
        }
        to.write(buffer, bytesRead);
        

    }
    
    
    to.flush();
  }
  cout << oslock << buildTunnelString(client, server) << "Tearing down HTTPS tunnel." << endl << osunlock;
}

void HTTPRequestHandler::handleCONNECTRequest(const HTTPRequest& request, class iosockstream& ss) {
   
   int socketD = createSocket(request);    
   sockbuf socketBuffer(socketD);
   iosockstream serverStream(&socketBuffer);
        
   HTTPResponse response;  
   response.setResponseCode(HTTPStatus::OK);
   response.setProtocol(kDefaultProtocol);
   
   ss << response;//first handshake
   ss.flush();
   
   manageClientServerBridge(ss, serverStream);  
}

string HTTPRequestHandler::buildTunnelString(iosockstream& from, iosockstream& to) const {
  return "[" + to_string(from.rdbuf()->sd()) + " --> " + to_string(to.rdbuf()->sd()) + "]: ";
}

/**
 * Responds to the client with code 400 and the supplied message.
 */
void HTTPRequestHandler::handleBadRequestError(iosockstream& ss, const string& message) const {
  handleError(ss, kDefaultProtocol, HTTPStatus::BadRequest, message);
}

/**
 * Responds to the client with code 405 and the provided message.
 */
void HTTPRequestHandler::handleUnsupportedMethodError(iosockstream& ss, const string& message) const {
  handleError(ss, kDefaultProtocol, HTTPStatus::MethodNotAllowed, message);
}

/**
 * Generic error handler used when our proxy server
 * needs to invent a response because of some error.
 */
void HTTPRequestHandler::handleError(iosockstream& ss, const string& protocol,
                                     HTTPStatus responseCode, const string& message) const {
  HTTPResponse response;
  response.setProtocol(protocol);
  response.setResponseCode(responseCode);
  response.setPayload(message);
  ss << response << flush;
}

// the following two methods needs to be completed 
// once you incorporate your HTTPCache into your HTTPRequestHandler
void HTTPRequestHandler::clearCache() {}
void HTTPRequestHandler::setCacheMaxAge(long maxAge) {}
