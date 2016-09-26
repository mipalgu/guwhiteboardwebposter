/**                                                                     
 *  /file guwhiteboardwebposter/main.cpp
 *                                                                      
 *  Created by Carl Lusty in 2016.                                      
 *  Copyright (c) 2016 Carl Lusty                                       
 *  All rights reserved.                                                
 */                                                                     

#include <assert.h>


#include <cstdio>
#include <sstream>
#include <cstdlib>
#include <cstring>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <err.h>




#include "gu_util.h"
#include "gusimplewhiteboard.h"
#include "gugenericwhiteboardobject.h"
#include "guwhiteboardtypelist_generated.h"
#include "guwhiteboardgetter.h"


#define DEFAULT_PORT 4242

/** socket variables */
typedef struct socket_s
{
    int                 socket;         ///< socket file descriptor

    void *data;         //recv buffer poitner

    struct timeval timestamp; ///< when data was received
    
    size_t data_size;   ///< size of the data

} socket_descriptor;

struct header_info_s;

enum HTTP_Verb
{
    HTTP_GET = 0,
    HTTP_HEAD,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_TRACE,
    HTTP_OPTIONS,
    HTTP_CONNECT,
    HTTP_PATCH,
    NUM_HTTP_VERBS,
    HTTP_UNKNOWN
};

enum HTTP_Code
{
    //compiler reuqires that variable names not start with a number, added underscore prefix
    _200_OK = 0, 
    _201_Created,
    _202_Accepted,
    _204_No_Content,
    _400_Bad_Request,
    _422_Unprocessable_Entity,
    _404_Not_Found,
    _411_Length_Required,
    _415_Unsupported_Media_Type,
    _418_Im_a_teapot,
    _501_Not_Implemented,
    NUM_HTTP_CODES,
    UNKNOWN_HTTP_CODE
};

const char *HTTP_Code_Strings[] = 
{
        "200 OK",
        "201 Created",
        "202 Accepted",
        "204 No Content",
        "400 Bad Request",
        "422 Unprocessable Entity",
        "404 Not Found",
        "411 Length Required",
        "415 Unsupported Media Type",
        "418 I'm a teapot",
        "501 Not Implemented"
};

enum HTTP_Version
{
    HTTP_V1 = 0, 
    HTTP_V1_1,
    NUM_HTTP_VERSIONS,
    UNKNOWN_HTTP_VERSION
};

const char *HTTP_Version_Strings[] = 
{
        "HTTP/1.0",
        "HTTP/1.1"
};

enum Content_Type
{
    Text_HTML = 0,
    Application_vnd_api_json,
    WildCard,
    NUM_SUPPORTED_CONTENT_TYPES
};

const char *Content_Type_Strings[] = 
{
        "text/html",
        "application/vnd.api+json",
        "*/*"
};

struct header_info_s
{
    enum HTTP_Verb verb;
    char url[100];
    enum HTTP_Version version;
    //enum Content_Type content_type;
    enum Content_Type accept; //text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8
};

void serverd(const char *wbname, int port);
socket_descriptor *init_socket(int port);
std::string recv_header(int *fd);
static void signal_handler();
void close_socket(socket_descriptor *sd);

//request handling
void handle_request(int *fd, gu_simple_whiteboard_descriptor *wbd, char *header);
void handle_html(int *fd, gu_simple_whiteboard_descriptor *wbd, struct header_info_s *header, char *body);
void handle_json(int *fd, gu_simple_whiteboard_descriptor *wbd, struct header_info_s *header, char *body);
void handle_get_request_json(int *fd, gu_simple_whiteboard_descriptor *wbd, struct header_info_s *header);
void handle_post_request_json(int *fd, gu_simple_whiteboard_descriptor *wbd, struct header_info_s *header, char *body);
void handle_get_request_html(int *fd, gu_simple_whiteboard_descriptor *wbd, struct header_info_s *header);
void usage_page(int *fd, gu_simple_whiteboard_descriptor *wbd, struct header_info_s *header, char *body);
void generate_response(int *fd, enum HTTP_Version version, enum HTTP_Code code, enum Content_Type type, std::string body);

//Parser functions
bool parse_header(char *header, struct header_info_s *header_s);
inline bool get_header_line(char *header, const char *field, char *output);
enum HTTP_Verb parse_verb(char *verb);
enum HTTP_Version parse_version(char *version);
enum Content_Type parse_content_type(char *content_type);
int detect_content_length(char *header);


int main(int argc, char **argv) 
{
	fprintf(stderr, "\n **** GU WHITEBOARD WEB POSTER ****\n (c) 2016 Carl Lusty\n\n");
	
	//Get passed in values
	//-----------------------------------
	int op;

	const char *wbname = NULL;
    int port = DEFAULT_PORT;
#ifdef CUSTOM_WB_NAME
	const char *default_name = CUSTOM_WB_NAME;
#else
	const char *default_name = GSW_DEFAULT_NAME;
#endif
	wbname = default_name;

	while((op = getopt(argc, argv, "p:w:")) != -1)
	{
		switch(op)
		{
			case 'p':
				port = atoi(optarg);
				break;
			case 'w':
				wbname = optarg;
				break;
			case '?':			
				fprintf(stderr, "\n\nUsage: guwhiteboardwebposter [OPTION] . . . \n");
				fprintf(stderr, "-p\tWeb Server Port, default: %d\n", DEFAULT_PORT);
				fprintf(stderr, "-w\tname of the whiteboard to interact with, default: %s\n", default_name);
				return EXIT_FAILURE;
			default:
				break;
		}
	}	
	//-----------------------------------
    
    argv += optind;
    argc -= optind;
    
	//Start
    serverd(wbname, port); //Returns on server shutdown signal
}

char response_header[] = "HTTP/1.1 200 OK\r\n"
"Content-Type: application/xml; charset=UTF-8\r\n\r\n";
char response_wb[] = ""
"";

void serverd(const char *wbname, int port)
{
    socket_descriptor *sd = init_socket(port);
	gu_simple_whiteboard_descriptor *wbd = gsw_new_whiteboard(wbname);

    listen(sd->socket, 5);

    int fd;

    while (1) 
    {
        fd = accept(sd->socket, NULL, NULL);

        std::string header = recv_header(&fd);

        handle_request(&fd, wbd, (char *)header.c_str());

        //close(client_fd);
    }

	if (wbd) gsw_free_whiteboard(wbd);
}

bool parse_header(char *header, struct header_info_s *header_s)
{
    std::string header_str = std::string(header);
    //get first line
    std::string first = header_str.substr(0, header_str.find("\r\n"));
#ifdef PARSE_DEBUG
    fprintf(stderr, "First header line: '%s'\n", first.c_str());
#endif
    //get HTTP verb, URL and HTTP version
    char verb[7]; memset(&verb[0], 0, sizeof(verb));
    char url[100]; memset(&url[0], 0, sizeof(url));
    char http_ver[100]; memset(&http_ver[0], 0, sizeof(http_ver));
    int r = sscanf(header, "%s %s %s", verb, url, http_ver);
    if(r != 3)
        return false;
#ifdef PARSE_DEBUG
    fprintf(stderr, "Header Field Parser, Field 'HTTP Verb' = '%s'\n", verb);
    fprintf(stderr, "Header Field Parser, Field 'URL' = '%s'\n", url);
    fprintf(stderr, "Header Field Parser, Field 'HTTP Version' = '%s'\n", http_ver);
#endif
    header_s->verb = parse_verb(verb);
    header_s->version = parse_version(http_ver);
    memcpy(header_s->url, url, sizeof(url));

    //get fields
    char accept[100];
    memset(&accept[0], 0, sizeof(accept));
    if(get_header_line(header, "Accept", accept) == false)
        return false;

    header_s->accept = parse_content_type(&accept[0]);
    if(header_s->accept == NUM_SUPPORTED_CONTENT_TYPES)
        return false;

    return true;
}

enum Content_Type parse_content_type(char *content_type)
{
    std::vector<std::string> types = components_of_string_separated(content_type, ',');
    for(int t = 0; t < types.size(); t++)
    {
        std::string type = types[t];
        for(int i = 0; i < NUM_SUPPORTED_CONTENT_TYPES; i++)
        {
            if(strcmp(type.c_str(), Content_Type_Strings[i]) == 0)
            {
#ifdef PARSE_DEBUG
                fprintf(stderr, "Found Supported Content Type: '%s'\n", type.c_str());
#endif
                return static_cast<enum Content_Type>(i);
            }
        }
    }
    return NUM_SUPPORTED_CONTENT_TYPES; //Not Supported
}

enum HTTP_Version parse_version(char *version)
{
    if(strcmp(version, "HTTP/1.0") == 0)
        return HTTP_V1;
    else if(strcmp(version, "HTTP/1.1") == 0)
        return HTTP_V1_1;
    else
        return UNKNOWN_HTTP_VERSION;
}

enum HTTP_Verb parse_verb(char *verb)
{
    if(strcmp(verb, "GET") == 0)
        return HTTP_GET;
    else if(strcmp(verb, "HEAD") == 0)
        return HTTP_HEAD;
    else if(strcmp(verb, "POST") == 0)
        return HTTP_POST;
    else if(strcmp(verb, "PUT") == 0)
        return HTTP_PUT;
    else if(strcmp(verb, "DELETE") == 0)
        return HTTP_DELETE;
    else if(strcmp(verb, "TRACE") == 0)
        return HTTP_TRACE;
    else if(strcmp(verb, "OPTIONS") == 0)
        return HTTP_OPTIONS;
    else if(strcmp(verb, "CONNECT") == 0)
        return HTTP_CONNECT;
    else if(strcmp(verb, "PATCH") == 0)
        return HTTP_PATCH;
    else
        return HTTP_UNKNOWN;
}

inline bool get_header_line(char *header, const char *field, char *output)
{
    std::string header_str = std::string(header);
    std::string target = std::string(field).append(": ");
    int index = header_str.find(target);
    if(index == std::string::npos)
    {
#ifdef PARSE_DEBUG
    fprintf(stderr, "Header Field Parser, Field '%s' = NOT FOUND\n", field);
#endif
        return false;
    }
    int start_value_pos = index + target.length();
    int end_value_pos = header_str.find("\r\n", start_value_pos);
    std::string value = header_str.substr(start_value_pos, end_value_pos - start_value_pos);
    memcpy(output, value.c_str(), sizeof(char)*value.length());
#ifdef PARSE_DEBUG
    fprintf(stderr, "Header Field Parser, Field '%s' = '%s'\n", field, value.c_str());
#endif
    return value.length() > 0;
}

void handle_html(int *fd, gu_simple_whiteboard_descriptor *wbd, struct header_info_s *header, char *body)
{
    switch(header->verb)
    {
        case HTTP_GET:
        {
            handle_get_request_html(fd, wbd, header);
            break;
        }
        case HTTP_POST:
        {
            //handle_post_request_html(fd, wbd, header, body);
            break;
        }
        case HTTP_PUT:
        {
            break;
        }
        case HTTP_DELETE:
        {
            break;
        }
        case HTTP_PATCH:
        {
            break;
        }
        default:
            usage_page(fd, wbd, header, body);
            break;
    }
}
   
void handle_json(int *fd, gu_simple_whiteboard_descriptor *wbd, struct header_info_s *header, char *body)
{
    switch(header->verb)
    {
        case HTTP_GET:
        {
            handle_get_request_json(fd, wbd, header);
            break;
        }
        case HTTP_POST:
        {
            handle_post_request_json(fd, wbd, header, body);
            break;
        }
        case HTTP_PUT:
        {
            break;
        }
        case HTTP_DELETE:
        {
            break;
        }
        case HTTP_PATCH:
        {
            break;
        }
        default:
            usage_page(fd, wbd, header, body);
            break;
    }
}

void handle_request(int *fd, gu_simple_whiteboard_descriptor *wbd, char *header)
{
    struct header_info_s header_info;
    memset(&header_info, 0, sizeof(struct header_info_s));

    bool header_parsed = parse_header(&header[0], &header_info);
    if(!header_parsed)
    {
        generate_response(fd, HTTP_V1_1, _400_Bad_Request, Text_HTML, "");
        return;
    }
    if(strcmp(header_info.url, "/favicon.ico") == 0)
    {
        fprintf(stderr, "Ignoring 'favicon.ico' request\n");
        generate_response(fd, HTTP_V1_1, _404_Not_Found, Text_HTML, "");
    }

#define BODY_BUF_SIZE 1000
    char body[BODY_BUF_SIZE];
    memset(&body[0], 0, sizeof body);
    int content_length = detect_content_length(&header[0]);
    if(content_length > 0) //Need to read message body?
    {   
        if(content_length <= BODY_BUF_SIZE)
            recv(*fd, body, sizeof(content_length), 0);
        else
            fprintf(stderr, "Message Body size of '%d' is larger than buffer", content_length);
    }

    switch(header_info.verb)
    {
        case Text_HTML:
        {
            handle_html(fd, wbd, &header_info, body);
            break;
        }
        case Application_vnd_api_json:
        {
            handle_json(fd, wbd, &header_info, body);
            break;
        }
        default:
        {
            break;
        }
    }
}

std::string recv_header(int *fd)
{
    std::string s;
    char c[2];
    int r;
    do
    {
        r = recv(*fd, c, sizeof(char), 0);
        s.append(&c[0]);
    }
    while(r == 1 && s.find("\r\n\r\n") == std::string::npos);
    return s;
}

void generate_response(int *fd, enum HTTP_Version version, enum HTTP_Code code, enum Content_Type type, std::string body)
{
    std::string response;
    response.append(HTTP_Version_Strings[version]);
    response.append(" ");
    response.append(HTTP_Code_Strings[code]);
    response.append("\r\n");
    response.append("Content-Type: ");
    response.append(Content_Type_Strings[type]);
    response.append(";");
    response.append("charset=UTF-8");
    response.append("\r\n");
    response.append("\r\n");
    response.append(body);

    write(*fd, response.c_str(), sizeof(char) * response.length()); 
    close(*fd);
}

void handle_get_request_json(int *fd, gu_simple_whiteboard_descriptor *wbd, struct header_info_s *header)
{
    std::string response = std::string("HTTP/1.1 200 OK\r\nContent-Type: application/vnd.api+json; charset=UTF-8\r\n\r\n");

    if(strcmp(header->url, "/") == 0 || strlen(header->url) == 0)
    {   //URL == /           - all messages, array
        response.append("{\"types\":[\r\n");

        for(int i = 0; i < GSW_NUM_TYPES_DEFINED; i++)
        {
            response.append("\t{\"type\":\"");
            response.append(guWhiteboard::WBTypes_stringValues[i]);
            response.append("\", \"parsable\":");
            char *s = whiteboard_get_from(wbd, guWhiteboard::WBTypes_stringValues[i]);
            if(strcmp(s, "##unsupported##") == 0)
                response.append("false");
            else
                response.append("true");
            free(s);
            response.append("}");
            if(i != GSW_NUM_TYPES_DEFINED - 1)
                response.append(",");
            response.append("\r\n");
        }
        response.append("]}\r\n");
    } 
    else 
    {   //URL == /$(msg) 
        response.append("{\"value\":\"");
        char msg_string[100]; msg_string[0] = '\0';
        sscanf(header->url, "/%s", msg_string);
        char *s = whiteboard_get_from(wbd, msg_string);
        response.append(s);
        free(s);
        response.append("\"}");
    } 
    write(*fd, response.c_str(), sizeof(char) * response.length()); 
    close(*fd);
}

void handle_post_request_json(int *fd, gu_simple_whiteboard_descriptor *wbd, struct header_info_s *header, char *body)
{
    std::string response = std::string("HTTP/1.1 200 OK\r\nContent-Type: application/vnd.api+json; charset=UTF-8\r\n\r\n");

    if(strcmp(header->url, "/") == 0 || strlen(header->url) == 0)
    {   //URL == /           - all messages, array
        response.append("{\"types\":[\r\n");

        for(int i = 0; i < GSW_NUM_TYPES_DEFINED; i++)
        {
            response.append("\t{\"type\":\"");
            response.append(guWhiteboard::WBTypes_stringValues[i]);
            response.append("\", \"parsable\":");
            char *s = whiteboard_get_from(wbd, guWhiteboard::WBTypes_stringValues[i]);
            if(strcmp(s, "##unsupported##") == 0)
                response.append("false");
            else
                response.append("true");
            free(s);
            response.append("}");
            if(i != GSW_NUM_TYPES_DEFINED - 1)
                response.append(",");
            response.append("\r\n");
        }
        response.append("]}\r\n");
    } 
    else 
    {   //URL == /$(msg) 
        response.append("{\"value\":\"");
        char msg_string[100]; msg_string[0] = '\0';
        sscanf(header->url, "/%s", msg_string);
        char *s = whiteboard_get_from(wbd, msg_string);
        response.append(s);
        free(s);
        response.append("\"}");
    } 
    write(*fd, response.c_str(), sizeof(char) * response.length()); 
    close(*fd);
}

void handle_get_request_html(int *fd, gu_simple_whiteboard_descriptor *wbd, struct header_info_s *header)
{
    std::string response = std::string(""
"<!DOCTYPE html><html><head><title>guwhiteboardwebposter</title>"
"<style>body { background-color: #FFFFFF }"
"</style></head>"
"<body onload=\"whiteboardMonitor();\">\r\n");

    if(strcmp(header->url, "/") == 0 || strlen(header->url) == 0)
    {   //URL == /           - all messages, array
        response.append("<script>\r\n"
		"function whiteboardMonitor() {\r\n"
		"	setInterval(function(){\r\n"
        "		checkboxes = document.getElementsByName('wbmonitor');\r\n"
        "		for(var i=0, n=checkboxes.length;i<n;i++) {\r\n"
        "    		if(checkboxes[i].checked)\r\n"
        "    			handleClick(checkboxes[i]);\r\n"
        "		}\r\n"
		"	}, 1000);\r\n"
		"}\r\n"
        "function handleClick(cb) {\r\n"
        "	var xhttp = new XMLHttpRequest();\r\n"
  		"	xhttp.onreadystatechange = function() {\r\n"
    	"		if (this.readyState == 4 && this.status == 200) {\r\n"
		"			var arr = JSON.parse(this.responseText);\r\n"
        "    		document.getElementById(cb.id.substring(4, cb.id.length)).innerHTML = arr.value;\r\n"
    	"		}\r\n"
  		"	};\r\n"
  		"	xhttp.open(\"GET\", \"/json/\" + cb.id.substring(4, cb.id.length), true);\r\n"
  		"	xhttp.send();\r\n"
        "}\r\n"
        "function toggleAll(source) {\r\n"
            "checkboxes = document.getElementsByName('wbmonitor');\r\n"
            "for(var i=0, n=checkboxes.length;i<n;i++) {\r\n"
            "    checkboxes[i].checked = source.checked;\r\n"
            "    handleClick(checkboxes[i]);\r\n"
            "}\r\n"
        "}\r\n"
        "</script>\r\n");
        response.append("<h1>Whiteboard Types</h1>\r\n");
        response.append("<table>\r\n");

        response.append("<tr>\r\n");
        response.append("<td>\r\n");
        response.append("<input type=\"checkbox\" onClick=\"toggleAll(this)\" />");
        response.append("</td>\r\n");
        response.append("<td>\r\n");
        response.append("Toggle All");
        response.append("</td>\r\n");
        response.append("</tr>\r\n");

        for(int i = 0; i < GSW_NUM_TYPES_DEFINED; i++)
        {
            const char *msg_name = guWhiteboard::WBTypes_stringValues[i];
            char *msg_value = whiteboard_get_from(wbd, msg_name);
            response.append("<tr>\r\n");
            response.append("<td>\r\n");
            std::string id; 
                id.append("id='chk_"); 
                id.append(msg_name); 
                id.append("'");
            response.append("<input type='checkbox' ");
                response.append(id); 
                response.append(" name='wbmonitor' onclick='handleClick(this);'>");
            response.append("</td>\r\n");
            response.append("<td>\r\n");
            if(!(strcmp(msg_value, "##unsupported##") == 0))
            {
                response.append("<a href=\"/html/");
                response.append(msg_name);
                response.append("\">");
                response.append(msg_name);
                response.append("</a>\r\n");

                std::string td_id; 
                    td_id.append("id='"); 
                    td_id.append(msg_name); 
                    td_id.append("'");
                response.append("<td "); 
                    response.append(td_id); 
                    response.append(">\r\n");
                response.append(msg_value);
                response.append("</td>\r\n");
            }
            else
                response.append(msg_name);
            free(msg_value);
            response.append("</td>\r\n");
            response.append("</tr>\r\n");
        }

        response.append("</table>\r\n");
    } 
    else 
    {   //URL == /$(msg) 
        char msg_name[100]; msg_name[0] = '\0';
        sscanf(header->url, "/%s", msg_name);
        response.append("<script>\r\n"
		"function submitJSON(e) {\r\n"
        "   var value = encodeURIComponent(document.getElementById('textarea').value);\r\n"
        "   var params = \"value=\" + value;"
		"	e.preventDefault();\r\n"
		"   var xhttp = new XMLHttpRequest();\r\n"
  		"	xhttp.onreadystatechange = function() {\r\n"
    	"		if (this.readyState == 4 && this.status == 200) {\r\n"
		"			var arr = JSON.parse(this.responseText);\r\n"
        "    		document.getElementById('textarea').innerHTML = arr.value;\r\n"
    	"		}\r\n"
  		"	};\r\n"
  		"	xhttp.open(\"POST\", \"/json/"); response.append(msg_name); response.append("\", true);\r\n"
        "   xhttp.setRequestHeader(\"Content-type\", \"application/x-www-form-urlencoded\");"
  		"	\r\n"
  		"	xhttp.send(params);\r\n"
        "   return false;\r\n"
		"\r\n"
		"}\r\n"
		"</script>\r\n");
        char *msg_value = whiteboard_get_from(wbd, msg_name);
        response.append("<h1>");
        response.append(msg_name);
        response.append("</h1>\r\n"
		"<form id='form' method='POST' >\r\n"
		"<div style='width:50%;'>\r\n"
  		"	<textarea id='textarea' style='width:100%;' rows='20'>");
        response.append(msg_value);
		response.append("</textarea>"
		"</div>\r\n"
		"<div style='width:50%; text-align:right;' >\r\n"
  		"	<input type='submit' />\r\n"
		"</div>\r\n"
		"	</form>\r\n"
		"	<script>document.getElementById('form').addEventListener('submit', submitJSON)</script>\r\n");
        free(msg_value);
    } 
    response.append("</body></html>\r\n");

    generate_response(fd, HTTP_V1_1, _200_OK, Text_HTML, response);
}

void usage_page(int *fd, gu_simple_whiteboard_descriptor *wbd, struct header_info_s *header, char *body)
{
    char usage_response[] = ""
"<!DOCTYPE html><html><head><title>guwhiteboardwebposter</title>"
"<style>body { background-color: #FFFFFF }"
"h1 { font-size:4cm; text-align: center; color: black;"
" text-shadow: 0 0 2mm red}</style></head>"
"<body><h1>Usage Page</h1>\r\n"
"<h2>TODO: Fill in</h2>"
"</body></html>\r\n";

    generate_response(fd, HTTP_V1_1, _200_OK, Text_HTML, usage_response);
}

int detect_content_length(char *header)
{
    std::string header_s = std::string(header);
    std::string field = std::string("Content-Length: ");
    int index = header_s.find(field);
    if(index == std::string::npos)
        return -1;
    int field_data_pos = index + field.length();
    int end_of_field_data_pos = header_s.substr(field_data_pos, header_s.length()).find("\r\n");
    std::string field_value = header_s.substr(field_data_pos, end_of_field_data_pos);
    int i = atoi(field_value.c_str());
    return i;
}

static void signal_handler()
{

}



socket_descriptor *init_socket(int port)
{
    socket_descriptor *sd = (socket_descriptor *)calloc(sizeof(socket_descriptor), 1);
    assert(sd);

    //modified from: http://beej.us/guide/bgnet/examples/listener.c
    struct addrinfo hints, *servinfo, *p;
    int rv;
  
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; 
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    char port_s[4];
    snprintf(&port_s[0], 5, "%d", port);

    assert ((rv = getaddrinfo(NULL, &port_s[0], &hints, &servinfo)) == 0); 

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((sd->socket = socket(p->ai_family, p->ai_socktype, 0)) == -1) 
        {
            perror("listener: socket");
            continue;
        }

        int on = 1;

        assert(!(setsockopt(sd->socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0));

        assert(!(setsockopt(sd->socket, SOL_SOCKET, SO_TIMESTAMP, &on, sizeof(on)) < 0));

        if (bind(sd->socket, p->ai_addr, p->ai_addrlen) == -1) {
            close(sd->socket);
            perror("listener: bind");
            continue;
        }
        break;
    }

    assert (p != NULL);

    freeaddrinfo(servinfo);

    return sd;
}

void close_socket(socket_descriptor *sd)
{
    if(sd)
    {
        if(sd->socket)
            close(sd->socket);
        free(sd);
    }
}

