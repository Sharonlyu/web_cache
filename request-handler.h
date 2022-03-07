/**
 * File: request-handler.h
 * -----------------------
 * Defines the HTTPRequestHandler class, which fully proxies and
 * services a single client request.  
 */

#ifndef _request_handler_
#define _request_handler_

#include <utility>
#include <string>
#include <map>
#include "request.h"
#include "response.h"
#include "header.h"
#include "client-socket.h"
#include "cache.h"
#include "strike-set.h"

class HTTPRequestHandler {
 public:
  HTTPRequestHandler();
  void serviceRequest(const std::pair<int, std::string>& connection) noexcept;
  void clearCache();
  void setCacheMaxAge(long maxAge);

 private:
    StrikeSet strikeSet;
    HTTPCache cache;
    
  typedef void (HTTPRequestHandler::*handlerMethod)(const HTTPRequest& request, class iosockstream& ss);
  std::map<std::string, handlerMethod> handlers;
    int createSocket(const HTTPRequest& request);
    void addHeaders(const HTTPRequest& request);
  void handleRequest(const HTTPRequest& request, class iosockstream& ss);
  void handleCONNECTRequest(const HTTPRequest& request, class iosockstream& clientStream);
  
  void manageClientServerBridge(iosockstream& client, iosockstream& server);
  std::string buildTunnelString(iosockstream& from, iosockstream& to) const;

  void handleBadRequestError(class iosockstream& ss, const std::string& message) const;
  void handleUnsupportedMethodError(class iosockstream& ss, const std::string& message) const;
  void handleError(class iosockstream& ss, const std::string& protocol,
                   HTTPStatus responseCode, const std::string& message) const;
};

#endif
