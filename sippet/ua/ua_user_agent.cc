// Copyright (c) 2014 The Sippet Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sippet/ua/ua_user_agent.h"

#include "base/md5.h"
#include "base/build_time.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/i18n/time_formatting.h"
#include "net/base/net_errors.h"
#include "sippet/ua/dialog.h"
#include "sippet/uri/uri.h"
#include "sippet/base/tags.h"
#include "sippet/base/sequences.h"
#include "sippet/base/stl_extras.h"

namespace sippet {
namespace ua {

UserAgent::IncomingRequestContext::IncomingRequestContext(
      const scoped_refptr<Request>& incoming_request)
  : incoming_request_(incoming_request) {
}

UserAgent::IncomingRequestContext::~IncomingRequestContext() {
}

UserAgent::OutgoingRequestContext::OutgoingRequestContext(
      const scoped_refptr<Request>& outgoing_request)
  : outgoing_request_(outgoing_request) {
}

UserAgent::OutgoingRequestContext::~OutgoingRequestContext() {
}

UserAgent::UserAgent(AuthHandlerFactory *auth_handler_factory,
    PasswordHandler::Factory *password_handler_factory,
    SSLCertErrorHandler::Factory *ssl_cert_error_handler_factory,
    const net::BoundNetLog &net_log)
    : auth_handler_factory_(auth_handler_factory),
      net_log_(net_log),
      password_handler_factory_(password_handler_factory),
      ssl_cert_error_handler_factory_(ssl_cert_error_handler_factory),
      weak_factory_(this) {
  DCHECK(auth_handler_factory);
  DCHECK(password_handler_factory);
  DCHECK(ssl_cert_error_handler_factory);
}

UserAgent::~UserAgent() {
  STLDeleteContainerPairSecondPointers(
      outgoing_requests_.begin(), outgoing_requests_.end());
}

void UserAgent::AppendHandler(Delegate *delegate) {
  handlers_.push_back(delegate);
}

scoped_refptr<Request> UserAgent::CreateRequest(
    const Method &method,
    const GURL &request_uri,
    const GURL &from_uri,
    const GURL &to_uri,
    unsigned local_sequence) {
  scoped_refptr<Request> request(
    new Request(method, request_uri));
  scoped_ptr<To> to(new To(to_uri));
  request->push_back(to.PassAs<Header>());
  
  // Add the From header and a local tag (48-bit random string)
  scoped_ptr<From> from(new From(from_uri));
  from->set_tag(CreateTag());
  request->push_back(from.PassAs<Header>());
  
  // The Call-ID is formed by a 120-bit random string
  scoped_ptr<CallId> call_id(new CallId(CreateCallId()));
  request->push_back(call_id.PassAs<Header>());
  
  // Cseq always contain the request method and a new (random) local sequence
  if (local_sequence == 0) {
    local_sequence = Create16BitRandomInteger();
    if (local_sequence == 0)
      local_sequence = 1;
  }

  scoped_ptr<Cseq> cseq(new Cseq(local_sequence, method));
  request->push_back(cseq.PassAs<Header>());

  // Max-Forwards header field is always 70.
  scoped_ptr<MaxForwards> max_forwards(new MaxForwards(70));
  request->push_back(max_forwards.PassAs<Header>());

  scoped_ptr<Supported> supported(new Supported);
  supported->push_back("path");
  supported->push_back("outbound");
  request->push_back(supported.PassAs<Header>());

  std::string contact_address("sip:");
  contact_address += "domain.invalid";
  scoped_ptr<Contact> contact(new Contact(GURL(contact_address)));
  // Now uses the build time to generate a single instance ID
  base::string16 build_time =
    base::TimeFormatShortDateAndTime(base::GetBuildTime());
  std::string instance = base::MD5String(base::WideToUTF8(build_time));
  std::string instance_id = base::StringPrintf("\"<urn:uuid:%s-%s-%s-%s-%s>\"",
    instance.substr(0, 8).c_str(),
    instance.substr(8, 4).c_str(),
    instance.substr(12, 4).c_str(),
    instance.substr(16, 4).c_str(),
    instance.substr(20, 12).c_str());
  contact->front().param_set("+sip.instance", instance_id);
  if (Method::REGISTER == method) {
    contact->front().param_set("reg-id", "1");
  }
  request->push_back(contact.PassAs<Header>());
  return request;
}

int UserAgent::Send(
    const scoped_refptr<Message> &message,
    const net::CompletionCallback& callback) {
  if (isa<Response>(message)) {
    scoped_refptr<Response> response = dyn_cast<Response>(message);
    HandleDialogStateOnResponse(response);
  } else {
    scoped_refptr<Request> request = dyn_cast<Request>(message);
    outgoing_requests_.insert(std::make_pair(request->id(),
        new OutgoingRequestContext(request)));
  }
  return network_layer_->Send(message, callback);
}

scoped_refptr<Dialog> UserAgent::HandleDialogStateOnResponse(
    const scoped_refptr<Response> &response) {
  scoped_refptr<Dialog> dialog;
  scoped_refptr<Request> request(response->refer_to());
  int response_code = response->response_code();
  // Create dialog on response_code > 100 for INVITE requests
  if (Method::INVITE == request->method()
      && response_code > 100
      && response->get<To>()->HasTag()) {
    DialogMapType::iterator i;
    tie(dialog, i) = GetDialog(response);
    if (!dialog) {
      switch (response_code/100) {
        case 1:
        case 2:
          dialog = Dialog::Create(response);
          dialogs_.insert(std::make_pair(dialog->id(), dialog));
          break;
      }
    }
    else {
      switch (response_code/100) {
        case 1:
          break;
        case 2:
          dialog->set_state(Dialog::STATE_CONFIRMED);
          break;
        default:
          dialog->set_state(Dialog::STATE_TERMINATED);
          dialogs_.erase(i);
          break;
      }
    }
  }
  // Terminate UAC dialog on response_code 2xx for BYE requests
  else if (Method::BYE == request->method()
           && response_code/100 == 2) {
    DialogMapType::iterator i;
    tie(dialog, i) = GetDialog(response);
    if (dialog) {
      dialog->set_state(Dialog::STATE_TERMINATED);
      dialogs_.erase(i);
    }
  }
  return dialog;
}

scoped_refptr<Dialog> UserAgent::HandleDialogStateOnError(
    const scoped_refptr<Request> &request) {
  scoped_refptr<Dialog> dialog;
  if (Message::Outgoing == request->direction()) {
    // UAC timeout or transport error
    DialogMapType::iterator i;
    tie(dialog, i) = GetDialog(request);
    if (dialog) {
      dialog->set_state(Dialog::STATE_TERMINATED);
      dialogs_.erase(i);
    }
  }
  return dialog;
}

bool UserAgent::HandleChallengeAuthentication(
    const scoped_refptr<Response> &response,
    const scoped_refptr<Dialog> &dialog) {
  int response_code = response->response_code();
  if (SIP_UNAUTHORIZED != response_code
      && SIP_PROXY_AUTHENTICATION_REQUIRED != response_code)
    return false;
  scoped_refptr<Request> request(response->refer_to());
  OutgoingRequestMap::iterator i = outgoing_requests_.find(request->id());
  if (outgoing_requests_.end() == i)
    return false;
  if (!i->second->auth_transaction_.get()) {
    i->second->auth_transaction_.reset(new AuthTransaction(&auth_cache_,
        auth_handler_factory_, password_handler_factory_, net_log_));
  }
  i->second->last_dialog_ = dialog;
  i->second->last_response_ = response;
  int rv = i->second->auth_transaction_->HandleChallengeAuthentication(
      response, base::Bind(&UserAgent::OnAuthenticationComplete,
          weak_factory_.GetWeakPtr(), request->id()));
  if (net::ERR_IO_PENDING == rv) {
    return true;
  } else if (net::OK == rv) {
    OnAuthenticationComplete(request->id(), rv);
    return true;
  }
  return false;
}

void UserAgent::OnAuthenticationComplete(
    const std::string &request_id, int rv) {
  DCHECK_NE(rv, net::ERR_IO_PENDING);
  OutgoingRequestMap::iterator i = outgoing_requests_.find(request_id);
  if (outgoing_requests_.end() == i)
    return;
  if (net::OK == rv) {
    // Remove topmost Via header
    Message::iterator j = i->second->outgoing_request_->find_first<Via>();
    if (i->second->outgoing_request_->end() != j) {
      i->second->outgoing_request_->erase(j);
    }
    rv = network_layer_->Send(i->second->outgoing_request_,
        base::Bind(&UserAgent::OnResendRequestComplete,
            weak_factory_.GetWeakPtr(), request_id));
    if (net::ERR_IO_PENDING != rv) {
      OnResendRequestComplete(request_id, rv);
    }
  } else {
    RunUserIncomingResponseCallback(i->second->last_response_,
        i->second->last_dialog_);
  }
}

void UserAgent::OnResendRequestComplete(
    const std::string &request_id, int rv) {
  DCHECK_NE(rv, net::ERR_IO_PENDING);
  OutgoingRequestMap::iterator i = outgoing_requests_.find(request_id);
  if (outgoing_requests_.end() == i)
    return;
  if (net::OK != rv) {
    RunUserTransportErrorCallback(i->second->outgoing_request_, rv,
        i->second->last_dialog_);
  }
}

void UserAgent::OnChannelConnected(const EndPoint &destination, int err) {
  for (std::vector<Delegate*>::iterator i = handlers_.begin();
       i != handlers_.end(); i++) {
    (*i)->OnChannelConnected(destination, err);
  }
}

void UserAgent::OnChannelClosed(const EndPoint &destination) {
  for (std::vector<Delegate*>::iterator i = handlers_.begin();
       i != handlers_.end(); i++) {
    (*i)->OnChannelClosed(destination);
  }
}

void UserAgent::OnSSLCertificateError(const EndPoint &destination,
    const net::SSLInfo &ssl_info, bool fatal) {
  for (std::vector<Delegate*>::iterator i = handlers_.begin();
       i != handlers_.end(); i++) {
    (*i)->OnSSLCertificateError(destination, ssl_info, fatal);
  }
}

void UserAgent::OnIncomingRequest(
    const scoped_refptr<Request> &request) {
  scoped_refptr<Dialog> dialog = GetDialog(request).first;
  RunUserIncomingRequestCallback(request, dialog);
}

void UserAgent::OnIncomingResponse(
    const scoped_refptr<Response> &response) {
  scoped_refptr<Dialog> dialog = HandleDialogStateOnResponse(response);
  if (HandleChallengeAuthentication(response, dialog))
    return;
  RunUserIncomingResponseCallback(response, dialog);
}

void UserAgent::OnTimedOut(const scoped_refptr<Request> &request) {
  scoped_refptr<Dialog> dialog = HandleDialogStateOnError(request);
  for (std::vector<Delegate*>::iterator i = handlers_.begin();
       i != handlers_.end(); i++) {
    (*i)->OnTimedOut(request, dialog);
  }
}

void UserAgent::OnTransportError(
    const scoped_refptr<Request> &request, int err) {
  scoped_refptr<Dialog> dialog = HandleDialogStateOnError(request);
  for (std::vector<Delegate*>::iterator i = handlers_.begin();
       i != handlers_.end(); i++) {
    (*i)->OnTransportError(request, err, dialog);
  }
}

void UserAgent::RunUserIncomingRequestCallback(
    const scoped_refptr<Request> &request,
    const scoped_refptr<Dialog> &dialog) {
  for (std::vector<Delegate*>::iterator i = handlers_.begin();
       i != handlers_.end(); i++) {
    (*i)->OnIncomingRequest(request, dialog);
  }
}

void UserAgent::RunUserIncomingResponseCallback(
    const scoped_refptr<Response> &response,
    const scoped_refptr<Dialog> &dialog) {
  for (std::vector<Delegate*>::iterator i = handlers_.begin();
       i != handlers_.end(); i++) {
    (*i)->OnIncomingResponse(response, dialog);
  }
}

void UserAgent::RunUserTransportErrorCallback(
      const scoped_refptr<Request> &request,
      int error,
      const scoped_refptr<Dialog> &dialog) {
  for (std::vector<Delegate*>::iterator i = handlers_.begin();
       i != handlers_.end(); i++) {
    (*i)->OnTransportError(request, error, dialog);
  }
}

} // End of ua namespace
} // End of sippet namespace
