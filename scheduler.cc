/**
 * File: scheduler.cc
 * ------------------
 * Presents the implementation of the HTTPProxyScheduler class.
 */

#include "scheduler.h"
#include <utility>
using namespace std;

size_t numThreads = 64;//64
HTTPProxyScheduler::HTTPProxyScheduler(): pool(numThreads){
    
}


void HTTPProxyScheduler::scheduleRequest(int clientfd, const string& clientIPAddress) {
    pool.schedule([this, clientfd, clientIPAddress] {
                      requestHandler.serviceRequest(make_pair(clientfd, clientIPAddress));
                  });
    
}

void HTTPProxyScheduler::setProxy(const std::string& server, unsigned short port) {
  // TODO: configure RequestHandler to forward requests to the specified proxy
}
