#guwhiteboardwebposter
*A web server for interacting with the Whiteboard over HTTP*

---

Default port is: 4242, configurable with -p
Whiteboard can be specified with -w or a default is used.

Current supported calls:
    GET html
    GET json
    POST / PATCH json - for the sake of this project POST and PATCH are currently identical

Web browser calls to 'hostname:4242/' or simply 'hostname:4242' will display a list of all Whiteboard messages. 
    Messages that have string parsers will be displayed with an HTML link to their individual pages.
    Messages also have a tickbox beside them, ticking this automatically updates the message value once a second.
    Tick box at the top of the page will tick or untick all of the Whiteboard messages.

Messages can be viewed individually and altered. URL format is 'hostname:4242/Speech' for the 'Speech' message etc..
    Message values can be changed in the textarea provided, click submit to have it Posted to the Whiteboard.
    If the new value has been passed to the Message parser successfully, the submit button will turn green briefly.
        This Does NOT mean that the message was Parsed correctly, just that it was received by the Parser.
    If there was a problem, the submit button will turn red briefly. Look at your browsers Console or Error Log for the HTTP status that was returned.

JSON format:
    Accepted POST format is identical to the format returned by GET requests.

NOTES:
    If you issue a request, the HTTP Header field 'Accept' determines what you'll get back (HTML or JSON)
        Accepted values are:
            text/html
            application/vnd.api+json
    This is intended to be based around 'http://jsonapi.org/format/1.1/'. It is Not fully to spec.
        One of the main issues is the JSON format. Expect 'breaking' format changes.

