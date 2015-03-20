---
layout: page
title: Using fluent SIP Messages
---

Sippet messages are a set of classes that gives the high-level application
programmer a fluent usage of the request and response data. Messages can be of
type `Request` or `Response`, and are composed of `Header` objects and a
content body.

Instead of parsing each SIP header by yourself, or creating each one by
composing strings following RFCs specifications, this is delegated to an
hierarchy of classes, all derived from `Header`.


# Creating Messages

Here is an example of a full INVITE request creation:

    #include "sippet/message/message.h"

    using namespace sippet;

    scoped_refptr<Request> CreateInvite() {
      scoped_refptr<Request> request(
        new Request(Method::INVITE, GURL("sip:bob@biloxi.com")));

      scoped_ptr<Via> via(
        new Via(ViaParam(Protocol::UDP,
                         net::HostPortPair::FromString(pc33.atlanta.com))));
      via->front().set_branch("z9hG4bK776asdhds");
      request->push_back(via.Pass());

      scoped_ptr<MaxForwards> maxfw(new MaxForwards(70));
      request->push_back(maxfw.Pass());

      scoped_ptr<To> to(new To(GURL("sip:bob@biloxi.com"), "Bob"));
      request->push_back(to.Pass());

      scoped_ptr<From> from(
        new From(GURL("sip:alice@atlanta.com"), "Alice"));
      from->set_tag("1928301774");
      request->push_back(from.Pass());

      scoped_ptr<CallId> callid(
        new CallId("a84b4c76e66710@pc33.atlanta.com"));
      request->push_back(callid.Pass());

      scoped_ptr<Cseq> cseq(new Cseq(314159, Method::INVITE));
      request->push_back(cseq.Pass());

      scoped_ptr<Contact> contact(new Contact(GURL("sip:alice@pc33.atlanta.com")));
      request->push_back(contact.Pass());

      scoped_ptr<ContentType> content_type(new ContentType);
      content_type->push_back(MediaType("application", "sdp"));
      request->push_back(content_type.Pass());

      // The Content-Length is automatically set.

      return request;
    }

This is a copy of the example message given originally in RFC 3261:

    INVITE sip:bob@biloxi.com SIP/2.0
    Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bK776asdhds
    Max-Forwards: 70
    To: Bob <sip:bob@biloxi.com>
    From: Alice <sip:alice@atlanta.com>;tag=1928301774
    Call-ID: a84b4c76e66710@pc33.atlanta.com
    CSeq: 314159 INVITE
    Contact: <sip:alice@pc33.atlanta.com>
    Content-Type: application/sdp
    Content-Length: 0

The response generation follows about the same sequence, with the difference
you have to use a `Response` instead of a `Request` type.

The Sippet library offers message builders to ease the work for you. Check
classes `sippet::ua::UserAgent` and `sippet::Dialog`.


# SIP and Tel URIs canonicalization

Sippet comes with `SipURI` and `TelURI` classes for URI canonicalization. Both
accept conversions from GURL, but it's required to check the scheme first:

    #include "sippet/uri/uri.h"

    using namespace sippet;

    void HandleIncomingUrl(const GURL &url) {
      if (url.SchemeIs("sip") || url.SchemeIs("sips")) {
        SipURI uri(url);
        // do something with the SipURI
      }
      else if (url.SchemeIs("tel")) {
        TelURI uri(url);
        // do something with the TelURI
      }
      else {
        // process normal http://, https://, file://, etc.
      }
    }

It's possible to "convert" a `TelURI` into a `SipURI`. For example, the URI
**tel:+1555000000** can be converted into
**sip:+1555000000@biloxi.com;user=phone** using the following code:

    TelURI tel_uri("tel:+1555000000");
    SipURI origin("sip:bob@biloxi.com");
    SipURI sip_uri(tel_uri.ToSipURI(origin));

If you have to convert a `SipURI` into a `GURL`, the following code can be
used:

    SipURI sip_uri("sip:bob@biloxi.com");
    GURL url(sip_uri.spec());
