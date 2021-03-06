// Copyright (c) 2013 The Sippet Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sippet/uri/uri.h"

#include <utility>
#include <string>

#include "base/basictypes.h"

#include "testing/gtest/include/gtest/gtest.h"

using sippet::SipURI;
using sippet::TelURI;

TEST(SipURI, Parser) {
  struct {
    const char *input;
    bool valid;
    const char *host;
    int port;
    int effective_port;
    bool has_username;
    const char *username;
    bool has_password;
    const char *password;
    const char *parameters;
    const char *headers;
  } tests[] = {
    {"sip:sip.domain.com", true,
     "sip.domain.com", -1, 5060, false, "", false, "", "", "" },
    {"sip:user@sip.domain.com", true,
     "sip.domain.com", -1, 5060, true, "user", false, "", "", "" },
    {"sip:user@sip.domain.com;param=1234", true,
     "sip.domain.com", -1, 5060, true, "user", false, "",
     ";param=1234", "" },
    {"sip:1234@sip.domain.com:5060;TCID-0", true,
     "sip.domain.com", -1, 5060, true, "1234", false, "",
     ";TCID-0", "" },
    {"sip:user@sip.domain.com?header=1234", true, "sip.domain.com", -1, 5060,
     true, "user", false, "", "", "header=1234" },
    {"sip:[5f1b:df00:ce3e:e200:20:800:121.12.131.12]", true,
     "[5f1b:df00:ce3e:e200:20:800:790c:830c]", -1, 5060,
     false, "", false, "", "", "" },
    {"sip:user@[5f1b:df00:ce3e:e200:20:800:121.12.131.12]", true,
     "[5f1b:df00:ce3e:e200:20:800:790c:830c]", -1, 5060,
     true, "user", false, "", "", "" },
    {"sips:192.168.2.12", true,
     "192.168.2.12", -1, 5061, false, "", false, "", "", "" },
    {"sips:host.foo.com", true,
     "host.foo.com", -1, 5061, false, "", false, "", "", "" },
    {"sip:user;x-v17:password@host.com:5555", true, "host.com",
     5555, 5555, true, "user;x-v17", true, "password", "", "" },
    {"sip:wombat@192.168.2.221:5062;transport=Udp", true,
     "192.168.2.221", 5062, 5062, true, "wombat", false, "",
     ";transport=Udp", "" },
    {"sip:+358-555-1234567;isub=1411;postd=pp2@company.com;user=phone", true,
     "company.com", -1, 5060,
     true, "+358-555-1234567;isub=1411;postd=pp2", false, "",
     ";user=phone", "" },
    {"sip:biloxi.com;transport=tcp;method=REGISTER?to=sip:bob%40biloxi.com",
     true, "biloxi.com", -1, 5060, false, "", false, "",
     ";transport=tcp;method=REGISTER", "to=sip:bob%40biloxi.com" },
    {"sip:alice@atlanta.com?subject=Project%20X&priority=urgent", true,
     "atlanta.com", -1, 5060, true, "alice", false, "",
     "", "subject=Project%20X&priority=urgent" },
    {"sip:alice@atlanta.com;param=@route66?subject=Project X&priority=urgent",
     true, "atlanta.com", -1, 5060, true, "alice", false, "",
     ";param=%40route66", "subject=Project%20X&priority=urgent" },
    {"tel:+358-555-1234567;pOstd=pP2;isUb=1411", false },
    {"tel:+358 (555) 1234567;pOstd=pP2;isUb=1411", false },
    {"*", false},
    // Escaped characters
    {"sip:user;par=u%40example.net@example.com", true,
     "example.com", -1, 5060, true, "user;par=u@example.net", false,
     "", "", ""},
    // lr parameter
    {"sip:example.com;lr", true,
     "example.com", -1, 5060, false, "", false,
     "", ";lr", ""},
  };

  for (int i = 0; i < arraysize(tests); ++i) {
    SipURI uri(tests[i].input);
    EXPECT_EQ(tests[i].valid, uri.is_valid());
    if (tests[i].valid) {
      EXPECT_EQ(tests[i].host, uri.host());
      EXPECT_EQ(tests[i].port, uri.IntPort());
      EXPECT_EQ(tests[i].effective_port, uri.EffectiveIntPort());
      EXPECT_EQ(tests[i].has_username, uri.has_username());
      if (tests[i].has_username)
        EXPECT_EQ(tests[i].username, uri.username());
      EXPECT_EQ(tests[i].has_password, uri.has_password());
      if (tests[i].has_password)
        EXPECT_EQ(tests[i].password, uri.password());
      EXPECT_EQ(tests[i].parameters[0] != 0, uri.has_parameters());
      if (tests[i].parameters[0] != 0)
        EXPECT_EQ(tests[i].parameters, uri.parameters());
      EXPECT_EQ(tests[i].headers[0] != 0, uri.has_headers());
      if (tests[i].headers[0] != 0)
        EXPECT_EQ(tests[i].headers, uri.headers());
    }
  }
}

TEST(SipURI, ParameterAndHeaders) {
  SipURI uri("sip:alice@atlanta.com;param=%40route66?subject=Project%20X");
  ASSERT_TRUE(uri.is_valid());

  std::pair<bool, std::string> p =
    uri.parameter("param");
  EXPECT_TRUE(p.first);
  EXPECT_EQ("@route66", p.second);

  std::pair<bool, std::string> h =
    uri.header("Subject");
  EXPECT_TRUE(h.first);
  EXPECT_EQ("Project X", h.second);
}

TEST(SipURI, OnlyHeaders) {
  SipURI uri("sip:alice@atlanta.com?"
      "to=sip%3Aalice%40atlanta.com&subject=Project%20X");
  ASSERT_TRUE(uri.is_valid());

  std::pair<bool, std::string> h1 =
    uri.header("Subject");
  EXPECT_TRUE(h1.first);
  EXPECT_EQ("Project X", h1.second);

  std::pair<bool, std::string> h2 =
    uri.header("To");
  EXPECT_TRUE(h2.first);
  EXPECT_EQ("sip:alice@atlanta.com", h2.second);
}

TEST(SipURI, LooseRoutingParameter) {
  SipURI uri("sip:192.168.0.1;lr");
  ASSERT_TRUE(uri.is_valid());

  std::pair<bool, std::string> p =
    uri.parameter("lr");
  EXPECT_TRUE(p.first);
  EXPECT_EQ("", p.second);
}

TEST(TelURI, Parser) {
  struct {
    const char *input;
    bool valid;
    const char *telefone_subscriber;
    const char *parameters;
  } tests[] = {
    {"tel:+358-555-1234567;pOstd=pP2;isUb=1411", true,
     "+358-555-1234567", ";pOstd=pP2;isUb=1411" },
    {"tel:+358 (555) 1234567;pOstd=pP2;isUb=1411", true,
     "+358%20(555)%201234567", ";pOstd=pP2;isUb=1411" },
    {"sip:user@sip.domain.com", false },
    {"*", false},
  };

  for (int i = 0; i < arraysize(tests); ++i) {
    TelURI uri(tests[i].input);
    EXPECT_EQ(tests[i].valid, uri.is_valid());
    if (tests[i].valid) {
      EXPECT_EQ(tests[i].telefone_subscriber, uri.telephone_subscriber());
      EXPECT_EQ(tests[i].parameters[0] != 0, uri.has_parameters());
      if (tests[i].parameters[0] != 0)
        EXPECT_EQ(tests[i].parameters, uri.parameters());
    }
  }
}

TEST(TelURI, ToSipURI) {
  struct {
    const char *origin;
    const char *input;
    const char *output;
  } tests[] = {
    { "sip:foo.com", "tel:+358-555-1234567",
      "sip:+358-555-1234567@foo.com;user=phone" },
    { "sip:foo.com", "tel:+358 (555) 1234567",
      "sip:+358%20(555)%201234567@foo.com;user=phone" },
    { "sip:foo.com", "tel:+358-555-1234567;postd=pp22",
      "sip:+358-555-1234567;postd=pp22@foo.com;user=phone" },
    { "sip:foo.com", "tel:+358-555-1234567;POSTD=PP22",
      "sip:+358-555-1234567;POSTD=PP22@foo.com;user=phone" },
    { "sip:foo.com:5555", "tel:+358-555-1234567;postd=pp22",
      "sip:+358-555-1234567;postd=pp22@foo.com:5555;user=phone" },
    { "sip:foo.com:5555;param=abc", "tel:+358-555-1234567;postd=pp22",
      "sip:+358-555-1234567;postd=pp22@foo.com:5555;user=phone" },
    { "sip:foo.com:5555?header=", "tel:+358-555-1234567;postd=pp22",
      "sip:+358-555-1234567;postd=pp22@foo.com:5555;user=phone" },
    { "sip:foo.com:5555;param=abc?header=", "tel:+358-555-1234567;postd=pp22",
      "sip:+358-555-1234567;postd=pp22@foo.com:5555;user=phone" },
  };

  for (int i = 0; i < arraysize(tests); ++i) {
    SipURI origin(tests[i].origin);
    TelURI uri(tests[i].input);
    EXPECT_EQ(tests[i].output, uri.ToSipURI(origin).spec());
  }
}
