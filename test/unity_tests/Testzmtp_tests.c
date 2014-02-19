#include "unity.h"
#include "pico_zmtp.c"
#include "Mockpico_socket.h"
#include <stdint.h>
#include "Mockpico_vector.h"
#include "Mockpico_mm.h"
#include "Mockpico_tree.h"

volatile pico_err_t pico_err;
#define BLACK 1
struct pico_tree_node LEAF = {
    NULL, /* key */
    &LEAF, &LEAF, &LEAF, /* parent, left,right */
    BLACK, /* color */
};


struct pico_socket* e_pico_s;
struct pico_socket* e_new_pico_s;
struct zmtp_socket* e_zmtp_s;
void* e_buf;
int e_len;
void* read_data;
void* e_data;
uint16_t e_ev;
size_t zmq_cb_counter;

void setUp(void)
{
}

void tearDown(void)
{
}

/* parameters to set: e_pico_s, e_buf, e_len e_data*/
int pico_socket_write_cb(struct pico_socket* a_pico_s, const void* a_buf, int a_len, int numCalls)
{
    IGNORE_PARAMETER(numCalls);
    TEST_ASSERT_EQUAL(e_pico_s, a_pico_s);
    TEST_ASSERT_EQUAL(e_buf, a_buf);
    TEST_ASSERT_EQUAL_INT(e_len, a_len);
    TEST_ASSERT_EQUAL_MEMORY(e_data, a_buf, e_len);

    return e_len;
}

/* alternative function for pico_socket_write_cb to prevent expect_greeting of changing e_pico_s */
struct pico_socket* e_pico_s_greeting;
int e_len_greeting;
void* e_data_greeting;
int pico_socket_write_greeting_cb(struct pico_socket* a_pico_s, const void* a_buf, int a_len, int numCalls)
{
    IGNORE_PARAMETER(numCalls);
    TEST_ASSERT_EQUAL_PTR(e_pico_s_greeting, a_pico_s);
    TEST_ASSERT_EQUAL_INT(e_len_greeting, a_len);
    TEST_ASSERT_EQUAL_MEMORY(e_data_greeting, a_buf, e_len);

    free(e_data_greeting);

    return e_len_greeting;
}

void expect_greeting(struct pico_socket *pico_s, uint8_t type)
{
    uint8_t greeting[14] = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x01, type, 0x00, 0x00};

    e_len_greeting = 14;
    e_pico_s_greeting = pico_s;

    e_data_greeting = calloc(1, (size_t)e_len_greeting);
    memcpy(e_data_greeting, greeting, (size_t)e_len_greeting);
    pico_socket_write_StubWithCallback(&pico_socket_write_greeting_cb);
}

void map_tcp_to_zmtp(struct zmtp_socket *zmtp_s)
{
    pico_tree_findKey_IgnoreAndReturn(zmtp_s);
}

void zmq_cb_mock(uint16_t ev, struct zmtp_socket* zmtp_s)
{
    TEST_ASSERT_EQUAL_UINT16(e_ev, ev);
    TEST_ASSERT_EQUAL(e_zmtp_s, zmtp_s);
    zmq_cb_counter ++;
    e_ev = 0;
    e_zmtp_s = NULL;
}

/* compares the argument values and writes read_data with lenght e_len into a_buf */
int pico_socket_read_cb(struct pico_socket* a_pico_s, void* a_buf, int a_len, int numCalls)
{
    IGNORE_PARAMETER(numCalls);
    TEST_ASSERT_EQUAL(e_pico_s, a_pico_s);
    TEST_ASSERT_EQUAL(e_buf, a_buf);
    TEST_ASSERT_EQUAL(e_len, a_len);
    memcpy(a_buf, read_data, (size_t)e_len);
    return e_len;
}

struct pico_socket* pico_socket_accept_cb(struct pico_socket* pico_s, struct pico_ip4* orig, uint16_t* port)
{
    IGNORE_PARAMETER(orig);
    IGNORE_PARAMETER(port);
    TEST_ASSERT_EQUAL_PTR(e_pico_s, pico_s);
    return e_new_pico_s;
}

void test_check_signature(void)
{
    struct zmtp_socket *zmtp_s;
    struct pico_socket *pico_s;
    void* a_buf;
    uint8_t signature[10] = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f};

    pico_s = calloc(1, sizeof(struct pico_socket));
    zmtp_s = calloc(1, sizeof(struct zmtp_socket));
    zmtp_s->sock = pico_s;

    e_len = 10;
    a_buf = calloc(1, (size_t)e_len);
    e_buf = a_buf;
    e_pico_s = pico_s;
    read_data = calloc(1, (size_t)e_len);

    /* Good signatures */
    zmtp_s->state = ZMTP_ST_SND_GREETING;
    memcpy(read_data, signature, (size_t)e_len);
    pico_mem_zalloc_ExpectAndReturn((size_t)e_len, a_buf);
    pico_mem_free_Expect(e_buf);
    pico_socket_read_StubWithCallback(&pico_socket_read_cb);
    TEST_ASSERT_EQUAL(0 ,check_signature(zmtp_s));
    TEST_ASSERT_EQUAL(ZMTP_ST_RCVD_SIGNATURE, zmtp_s->state);

    signature[8] = 0x01;
    zmtp_s->state = ZMTP_ST_SND_GREETING;
    memcpy(read_data, signature, (size_t)e_len);
    pico_mem_zalloc_ExpectAndReturn((size_t)e_len, a_buf);
    pico_mem_free_Expect(e_buf);
    pico_socket_read_StubWithCallback(&pico_socket_read_cb);
    TEST_ASSERT_EQUAL(0 ,check_signature(zmtp_s));
    TEST_ASSERT_EQUAL(ZMTP_ST_RCVD_SIGNATURE, zmtp_s->state);

   /* Bad signatures */
    signature[8] = 0x00;
    signature[0] = 0xfe;
    zmtp_s->state = ZMTP_ST_SND_GREETING;
    memcpy(read_data, signature, (size_t)e_len);
    pico_mem_zalloc_ExpectAndReturn((size_t)e_len, a_buf);
    pico_mem_free_Expect(e_buf);
    pico_socket_read_StubWithCallback(&pico_socket_read_cb);
    TEST_ASSERT_EQUAL(-1 ,check_signature(zmtp_s));
    TEST_ASSERT_EQUAL(ZMTP_ST_SND_GREETING, zmtp_s->state);

    signature[0] = 0xff;
    signature[9] = 0x8f;
    zmtp_s->state = ZMTP_ST_SND_GREETING;
    memcpy(read_data, signature, (size_t)e_len);
    pico_mem_zalloc_ExpectAndReturn((size_t)e_len, a_buf);
    pico_mem_free_Expect(e_buf);
    pico_socket_read_StubWithCallback(&pico_socket_read_cb);
    TEST_ASSERT_EQUAL(-1, check_signature(zmtp_s));
    TEST_ASSERT_EQUAL(ZMTP_ST_SND_GREETING, zmtp_s->state);

    signature[9] = 0x7f;
    signature[4] = 0x90;
    zmtp_s->state = ZMTP_ST_SND_GREETING;
    memcpy(read_data, signature, (size_t)e_len);
    pico_mem_zalloc_ExpectAndReturn((size_t)e_len, a_buf);
    pico_mem_free_Expect(e_buf);
    pico_socket_read_StubWithCallback(&pico_socket_read_cb);
    TEST_ASSERT_EQUAL(-1, check_signature(zmtp_s));
    TEST_ASSERT_EQUAL(ZMTP_ST_SND_GREETING, zmtp_s->state);


    free(zmtp_s);
    free(pico_s);
    free(a_buf);
    free(read_data);
}

void test_check_revision(void)
{
    struct zmtp_socket *zmtp_s;
    struct pico_socket *pico_s;
    void* a_buf;
    uint8_t revision[1] = {0x01};


    pico_s = calloc(1, sizeof(struct pico_socket));
    zmtp_s = calloc(1, sizeof(struct zmtp_socket));
    zmtp_s->sock = pico_s;

    e_len = 1;
    a_buf = calloc(1, (size_t)e_len);
    e_buf = a_buf;
    e_pico_s = pico_s;
    read_data = calloc(1, (size_t)e_len);


    zmtp_s->state = ZMTP_ST_RCVD_SIGNATURE;
    memcpy(read_data, revision, (size_t)e_len);
    pico_mem_zalloc_ExpectAndReturn((size_t)e_len, a_buf);
    pico_mem_free_Expect(e_buf);
    pico_socket_read_StubWithCallback(&pico_socket_read_cb);
    TEST_ASSERT_EQUAL(0, check_revision(zmtp_s));
    TEST_ASSERT_EQUAL(ZMTP_ST_RCVD_REVISION, zmtp_s->state);

    revision[0] = 0x00;
    zmtp_s->state = ZMTP_ST_RCVD_SIGNATURE;
    memcpy(read_data, revision, (size_t)e_len);
    pico_mem_zalloc_ExpectAndReturn((size_t)e_len, a_buf);
    pico_mem_free_Expect(e_buf);
    pico_socket_read_StubWithCallback(&pico_socket_read_cb);
    TEST_ASSERT_EQUAL(0, check_revision(zmtp_s));
    TEST_ASSERT_EQUAL(ZMTP_ST_RCVD_REVISION, zmtp_s->state);


    free(pico_s);
    free(zmtp_s);
    free(a_buf);
    free(read_data);
}

void test_check_socket_type(void)
{
    struct zmtp_socket *zmtp_s;
    struct pico_socket *pico_s;
    void* a_buf;
    uint8_t type[1] = {0x01};


    pico_s = calloc(1, sizeof(struct pico_socket));
    zmtp_s = calloc(1, sizeof(struct zmtp_socket));
    zmtp_s->sock = pico_s;

    e_len = 1;
    a_buf = calloc(1, (size_t)e_len);
    e_buf = a_buf;
    e_pico_s = pico_s;
    read_data = calloc(1, (size_t)e_len);


    zmtp_s->state = ZMTP_ST_RCVD_REVISION;
    memcpy(read_data, type, (size_t)e_len);
    pico_mem_zalloc_ExpectAndReturn((size_t)e_len, a_buf);
    pico_mem_free_Expect(e_buf);
    pico_socket_read_StubWithCallback(&pico_socket_read_cb);
    TEST_ASSERT_EQUAL(0, check_socket_type(zmtp_s));
    TEST_ASSERT_EQUAL(ZMTP_ST_RCVD_TYPE, zmtp_s->state);


    free(pico_s);
    free(zmtp_s);
    free(a_buf);
    free(read_data);
}

void test_check_identity(void)
{
    struct zmtp_socket *zmtp_s;
    struct pico_socket *pico_s;
    void* a_buf;
    uint8_t identity[2] = {0x00, 0x00};


    pico_s = calloc(1, sizeof(struct pico_socket));
    zmtp_s = calloc(1, sizeof(struct zmtp_socket));
    zmtp_s->sock = pico_s;

    e_len = 2;
    a_buf = calloc(1, (size_t)e_len);
    e_buf = a_buf;
    e_pico_s = pico_s;
    read_data = calloc(1, (size_t)e_len);

    /* Empty identity */
    zmtp_s->state = ZMTP_ST_RCVD_TYPE;
    memcpy(read_data, identity, (size_t)e_len);
    pico_mem_zalloc_ExpectAndReturn((size_t)e_len, a_buf);
    pico_mem_free_Expect(e_buf);
    pico_socket_read_StubWithCallback(&pico_socket_read_cb);
    TEST_ASSERT_EQUAL(0, check_identity(zmtp_s));
    TEST_ASSERT_EQUAL(ZMTP_ST_RDY, zmtp_s->state);

    /* Bad identity flag */
    identity[0] = 0x03;
    zmtp_s->state = ZMTP_ST_RCVD_TYPE;
    memcpy(read_data, identity, (size_t)e_len);
    pico_mem_zalloc_ExpectAndReturn((size_t)e_len, a_buf);
    pico_mem_free_Expect(e_buf);
    pico_socket_read_StubWithCallback(&pico_socket_read_cb);
    TEST_ASSERT_EQUAL(-1, check_identity(zmtp_s));
    TEST_ASSERT_EQUAL(ZMTP_ST_RCVD_TYPE, zmtp_s->state);


    free(pico_s);
    free(zmtp_s);
    free(a_buf);
    free(read_data);
}

void test_zmtp_socket_accept(void)
{
    struct zmtp_socket* zmtp_s;
    struct zmtp_socket* new_zmtp_s;
    struct pico_socket* pico_s;
    struct pico_socket* new_pico_s;

    zmtp_s = calloc(1,sizeof(struct zmtp_socket));
    new_zmtp_s = calloc(1,sizeof(struct zmtp_socket));
    pico_s = calloc(1,sizeof(struct pico_socket));

    zmtp_s->zmq_cb = &zmq_cb_mock;
    zmtp_s->sock = pico_s;
    zmtp_s->type = ZMTP_TYPE_PUB;
    
    pico_mem_zalloc_ExpectAndReturn(sizeof(struct zmtp_socket), new_zmtp_s);
    e_pico_s = zmtp_s->sock;
    e_new_pico_s = new_pico_s;
    pico_socket_accept_StubWithCallback(&pico_socket_accept_cb);
    pico_tree_insert_IgnoreAndReturn(NULL);

    expect_greeting(new_pico_s, zmtp_s->type);
    new_zmtp_s = zmtp_socket_accept(zmtp_s);
    TEST_ASSERT_EQUAL(ZMTP_ST_IDLE, new_zmtp_s->state);
    TEST_ASSERT_EQUAL(zmtp_s->type, new_zmtp_s->type);
    TEST_ASSERT_EQUAL(new_pico_s, new_zmtp_s->sock);
    TEST_ASSERT_EQUAL(zmtp_s->zmq_cb, new_zmtp_s->zmq_cb);

    free(zmtp_s);
    free(new_zmtp_s);
    free(pico_s);
}

void test_zmtp_tcp_cb(void)
{
    uint16_t ev;
    struct zmtp_socket* zmtp_s;
    struct pico_socket* pico_s;

    zmtp_s = calloc(1, sizeof(struct zmtp_socket));
    pico_s = calloc(1, sizeof(struct pico_socket));

    zmtp_s->sock = pico_s;
    zmtp_s->type = ZMTP_TYPE_PUB;
    zmtp_s->zmq_cb = &zmq_cb_mock;
    
    /* 
    event: connection established 
    expected: callback to zmq
    */
    zmtp_s->state = ZMTP_ST_IDLE;
    ev = PICO_SOCK_EV_CONN;

    map_tcp_to_zmtp(zmtp_s);

    e_zmtp_s = zmtp_s;
    e_ev = ZMTP_EV_CONN;
    zmq_cb_counter = 0;

    zmtp_tcp_cb(ev, zmtp_s->sock);
    TEST_ASSERT_EQUAL(1, zmq_cb_counter);


    TEST_IGNORE();
    /* 
    event: signature available
    expected: read signature if greeting was send
    */
    zmtp_s->state = ZMTP_ST_SND_GREETING;
    ev = PICO_SOCK_EV_RD;
    map_tcp_to_zmtp(zmtp_s);
    zmtp_tcp_cb(ev, zmtp_s->sock);
    TEST_ASSERT_EQUAL_UINT8(ZMTP_ST_RCVD_SIGNATURE, zmtp_s->state);

    /*
    event: revision available
    expected: read revision
    */
    zmtp_s->state = ZMTP_ST_RCVD_SIGNATURE;
    ev = PICO_SOCK_EV_RD;
    map_tcp_to_zmtp(zmtp_s);
    zmtp_tcp_cb(ev, zmtp_s->sock);
    TEST_ASSERT_EQUAL_UINT8(ZMTP_ST_RCVD_REVISION, zmtp_s->state);

    /*
    event: socket type available
    expected: read socket type
    */
    zmtp_s->state = ZMTP_ST_RCVD_REVISION;
    ev = PICO_SOCK_EV_RD;
    map_tcp_to_zmtp(zmtp_s);
    zmtp_tcp_cb(ev, zmtp_s->sock);
    TEST_ASSERT_EQUAL_UINT8(ZMTP_ST_RCVD_TYPE, zmtp_s->state);


    /*
    event: empty identity frame available
    expected: read identity length
    */
    zmtp_s->state = ZMTP_ST_RCVD_TYPE;
    ev = PICO_SOCK_EV_RD;
    map_tcp_to_zmtp(zmtp_s);
    zmtp_tcp_cb(ev, zmtp_s->sock);
    TEST_ASSERT_EQUAL_UINT8(ZMTP_ST_RDY, zmtp_s->state);


    /*
    event: signature and revision availble
    expected: read both
    */
    zmtp_s->state = ZMTP_ST_SND_GREETING;
    ev = PICO_SOCK_EV_RD;
    map_tcp_to_zmtp(zmtp_s);
    zmtp_tcp_cb(ev, zmtp_s->sock);
    TEST_ASSERT_EQUAL_UINT8(ZMTP_ST_RCVD_REVISION, zmtp_s->state);


    /*
    event: revision and type available
    expected: read both
    */
    zmtp_s->state = ZMTP_ST_RCVD_SIGNATURE;
    ev = PICO_SOCK_EV_RD;
    map_tcp_to_zmtp(zmtp_s);
    zmtp_tcp_cb(ev, zmtp_s->sock);
    TEST_ASSERT_EQUAL_UINT8(ZMTP_ST_RCVD_TYPE, zmtp_s->state);



    free(zmtp_s);
    free(pico_s);
}


void test_zmtp_socket_send(void)
{
    TEST_IGNORE();
/*
    mocking variables
    void* aBytestream: actual bytestream, used as return value of the mocked pico_mem_zalloc
    
    expected variables
    void* eBytestream: expected bytestream, 
    int eBytestreamLen: expected bytestream length
*/

    /* mocking variables */
    void* aBytestream;

    /* expected variables */
    void* eBytestream;
    size_t eBytestreamLen;

    struct pico_vector* vec;
    struct zmtp_socket* zmtp_s;
    zmtp_s = calloc(1, sizeof(struct zmtp_socket));
    struct pico_socket* pico_s;
    zmtp_s->sock = pico_s;

    struct zmtp_frame_t* frame1;
    struct zmtp_frame_t* frame2;
    frame1 = calloc(1, sizeof(struct zmtp_frame_t));
    frame2 = calloc(1, sizeof(struct zmtp_frame_t));
    size_t msg1Len;
    size_t msg2Len;
    uint8_t* msg1;
    uint8_t* msg2;
    
    uint8_t i;
    uint8_t* bytestreamPtr;
    

    /* vec 1 msg, 0 char */
    msg1Len = 0;
    eBytestreamLen = 2 + msg1Len;
    msg1 = (uint8_t*)calloc(1,msg1Len);
    for(i = 0; i < msg1Len; i++)
        msg1[i] = i; 
    frame1->len = msg1Len;
    frame1->buf = msg1;
    aBytestream = calloc(1,eBytestreamLen);
    eBytestream = calloc(1,eBytestreamLen);
    ((uint8_t*)eBytestream)[0] = 0; /* final-short */
    ((uint8_t*)eBytestream)[1] = 0; 
    for(i = 0; i < msg1Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 2;
        *bytestreamPtr = msg1[i];
    }

    pico_vector_begin_ExpectAndReturn(vec, (struct pico_vector_iterator*)&frame1);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame1, NULL);
    pico_mem_zalloc_ExpectAndReturn(eBytestreamLen, aBytestream);
    pico_socket_send_ExpectAndReturn(zmtp_s->sock, aBytestream, eBytestreamLen, 0); /* expect pointer to aBytestream but content of eBytestream */
    
    TEST_ASSERT_EQUAL_INT(0, zmtp_socket_send(zmtp_s, vec));
    TEST_ASSERT_EQUAL_MEMORY(eBytestream, aBytestream, eBytestreamLen);

    free(msg1);
    free(aBytestream);
    free(eBytestream);


    //vec 1 msg, 1 char
    msg1Len = 0;
    eBytestreamLen = 2 + msg1Len;
    msg1 = (uint8_t*)calloc(1,msg1Len);
    for(i = 0; i < msg1Len; i++)
        msg1[i] = i; 
    frame1->len = msg1Len;
    frame1->buf = msg1;
    aBytestream = calloc(1,eBytestreamLen);
    eBytestream = calloc(1,eBytestreamLen);
    ((uint8_t*)eBytestream)[0] = 0; /* final-short */
    ((uint8_t*)eBytestream)[1] = 0; 
    for(i = 0; i < msg1Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 2;
        *bytestreamPtr = msg1[i];
    }

    pico_vector_begin_ExpectAndReturn(vec, (struct pico_vector_iterator*)&frame1);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame1, NULL);
    pico_mem_zalloc_ExpectAndReturn(eBytestreamLen, aBytestream);
    pico_socket_send_ExpectAndReturn(zmtp_s->sock, aBytestream, eBytestreamLen, 0);
    
    TEST_ASSERT_EQUAL_INT(0, zmtp_socket_send(zmtp_s, vec));
    TEST_ASSERT_EQUAL_MEMORY(eBytestream, aBytestream, eBytestreamLen);

    free(msg1);
    free(aBytestream);
    free(eBytestream);


    //vec 1 msg, 255 char
    msg1Len = 255;
    eBytestreamLen = 2 + msg1Len;
    msg1 = (uint8_t*)calloc(1,msg1Len);
    for(i = 0; i < msg1Len; i++)
        msg1[i] = i; 
    frame1->len = msg1Len;
    frame1->buf = msg1;
    aBytestream = calloc(1,eBytestreamLen);
    eBytestream = calloc(1,eBytestreamLen);
    ((uint8_t*)eBytestream)[0] = 0; /* final-short */
    ((uint8_t*)eBytestream)[1] = 0; 
    for(i = 0; i < msg1Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 2;
        *bytestreamPtr = msg1[i];
    }

    pico_vector_begin_ExpectAndReturn(vec, (struct pico_vector_iterator*)&frame1);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame1, NULL);
    pico_mem_zalloc_ExpectAndReturn(eBytestreamLen, aBytestream);
    pico_socket_send_ExpectAndReturn(zmtp_s->sock, aBytestream, eBytestreamLen, 0);
    
    TEST_ASSERT_EQUAL_INT(0, zmtp_socket_send(zmtp_s, vec));
    TEST_ASSERT_EQUAL_MEMORY(eBytestream, aBytestream, eBytestreamLen);

    free(msg1);
    free(aBytestream);
    free(eBytestream);


    //vec 1 msg, 256 char
    msg1Len = 256;
    eBytestreamLen = 9 + msg1Len;
    msg1 = (uint8_t*)calloc(1,msg1Len);
    for(i = 0; i < msg1Len; i++)
        msg1[i] = i; 
    frame1->len = msg1Len;
    frame1->buf = msg1;
    aBytestream = calloc(1,eBytestreamLen);
    eBytestream = calloc(1,eBytestreamLen);
    ((uint8_t*)eBytestream)[0] = 2; /* final-long */
    ((uint8_t*)eBytestream)[7] = 1; /* 256 in 8 bytes: 0 0 0 0 0 0 1 0 */
    ((uint8_t*)eBytestream)[8] = 0; 
    for(i = 0; i < msg1Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 9;
        *bytestreamPtr = msg1[i];
    }

    pico_vector_begin_ExpectAndReturn(vec, (struct pico_vector_iterator*)&frame1);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame1, NULL);
    pico_mem_zalloc_ExpectAndReturn(eBytestreamLen, aBytestream);
    pico_socket_send_ExpectAndReturn(zmtp_s->sock, aBytestream, eBytestreamLen, 0);
    
    TEST_ASSERT_EQUAL_INT(0, zmtp_socket_send(zmtp_s, vec));
    TEST_ASSERT_EQUAL_MEMORY(eBytestream, aBytestream, eBytestreamLen);

    free(msg1);
    free(aBytestream);
    free(eBytestream);


    //vec 1 msg, 600 char
    msg1Len = 600;
    eBytestreamLen = 9 + msg1Len;
    msg1 = (uint8_t*)calloc(1,msg1Len);
    for(i = 0; i < msg1Len; i++)
        msg1[i] = i; 
    frame1->len = msg1Len;
    frame1->buf = msg1;
    aBytestream = calloc(1,eBytestreamLen);
    eBytestream = calloc(1,eBytestreamLen);
    ((uint8_t*)eBytestream)[0] = 0; /* final-long */
    ((uint8_t*)eBytestream)[7] = 2; /* 600 in 8 bytes: 0 0 0 0 0 0 2 88 */
    ((uint8_t*)eBytestream)[8] = 88; /* 512 + 88 */
    for(i = 0; i < msg1Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 9;
        *bytestreamPtr = msg1[i];
    }

    pico_vector_begin_ExpectAndReturn(vec, (struct pico_vector_iterator*)&frame1);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame1, NULL);
    pico_mem_zalloc_ExpectAndReturn(eBytestreamLen, aBytestream);
    pico_socket_send_ExpectAndReturn(zmtp_s->sock, aBytestream, eBytestreamLen, 0);
    
    TEST_ASSERT_EQUAL_INT(0, zmtp_socket_send(zmtp_s, vec));
    TEST_ASSERT_EQUAL_MEMORY(eBytestream, aBytestream, eBytestreamLen);

    free(msg1);
    free(aBytestream);
    free(eBytestream);


    //vec 2 msg, 0 char, 0 char
    msg1Len = 0;
    msg2Len = 0;
    eBytestreamLen = (2 + msg1Len) + (2 + msg2Len);
    msg1 = (uint8_t*)calloc(1,msg1Len);
    msg2 = (uint8_t*)calloc(1,msg2Len);
    for(i = 0; i < msg1Len; i++)
        msg1[i] = i; 
    for(i = 0; i < msg2Len; i++)
        msg2[i] = i;
    frame1->len = msg1Len;
    frame1->buf = msg1;
    frame2->len = msg2Len;
    frame2->buf = msg2;
    aBytestream = calloc(1,eBytestreamLen);
    eBytestream = calloc(1,eBytestreamLen);
    ((uint8_t*)eBytestream)[0] = 1; /* more-short */
    ((uint8_t*)eBytestream)[1] = 0; 
    for(i = 0; i < msg1Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 2;
        *bytestreamPtr = msg1[i];
    }
    ((uint8_t*)eBytestream)[msg1Len+2+0] = 0; /* final-short */
    ((uint8_t*)eBytestream)[msg1Len+2+1] = 0; 
    for(i = 0; i < msg2Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 2 + msg1Len + 2;
        *bytestreamPtr = msg2[i];
    }
    pico_vector_begin_ExpectAndReturn(vec, (struct pico_vector_iterator*)&frame1);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame1, (struct pico_vector_iterator*)&frame2);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame2, NULL);
    pico_mem_zalloc_ExpectAndReturn(eBytestreamLen, aBytestream);
    pico_socket_send_ExpectAndReturn(zmtp_s->sock, aBytestream, eBytestreamLen, 0);
    
    TEST_ASSERT_EQUAL_INT(0, zmtp_socket_send(zmtp_s, vec));
    TEST_ASSERT_EQUAL_MEMORY(eBytestream, aBytestream, eBytestreamLen);

    free(msg1);
    free(msg2);
    free(aBytestream);
    free(eBytestream);


    //vec 2 msg, 0 char, 1 char
    msg1Len = 0;
    msg2Len = 1;
    eBytestreamLen = (2 + msg1Len) + (2 + msg2Len);
    msg1 = (uint8_t*)calloc(1,msg1Len);
    msg2 = (uint8_t*)calloc(1,msg2Len);
    for(i = 0; i < msg1Len; i++)
        msg1[i] = i; 
    for(i = 0; i < msg2Len; i++)
        msg2[i] = i;
    frame1->len = msg1Len;
    frame1->buf = msg1;
    frame2->len = msg2Len;
    frame2->buf = msg2;
    aBytestream = calloc(1,eBytestreamLen);
    eBytestream = calloc(1,eBytestreamLen);
    ((uint8_t*)eBytestream)[0] = 1; /* more-short */
    ((uint8_t*)eBytestream)[1] = 0; 
    for(i = 0; i < msg1Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 2;
        *bytestreamPtr = msg1[i];
    }
    ((uint8_t*)eBytestream)[msg1Len+2+0] = 0; /* final-short */
    ((uint8_t*)eBytestream)[msg1Len+2+1] = 1; 
    for(i = 0; i < msg2Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 2 + msg1Len + 2;
        *bytestreamPtr = msg2[i];
    }
    pico_vector_begin_ExpectAndReturn(vec, (struct pico_vector_iterator*)&frame1);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame1, (struct pico_vector_iterator*)&frame2);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame2, NULL);
    pico_mem_zalloc_ExpectAndReturn(eBytestreamLen, aBytestream);
    pico_socket_send_ExpectAndReturn(zmtp_s->sock, aBytestream, eBytestreamLen, 0);
    
    TEST_ASSERT_EQUAL_INT(0, zmtp_socket_send(zmtp_s, vec));
    TEST_ASSERT_EQUAL_MEMORY(eBytestream, aBytestream, eBytestreamLen);

    free(msg1);
    free(msg2);
    free(aBytestream);
    free(eBytestream);


    //vec 2 msg, 1 char, 0 char
    msg1Len = 1;
    msg2Len = 0;
    eBytestreamLen = (2 + msg1Len) + (2 + msg2Len);
    msg1 = (uint8_t*)calloc(1,msg1Len);
    msg2 = (uint8_t*)calloc(1,msg2Len);
    for(i = 0; i < msg1Len; i++)
        msg1[i] = i; 
    for(i = 0; i < msg2Len; i++)
        msg2[i] = i;
    frame1->len = msg1Len;
    frame1->buf = msg1;
    frame2->len = msg2Len;
    frame2->buf = msg2;
    aBytestream = calloc(1,eBytestreamLen);
    eBytestream = calloc(1,eBytestreamLen);
    ((uint8_t*)eBytestream)[0] = 1; /* more-short */
    ((uint8_t*)eBytestream)[1] = 1; 
    for(i = 0; i < msg1Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 2;
        *bytestreamPtr = msg1[i];
    }
    ((uint8_t*)eBytestream)[msg1Len+2+0] = 0; /* final-short */
    ((uint8_t*)eBytestream)[msg1Len+2+1] = 0; 
    for(i = 0; i < msg2Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 2 + msg1Len + 2;
        *bytestreamPtr = msg2[i];
    }
    pico_vector_begin_ExpectAndReturn(vec, (struct pico_vector_iterator*)&frame1);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame1, (struct pico_vector_iterator*)&frame2);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame2, NULL);
    pico_mem_zalloc_ExpectAndReturn(eBytestreamLen, aBytestream);
    pico_socket_send_ExpectAndReturn(zmtp_s->sock, aBytestream, eBytestreamLen, 0);
    
    TEST_ASSERT_EQUAL_INT(0, zmtp_socket_send(zmtp_s, vec));
    TEST_ASSERT_EQUAL_MEMORY(eBytestream, aBytestream, eBytestreamLen);

    free(msg1);
    free(msg2);
    free(aBytestream);
    free(eBytestream);


    //vec 2 msg, 255 char, 255 char
    msg1Len = 255;
    msg2Len = 255;
    eBytestreamLen = (2 + msg1Len) + (2 + msg2Len);
    msg1 = (uint8_t*)calloc(1,msg1Len);
    msg2 = (uint8_t*)calloc(1,msg2Len);
    for(i = 0; i < msg1Len; i++)
        msg1[i] = i; 
    for(i = 0; i < msg2Len; i++)
        msg2[i] = i;
    frame1->len = msg1Len;
    frame1->buf = msg1;
    frame2->len = msg2Len;
    frame2->buf = msg2;
    aBytestream = calloc(1,eBytestreamLen);
    eBytestream = calloc(1,eBytestreamLen);
    ((uint8_t*)eBytestream)[0] = 1; /* more-short */
    ((uint8_t*)eBytestream)[1] = 255; 
    for(i = 0; i < msg1Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 2;
        *bytestreamPtr = msg1[i];
    }
    ((uint8_t*)eBytestream)[msg1Len+2+0] = 0; /* final-short */
    ((uint8_t*)eBytestream)[msg1Len+2+1] = 255; 
    for(i = 0; i < msg2Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 2 + msg1Len + 2;
        *bytestreamPtr = msg2[i];
    }
    pico_vector_begin_ExpectAndReturn(vec, (struct pico_vector_iterator*)&frame1);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame1, (struct pico_vector_iterator*)&frame2);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame2, NULL);
    pico_mem_zalloc_ExpectAndReturn(eBytestreamLen, aBytestream);
    pico_socket_send_ExpectAndReturn(zmtp_s->sock, aBytestream, eBytestreamLen, 0);
    
    TEST_ASSERT_EQUAL_INT(0, zmtp_socket_send(zmtp_s, vec));
    TEST_ASSERT_EQUAL_MEMORY(eBytestream, aBytestream, eBytestreamLen);

    free(msg1);
    free(msg2);
    free(aBytestream);
    free(eBytestream);


    //vec 2 msg, 256 char, 255 char
    msg1Len = 256;
    msg2Len = 255;
    eBytestreamLen = (2 + msg1Len) + (2 + msg2Len);
    msg1 = (uint8_t*)calloc(1,msg1Len);
    msg2 = (uint8_t*)calloc(1,msg2Len);
    for(i = 0; i < msg1Len; i++)
        msg1[i] = i; 
    for(i = 0; i < msg2Len; i++)
        msg2[i] = i;
    frame1->len = msg1Len;
    frame1->buf = msg1;
    frame2->len = msg2Len;
    frame2->buf = msg2;
    aBytestream = calloc(1,eBytestreamLen);
    eBytestream = calloc(1,eBytestreamLen);
    ((uint8_t*)eBytestream)[0] = 3; /* more-long */
    ((uint8_t*)eBytestream)[7] = 1; 
    for(i = 0; i < msg1Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 9;
        *bytestreamPtr = msg1[i];
    }
    ((uint8_t*)eBytestream)[msg1Len+9+0] = 0; /* final-short */
    ((uint8_t*)eBytestream)[msg1Len+9+1] = 255; 
    for(i = 0; i < msg2Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 9 + msg1Len + 2;
        *bytestreamPtr = msg2[i];
    }
    pico_vector_begin_ExpectAndReturn(vec, (struct pico_vector_iterator*)&frame1);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame1, (struct pico_vector_iterator*)&frame2);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame2, NULL);
    pico_mem_zalloc_ExpectAndReturn(eBytestreamLen, aBytestream);
    pico_socket_send_ExpectAndReturn(zmtp_s->sock, aBytestream, eBytestreamLen, 0);
    
    TEST_ASSERT_EQUAL_INT(0, zmtp_socket_send(zmtp_s, vec));
    TEST_ASSERT_EQUAL_MEMORY(eBytestream, aBytestream, eBytestreamLen);

    free(msg1);
    free(msg2);
    free(aBytestream);
    free(eBytestream);


    //vec 2 msg, 600 char, 255 char
    msg1Len = 600;
    msg2Len = 255;
    eBytestreamLen = (2 + msg1Len) + (2 + msg2Len);
    msg1 = (uint8_t*)calloc(1,msg1Len);
    msg2 = (uint8_t*)calloc(1,msg2Len);
    for(i = 0; i < msg1Len; i++)
        msg1[i] = i; 
    for(i = 0; i < msg2Len; i++)
        msg2[i] = i;
    frame1->len = msg1Len;
    frame1->buf = msg1;
    frame2->len = msg2Len;
    frame2->buf = msg2;
    aBytestream = calloc(1,eBytestreamLen);
    eBytestream = calloc(1,eBytestreamLen);
    ((uint8_t*)eBytestream)[0] = 3; /* more-long */
    ((uint8_t*)eBytestream)[7] = 2; 
    ((uint8_t*)eBytestream)[8] = 88; 
    for(i = 0; i < msg1Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 9;
        *bytestreamPtr = msg1[i];
    }
    ((uint8_t*)eBytestream)[msg1Len+9+0] = 0; /* final-short */
    ((uint8_t*)eBytestream)[msg1Len+9+1] = 255; 
    for(i = 0; i < msg2Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 9 + msg1Len + 2;
        *bytestreamPtr = msg2[i];
    }
    pico_vector_begin_ExpectAndReturn(vec, (struct pico_vector_iterator*)&frame1);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame1, (struct pico_vector_iterator*)&frame2);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame2, NULL);
    pico_mem_zalloc_ExpectAndReturn(eBytestreamLen, aBytestream);
    pico_socket_send_ExpectAndReturn(zmtp_s->sock, aBytestream, eBytestreamLen, 0);
    
    TEST_ASSERT_EQUAL_INT(0, zmtp_socket_send(zmtp_s, vec));
    TEST_ASSERT_EQUAL_MEMORY(eBytestream, aBytestream, eBytestreamLen);

    free(msg1);
    free(msg2);
    free(aBytestream);
    free(eBytestream);


    //vec 2 msg, 600 char, 256 char
    msg1Len = 600;
    msg2Len = 256;
    eBytestreamLen = (2 + msg1Len) + (2 + msg2Len);
    msg1 = (uint8_t*)calloc(1,msg1Len);
    msg2 = (uint8_t*)calloc(1,msg2Len);
    for(i = 0; i < msg1Len; i++)
        msg1[i] = i; 
    for(i = 0; i < msg2Len; i++)
        msg2[i] = i;
    frame1->len = msg1Len;
    frame1->buf = msg1;
    frame2->len = msg2Len;
    frame2->buf = msg2;
    aBytestream = calloc(1,eBytestreamLen);
    eBytestream = calloc(1,eBytestreamLen);
    ((uint8_t*)eBytestream)[0] = 3; /* more-long */
    ((uint8_t*)eBytestream)[7] = 2; 
    ((uint8_t*)eBytestream)[8] = 88; 
    for(i = 0; i < msg1Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 9;
        *bytestreamPtr = msg1[i];
    }
    ((uint8_t*)eBytestream)[msg1Len+9+0] = 2; /* final-long */
    ((uint8_t*)eBytestream)[msg1Len+9+7] = 1; 
    for(i = 0; i < msg2Len; i++)
    {
        bytestreamPtr = (uint8_t*)eBytestream + i + 9 + msg1Len + 9;
        *bytestreamPtr = msg2[i];
    }
    pico_vector_begin_ExpectAndReturn(vec, (struct pico_vector_iterator*)&frame1);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame1, (struct pico_vector_iterator*)&frame2);
    pico_vector_iterator_next_ExpectAndReturn((struct pico_vector_iterator*)&frame2, NULL);
    pico_mem_zalloc_ExpectAndReturn(eBytestreamLen, aBytestream);
    pico_socket_send_ExpectAndReturn(zmtp_s->sock, aBytestream, eBytestreamLen, 0);
    
    TEST_ASSERT_EQUAL_INT(0, zmtp_socket_send(zmtp_s, vec));
    TEST_ASSERT_EQUAL_MEMORY(eBytestream, aBytestream, eBytestreamLen);

    free(msg1);
    free(msg2);
    free(aBytestream);
    free(eBytestream);

    free(frame1);
    free(frame2);
}

void dummy_callback(uint16_t ev, struct zmtp_socket*s)
{
    TEST_FAIL();
}

/* set callback_include_count to false! */
int stub_callback1(struct pico_socket* s, const void* buf, int len, int numCalls)
{
    uint8_t greeting[14] = {0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x01, ZMTP_TYPE_REQ, 0x00, 0x00};
    int greeting_len = 14;

    TEST_ASSERT_EQUAL_INT(greeting_len, len);
    TEST_ASSERT_EQUAL_MEMORY(greeting, buf, greeting_len);
    
    return 0;
}

void zmtp_socket_callback(uint16_t ev, struct zmtp_socket* s)
{

}

void test_zmtp_socket_connect(void)
{
    TEST_IGNORE();
    /* Only supporting zmtp2.0 (whole greeting send at once) */

    /* Add tests for NULL arguments */ 

    struct zmtp_socket* zmtp_s;
    struct pico_socket* pico_s;
    void* srv_addr;
    uint16_t remote_port = 1320;
    uint8_t socket_type = ZMTP_TYPE_REQ;


    zmtp_s = calloc(1, sizeof(struct zmtp_socket));
    pico_mem_zalloc_IgnoreAndReturn(NULL);
    /* Setting up zmtp_socket */
    pico_socket_open_ExpectAndReturn(PICO_PROTO_IPV4, PICO_PROTO_TCP, &zmtp_tcp_cb, pico_s);
    zmtp_s = zmtp_socket_open(PICO_PROTO_IPV4, PICO_PROTO_TCP, socket_type, &zmtp_socket_callback);
    TEST_ASSERT_NOT_NULL(zmtp_s);
    TEST_ASSERT_EQUAL_UINT8(zmtp_s->type, socket_type);

    /*----=== Test valid arguments ===----*/
    /* Setup mocking objects */
    pico_socket_connect_ExpectAndReturn(zmtp_s->sock, srv_addr, remote_port, 0);
    pico_socket_write_StubWithCallback(stub_callback1);

    /* Test */
    TEST_ASSERT_EQUAL_INT(0, zmtp_socket_connect(zmtp_s, srv_addr, remote_port));

    /*----=== Test invalid arguments ===----
    The zmq_connect only returns -1 if the zmtp_socket was NULL 
    or if pico_socket_connect returns -1*/

    /* Setup mocking objects */
    pico_socket_connect_ExpectAndReturn(zmtp_s->sock, srv_addr, remote_port, -1);

    /* Test */
    TEST_ASSERT_EQUAL_INT(-1, zmtp_socket_connect(zmtp_s, srv_addr, remote_port));
    TEST_ASSERT_EQUAL_INT(-1, zmtp_socket_connect(NULL, srv_addr, remote_port));
}

void test_zmtp_socket_open(void)
{
    TEST_IGNORE();
    uint16_t net = PICO_PROTO_IPV4;
    uint16_t proto = PICO_PROTO_TCP;
    uint8_t type = ZMTP_TYPE_PUB;
    struct zmtp_socket* zmtp_s;
    struct zmtp_socket* zmtp_ret_s;
    struct pico_socket* pico_s;
    zmtp_s = calloc(1, sizeof(struct zmtp_socket));
    pico_s = calloc(1, sizeof(struct pico_socket));
    /* ---=== Test failing pico_zalloc ===----*/
    //pico_zalloc_ExpectAndReturn(sizeof(struct zmtp_socket), NULL);
    //pico_socket_open_IgnoreAndReturn(NULL);
 
    /* Test */
    //TEST_ASSERT_NULL(zmtp_socket_open(net, proto, type, &zmtp_socket_callback));
    //TEST_ASSERT_EQUAL_INT(PICO_ERR_ENOMEM, pico_err);     
    /*----=== Test invalid arguments ===----*/
    TEST_ASSERT_NULL(zmtp_socket_open(net, proto, -1, &zmtp_socket_callback));
    TEST_ASSERT_NULL(zmtp_socket_open(net, proto, ZMTP_TYPE_END, &zmtp_socket_callback));
    TEST_ASSERT_NULL(zmtp_socket_open(net, proto, type, NULL));

    pico_socket_open_ExpectAndReturn(net, proto, &zmtp_tcp_cb, NULL);
    TEST_ASSERT_NULL(zmtp_socket_open(net, proto, type, &zmtp_socket_callback));

    /*----=== Test valid arguments ===----*/
    //pico_zalloc_ExpectAndReturn(sizeof(struct zmtp_socket), zmtp_s);
    pico_socket_open_ExpectAndReturn(net, proto, &zmtp_tcp_cb, pico_s);
    zmtp_ret_s = zmtp_socket_open(net, proto, type, &zmtp_socket_callback); 
    TEST_ASSERT_EQUAL_PTR(pico_s, zmtp_ret_s->sock);
    TEST_ASSERT_EQUAL_INT(ZMTP_ST_IDLE, zmtp_ret_s->state);

    free(zmtp_s);
    free(pico_s);

}

void test_zmtp_socket_bind(void)
{
    TEST_IGNORE();
   struct zmtp_socket* zmtp_s;
   struct pico_socket* pico_s;
   uint16_t port = 23445;

   zmtp_s = calloc(1, sizeof(struct zmtp_socket));
   pico_s = calloc(1, sizeof(struct pico_socket));
   /*----=== Test empty sockets ===----*/
   /*we don't test pico_socket_bind here so we can use whatever value for the local_addr and port*/
   TEST_ASSERT_EQUAL_INT(zmtp_socket_bind(NULL, NULL, &port), PICO_ERR_EFAULT);
   TEST_ASSERT_EQUAL_INT(zmtp_socket_bind(zmtp_s, NULL, &port), PICO_ERR_EFAULT);

   /*----=== Test valid arguments ===----*/
   zmtp_s->sock = pico_s;
   pico_socket_bind_IgnoreAndReturn(0);
   TEST_ASSERT_EQUAL_INT(zmtp_socket_bind(zmtp_s, NULL, &port), 0);
   
   free(zmtp_s);
   free(pico_s);
   
}

void test_zmtp_socket_close(void)
{
   struct zmtp_socket* zmtp_s;
   struct pico_socket* pico_s;
   zmtp_s = calloc(1, sizeof(struct zmtp_socket));
   pico_s = calloc(1, sizeof(struct pico_socket));
   /*----=== Test empty sockets ===----*/
   TEST_ASSERT_EQUAL_INT(zmtp_socket_close(NULL), -1);
   TEST_ASSERT_EQUAL_INT(zmtp_socket_close(zmtp_s),-1);

   zmtp_s->sock = pico_s;
   pico_socket_close_IgnoreAndReturn(-1);
   TEST_ASSERT_EQUAL_INT(zmtp_socket_close(zmtp_s), -1);
   /*----=== Test valid arguments ===----*/
   pico_socket_close_IgnoreAndReturn(0);
   TEST_ASSERT_EQUAL_INT(zmtp_socket_close(zmtp_s), 0);

   free(zmtp_s);
   free(pico_s);
}

void test_zmtp_socket_read(void)
{
   struct zmtp_socket* zmtp_s;
   struct pico_socket* pico_s;
   int buffLen = 20;
   char buff[buffLen];
   zmtp_s = calloc(1, sizeof(struct zmtp_socket));
   pico_s = calloc(1, sizeof(struct pico_socket));
   /*----=== Test empty sockets ===----*/
   TEST_ASSERT_EQUAL_INT(zmtp_socket_read(NULL, (void*)buff, buffLen), -1);
   TEST_ASSERT_EQUAL_INT(zmtp_socket_read(zmtp_s, (void*)buff, buffLen), -1);
   /*invalid buff or buffLen should be handled by pico_socket*/

   zmtp_s->sock = pico_s;
   pico_socket_read_IgnoreAndReturn(-1);
   TEST_ASSERT_EQUAL_INT(zmtp_socket_read(zmtp_s, (void*)buff, buffLen), -1);
   /*----=== Test valid arguments ===----*/
   pico_socket_read_IgnoreAndReturn(0);
   TEST_ASSERT_EQUAL_INT(zmtp_socket_read(zmtp_s, (void*)buff, buffLen), 0);

   free(zmtp_s);
   free(pico_s);
}

