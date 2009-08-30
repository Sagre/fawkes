
/***************************************************************************
 *  page_reply.h - Web request reply for a normal page
 *
 *  Created: Thu Oct 23 16:13:48 2008
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

#include <webview/page_reply.h>
#include <webview/page_header_generator.h>
#include <webview/page_footer_generator.h>
#include <utils/system/hostinfo.h>

#include <cstdlib>
#include <cstring>
#include <cstdio>

/** @class WebPageReply "page_reply.h"
 * Basic page reply.
 * This reply adds header and footer as appropriate to form a HTML document
 * with logo and navigation.
 * @author Tim Niemueller
 */

/** Page header template. */
const char *  WebPageReply::PAGE_HEADER =
  "<html>\n"
  " <head>\n"
  "  <title>%s</title>\n"
  "  <link rel=\"stylesheet\" type=\"text/css\" href=\"/static/webview.css\" />\n"
  " </head>\n"
  " <body>\n";

/** Page footer template. */
const char *  WebPageReply::PAGE_FOOTER =
  "\n </body>\n"
  "</html>\n";

/** Constructor.
 * @param title title of the page
 * @param body Optional initial body of the page
 */
WebPageReply::WebPageReply(std::string title, std::string body)
  : StaticWebReply(WebReply::HTTP_OK, body)
{
  _title = title;
}


/** Base constructor.
 * Constructor that does not set a title or anything. Use for sub-classes.
 * @param code HTTP code for this reply
 */
WebPageReply::WebPageReply(response_code_t code)
  : StaticWebReply(code)
{
}


void
WebPageReply::pack(std::string active_baseurl,
		   WebPageHeaderGenerator *headergen,
		   WebPageFooterGenerator *footergen)
{
  if (headergen)  __merged_body += headergen->html_header(_title, active_baseurl);
  else {
    fawkes::HostInfo hi;
    char *s;
    if ( asprintf(&s, PAGE_HEADER, _title.c_str(), hi.short_name()) != -1 ) {
      __merged_body += s;
      free(s);
    }
  }

  __merged_body  += _body;

  if (footergen)  __merged_body += footergen->html_footer();
  else            __merged_body += PAGE_FOOTER;
}

std::string::size_type
WebPageReply::body_length()
{
  return __merged_body.length();
}


const std::string &
WebPageReply::body()
{
  return __merged_body;
}


