/*
** pork_html.c - functions for dealing with HTML
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <config.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>

#include <pork.h>
#include <pork_set.h>
#include <pork_missing.h>
#include <pork_util.h>
#include <pork_list.h>
#include <pork_html.h>

/*
** Strip any html tags from "str"
** Replace common "&*;" forms with their ascii representation.
**
** This function uses xmalloc() to grab its memory and so it must be
** free()'d when you're done using it.
*/

char *strip_html(char *str) {
	char *p;
	char *buf;
	char *ret;
	int ignore = 0;
	char *last_tag = NULL;
	char last_url[512];
	size_t len;

	if (str == NULL)
		return (xstrdup(""));

	len = strlen(str) * 2 + 1;
	last_url[0] = '\0';

	buf = xmalloc(len);
	ret = buf;

	for (p = str ; *p != '\0' ; p++) {
		if (ignore == 1) {
			if (*p == '>') {
				ignore = 0;

				if (last_tag != NULL) {
					if (!strncasecmp(last_tag, "<br>", 4) ||
						!strncasecmp(last_tag, "<p>", 3))
					{
						if (len < 2)
							goto out_oom;

						len--;
						*buf++ = '\n';
					} else if (!strncasecmp(last_tag, "</a>", 4)) {
						if (last_url[0] != '\0') {
							size_t url_len = strlen(last_url);

							/*
							** URL len + " (URL: " + ')' + '\0'
							*/
							if (len < url_len + 7 + 1 + 1)
								goto out_oom;

							strcpy(buf, " (URL: ");
							buf += 7;

							strcpy(buf, last_url);
							buf += url_len;
							*buf++ = ')';

							len -= url_len + 8;
							last_url[0] = '\0';
						}
					} else if (!strncasecmp(last_tag, "<a", 2)) {
						char *start = last_tag + 2;
						char *p = start;

						while (*p == ' ' || *p == '\t')
							p++;

						if (start != p && !strncasecmp(p, "href", 4)) {
							start = strchr(last_tag, '=');
							if (start != NULL) {
								char *end;

								start++;

								while (*start == ' ' || *start == '\t')
									start++;

								if (*start == '"')
									start++;

								end = strchr(start, '>');
								if (end != NULL) {
									char *close_tag = end;
									size_t i = 0;

									end = strchr(start, '"');
									if (end == NULL) {
										end = strchr(start, ' ');
										if (end == NULL) {
											end = strchr(start, '\t');
											if (end == NULL)
												end = close_tag;
										}
									}

									while (start != end && *start != '\0' &&
											i < sizeof(last_url) - 1)
									{
										if (*start == '%') {
											last_url[i++] = '%';

											if (i >= sizeof(last_url) - 1)
												break;
										}

										last_url[i++] = *start++;
									}

									last_url[i] = '\0';
								}
							}
						}
					}

					last_tag = NULL;
				}
			} else if (*p == '<')
				last_tag = p;

			continue;
		}

		if (*p == '<') {
			ignore = 1;
			last_tag = p;
			continue;
		} else if (*p == '\f') {
			if (len < 2)
				goto out_oom;

			*buf++ = ' ';
			len--;
			continue;
		} else if (*p == '%') {
			if (len < 3)
				goto out_oom;

			/*
			** Double any '%' characters in HTML messages.
			*/
			buf[0] = '%';
			buf[1] = '%';
			buf += 2;
			len -= 2;
		} else if (*p == '&') {
			char *q = strchr(p, ';');
			char result;

			if (q != NULL) {
				p++;
				*q = '\0';

				if (!strcasecmp(p, "nbsp"))
					result = ' ';
				else if (!strcasecmp(p, "lt"))
					result = '<';
				else if (!strcasecmp(p, "gt"))
					result = '>';
				else if (!strcasecmp(p, "amp"))
					result = '&';
				else if (!strcasecmp(p, "quot"))
					result = '"';
				else if (!strcasecmp(p, "apos"))
					result = '\'';
				else if (!strcasecmp(p, "hyphen"))
					result = '-';
				else if (!strcasecmp(p, "comma"))
					result = ',';
				else if (!strcasecmp(p, "excl"))
					result = '!';
				else if (!strcasecmp(p, "iexcl"))
					result = 'i';
				else if (!strcasecmp(p, "percnt"))
					result = '%';
				else if (!strcasecmp(p, "num"))
					result = '#';
				else if (!strcasecmp(p, "ast") || !strcasecmp(p, "mul"))
					result = '*';
				else if (!strcasecmp(p, "commat"))
					result = '@';
				else if (!strcasecmp(p, "lsqb"))
					result = '[';
				else if (!strcasecmp(p, "rsqb"))
					result = ']';
				else if (!strcasecmp(p, "bsol"))
					result = '\\';
				else if (!strcasecmp(p, "lcub"))
					result = '{';
				else if (!strcasecmp(p, "plus"))
					result = '+';
				else if (!strcasecmp(p, "divide"))
					result = '/';
				else if (!strcasecmp(p, "rcub"))
					result = '}';
				else if (!strcasecmp(p, "colon"))
					result = ':';
				else if (!strcasecmp(p, "period"))
					result = '.';
				else if (!strcasecmp(p, "semi"))
					result = ';';
				else {
					if (len < 2)
						goto out_oom;

					p--;
					*q = ';';

					*buf++ = '&';
					len--;
					continue;
				}

				if (len < 2)
					goto out_oom;

				p = q;
				*buf++ = result;
				len--;

				continue;
			} else {
				if (len < 2)
					goto out_oom;

				*buf++ = '&';
				len--;
				continue;
			}
		} else if (*p == '\r') {
			if (len < 2)
				goto out_oom;

			*buf++ = '\n';
			len--;
		} else {
			if (len < 2)
				goto out_oom;

			*buf++ = *p;
			len--;
		}
	}

	if (len < 1)
		goto out_oom;

	*buf++ = '\0';
	return (ret);

out_oom:
	free(ret);
	return (NULL);
}

/*
** Converts plaintext to HTML.
**
** This returns a pointer to a static buffer to speed
** things up slightly. Don't call free on the pointer
** it returns.
*/

char *text_to_html(const char *src) {
	static char buf[8192];
	u_int32_t i = 0;
	char *font_face;
	char *font_size;
	char *font_bgcolor;
	char *font_fgcolor;
	int ret;

	/*
	** Message is already formatted as HTML -- leave it alone.
	*/
	if (!strncasecmp(src, "<html>", 6)) {
		xstrncpy(buf, src, sizeof(buf));
		return (buf);
	}

	buf[0] = '\0';

	font_face = opt_get_str(OPT_OUTGOING_MSG_FONT);
	font_size = opt_get_str(OPT_OUTGOING_MSG_FONT_SIZE);
	font_bgcolor = opt_get_str(OPT_OUTGOING_MSG_FONT_BGCOLOR);
	font_fgcolor = opt_get_str(OPT_OUTGOING_MSG_FONT_FGCOLOR);

	ret = snprintf(buf, sizeof(buf), "<FONT LANG=\"0\"");
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (NULL);
	i += ret;

	if (font_face != NULL && *font_face != '\0') {
		ret = snprintf(&buf[i], sizeof(buf) - i,
				" FACE=\"%s\"", font_face);
		if (ret < 0 || (size_t) ret >= sizeof(buf) - i)
			return (NULL);
		i += ret;
	}

	if (font_size != NULL && *font_size != '\0') {
		ret = snprintf(&buf[i], sizeof(buf) - i,
				" SIZE=\"%s\"", font_size);
		if (ret < 0 || (size_t) ret >= sizeof(buf) - i)
			return (NULL);
		i += ret;
	}

	if (font_bgcolor != NULL && *font_bgcolor != '\0') {
		ret = snprintf(&buf[i], sizeof(buf) - i,
				" BACK=\"%s\"", font_bgcolor);
		if (ret < 0 || (size_t) ret >= sizeof(buf) - i)
			return (NULL);
		i += ret;
	}

	if (font_fgcolor != NULL && *font_fgcolor != '\0') {
		ret = snprintf(&buf[i], sizeof(buf) - i,
				" COLOR=\"%s\"", font_fgcolor);
		if (ret < 0 || (size_t) ret >= sizeof(buf) - i)
			return (NULL);
		i += ret;
	}

	if (i < sizeof(buf) - 1)
		buf[i++] = '>';

	while (*src != '\0' && i < sizeof(buf) - 1) {
		switch (*src) {
			int ret;

			case '<':
				ret = xstrncpy(&buf[i], "&lt;", sizeof(buf) - i);
				if (ret == -1)
					return (buf);
				i += ret;
				break;

			case '>':
				ret = xstrncpy(&buf[i], "&gt;", sizeof(buf) - i);
				if (ret == -1)
					return (buf);
				i += ret;
				break;

			case '&':
				ret = xstrncpy(&buf[i], "&amp;", sizeof(buf) - i);
				if (ret == -1)
					return (buf);
				i += ret;
				break;

			default:
				buf[i++] = *src;
				break;
		}

		src++;
	}

	buf[i] = '\0';
	return (buf);
}
