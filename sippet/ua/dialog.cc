// Copyright (c) 2014 The Sippet Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sippet/ua/dialog.h"
#include "sippet/base/sequences.h"
#include "sippet/message/request.h"
#include "sippet/message/response.h"
#include "sippet/uri/uri.h"
#include <algorithm>

namespace sippet {

namespace {

std::vector<GURL> GetRouteSet(
    const std::vector<RecordRoute*> &rr) {
  std::vector<GURL> result;
  for (std::vector<RecordRoute*>::const_iterator i = rr.begin(),
       ie = rr.end(); i != ie; i++) {
    for (Route::const_iterator j = (*i)->begin(), je = (*i)->end();
         j != je; j++) {
      result.push_back(j->address());
    }
  }
  return result;
}

}

scoped_refptr<Request> Dialog::CreateRequest(const Method &method) {
  if (Method::ACK == method) {
    DVLOG(1) << "ACK requests for 2xx are created by Dialog::CreateAck()";
    return 0;
  }
  if (Method::CANCEL == method) {
    DVLOG(1) << "CANCEL requests are created by Request::CreateCancel()";
    return 0;
  }
  return CreateRequestInternal(method, GetNewLocalSequence());
}

scoped_refptr<Request> Dialog::CreateAck(
    const scoped_refptr<Request> &invite) {
  if (Method::INVITE != invite->method()) {
    DVLOG(1) << "ACK requests require the INVITE being acknowledged";
    return 0;
  }
  unsigned local_sequence = invite->get<Cseq>()->sequence();
  scoped_refptr<Request> ack(
      CreateRequestInternal(Method::ACK, local_sequence));
  invite->copy_to<WwwAuthenticate>(ack);
  invite->copy_to<ProxyAuthenticate>(ack);
  return ack;
}

scoped_refptr<Dialog> Dialog::CreateClientDialog(
    const scoped_refptr<Request> &request,
    const scoped_refptr<Response> &response) {
  State state = response->response_code() / 100 == 1
      ? STATE_EARLY : STATE_CONFIRMED;
  bool is_secure = request->request_uri().SchemeIs("sips");
  std::vector<GURL> route_set(GetRouteSet(response->filter<RecordRoute>()));
  std::reverse(route_set.begin(), route_set.end());
  GURL remote_target(response->get<Contact>()->front().address());
  bool has_local_sequence = true;
  unsigned local_sequence = request->get<Cseq>()->sequence();
  bool has_remote_sequence = false;
  unsigned remote_sequence = 0;
  std::string call_id(request->get<CallId>()->value());
  std::string local_tag(request->get<From>()->tag());
  std::string remote_tag;
  To *to = response->get<To>();
  if (to->HasTag())
    remote_tag = to->tag();
  GURL remote_uri(request->get<To>()->address());
  GURL local_uri(request->get<From>()->address());
  return new Dialog(state, call_id, local_tag, remote_tag,
      has_local_sequence, local_sequence, has_remote_sequence, remote_sequence,
      local_uri, remote_uri, remote_target, is_secure, route_set);
}

scoped_refptr<Dialog> Dialog::CreateServerDialog(
    const scoped_refptr<Request> &request,
    const scoped_refptr<Response> &response) {
  State state = response->response_code() / 100 == 1
      ? STATE_EARLY : STATE_CONFIRMED;
  bool is_secure = request->request_uri().SchemeIs("sips");
  std::vector<GURL> route_set(GetRouteSet(request->filter<RecordRoute>()));
  GURL remote_target(request->get<Contact>()->front().address());
  bool has_remote_sequence = true;
  unsigned remote_sequence = request->get<Cseq>()->sequence();
  bool has_local_sequence = false;
  unsigned local_sequence = 0;
  std::string call_id(request->get<CallId>()->value());
  std::string local_tag(response->get<To>()->tag());
  std::string remote_tag;
  From *from = request->get<From>();
  if (from->HasTag())
    remote_tag = from->tag();
  GURL remote_uri(request->get<From>()->address());
  GURL local_uri(request->get<To>()->address());
  return new Dialog(state, call_id, local_tag, remote_tag,
      has_local_sequence, local_sequence, has_remote_sequence, remote_sequence,
      local_uri, remote_uri, remote_target, is_secure, route_set);
}

unsigned Dialog::GetNewLocalSequence() {
  if (!has_local_sequence_) {
    local_sequence_ = Create16BitRandomInteger();
    has_local_sequence_ = true;
    return local_sequence_;
  }
  else {
    return ++local_sequence_;
  }
}

scoped_refptr<Request> Dialog::CreateRequestInternal(
    const Method &method, unsigned local_sequence) {
  GURL request_uri;
  scoped_ptr<Route> route;
  if (route_set().empty()) {
    request_uri = remote_target();
  }
  else {
    GURL first(route_set().front());
    SipURI uri(first);
    if (uri.has_parameters() && uri.parameter("lr").first) {
      request_uri = remote_target();
      route.reset(new Route);
      for (std::vector<GURL>::const_iterator i = route_set().begin(),
           ie = route_set().end(); i != ie; i++) {
        route->push_back(RouteParam(*i));
      }
    }
    else {
      request_uri = first; // TODO: strip not allowed parameters
      std::vector<GURL>::const_iterator i = route_set().begin(),
                                        ie = route_set().end();
      i++; // discard the first
      route.reset(new Route);
      for (; i != ie; i++)
        route->push_back(RouteParam(*i));
      route->push_back(RouteParam(remote_target()));
    }
  }
  scoped_refptr<Request> request = new Request(method, request_uri);
  scoped_ptr<MaxForwards> max_forwards(new MaxForwards(70));
  request->push_back(max_forwards.PassAs<Header>());
  scoped_ptr<From> from(new From(local_uri()));
  from->set_tag(local_tag());
  request->push_back(from.PassAs<Header>());
  scoped_ptr<To> to(new To(remote_uri()));
  if (!remote_tag().empty())
    to->set_tag(remote_tag());
  request->push_back(to.PassAs<Header>());
  scoped_ptr<CallId> call_id(new CallId(call_id()));
  request->push_back(call_id.PassAs<Header>());
  scoped_ptr<Cseq> cseq(new Cseq(local_sequence, method));
  request->push_back(cseq.PassAs<Header>());
  if (route)
    request->push_back(route.PassAs<Header>());
  return request;
}

} // End of sippet namespace