/* config_parse.c - parse YAML hostclass config files
 *
 * Copyright (c) 2013, Groupon, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of GROUPON nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <stdlib.h>
#ifndef HAVE_STRLCPY
    #include "strlcpy.h"
#endif
#ifdef HAVE_STRING_H
    #include <string.h>
#endif
#include <yaml.h>
#include "config_parse.h"
#include "log.h"

#define MAX_ERROR_LENGTH 8192
#define MAX_KEY_SIZE 1024

static void log_parse_error(const yaml_parser_t *parser, const char *type) {
    char msg[MAX_ERROR_LENGTH];
    int length = 0;

    length += snprintf(
        msg,
        MAX_ERROR_LENGTH,
        "YAML parse error in %s file",
        type
    );

    if(parser->problem) {
        length += snprintf(
            msg + length,
            MAX_ERROR_LENGTH - length,
            ": %s",
            parser->problem
         );

        if(parser->problem_mark.line || parser->problem_mark.column) {
            length += snprintf(
                msg + length,
                MAX_ERROR_LENGTH - length,
                ", line: %lu, column %lu",
                parser->problem_mark.line + 1,
                parser->problem_mark.column + 1
             );
        }
    }
    length += snprintf(
        msg + length,
        MAX_ERROR_LENGTH - length,
        "."
     );

    msg[length++] = '\0';
    log_error(msg);
}

int parse_host_config(host_config_t *host_config, FILE *host_file) {
    yaml_parser_t parser;
    yaml_event_t event;
    int ok = 1;
    int done = 0;

    enum {
        STATE_START,
        STATE_HOSTCLASS_TAG
    } parse_state = STATE_START;

    memset(&parser, 0, sizeof(parser));
    memset(&event, 0, sizeof(event));
    memset(host_config, 0, sizeof(host_config_t));

    ok = yaml_parser_initialize(&parser);
    if(!ok) {
        log_error("Fatal error: could not initialize yaml parser.");
        return 0;
    }

    yaml_parser_set_input_file(&parser, host_file);

    while(!done) {
        /* get next parser event */
        if(!yaml_parser_parse(&parser, &event)) {
            goto parser_error;
        }

        /* handle event */
        switch(event.type) {
            case YAML_STREAM_START_EVENT:
                break;
            case YAML_STREAM_END_EVENT:
                done = 1;
                break;
            case YAML_DOCUMENT_START_EVENT:
                break;
            case YAML_DOCUMENT_END_EVENT:
                break;
            case YAML_SCALAR_EVENT:
                switch(parse_state) {
                    case STATE_START:
                        if (!strcmp("hostclass",
                                    (char *)event.data.scalar.value)) {
                            parse_state = STATE_HOSTCLASS_TAG;
                        }
                        break;
                    case STATE_HOSTCLASS_TAG:
                        strlcpy((char *)host_config->hostclass_tag,
                                (char *)event.data.scalar.value,
                                MAX_VALUE_SIZE);
                        parse_state = STATE_START;
                        break;
                }
                break;
            case YAML_SEQUENCE_START_EVENT:
                break;
            case YAML_SEQUENCE_END_EVENT:
                break;
            case YAML_MAPPING_START_EVENT:
                break;
            case YAML_MAPPING_END_EVENT:
                break;
            case YAML_NO_EVENT:
                break;
            case YAML_ALIAS_EVENT:
                break;
        }

        yaml_event_delete(&event);
    }

    goto done;

 parser_error:
    log_parse_error(&parser, "host");

    memset(host_config, 0, sizeof(host_config_t));
    ok = 0;
 done:
    yaml_parser_delete(&parser);
    return ok;
}

int parse_hostclass_config(hostclass_config_t *hostclass_config, FILE *hostclass_file) {
    yaml_parser_t parser;
    yaml_event_t event;
    int ok = 1;
    int done = 0;
    unsigned char last_package_group[MAX_VALUE_SIZE];
    package_spec_t *package_spec = NULL;
    package_spec_t *last_package_spec = NULL;
    os_image_spec_t *os_image_spec = NULL;
    os_image_spec_t *last_os_image_spec = NULL;

    enum {
        STATE_START,
        STATE_IMAGES_HARDWARE,
        STATE_IMAGES_IMAGE,
        STATE_PACKAGES_GROUP,
        STATE_PACKAGES_NAME
    } parse_state = STATE_START;

    memset(&parser, 0, sizeof(parser));
    memset(&event, 0, sizeof(event));
    memset(hostclass_config, 0, sizeof(hostclass_config_t));

    ok = yaml_parser_initialize(&parser);
    if(!ok) {
        log_error("Fatal error: could not initialize yaml parser.");
        return 0;
    }

    yaml_parser_set_input_file(&parser, hostclass_file);

    while(!done) {
        /* get next parser event */
        if(!yaml_parser_parse(&parser, &event)) {
            goto parser_error;
        }

        /* handle event */
        switch(event.type) {
            case YAML_STREAM_START_EVENT:
                break;
            case YAML_STREAM_END_EVENT:
                done = 1;
                break;
            case YAML_DOCUMENT_START_EVENT:
                break;
            case YAML_DOCUMENT_END_EVENT:
                break;
            case YAML_SCALAR_EVENT:

                switch(parse_state) {
                    case STATE_START:
                        if(!strcmp("images",
                                   (char *)event.data.scalar.value)) {
                            parse_state = STATE_IMAGES_HARDWARE;
                        } else if (!strcmp("packages",
                                           (char *)event.data.scalar.value)) {
                            parse_state = STATE_PACKAGES_GROUP;
                        }
                        break;

                    case STATE_IMAGES_HARDWARE:
                        os_image_spec =
                            (os_image_spec_t *)malloc(sizeof(os_image_spec_t));
                        if(!os_image_spec) {
                            log_error("Fatal error: out of memory.");
                            goto error;
                        }
                        memset(os_image_spec, 0, sizeof(os_image_spec_t));
                        strlcpy((char *)os_image_spec->hardware_tag,
                                (char *)event.data.scalar.value,
                                MAX_VALUE_SIZE);
                        parse_state = STATE_IMAGES_IMAGE;
                        break;

                    case STATE_IMAGES_IMAGE:
                        strlcpy((char *)os_image_spec->image_name,
                                (char *)event.data.scalar.value,
                                MAX_VALUE_SIZE);
                        if(!last_os_image_spec) {
                            hostclass_config->os_image_list =
                                os_image_spec;
                        } else {
                            last_os_image_spec->next =
                                os_image_spec;
                        }
                        last_os_image_spec = os_image_spec;
                        os_image_spec = NULL;
                        parse_state = STATE_IMAGES_HARDWARE;
                        break;
                    case STATE_PACKAGES_GROUP:
                        strlcpy((char *)last_package_group,
                                (char *)event.data.scalar.value,
                                MAX_VALUE_SIZE);
                        parse_state = STATE_PACKAGES_NAME;
                        break;
                    case STATE_PACKAGES_NAME:
                        package_spec =
                            (package_spec_t *)malloc(sizeof(package_spec_t));
                        if(!package_spec) {
                            log_error("Fatal error: out of memory.");
                            goto error;
                        }
                        memset(package_spec, 0, sizeof(package_spec_t));
                        strlcpy((char *)package_spec->group,
                                (char *)last_package_group,
                                MAX_VALUE_SIZE);
                        strlcpy((char *)package_spec->package_name,
                                (char *)event.data.scalar.value,
                                MAX_VALUE_SIZE);
                        if(!last_package_spec) {
                            hostclass_config->package_list =
                                package_spec;
                        } else {
                            last_package_spec->next =
                                package_spec;
                        }
                        last_package_spec = package_spec;
                        if(!hostclass_config->has_failsafe &&
                           !strcmp((char *)package_spec->group, FAILSAFE_GROUP_NAME)) {
                            hostclass_config->has_failsafe = 1;
                        }
                        package_spec = NULL;
                        break;
                }
                break;
            case YAML_SEQUENCE_START_EVENT:
                break;
            case YAML_SEQUENCE_END_EVENT:
                switch(parse_state) {
                    case STATE_START:
                    case STATE_IMAGES_HARDWARE:
                    case STATE_IMAGES_IMAGE:
                    case STATE_PACKAGES_GROUP:
                        parse_state = STATE_START;
                        break;
                    case STATE_PACKAGES_NAME:
                        parse_state = STATE_PACKAGES_GROUP;
                        break;
                }
                break;
            case YAML_MAPPING_START_EVENT:
                break;
            case YAML_MAPPING_END_EVENT:
                switch(parse_state) {
                    case STATE_START:
                    case STATE_IMAGES_HARDWARE:
                    case STATE_IMAGES_IMAGE:
                    case STATE_PACKAGES_GROUP:
                        parse_state = STATE_START;
                        break;
                    case STATE_PACKAGES_NAME:
                        parse_state = STATE_PACKAGES_GROUP;
                        break;
                }
                break;
            case YAML_NO_EVENT:
                break;
            case YAML_ALIAS_EVENT:
                break;
        }

        yaml_event_delete(&event);
    }

    goto done;

 parser_error:
    log_parse_error(&parser, "hostclass");
 error:
    free_hostclass_config(hostclass_config);
    memset(hostclass_config, 0, sizeof(hostclass_config_t));
    ok = 0;
 done:
    if(os_image_spec) {
        free(os_image_spec);
    }
    if(package_spec) {
        free(package_spec);
    }
    yaml_parser_delete(&parser);
    return ok;
}

void free_hostclass_config(hostclass_config_t *hostclass_config) {
    os_image_spec_t *os_image_spec, *os_image_spec_temp;
    package_spec_t *package_spec, *package_spec_temp;

    os_image_spec = hostclass_config->os_image_list;
    while(os_image_spec) {
        os_image_spec_temp = os_image_spec;
        os_image_spec = os_image_spec->next;
        free(os_image_spec_temp);
    }
    hostclass_config->os_image_list = NULL;

    package_spec = hostclass_config->package_list;
    while(package_spec) {
        package_spec_temp = package_spec;
        package_spec = package_spec->next;
        free(package_spec_temp);
    }
    hostclass_config->package_list = NULL;
}
