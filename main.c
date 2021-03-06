#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "hiredis/hiredis.h"
#include "hiredis/async.h"
#include "hiredis/adapters/libevent.h"
#include <json-c/json.h>
#include <pjsua-lib/pjsua.h>

#include <pthread.h>
#include <libconfig.h>

int init_pjsua();
int add_account(char* sip_user,char* sip_domiain,char* sip_passwd);
int send_sip_im(int acc_id,char*to_uri,char*msg);
// pjsua callbacks
static void on_pager(pjsua_call_id call_id, const pj_str_t *from,
        const pj_str_t *to, const pj_str_t *contact,
        const pj_str_t *mime_type, const pj_str_t *text);
static void on_pager_status(pjsua_call_id call_id, const pj_str_t *to,
        const pj_str_t *body, void *user_data,
        pjsip_status_code status, const pj_str_t *reason);













#include <stdio.h>
#define THIS_FILE "main.c"



/* Display error and exit application */
static void error_exit(const char *title, pj_status_t status)
{
    pjsua_perror(THIS_FILE, title, status);
    pjsua_destroy();
    exit(1);
}






//must register external thread to pjsua
void register_thread(){


    pj_status_t status;

    pj_thread_desc aPJThreadDesc;
    if (!pj_thread_is_registered()) {
        pj_thread_t *pjThread;
        status = pj_thread_register(NULL, aPJThreadDesc, &pjThread);
        if (status != PJ_SUCCESS)
            error_exit("Error registering thread at PJSUA", status);
    }



}


int init_pjsua(){


    pj_status_t status;

    /* Create pjsua first! */
    status = pjsua_create();
    if (status != PJ_SUCCESS) error_exit("Error in pjsua_create()", status);

    pjsua_config cfg;

    /* Init pjsua */
    {

        pjsua_logging_config log_cfg;

        pjsua_config_default(&cfg);
//        cfg.cb.on_pager = &on_pager;
        cfg.cb.on_pager_status = &on_pager_status;

        pjsua_logging_config_default(&log_cfg);
        log_cfg.console_level = 4;

        status = pjsua_init(&cfg, &log_cfg, NULL);
        if (status != PJ_SUCCESS) error_exit("Error in pjsua_init()", status);
    }

    /* Add UDP transport. */
    {
        pjsua_transport_config cfg;

        pjsua_transport_config_default(&cfg);
        status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, NULL);
        if (status != PJ_SUCCESS) error_exit("Error creating transport", status);
    }


    /* Add TCP (TLS) transport. */
    {
        pjsua_transport_config tcp_cfg;

        pjsua_transport_config_default(&tcp_cfg);
        tcp_cfg.tls_setting.verify_client = PJ_FALSE;
        tcp_cfg.tls_setting.require_client_cert = PJ_FALSE;
        tcp_cfg.port = 0;
        status = pjsua_transport_create(PJSIP_TRANSPORT_TLS, &tcp_cfg, NULL);
        if (status != PJ_SUCCESS) error_exit("Error creating transport", status);
    }


    /* Initialization is done, now start pjsua */
    status = pjsua_start();
    if (status != PJ_SUCCESS) error_exit("Error starting pjsua", status);

    return status;
}


int add_account(char* sip_user,char* sip_domiain,char* sip_passwd){


    /* Register to SIP server by creating SIP account. */
    pj_status_t status;
    pjsua_acc_id acc_id;
    pjsua_acc_config cfg;
    char id[80], registrar[80];

    pjsua_acc_config_default(&cfg);


    sprintf(id,"sip:%s@%s",sip_user,sip_domiain);
    sprintf(registrar, "sip:%s;transport=tls",sip_domiain);

    cfg.id = pj_str(id);
    cfg.reg_uri = pj_str(registrar);

    cfg.cred_count = 1;
    cfg.cred_info[0].realm = pj_str("*");
    cfg.cred_info[0].scheme = pj_str("digest");
    cfg.cred_info[0].username = pj_str(sip_user);
    cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    cfg.cred_info[0].data = pj_str(sip_passwd);

    status = pjsua_acc_add(&cfg, PJ_TRUE, &acc_id);
    if (status != PJ_SUCCESS) error_exit("Error adding account", status);


    return acc_id;




}


int send_sip_im(int acc_id,char*to,char*content){

    pj_status_t status;
    pj_str_t to_str = pj_str(to);
    pj_str_t content_str = pj_str(content);
    status=  pjsua_im_send(acc_id, &to_str, NULL, &content_str, NULL, NULL);

    return status;

}







static redisAsyncContext *out_conn;
static redisContext *in_conn;
void onInMessage(redisAsyncContext *c, void *reply, void *privdata);
void onOutMessage(redisAsyncContext *c, void *reply, void *privdata);




// pjsua callbacks
static void on_pager(pjsua_call_id call_id, const pj_str_t *from,
        const pj_str_t *to, const pj_str_t *contact,
        const pj_str_t *mime_type, const pj_str_t *text){



//    construct raw message for sip_in_worker;
    json_object * sidkiq_raw_payload = json_object_new_object();
    json_object_object_add(sidkiq_raw_payload, "retry", json_object_new_string("true"));
    json_object_object_add(sidkiq_raw_payload, "queue", json_object_new_string("default"));
    json_object_object_add(sidkiq_raw_payload, "class", json_object_new_string("SipInWorker"));

//  arguments to sip_in_worker's perform methods
    json_object * sip_in_worker_args= json_object_new_array();
    json_object_array_add(sip_in_worker_args,json_object_new_string(from->ptr));
    json_object_array_add(sip_in_worker_args,json_object_new_string(text->ptr));

    json_object_object_add(sidkiq_raw_payload, "args", sip_in_worker_args);

    redisCommand(in_conn,"lpush queue:default %s",json_object_to_json_string(sidkiq_raw_payload));

}

static void on_pager_status(pjsua_call_id call_id, const pj_str_t *to,
        const pj_str_t *body, void *user_data,
        pjsip_status_code status, const pj_str_t *reason){

    printf("%s",__func__);


}



void onInMessage(redisAsyncContext *c, void *reply, void *privdata) {
    redisReply *r = reply;
    if (reply == NULL) return;

}



// when recevie message of 'valta_sip_out' channel from redis, use sip to send it out
void onOutMessage(redisAsyncContext *c, void *reply, void *privdata) {
    redisReply *r = reply;
    if (reply == NULL) return;

    if (r->type == REDIS_REPLY_ARRAY) {



//        copy a redisRelay->str to a NULL terminated string using sprintf
        int len =r->element[r->elements-1]->len;
        if(len>0){
            char source[len];
            sprintf(source,"%s",r->element[r->elements-1]->str);


//        extract sip address and payload
            json_object * jobj = json_tokener_parse(source);
            enum json_type type  =  json_object_get_type(jobj);

            if (type==json_type_array) {

                if (json_object_array_length(jobj)==2){

                    struct json_object* to,*payload;

                    to = json_object_array_get_idx(jobj, 0);
                    payload = json_object_array_get_idx(jobj, 1);
#ifdef debug
                printf("%s and %s",json_object_get_string(to),json_object_get_string(payload));
#endif

                    send_sip_im(0, json_object_get_string(to),json_object_get_string(payload));


                }

            }
        }
    }
}




int main (int argc, char **argv) {


//    read config file

    config_t cfg;
    config_setting_t *setting;
    const char *sip_addr;
    const char *sip_port;
    const char *sip_acc;
    const char *sip_pass;

    const char *redis_addr;
    const int *redis_port;

    config_init(&cfg);

    /* Read the file. If there is an error, report it and exit. */
    if(! config_read_file(&cfg, "redis_sip_worker.cfg"))
    {
        fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
                config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        return(EXIT_FAILURE);
    }

    /* Get the SIP Config. */
    if(!config_lookup_string(&cfg,"sip.address",&sip_addr)){
        fprintf(stderr, "No SIP Server Address in configuration file.\n");
        return(EXIT_FAILURE);
    }

    if(!config_lookup_string(&cfg,"sip.port",&sip_port)){
        fprintf(stderr, "No SIP Server Port in configuration file.\n");
        return(EXIT_FAILURE);
    }

    if(!config_lookup_string(&cfg,"sip.account",&sip_acc)){
        fprintf(stderr, "No SIP Server Account in configuration file.\n");
        return(EXIT_FAILURE);
    }
    if(!config_lookup_string(&cfg,"sip.password",&sip_pass)){
        fprintf(stderr, "No SIP Server Password in configuration file.\n");
        return(EXIT_FAILURE);
    }


    /* Get Redis Config */
    if(!config_lookup_string(&cfg,"redis.address",&redis_addr)){
        fprintf(stderr, "No Reids Server Address in configuration file.\n");
        return(EXIT_FAILURE);
    }

    if(!config_lookup_int(&cfg,"redis.port",&redis_port)){
        fprintf(stderr, "No Redis Server Port in configuration file.\n");
        return(EXIT_FAILURE);
    }




//   sip part
    init_pjsua();
//    add account
    add_account(sip_acc,sip_addr,sip_pass);

//    redis part
    signal(SIGPIPE, SIG_IGN);
    struct event_base *base = event_base_new();

//    set up connection for sip out
//    out_conn = redisAsyncConnect("223.255.138.226", 6379);
    out_conn = redisAsyncConnect(redis_addr, redis_port);
    if (out_conn->err) {
        printf("error: %s\n", out_conn->errstr);
        return 1;
    }
    redisLibeventAttach(out_conn, base);
    redisAsyncCommand(out_conn, onOutMessage, NULL, "SUBSCRIBE valta_sip_out");


    //    set up connection for sip in
    struct timeval timeout = { 1, 500000 };
//    in_conn = redisConnectWithTimeout("223.255.138.226", 6379,timeout);
    in_conn = redisConnectWithTimeout(redis_addr,redis_port,timeout);
    if (in_conn->err) {
        printf("error: %s\n", in_conn->errstr);
        return 1;
    }

//       loop out_conn
    event_base_dispatch(base);
    return 0;
}
