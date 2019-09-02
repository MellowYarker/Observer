#include "reader.h"
#include <libwebsockets.h>
#include <string.h>


struct lws_context *context;
struct lws *client_wsi;
static int interrupted, zero_length_ping, port = 443,
	   ssl_connection = LCCSCF_USE_SSL;
static const char *server_address = "ws.blockchain.info", *pro = "wss";
int subscribed = 0; // becomes 1 when we've subscribed to blockchain service
char *transaction_buf;
int transaction_size;
int partial_write = 0;
int discard = 0;

static int connect_client(void) {
	struct lws_client_connect_info i;

	memset(&i, 0, sizeof(i));
    // our url is wss://ws.blockchain.info/inv
	i.context = context;
	i.port = port;
	i.address = server_address;
	i.path = "/inv";
	i.host = i.address;
	i.origin = i.address;
	i.ssl_connection = ssl_connection; // we will use ssl
	i.protocol = pro; // remote ws protocol
	i.local_protocol_name = "lws-transaction"; // the protocol we use locally
	i.pwsi = &client_wsi;

	return !lws_client_connect_via_info(&i);
}


static int
callback_tx_client(struct lws *wsi, enum lws_callback_reasons reason,
			void *user, void *in, size_t len) {
	int n;

	switch (reason) {

        case LWS_CALLBACK_PROTOCOL_INIT:
            goto try;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            lwsl_err("CLIENT_CONNECTION_ERROR: %s\n",
                in ? (char *)in : "(null)");
            client_wsi = NULL;
            lws_timed_callback_vh_protocol(lws_get_vhost(wsi),
                    lws_get_protocol(wsi), LWS_CALLBACK_USER, 1);
            break;

        /* --- client callbacks --- */

        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            lwsl_user("%s: established\n", __func__);
            lws_set_timer_usecs(wsi, 5 * LWS_USEC_PER_SEC);
            // We should only be writing in LWS_CALLBACK_CLIENT_WRITEABLE.
            // However, I ran into a scenario where the server never became
            // writeable, so we have to try here and in writeable as well.
            lwsl_user("Waiting for server to be writeable...\n");
            if (!subscribed) {
                uint8_t msg[LWS_PRE + 125];
                int m;
                n = 0;
                n = lws_snprintf((char *)msg + LWS_PRE, 125,
                                "{\"op\": \"unconfirmed_sub\"}");
                lwsl_user("Subscribing to unconfirmed transaction stream.\n");

                m = lws_write(wsi, msg + LWS_PRE, n, LWS_WRITE_TEXT);

                if (m < n) {
                    lwsl_err("Failed to send subscription: %d\n", m);
                    return -1;
                }
                subscribed = 1;
            }
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            // this is where we send {"op": "unconfirmed_sub"}
            // ONLY WRITE if we haven't subscribed yet
            if (!subscribed) {
                uint8_t msg[LWS_PRE + 125];
                int m;
                n = 0;
                n = lws_snprintf((char *)msg + LWS_PRE, 125, 
                                "{\"op\": \"unconfirmed_sub\"}");
                lwsl_user("Subscribing to unconfirmed transaction stream.\n");

                m = lws_write(wsi, msg + LWS_PRE, n, LWS_WRITE_TEXT);
                
                if (m < n) {
                    lwsl_err("Failed to send subscription: %d\n", m);
                    return -1;
                }
                subscribed = 1;
            }
            break;

        case LWS_CALLBACK_WS_CLIENT_DROP_PROTOCOL:
            client_wsi = NULL;
            lws_timed_callback_vh_protocol(lws_get_vhost(wsi),
                            lws_get_protocol(wsi),
                            LWS_CALLBACK_USER, 1);
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            lwsl_user("Received New Transaction!\n");
            if (transaction_buf != NULL) {
                free(transaction_buf);
                transaction_buf = NULL;
            }
            transaction_buf = malloc(len + 1);

            if (transaction_buf == NULL) {
                perror("malloc");
                return -1;
            } else {
                memset(transaction_buf, '\0', len + 1);
            }

            strcpy(transaction_buf, in);
            transaction_buf[len] = '\0';
            transaction_size = len;

            if (partial_write) {
                // discard previous read if we got an out of order message
                if (strstr(transaction_buf, "\"op\"") != NULL) {
                    discard = 1; // discard previous read
                }
            }

            // a partial write
            if (transaction_buf[len - 1] != '}' || transaction_size == 4082) {
                partial_write = 1;
            } else {
                partial_write = 0;
            }
            break;

        /* rate-limited client connect retries */

        case LWS_CALLBACK_USER:
            lwsl_notice("%s: LWS_CALLBACK_USER\n", __func__);
try:
		if (connect_client())
			lws_timed_callback_vh_protocol(lws_get_vhost(wsi),
						       lws_get_protocol(wsi),
						       LWS_CALLBACK_USER, 1);
		break;

	default:
        printf("Waiting for something to happen.\n");
		break;
	}

	return lws_callback_http_dummy(wsi, reason, user, in, len);
}

const struct lws_protocols protocols[] = {
	{
		"lws-transaction",
		callback_tx_client,
		0,
		0,
	},
	{ NULL, NULL, 0, 0 }
};
