
/***************************************************************************
 *  request_dispatcher.h - Web request dispatcher
 *
 *  Created: Mon Oct 13 22:44:33 2008
 *  Copyright  2006-2008  Tim Niemueller [www.niemueller.de]
 *
 ****************************************************************************/

/*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  Read the full text in the LICENSE.GPL file in the doc directory.
 */

#ifndef __PLUGINS_WEBVIEW_REQUEST_DISPATCHER_H_
#define __PLUGINS_WEBVIEW_REQUEST_DISPATCHER_H_

#include <string>
#include <map>
#include <stdint.h>

struct MHD_Connection;

namespace fawkes {
#if 0 /* just to make Emacs auto-indent happy */
}
#endif

class WebRequestProcessor;
class WebPageHeaderGenerator;
class WebPageFooterGenerator;
class StaticWebReply;

class WebRequestDispatcher
{
 public:
  WebRequestDispatcher(WebPageHeaderGenerator *headergen = 0,
		       WebPageFooterGenerator *footergen = 0);

  void add_processor(const char *url_prefix, WebRequestProcessor *processor);
  void remove_processor(const char *url_prefix);


  static int process_request_cb(void *callback_data,
				struct MHD_Connection * connection,
				const char *url,
				const char *method,
				const char *version,
				const char *upload_data,
				size_t *upload_data_size,
				void  **session_data);

  static int  dynamic_reply_data_cb(void *reply, uint64_t pos, char *buf, int max);
  static void dynamic_reply_free_cb(void *reply);

  int queue_static_reply(struct MHD_Connection * connection,
			 StaticWebReply *sreply);
  int process_request(struct MHD_Connection * connection,
		      const char *url,
		      const char *method,
		      const char *version,
		      const char *upload_data,
		      size_t *upload_data_size,
		      void **session_data);

 private:
  std::map<std::string, WebRequestProcessor *>  __processors;
  WebRequestProcessor                          *__startpage_processor;

  std::string                                   __active_baseurl;
  WebPageHeaderGenerator                       *__page_header_generator;
  WebPageFooterGenerator                       *__page_footer_generator;
};

} // end namespace fawkes

#endif