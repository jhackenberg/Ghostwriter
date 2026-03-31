#ifndef REMBRANDT_SRC_NETWORK_SERVER_H_
#define REMBRANDT_SRC_NETWORK_SERVER_H_

#include "rembrandt/network/message.h"
#include "rembrandt/network/utils.h"
#include "rembrandt/network/ucx/memory_region.h"
#include "rembrandt/network/ucx/endpoint.h"
#include "rembrandt/network/ucx/worker.h"
#include "message_handler.h"
#include <atomic>
#include <deque>
#include <thread>
#include <memory>

extern "C" {
#include <arpa/inet.h> /* inet_addr */
#include <ucp/api/ucp.h>
#include <ucs/type/status.h>
}

class Server {
 public:
  Server(std::unique_ptr<UCP::Worker> data_worker, std::unique_ptr<UCP::Worker> listening_worker, uint16_t port);
  void Listen();
  void Run(MessageHandler *message_handler);
  void Stop();
  void CreateServerEndpoint(ucp_address_t *addr);
  std::unique_ptr<Message> ReceiveMessage(const UCP::Endpoint &endpoint);
 private:
  MessageHandler *message_handler_;
  std::unique_ptr<UCP::Worker> data_worker_;
  int lsock;
  std::unordered_map<ucp_ep_h, std::unique_ptr<UCP::Endpoint>> endpoint_map_;
  std::thread listening_thread_;
  std::atomic<bool> running_ = false;
  static sockaddr_in CreateListenAddress(uint16_t port);
  static ucp_ep_params_t CreateEndpointParams(ucp_address_t *addr);
  void StartListener(uint16_t port);
  ucs_status_t Finish(ucs_status_ptr_t status_ptr);
  std::deque<UCP::Endpoint *> WaitUntilReadyToReceive();
};

#endif //REMBRANDT_SRC_NETWORK_SERVER_H_
