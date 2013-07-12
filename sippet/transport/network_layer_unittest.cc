// Copyright (c) 2013 The Sippet Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sippet/transport/network_layer.h"
#include "sippet/transport/transaction_factory.h"
#include "sippet/transport/channel_factory.h"
#include "sippet/message/message.h"

#include "base/string_util.h"
#include "net/socket/socket_test_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sippet {

namespace {

class MockNetworkLayerDelegate : public NetworkLayer::Delegate {
 public:
  MOCK_METHOD2(OnChannelClosed, void(const EndPoint&, int));
  MOCK_METHOD1(OnIncomingMessage, void(Message*));
};

class MockChannel : public Channel {
 public:
  MockChannel(Channel::Delegate *delegate = NULL,
              net::MockRead* reads = NULL, size_t reads_count = 0,
              net::MockWrite* writes = NULL, size_t writes_count = 0)
  : delegate_(delegate), is_secure_(false), is_connected_(false) {
    data_.reset(new net::DeterministicSocketData(reads, reads_count, writes, writes_count));
    net::DeterministicMockTCPClientSocket* wrapped_socket =
      new net::DeterministicMockTCPClientSocket(net_log_.net_log(), data_.get());
    data_->set_socket(wrapped_socket->AsWeakPtr());
    socket_.reset(wrapped_socket->AsWeakPtr());
  }
  ~MockChannel() {}

  void set_origin(const EndPoint &value) { origin_ = value; }
  void set_destination(const EndPoint &value) { destination_ = value; }
  void set_is_secure(bool value) { is_secure_ = value; }
  void set_is_connected(bool value) { is_connected_ = value; }
  
  virtual const EndPoint& origin() const { return origin_; }
  virtual const EndPoint& destination() const { return destination_; }
  virtual bool is_secure() const { return is_secure_; }
  virtual bool is_connected() const { return is_connected_; }

  virtual void Connect() {
    socket_->Connect(base::Bind(&MockChannel::OnConnected, this));
  }

  virtual int Send(const scoped_refptr<Message>& message,
                   const net::CompletionCallback& callback) {
    std::string buffer(message->ToString());
    scoped_refptr<net::IOBuffer> io_buffer(new net::IOBuffer(buffer.size()));
    memcpy(io_buffer->data(), buffer.data(), buffer.size());
    return socket_->Write(io_buffer, buffer.size(), callback);
  }

  MOCK_METHOD0(Close, void());
  MOCK_METHOD1(CloseWithError, void(int));
  MOCK_METHOD0(DetachDelegate, void());

 private:
  Channel::Delegate *delegate_;
  EndPoint origin_;
  EndPoint destination_;
  bool is_secure_;
  bool is_connected_;
  net::BoundNetLog net_log_;
  scoped_ptr<net::StreamSocket> socket_;
  scoped_ptr<net::DeterministicSocketData> data_;

  void OnConnected(int result) {
    delegate_->OnChannelConnected(this, result);
  }
};

class MockChannelFactory : public ChannelFactory {
 public:
  MockChannelFactory() {}
  ~MockChannelFactory() {}

  void ExpectCreateChannel(const EndPoint &destination,
                           scoped_refptr<MockChannel> channel) {
    expected_sockets_.push_back(std::make_pair(destination, channel));
  }

  virtual int CreateChannel(
          const EndPoint &destination,
          Channel::Delegate *delegate,
          scoped_refptr<Channel> *channel) {
    EXPECT_TRUE(!expected_sockets_.empty());
    if (expected_sockets_.empty())
      return net::ERR_ABORTED;
    EXPECT_EQ(expected_sockets_.front().first, destination);
    scoped_refptr<MockChannel> result = expected_sockets_.front().second;
    expected_sockets_.pop_front();
    *channel = result;
    return net::OK;
  }

 private:
  EndPoint origin_;
  std::list<std::pair<EndPoint, scoped_refptr<MockChannel> > >
    expected_sockets_;
};

class TransactionFactoryImpl : public TransactionFactory {
 public:
  virtual ClientTransaction *CreateClientTransaction(
      const Method &method,
      const std::string &transaction_id,
      const scoped_refptr<Channel> &channel,
      TransactionDelegate *delegate) {
    return 0;
  }

  virtual ServerTransaction *CreateServerTransaction(
      const Method &method,
      const std::string &transaction_id,
      const scoped_refptr<Channel> &channel,
      TransactionDelegate *delegate) {
    return 0;
  }
};

} // End of empty namespace

class NetworkLayerTest : public testing::Test {
 public:
  void Finish() {
    MessageLoop::current()->RunUntilIdle();
  }

  void Initialize() {
    network_layer_ = new NetworkLayer(&delegate_, &transaction_factory_);
  }

  MockNetworkLayerDelegate delegate_;
  TransactionFactoryImpl transaction_factory_;
  scoped_refptr<NetworkLayer> network_layer_;
};

TEST_F(NetworkLayerTest, StaticFunctions) {
  std::string branch = NetworkLayer::CreateBranch();
  EXPECT_TRUE(StartsWithASCII(branch, NetworkLayer::kMagicCookie, true));

  scoped_refptr<MockChannel> mock_channel = new MockChannel;
  mock_channel->set_origin(EndPoint("1.0.0.1",6001,Protocol::UDP));
  mock_channel->set_destination(EndPoint("2.0.0.2",1042,Protocol::UDP));

  scoped_refptr<Request> client_request =
    new Request(Method::INVITE, GURL("sip:foo@bar.com"));
  NetworkLayer::StampClientTopmostVia(client_request, mock_channel);
  EXPECT_TRUE(StartsWithASCII(client_request->ToString(),
    "INVITE sip:foo@bar.com SIP/2.0\r\n"
    "v: SIP/2.0/UDP 1.0.0.1:6001;rport;branch=z9", true));

  scoped_refptr<Request> empty_via_request =
    new Request(Method::INVITE, GURL("sip:bar@foo.com"));
  NetworkLayer::StampServerTopmostVia(empty_via_request, mock_channel);
  EXPECT_TRUE(StartsWithASCII(empty_via_request->ToString(),
    "INVITE sip:bar@foo.com SIP/2.0\r\n"
    "v: SIP/2.0/UDP 2.0.0.2:1042;rport", true));

  scoped_refptr<Request> single_via_request =
    new Request(Method::INVITE, GURL("sip:foobar@foo.com"));
  scoped_ptr<Via> via(new Via);
  net::HostPortPair hostport("192.168.0.1", 7001);
  via->push_back(ViaParam(Protocol::UDP, hostport));
  single_via_request->push_front(via.PassAs<Header>());
  NetworkLayer::StampServerTopmostVia(single_via_request, mock_channel);
  EXPECT_TRUE(StartsWithASCII(single_via_request->ToString(),
    "INVITE sip:foobar@foo.com SIP/2.0\r\n"
    "v: SIP/2.0/UDP 192.168.0.1:7001;received=2.0.0.2;rport=1042", true));

  EndPoint single_via_request_endpoint =
    NetworkLayer::GetMessageEndPoint(single_via_request);
  EXPECT_EQ(EndPoint("foo.com",5060,Protocol::UDP),
    single_via_request_endpoint);

  single_via_request->set_request_uri(GURL("sip:foobar@foo.com;transport=TCP"));
  single_via_request_endpoint =
    NetworkLayer::GetMessageEndPoint(single_via_request);
  EXPECT_EQ(EndPoint("foo.com",5060,Protocol::TCP),
    single_via_request_endpoint);

  scoped_refptr<Response> single_via_response = new Response(200);
  via.reset(new Via);
  via->push_back(ViaParam(Protocol::UDP, net::HostPortPair("192.168.0.1", 7001)));
  single_via_response->push_front(via.PassAs<Header>());
  EndPoint single_via_response_endpoint =
    NetworkLayer::GetMessageEndPoint(single_via_response);
  EXPECT_EQ(EndPoint("192.168.0.1",7001,Protocol::UDP),
    single_via_response_endpoint);

  single_via_response = new Response(200);
  via.reset(new Via);
  via->push_back(ViaParam(Protocol::UDP, net::HostPortPair("192.168.0.1", 7001)));
  via->front().set_received("189.187.200.23");
  single_via_response->push_front(via.PassAs<Header>());
  single_via_response_endpoint =
    NetworkLayer::GetMessageEndPoint(single_via_response);
  EXPECT_EQ(EndPoint("189.187.200.23",7001,Protocol::UDP),
    single_via_response_endpoint);

  single_via_response = new Response(200);
  via.reset(new Via);
  via->push_back(ViaParam(Protocol::UDP, net::HostPortPair("192.168.0.1", 7001)));
  via->front().set_received("189.187.200.23");
  via->front().set_rport(5002);
  single_via_response->push_front(via.PassAs<Header>());
  single_via_response_endpoint =
    NetworkLayer::GetMessageEndPoint(single_via_response);
  EXPECT_EQ(EndPoint("189.187.200.23",5002,Protocol::UDP),
    single_via_response_endpoint);
}

} // End of sippet namespace