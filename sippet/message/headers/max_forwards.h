// Copyright (c) 2013 The Sippet Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SIPPET_MESSAGE_HEADERS_MAX_FORWARDS_H_
#define SIPPET_MESSAGE_HEADERS_MAX_FORWARDS_H_

#include "sippet/message/header.h"
#include "sippet/message/headers/bits/single_value.h"
#include "sippet/base/raw_ostream.h"

namespace sippet {

class MaxForwards :
  public Header,
  public single_value<unsigned> {
 private:
  DISALLOW_ASSIGN(MaxForwards);
  MaxForwards(const MaxForwards &other);
  MaxForwards *DoClone() const override;

 public:
  MaxForwards();
  MaxForwards(const single_value::value_type &n);
  ~MaxForwards() override;

  scoped_ptr<MaxForwards> Clone() const {
    return scoped_ptr<MaxForwards>(DoClone());
  }

  void print(raw_ostream &os) const override;
};

} // End of sippet namespace

#endif // SIPPET_MESSAGE_HEADERS_MAX_FORWARDS_H_
