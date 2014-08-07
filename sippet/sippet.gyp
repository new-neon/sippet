# Copyright (c) 2013 The Sippet Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../build/win_precompile.gypi',
  ],
  'targets': [
    {
      'target_name': 'sippet',
      'type': 'static_library',
      'dependencies': [
        'sippet_version',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
      ],
      'include_dirs': [
        '<(DEPTH)',
        '<(DEPTH)/third_party',
      ],
      'sources': [
        'base/casting.h',
        'base/format.h',
        'base/ilist.h',
        'base/ilist_node.h',
        'base/raw_ostream.cc',
        'base/raw_ostream.h',
        'base/sequences.h',
        'base/stl_extras.h',
        'base/string_extras.h',
        'base/tags.h',
        'base/tags.cc',
        'base/type_traits.h',
        'base/user_agent_utils.h',
        'base/user_agent_utils.cc',
        'base/user_agent_utils_ios.mm',
        'base/version.h',
        'base/version.cc',
        'message/atom.h',
        'message/header.h',
        'message/header.cc',
        'message/headers.h',
        'message/headers/accept_encoding.h',
        'message/headers/accept_encoding.cc',
        'message/headers/accept.h',
        'message/headers/accept.cc',
        'message/headers/accept_language.h',
        'message/headers/accept_language.cc',
        'message/headers/alert_info.h',
        'message/headers/alert_info.cc',
        'message/headers/allow.h',
        'message/headers/allow.cc',
        'message/headers/authentication_info.h',
        'message/headers/authentication_info.cc',
        'message/headers/authorization.h',
        'message/headers/authorization.cc',
        'message/headers/call_id.h',
        'message/headers/call_id.cc',
        'message/headers/call_info.h',
        'message/headers/call_info.cc',
        'message/headers/contact.h',
        'message/headers/contact.cc',
        'message/headers/content_disposition.h',
        'message/headers/content_disposition.cc',
        'message/headers/content_encoding.h',
        'message/headers/content_encoding.cc',
        'message/headers/content_language.h',
        'message/headers/content_language.cc',
        'message/headers/content_length.h',
        'message/headers/content_length.cc',
        'message/headers/content_type.h',
        'message/headers/content_type.cc',
        'message/headers/cseq.h',
        'message/headers/cseq.cc',
        'message/headers/date.h',
        'message/headers/date.cc',
        'message/headers/error_info.h',
        'message/headers/error_info.cc',
        'message/headers/expires.h',
        'message/headers/expires.cc',
        'message/headers/from.h',
        'message/headers/from.cc',
        'message/headers/generic.h',
        'message/headers/generic.cc',
        'message/headers/in_reply_to.h',
        'message/headers/in_reply_to.cc',
        'message/headers/max_forwards.h',
        'message/headers/max_forwards.cc',
        'message/headers/mime_version.h',
        'message/headers/mime_version.cc',
        'message/headers/min_expires.h',
        'message/headers/min_expires.cc',
        'message/headers/organization.h',
        'message/headers/organization.cc',
        'message/headers/priority.h',
        'message/headers/priority.cc',
        'message/headers/proxy_authenticate.h',
        'message/headers/proxy_authenticate.cc',
        'message/headers/proxy_authorization.h',
        'message/headers/proxy_authorization.cc',
        'message/headers/proxy_require.h',
        'message/headers/proxy_require.cc',
        'message/headers/record_route.h',
        'message/headers/record_route.cc',
        'message/headers/reply_to.h',
        'message/headers/reply_to.cc',
        'message/headers/require.h',
        'message/headers/require.cc',
        'message/headers/retry_after.h',
        'message/headers/retry_after.cc',
        'message/headers/route.h',
        'message/headers/route.cc',
        'message/headers/server.h',
        'message/headers/server.cc',
        'message/headers/subject.h',
        'message/headers/subject.cc',
        'message/headers/supported.h',
        'message/headers/supported.cc',
        'message/headers/timestamp.h',
        'message/headers/timestamp.cc',
        'message/headers/to.h',
        'message/headers/to.cc',
        'message/headers/unsupported.h',
        'message/headers/unsupported.cc',
        'message/headers/user_agent.h',
        'message/headers/user_agent.cc',
        'message/headers/via.h',
        'message/headers/via.cc',
        'message/headers/warning.h',
        'message/headers/warning.cc',
        'message/headers/www_authenticate.h',
        'message/headers/www_authenticate.cc',
        'message/headers/bits/auth_setters.h',
        'message/headers/bits/has_auth_params.h',
        'message/headers/bits/has_multiple.h',
        'message/headers/bits/has_parameters.h',
        'message/headers/bits/has_parameters.cc',
        'message/headers/bits/param_setters.h',
        'message/headers/bits/single_value.h',
        'message/header_list.h',
        'message/method_list.h',
        'message/protocol_list.h',
        'message/status_code_list.h',
        'message/message.h',
        'message/message.cc',
        'message/method.h',
        'message/method.cc',
        'message/parser/parser.cc',
        'message/parser/tokenizer.h',
        'message/parser/tokenizer.cc',
        'message/protocol.h',
        'message/protocol.cc',
        'message/request.h',
        'message/request.cc',
        'message/response.h',
        'message/response.cc',
        'message/version.h',
        'message/status_code.h',
        'message/status_code.cc',
        'uri/uri.h',
        'uri/uri.cc',
        'uri/uri_canon.h',
        'uri/uri_canon.cc',
        'uri/uri_canon_internal.h',
        'uri/uri_canon_internal.cc',
        'uri/uri_parse.h',
        'uri/uri_parse.cc',
        'uri/uri_parse_internal.h',
        'uri/uri_util.h',
        'uri/uri_util.cc',
        'transport/aliases_map.h',
        'transport/aliases_map.cc',
        'transport/end_point.h',
        'transport/end_point.cc',
        'transport/network_layer.h',
        'transport/network_layer.cc',
        'transport/network_settings.h',
        'transport/network_settings.cc',
        'transport/branch_factory.h',
        'transport/branch_factory.cc',
        'transport/channel.h',
        'transport/channel_factory.h',
        'transport/transaction_delegate.h',
        'transport/transaction_factory.h',
        'transport/transaction_factory.cc',
        'transport/client_transaction.h',
        'transport/client_transaction_impl.h',
        'transport/client_transaction_impl.cc',
        'transport/server_transaction.h',
        'transport/server_transaction_impl.h',
        'transport/server_transaction_impl.cc',
        'transport/time_delta_provider.h',
        'transport/time_delta_factory.h',
        'transport/time_delta_factory.cc',
        'transport/chrome/chrome_stream_writer.h',
        'transport/chrome/chrome_stream_writer.cc',
        'transport/chrome/chrome_datagram_writer.h',
        'transport/chrome/chrome_datagram_writer.cc',
        'transport/chrome/chrome_channel_factory.h',
        'transport/chrome/chrome_channel_factory.cc',
        'transport/chrome/chrome_stream_channel.h',
        'transport/chrome/chrome_stream_channel.cc',
        'transport/chrome/chrome_datagram_channel.h',
        'transport/chrome/chrome_datagram_channel.cc',
        'transport/chrome/chrome_stream_reader.h',
        'transport/chrome/chrome_stream_reader.cc',
        'ua/ua_user_agent.h',
        'ua/ua_user_agent.cc',
        'ua/dialog.h',
        'ua/dialog.cc',
        'ua/auth.h',
        'ua/auth.cc',
        'ua/auth_cache.h',
        'ua/auth_cache.cc',
        'ua/auth_handler.h',
        'ua/auth_handler.cc',
        'ua/auth_handler_digest.h',
        'ua/auth_handler_digest.cc',
        'ua/auth_handler_factory.h',
        'ua/auth_handler_factory.cc',
        'ua/auth_controller.h',
        'ua/auth_controller.cc',
      ],
      'conditions': [
        ['OS == "ios"', {
          # iOS has different user-agent construction utilities.
          'sources!': [
            'base/user_agent_util.cc',
          ],
        }, {  # OS != "ios"
          'sources!': [
            'base/user_agent_utils_ios.mm',
          ],
        }],
      ],
    },  # target sippet
    {
      'target_name': 'sippet_version',
      'type': 'none',
      'actions': [
        {
          'action_name': 'sippet_version',
          'inputs': [
            '<(script)',
            '<(lastchange)',
            '<(template)',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/sippet_version.h',
          ],
          'action': ['python',
                     '<(script)',
                     '-f', '<(lastchange)',
                     '<(template)',
                     '<@(_outputs)',
                   ],
          'variables': {
            'script': '<(DEPTH)/chrome/tools/build/version.py',
            'lastchange': '<(DEPTH)/build/util/LASTCHANGE',
            'template': 'build/sippet_version.h.in',
          },
        },
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      # Dependents may rely on files generated by this target or one of its
      # own hard dependencies.
      'hard_dependency': 1,
    },
  ],
}
